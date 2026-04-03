#include "window.h"
#include "containers_table.h"
#include "images_table.h"
#include "networks_table.h"
#include "volumes_table.h"
#include "../models/container.h"
#include "../models/image.h"
#include "../models/network.h"
#include "../models/volume.h"
#include "../docker/docker_command.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <pango/pango.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define CHART_MAX_POINTS 60
#define CHART_MAX_LINES 32
#define CHART_HEIGHT 80
#define CHART_DATA_INTERVAL_MS 3000   // Intervalo de coleta de dados (ms)
#define CHART_ANIM_INTERVAL_MS 50     // Intervalo de animação (~20fps)

typedef struct {
    gdouble r, g, b;
} ChartColor;

typedef struct {
    gchar name[256];
    gdouble points[CHART_MAX_POINTS];
    ChartColor color;
    gboolean active;
} CpuChartLine;

typedef struct {
    GtkWidget* drawing_area;
    GtkWidget* value_label;
    CpuChartLine lines[CHART_MAX_LINES];
    gint num_lines;
    gint num_points;
    gdouble anim_offset;      // 0.0 → 1.0, progresso do scroll entre coletas
    gint64 total_updates;     // Total de coletas realizadas (para scroll do grid)
    guint anim_timer_id;      // Timer de animação (50ms)
} CpuChartData;

typedef struct {
    GtkWidget* drawing_area;
    GtkWidget* value_label;
    gdouble points[CHART_MAX_POINTS];
    gint num_points;
    gdouble system_total_memory;
    gdouble anim_offset;      // 0.0 → 1.0, progresso do scroll entre coletas
    gint64 total_updates;     // Total de coletas realizadas (para scroll do grid)
    guint anim_timer_id;      // Timer de animação (50ms)
} MemoryChartData;

typedef struct {
    GtkWidget* drawing_area;
    GtkWidget* value_label;
    gdouble read_points[CHART_MAX_POINTS];
    gdouble write_points[CHART_MAX_POINTS];
    gint num_points;
    gdouble anim_offset;      // 0.0 → 1.0, progresso do scroll entre coletas
    gint64 total_updates;     // Total de coletas realizadas (para scroll do grid)
    guint anim_timer_id;      // Timer de animação (50ms)
} DiskIOChartData;

typedef struct {
    GtkWidget* drawing_area;
    GtkWidget* value_label;
    gdouble received_points[CHART_MAX_POINTS];
    gdouble sent_points[CHART_MAX_POINTS];
    gint num_points;
    gdouble anim_offset;      // 0.0 → 1.0, progresso do scroll entre coletas
    gint64 total_updates;     // Total de coletas realizadas (para scroll do grid)
    guint anim_timer_id;      // Timer de animação (50ms)
} NetworkIOChartData;

static void on_window_close(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
}
typedef struct {
    GtkStack *stack;
    GtkWidget *containers_button;
    GtkWidget *images_button;
    GtkWidget *networks_button;
    GtkWidget *volumes_button;
} ViewSwitcherData;
typedef struct {
    GtkTreeStore *containers_store;
    GtkWidget *containers_tree_view;  // Referência ao tree_view dos containers para preservar expansão
    GtkListStore *images_store;
    GtkListStore *networks_store;
    GtkListStore *volumes_store;
} TableStoresData;
static void update_button_icon(GtkWidget *button);
static void on_stack_visible_child_changed(GtkStack *stack, GParamSpec *pspec, gpointer user_data) {
    ViewSwitcherData *data = (ViewSwitcherData *)user_data;
    const gchar *visible_name = gtk_stack_get_visible_child_name(stack);
    gtk_style_context_remove_class(gtk_widget_get_style_context(data->containers_button), "suggested-action");
    gtk_style_context_remove_class(gtk_widget_get_style_context(data->images_button), "suggested-action");
    gtk_style_context_remove_class(gtk_widget_get_style_context(data->networks_button), "suggested-action");
    gtk_style_context_remove_class(gtk_widget_get_style_context(data->volumes_button), "suggested-action");
    if (g_strcmp0(visible_name, "containers") == 0) {
        gtk_style_context_add_class(gtk_widget_get_style_context(data->containers_button), "suggested-action");
    } else if (g_strcmp0(visible_name, "images") == 0) {
        gtk_style_context_add_class(gtk_widget_get_style_context(data->images_button), "suggested-action");
    } else if (g_strcmp0(visible_name, "networks") == 0) {
        gtk_style_context_add_class(gtk_widget_get_style_context(data->networks_button), "suggested-action");
    } else if (g_strcmp0(visible_name, "volumes") == 0) {
        gtk_style_context_add_class(gtk_widget_get_style_context(data->volumes_button), "suggested-action");
    }
    update_button_icon(data->containers_button);
    update_button_icon(data->images_button);
    update_button_icon(data->networks_button);
    update_button_icon(data->volumes_button);
}
static void on_view_switch(GtkButton *button, gpointer user_data) {
    GtkStack *stack = GTK_STACK(user_data);
    const gchar *view_name = (const gchar *)g_object_get_data(G_OBJECT(button), "view-name");
    if (view_name) {
        gtk_stack_set_visible_child_name(stack, view_name);
    }
}
static gchar* get_icon_path(const gchar *icon_filename) {
    gchar *relative_path = g_build_filename("assets", "icons", icon_filename, NULL);
    if (g_file_test(relative_path, G_FILE_TEST_EXISTS)) {
        return relative_path;
    }
    g_free(relative_path);
    gchar *install_path = g_build_filename("/usr/share/dodo", "assets", "icons", icon_filename, NULL);
    if (g_file_test(install_path, G_FILE_TEST_EXISTS)) {
        return install_path;
    }
    g_free(install_path);
    
    return NULL;
}
static gboolean is_dark_theme(void) {
    GtkSettings *settings = gtk_settings_get_default();
    if (!settings) {
        return FALSE;
    }
    gboolean prefer_dark = FALSE;
    g_object_get(settings, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
    if (prefer_dark) return TRUE;
    gchar *theme_name = NULL;
    g_object_get(settings, "gtk-theme-name", &theme_name, NULL);
    if (theme_name) {
        gchar *lower = g_ascii_strdown(theme_name, -1);
        gboolean has_dark = (g_strrstr(lower, "dark") != NULL);
        g_free(lower);
        g_free(theme_name);
        if (has_dark) return TRUE;
    }
    GSettingsSchemaSource *source = g_settings_schema_source_get_default();
    if (source) {
        GSettingsSchema *schema = g_settings_schema_source_lookup(
            source, "org.gnome.desktop.interface", TRUE);
        if (schema) {
            if (g_settings_schema_has_key(schema, "color-scheme")) {
                GSettings *gsettings = g_settings_new("org.gnome.desktop.interface");
                gchar *color_scheme = g_settings_get_string(gsettings, "color-scheme");
                gboolean is_dark = (g_strcmp0(color_scheme, "prefer-dark") == 0);
                g_free(color_scheme);
                g_object_unref(gsettings);
                g_settings_schema_unref(schema);
                return is_dark;
            }
            g_settings_schema_unref(schema);
        }
    }
    GtkWidgetPath *path = gtk_widget_path_new();
    gtk_widget_path_append_type(path, GTK_TYPE_WINDOW);
    GtkStyleContext *ctx = gtk_style_context_new();
    gtk_style_context_set_path(ctx, path);
    gtk_style_context_set_screen(ctx, gdk_screen_get_default());

    GdkRGBA fg_color;
    gtk_style_context_get_color(ctx, GTK_STATE_FLAG_NORMAL, &fg_color);

    g_object_unref(ctx);
    gtk_widget_path_free(path);

    double luminance = 0.299 * fg_color.red + 0.587 * fg_color.green + 0.114 * fg_color.blue;
    return luminance > 0.5;
}
static void load_icon_for_theme(GtkWidget *image, const gchar *base_name) {
    const gchar *suffix = is_dark_theme() ? ".svg" : "-dark.svg";
    gchar *icon_filename = g_strconcat(base_name, suffix, NULL);
    gchar *svg_path = get_icon_path(icon_filename);
    g_free(icon_filename);

    if (svg_path && g_file_test(svg_path, G_FILE_TEST_EXISTS)) {
        GError *error = NULL;
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(svg_path, &error);
        if (pixbuf) {
            GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
            gtk_image_set_from_pixbuf(GTK_IMAGE(image), scaled);
            g_object_unref(scaled);
            g_object_unref(pixbuf);
        }
        if (error) g_error_free(error);
    }

    g_free(svg_path);
}
static void update_button_icon(GtkWidget *button) {
    GtkWidget *image = GTK_WIDGET(g_object_get_data(G_OBJECT(button), "icon-image"));
    const gchar *base_name = (const gchar *)g_object_get_data(G_OBJECT(button), "icon-base-name");
    if (!image || !base_name) return;

    gboolean is_selected = gtk_style_context_has_class(
        gtk_widget_get_style_context(button), "suggested-action");
    gboolean use_white = is_selected || is_dark_theme();
    const gchar *suffix = use_white ? ".svg" : "-dark.svg";

    gchar *icon_filename = g_strconcat(base_name, suffix, NULL);
    gchar *svg_path = get_icon_path(icon_filename);
    g_free(icon_filename);

    if (svg_path && g_file_test(svg_path, G_FILE_TEST_EXISTS)) {
        GError *error = NULL;
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(svg_path, &error);
        if (pixbuf) {
            GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
            gtk_image_set_from_pixbuf(GTK_IMAGE(image), scaled);
            g_object_unref(scaled);
            g_object_unref(pixbuf);
        }
        if (error) g_error_free(error);
    }

    g_free(svg_path);
}
typedef struct {
    GtkWidget *buttons[4];
    int count;
} ThemeIconsData;
static void on_theme_icons_update(ThemeIconsData *data) {
    for (int i = 0; i < data->count; i++) {
        update_button_icon(data->buttons[i]);
    }
}
static void on_gtk_theme_name_changed(GObject *object, GParamSpec *pspec, gpointer user_data) {
    on_theme_icons_update((ThemeIconsData *)user_data);
}
static void on_color_scheme_changed(GSettings *settings, gchar *key, gpointer user_data) {
    on_theme_icons_update((ThemeIconsData *)user_data);
}
static GtkWidget* create_view_button(const gchar *icon_base_name, const gchar *label_text, 
                                     const gchar *view_name, GtkStack *stack) {
    GtkWidget *button = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *icon = gtk_image_new();
    GtkWidget *label = gtk_label_new(label_text);
    load_icon_for_theme(icon, icon_base_name);
    g_object_set_data(G_OBJECT(button), "icon-image", icon);
    g_object_set_data_full(G_OBJECT(button), "icon-base-name",
                           g_strdup(icon_base_name), g_free);
    gtk_label_set_xalign(GTK_LABEL(label), 0.5);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_margin_top(box, 4);
    gtk_widget_set_margin_bottom(box, 4);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button), box);
    g_object_set_data(G_OBJECT(button), "view-name", (gpointer)view_name);
    g_signal_connect(button, "clicked", G_CALLBACK(on_view_switch), stack);
    
    return button;
}
static gpointer get_store_from_scrolled_window(GtkWidget *widget, gboolean is_tree_store) {
    GtkWidget *scrolled_window = widget;
    if (GTK_IS_BOX(widget)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList *l = children; l != NULL; l = l->next) {
            if (GTK_IS_SCROLLED_WINDOW(l->data)) {
                scrolled_window = GTK_WIDGET(l->data);
                break;
            }
        }
        g_list_free(children);
    }
    
    if (!GTK_IS_SCROLLED_WINDOW(scrolled_window)) {
        return NULL;
    }
    
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(scrolled_window));
    if (GTK_IS_TREE_VIEW(child)) {
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(child));
        if (GTK_IS_TREE_MODEL_SORT(model)) {
            model = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(model));
        }
        if (GTK_IS_TREE_MODEL_FILTER(model)) {
            model = gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(model));
        }
        
        if (is_tree_store && GTK_IS_TREE_STORE(model)) {
            return GTK_TREE_STORE(model);
        } else if (!is_tree_store && GTK_IS_LIST_STORE(model)) {
            return GTK_LIST_STORE(model);
        }
    }
    return NULL;
}
static GtkWidget* get_tree_view_from_widget(GtkWidget *widget) {
    GtkWidget *scrolled_window = widget;
    if (GTK_IS_BOX(widget)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
        for (GList *l = children; l != NULL; l = l->next) {
            if (GTK_IS_SCROLLED_WINDOW(l->data)) {
                scrolled_window = GTK_WIDGET(l->data);
                break;
            }
        }
        g_list_free(children);
    }
    
    if (!GTK_IS_SCROLLED_WINDOW(scrolled_window)) {
        return NULL;
    }
    
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(scrolled_window));
    if (GTK_IS_TREE_VIEW(child)) {
        return child;
    }
    return NULL;
}
static void on_refresh_clicked(GtkButton *button, gpointer user_data) {
    TableStoresData *data = (TableStoresData *)user_data;
    if (data->containers_store) {
        refresh_containers_table_async(data->containers_store, data->containers_tree_view);
    }
    if (data->images_store) {
        refresh_images_table_async(data->images_store);
    }
    if (data->networks_store) {
        refresh_networks_table_async(data->networks_store);
    }
    if (data->volumes_store) {
        refresh_volumes_table_async(data->volumes_store);
    }
}
static gboolean on_legend_color_draw(GtkWidget* widget, cairo_t* cr, gpointer user_data) {
    gdouble* color = (gdouble*)g_object_get_data(G_OBJECT(widget), "legend-color");
    if (color) {
        gint width = gtk_widget_get_allocated_width(widget);
        gint height = gtk_widget_get_allocated_height(widget);
        gint size = (width < height) ? width : height;
        cairo_set_source_rgba(cr, color[0], color[1], color[2], 0.9);
        cairo_rectangle(cr, 0, 0, size, size);
        cairo_fill(cr);
    }
    return FALSE;
}
static GtkWidget* create_legend_item(const gchar* text, gdouble r, gdouble g, gdouble b) {
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_end(box, 6);
    GtkWidget* color_box = gtk_drawing_area_new();
    gtk_widget_set_size_request(color_box, 6, 6);
    gtk_widget_set_valign(color_box, GTK_ALIGN_CENTER);
    gdouble* color = g_new(gdouble, 3);
    color[0] = r;
    color[1] = g;
    color[2] = b;
    g_object_set_data_full(G_OBJECT(color_box), "legend-color", color, g_free);
    g_signal_connect(color_box, "draw", G_CALLBACK(on_legend_color_draw), NULL);
    GtkWidget* label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    
    gtk_box_pack_start(GTK_BOX(box), color_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    
    return box;
}
static gdouble hue_to_rgb_component(gdouble p, gdouble q, gdouble t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0 / 2.0) return q;
    if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
    return p;
}
static ChartColor generate_chart_color(gint index) {
    gdouble hue = fmod(index * 0.618033988749895, 1.0);
    gdouble saturation = 0.75;
    gdouble lightness = 0.55;

    gdouble q = lightness < 0.5 ? lightness * (1.0 + saturation)
                                : lightness + saturation - lightness * saturation;
    gdouble p = 2.0 * lightness - q;

    ChartColor color;
    color.r = hue_to_rgb_component(p, q, hue + 1.0 / 3.0);
    color.g = hue_to_rgb_component(p, q, hue);
    color.b = hue_to_rgb_component(p, q, hue - 1.0 / 3.0);

    return color;
}
static void draw_chart_grid(cairo_t *cr, gint width, gint height,
                            gdouble scroll_offset_px) {
    cairo_set_line_width(cr, 0.5);
    cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 0.15);
    for (int i = 0; i <= 4; i++) {
        gdouble y = (gdouble)i * height / 4.0;
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
    }
    gdouble grid_spacing = (gdouble)width / 6.0;
    gdouble grid_offset = fmod(scroll_offset_px, grid_spacing);
    for (gdouble x = width - grid_offset; x >= -1.0; x -= grid_spacing) {
        if (x >= 0.0 && x <= (gdouble)width) {
            cairo_move_to(cr, x, 0);
            cairo_line_to(cr, x, height);
        }
    }

    cairo_stroke(cr);
}
static void draw_smooth_line_points(cairo_t *cr, gdouble *x, gdouble *y, gint n) {
    if (n < 2) return;

    cairo_move_to(cr, x[0], y[0]);

    if (n == 2) {
        cairo_line_to(cr, x[1], y[1]);
        return;
    }

    for (gint i = 0; i < n - 1; i++) {
        gdouble x0 = (i > 0) ? x[i - 1] : x[i];
        gdouble y0 = (i > 0) ? y[i - 1] : y[i];
        gdouble x1 = x[i];
        gdouble y1 = y[i];
        gdouble x2 = x[i + 1];
        gdouble y2 = y[i + 1];
        gdouble x3 = (i + 2 < n) ? x[i + 2] : x[i + 1];
        gdouble y3 = (i + 2 < n) ? y[i + 2] : y[i + 1];
        gdouble cp1x = x1 + (x2 - x0) / 6.0;
        gdouble cp1y = y1 + (y2 - y0) / 6.0;
        gdouble cp2x = x2 - (x3 - x1) / 6.0;
        gdouble cp2y = y2 - (y3 - y1) / 6.0;

        cairo_curve_to(cr, cp1x, cp1y, cp2x, cp2y, x2, y2);
    }
}
static gboolean on_cpu_chart_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    CpuChartData *chart = (CpuChartData*)user_data;

    gint width = gtk_widget_get_allocated_width(widget);
    gint height = gtk_widget_get_allocated_height(widget);
    gdouble slot_width = (gdouble)width / (CHART_MAX_POINTS - 1);
    gdouble scroll_total_px = (chart->total_updates + chart->anim_offset) * slot_width;
    draw_chart_grid(cr, width, height, scroll_total_px);

    if (chart->num_points < 2) return FALSE;
    gdouble max_val = 100.0;
    for (gint l = 0; l < chart->num_lines; l++) {
        if (!chart->lines[l].active) continue;
        for (gint p = 0; p < chart->num_points; p++) {
            if (chart->lines[l].points[p] > max_val) {
                max_val = chart->lines[l].points[p];
            }
        }
    }
    max_val *= 1.1;

    gdouble scroll_px = chart->anim_offset * slot_width;
    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_clip(cr);
    for (gint l = 0; l < chart->num_lines; l++) {
        if (!chart->lines[l].active) continue;

        gint n = chart->num_points;
        gdouble x_vals[CHART_MAX_POINTS + 1];
        gdouble y_vals[CHART_MAX_POINTS + 1];

        for (gint p = 0; p < n; p++) {
            gdouble base_x = (CHART_MAX_POINTS - 1 - (n - 1 - p)) * slot_width;
            x_vals[p] = base_x - scroll_px;
            y_vals[p] = height - (chart->lines[l].points[p] / max_val) * height;
            if (y_vals[p] < 0) y_vals[p] = 0;
            if (y_vals[p] > height) y_vals[p] = height;
        }
        gint draw_n = n;
        if (chart->anim_offset > 0.01) {
            x_vals[n] = (gdouble)width;
            y_vals[n] = y_vals[n - 1];
            draw_n = n + 1;
        }

        cairo_set_source_rgba(cr, chart->lines[l].color.r,
                              chart->lines[l].color.g,
                              chart->lines[l].color.b, 0.9);
        cairo_set_line_width(cr, 2.0);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        draw_smooth_line_points(cr, x_vals, y_vals, draw_n);
        cairo_stroke(cr);
    }

    cairo_restore(cr);
    return FALSE;
}
static gboolean on_memory_chart_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    MemoryChartData *chart = (MemoryChartData*)user_data;

    gint width = gtk_widget_get_allocated_width(widget);
    gint height = gtk_widget_get_allocated_height(widget);
    gdouble slot_width = (gdouble)width / (CHART_MAX_POINTS - 1);
    gdouble scroll_total_px = (chart->total_updates + chart->anim_offset) * slot_width;
    draw_chart_grid(cr, width, height, scroll_total_px);

    if (chart->num_points < 2) return FALSE;
    gdouble max_val = chart->system_total_memory;
    if (max_val <= 0) {
        max_val = 1.0;
        for (gint p = 0; p < chart->num_points; p++) {
            if (chart->points[p] > max_val) max_val = chart->points[p];
        }
        max_val *= 1.1;
    }

    gint n = chart->num_points;
    gdouble scroll_px = chart->anim_offset * slot_width;
    gdouble x_vals[CHART_MAX_POINTS + 1];
    gdouble y_vals[CHART_MAX_POINTS + 1];

    for (gint p = 0; p < n; p++) {
        gdouble base_x = (CHART_MAX_POINTS - 1 - (n - 1 - p)) * slot_width;
        x_vals[p] = base_x - scroll_px;
        y_vals[p] = height - (chart->points[p] / max_val) * height;
        if (y_vals[p] < 0) y_vals[p] = 0;
        if (y_vals[p] > height) y_vals[p] = height;
    }
    gint draw_n = n;
    if (chart->anim_offset > 0.01) {
        x_vals[n] = (gdouble)width;
        y_vals[n] = y_vals[n - 1];
        draw_n = n + 1;
    }
    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_clip(cr);
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.1);
    draw_smooth_line_points(cr, x_vals, y_vals, draw_n);
    cairo_line_to(cr, x_vals[draw_n - 1], height);
    cairo_line_to(cr, x_vals[0], height);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    draw_smooth_line_points(cr, x_vals, y_vals, draw_n);
    cairo_stroke(cr);

    cairo_restore(cr);
    return FALSE;
}
static gboolean on_diskio_chart_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    DiskIOChartData *chart = (DiskIOChartData*)user_data;

    gint width = gtk_widget_get_allocated_width(widget);
    gint height = gtk_widget_get_allocated_height(widget);
    gdouble slot_width = (gdouble)width / (CHART_MAX_POINTS - 1);
    gdouble scroll_total_px = (chart->total_updates + chart->anim_offset) * slot_width;
    draw_chart_grid(cr, width, height, scroll_total_px);

    if (chart->num_points < 2) return FALSE;
    gdouble max_val = 1.0;
    for (gint p = 0; p < chart->num_points; p++) {
        if (chart->read_points[p] > max_val) {
            max_val = chart->read_points[p];
        }
        if (chart->write_points[p] > max_val) {
            max_val = chart->write_points[p];
        }
    }
    max_val *= 1.1;  // Adiciona 10% de margem no topo
    if (max_val <= 0) max_val = 1.0;

    gint n = chart->num_points;
    gdouble scroll_px = chart->anim_offset * slot_width;
    gdouble x_vals[CHART_MAX_POINTS + 1];
    gdouble read_y_vals[CHART_MAX_POINTS + 1];
    gdouble write_y_vals[CHART_MAX_POINTS + 1];

    for (gint p = 0; p < n; p++) {
        gdouble base_x = (CHART_MAX_POINTS - 1 - (n - 1 - p)) * slot_width;
        x_vals[p] = base_x - scroll_px;
        read_y_vals[p] = height - (chart->read_points[p] / max_val) * height;
        write_y_vals[p] = height - (chart->write_points[p] / max_val) * height;
        if (read_y_vals[p] < 0) read_y_vals[p] = 0;
        if (read_y_vals[p] > height) read_y_vals[p] = height;
        if (write_y_vals[p] < 0) write_y_vals[p] = 0;
        if (write_y_vals[p] > height) write_y_vals[p] = height;
    }
    gint draw_n = n;
    if (chart->anim_offset > 0.01) {
        x_vals[n] = (gdouble)width;
        read_y_vals[n] = read_y_vals[n - 1];
        write_y_vals[n] = write_y_vals[n - 1];
        draw_n = n + 1;
    }
    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_clip(cr);
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.1);
    draw_smooth_line_points(cr, x_vals, read_y_vals, draw_n);
    cairo_line_to(cr, x_vals[draw_n - 1], height);
    cairo_line_to(cr, x_vals[0], height);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.2, 0.6, 1.0, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    draw_smooth_line_points(cr, x_vals, read_y_vals, draw_n);
    cairo_stroke(cr);
    cairo_set_source_rgba(cr, 1.0, 0.6, 0.2, 0.1);
    draw_smooth_line_points(cr, x_vals, write_y_vals, draw_n);
    cairo_line_to(cr, x_vals[draw_n - 1], height);
    cairo_line_to(cr, x_vals[0], height);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 1.0, 0.6, 0.2, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    draw_smooth_line_points(cr, x_vals, write_y_vals, draw_n);
    cairo_stroke(cr);

    cairo_restore(cr);
    return FALSE;
}
static void on_cpu_chart_stats_received(gchar* output, gpointer user_data) {
    CpuChartData* chart = (CpuChartData*)user_data;

    if (chart == NULL || chart->value_label == NULL) {
        g_free(output);
        return;
    }
    gboolean line_seen[CHART_MAX_LINES];
    memset(line_seen, 0, sizeof(line_seen));
    gdouble total_cpu = 0.0;

    if (output != NULL && strlen(output) > 0) {
        gchar** lines = g_strsplit(output, "\n", -1);

        for (gint i = 0; lines[i] != NULL; i++) {
            gchar* line = g_strstrip(lines[i]);
            if (strlen(line) == 0) continue;
            gchar** parts = g_strsplit(line, ":::", 2);
            if (parts[0] == NULL || parts[1] == NULL) {
                g_strfreev(parts);
                continue;
            }

            gchar* name = g_strstrip(parts[0]);
            gchar* cpu_str = g_strstrip(parts[1]);
            gchar* percent_pos = strchr(cpu_str, '%');
            if (percent_pos) *percent_pos = '\0';

            gdouble cpu_value = g_strtod(cpu_str, NULL);
            total_cpu += cpu_value;
            gint line_idx = -1;
            for (gint l = 0; l < chart->num_lines; l++) {
                if (g_strcmp0(chart->lines[l].name, name) == 0) {
                    line_idx = l;
                    break;
                }
            }

            if (line_idx == -1 && chart->num_lines < CHART_MAX_LINES) {
                line_idx = chart->num_lines;
                g_strlcpy(chart->lines[line_idx].name, name, 256);
                chart->lines[line_idx].color = generate_chart_color(chart->num_lines);
                chart->lines[line_idx].active = TRUE;
                memset(chart->lines[line_idx].points, 0, sizeof(gdouble) * CHART_MAX_POINTS);
                chart->num_lines++;
            }

            if (line_idx >= 0) {
                line_seen[line_idx] = TRUE;
                chart->lines[line_idx].active = TRUE;
                if (chart->num_points < CHART_MAX_POINTS) {
                    chart->lines[line_idx].points[chart->num_points] = cpu_value;
                } else {
                    memmove(chart->lines[line_idx].points,
                            chart->lines[line_idx].points + 1,
                            (CHART_MAX_POINTS - 1) * sizeof(gdouble));
                    chart->lines[line_idx].points[CHART_MAX_POINTS - 1] = cpu_value;
                }
            }

            g_strfreev(parts);
        }

        g_strfreev(lines);
    }
    for (gint l = 0; l < chart->num_lines; l++) {
        if (!line_seen[l]) {
            if (chart->num_points < CHART_MAX_POINTS) {
                chart->lines[l].points[chart->num_points] = 0.0;
            } else {
                memmove(chart->lines[l].points,
                        chart->lines[l].points + 1,
                        (CHART_MAX_POINTS - 1) * sizeof(gdouble));
                chart->lines[l].points[CHART_MAX_POINTS - 1] = 0.0;
            }
        }
    }
    if (chart->num_points < CHART_MAX_POINTS) {
        chart->num_points++;
    }
    gchar* cpu_text = g_strdup_printf("%.1f%%", total_cpu);
    gtk_label_set_text(GTK_LABEL(chart->value_label), cpu_text);
    g_free(cpu_text);
    chart->anim_offset = 0.0;
    chart->total_updates++;
    if (chart->drawing_area) {
        gtk_widget_queue_draw(chart->drawing_area);
    }

    g_free(output);
}
static void update_cpu_chart_stats(CpuChartData* chart) {
    if (chart == NULL) return;
    execute_command_async("docker stats --no-stream --format '{{.Name}}:::{{.CPUPerc}}'",
                          on_cpu_chart_stats_received, chart);
}
static gboolean update_cpu_chart_stats_timer(gpointer user_data) {
    update_cpu_chart_stats((CpuChartData*)user_data);
    return TRUE;
}
static void format_memory(gdouble bytes, gchar** value, gchar** unit) {
    if (bytes < 1024.0 * 1024.0) {
        *value = g_strdup_printf("%.2f", bytes / 1024.0);
        *unit = g_strdup("KB");
    } else if (bytes < 1024.0 * 1024.0 * 1024.0) {
        *value = g_strdup_printf("%.2f", bytes / (1024.0 * 1024.0));
        *unit = g_strdup("MB");
    } else {
        *value = g_strdup_printf("%.2f", bytes / (1024.0 * 1024.0 * 1024.0));
        *unit = g_strdup("GB");
    }
}
static gdouble parse_docker_memory(const gchar* mem_str) {
    if (mem_str == NULL || strlen(mem_str) == 0) {
        return 0.0;
    }
    
    gchar* str = g_strdup(mem_str);
    g_strstrip(str);
    gchar* lower = g_ascii_strdown(str, -1);
    g_free(str);
    
    gchar* endptr;
    gdouble value = g_strtod(lower, &endptr);
    
    if (endptr == lower) {
        g_free(lower);
        return 0.0;
    }
    gdouble multiplier = 1.0;
    if (g_str_has_suffix(endptr, "kib") || g_str_has_suffix(endptr, "kb")) {
        multiplier = 1024.0;
    } else if (g_str_has_suffix(endptr, "mib") || g_str_has_suffix(endptr, "mb")) {
        multiplier = 1024.0 * 1024.0;
    } else if (g_str_has_suffix(endptr, "gib") || g_str_has_suffix(endptr, "gb")) {
        multiplier = 1024.0 * 1024.0 * 1024.0;
    } else if (g_str_has_suffix(endptr, "tib") || g_str_has_suffix(endptr, "tb")) {
        multiplier = 1024.0 * 1024.0 * 1024.0 * 1024.0;
    }
    
    g_free(lower);
    return value * multiplier;
}
static gdouble calculate_total_memory_usage(gchar* output) {
    if (output == NULL || strlen(output) == 0) {
        return 0.0;
    }
    
    gdouble total_memory = 0.0;
    gchar** lines = g_strsplit(output, "\n", -1);
    
    for (gint i = 0; lines[i] != NULL; i++) {
        gchar* line = g_strstrip(lines[i]);
        if (strlen(line) == 0) continue;
        gchar** parts = g_strsplit(line, "/", 2);
        if (parts[0] != NULL) {
            gdouble mem_value = parse_docker_memory(parts[0]);
            total_memory += mem_value;
        }
        g_strfreev(parts);
    }
    
    g_strfreev(lines);
    return total_memory;
}
static gdouble get_system_total_memory(void) {
    GError* error = NULL;
    gchar* contents = NULL;
    gsize length = 0;
    
    if (!g_file_get_contents("/proc/meminfo", &contents, &length, &error)) {
        if (error) {
            g_warning("Erro ao ler /proc/meminfo: %s", error->message);
            g_error_free(error);
        }
        return 0.0;
    }
    gchar** lines = g_strsplit(contents, "\n", -1);
    g_free(contents);
    
    gdouble total_mem_kb = 0.0;
    for (gint i = 0; lines[i] != NULL; i++) {
        if (g_str_has_prefix(lines[i], "MemTotal:")) {
            gchar** parts = g_strsplit(lines[i], ":", 2);
            if (parts[1] != NULL) {
                gchar* value_str = g_strstrip(parts[1]);
                gchar* endptr;
                total_mem_kb = g_strtod(value_str, &endptr);
                if (endptr == value_str || total_mem_kb <= 0.0) {
                    total_mem_kb = 0.0;
                }
            }
            g_strfreev(parts);
            break;
        }
    }
    
    g_strfreev(lines);
    return total_mem_kb * 1024.0;
}
static void parse_docker_blockio(const gchar* blockio_str, gdouble* read_mb, gdouble* write_mb) {
    *read_mb = 0.0;
    *write_mb = 0.0;
    
    if (blockio_str == NULL || strlen(blockio_str) == 0) {
        return;
    }
    
    gchar* str = g_strdup(blockio_str);
    g_strstrip(str);
    gchar** parts = g_strsplit(str, "/", 2);
    g_free(str);
    
    if (parts[0] != NULL) {
        gchar* read_str = g_strstrip(parts[0]);
        gchar* lower_read = g_ascii_strdown(read_str, -1);
        gchar* endptr;
        gdouble read_value = g_strtod(lower_read, &endptr);
        
        if (endptr != lower_read) {
            gdouble multiplier = 1.0;
            gchar* trimmed_endptr = g_strstrip(endptr);
            if (g_str_has_suffix(trimmed_endptr, "tib") || g_str_has_suffix(trimmed_endptr, "tb")) {
                multiplier = 1024.0 * 1024.0;  // TB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "gib") || g_str_has_suffix(trimmed_endptr, "gb")) {
                multiplier = 1024.0;  // GB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "mib") || g_str_has_suffix(trimmed_endptr, "mb")) {
                multiplier = 1.0;  // MB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "kib") || g_str_has_suffix(trimmed_endptr, "kb")) {
                multiplier = 1.0 / 1024.0;  // KB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "b")) {
                multiplier = 1.0 / (1024.0 * 1024.0);  // B → MB
            }
            *read_mb = read_value * multiplier;
        }
        g_free(lower_read);
    }
    
    if (parts[1] != NULL) {
        gchar* write_str = g_strstrip(parts[1]);
        gchar* lower_write = g_ascii_strdown(write_str, -1);
        gchar* endptr;
        gdouble write_value = g_strtod(lower_write, &endptr);
        
        if (endptr != lower_write) {
            gdouble multiplier = 1.0;
            gchar* trimmed_endptr = g_strstrip(endptr);
            if (g_str_has_suffix(trimmed_endptr, "tib") || g_str_has_suffix(trimmed_endptr, "tb")) {
                multiplier = 1024.0 * 1024.0;  // TB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "gib") || g_str_has_suffix(trimmed_endptr, "gb")) {
                multiplier = 1024.0;  // GB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "mib") || g_str_has_suffix(trimmed_endptr, "mb")) {
                multiplier = 1.0;  // MB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "kib") || g_str_has_suffix(trimmed_endptr, "kb")) {
                multiplier = 1.0 / 1024.0;  // KB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "b")) {
                multiplier = 1.0 / (1024.0 * 1024.0);  // B → MB
            }
            *write_mb = write_value * multiplier;
        }
        g_free(lower_write);
    }
    
    g_strfreev(parts);
}
static void parse_docker_netio(const gchar* netio_str, gdouble* received_mb, gdouble* sent_mb) {
    *received_mb = 0.0;
    *sent_mb = 0.0;
    
    if (netio_str == NULL || strlen(netio_str) == 0) {
        return;
    }
    
    gchar* str = g_strdup(netio_str);
    g_strstrip(str);
    gchar** parts = g_strsplit(str, "/", 2);
    g_free(str);
    
    if (parts[0] != NULL) {
        gchar* received_str = g_strstrip(parts[0]);
        gchar* lower_received = g_ascii_strdown(received_str, -1);
        gchar* endptr;
        gdouble received_value = g_strtod(lower_received, &endptr);
        
        if (endptr != lower_received) {
            gdouble multiplier = 1.0;
            gchar* trimmed_endptr = g_strstrip(endptr);
            if (g_str_has_suffix(trimmed_endptr, "tib") || g_str_has_suffix(trimmed_endptr, "tb")) {
                multiplier = 1024.0 * 1024.0;  // TB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "gib") || g_str_has_suffix(trimmed_endptr, "gb")) {
                multiplier = 1024.0;  // GB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "mib") || g_str_has_suffix(trimmed_endptr, "mb")) {
                multiplier = 1.0;  // MB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "kib") || g_str_has_suffix(trimmed_endptr, "kb")) {
                multiplier = 1.0 / 1024.0;  // KB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "b")) {
                multiplier = 1.0 / (1024.0 * 1024.0);  // B → MB
            }
            *received_mb = received_value * multiplier;
        }
        g_free(lower_received);
    }
    
    if (parts[1] != NULL) {
        gchar* sent_str = g_strstrip(parts[1]);
        gchar* lower_sent = g_ascii_strdown(sent_str, -1);
        gchar* endptr;
        gdouble sent_value = g_strtod(lower_sent, &endptr);
        
        if (endptr != lower_sent) {
            gdouble multiplier = 1.0;
            gchar* trimmed_endptr = g_strstrip(endptr);
            if (g_str_has_suffix(trimmed_endptr, "tib") || g_str_has_suffix(trimmed_endptr, "tb")) {
                multiplier = 1024.0 * 1024.0;  // TB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "gib") || g_str_has_suffix(trimmed_endptr, "gb")) {
                multiplier = 1024.0;  // GB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "mib") || g_str_has_suffix(trimmed_endptr, "mb")) {
                multiplier = 1.0;  // MB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "kib") || g_str_has_suffix(trimmed_endptr, "kb")) {
                multiplier = 1.0 / 1024.0;  // KB → MB
            } else if (g_str_has_suffix(trimmed_endptr, "b")) {
                multiplier = 1.0 / (1024.0 * 1024.0);  // B → MB
            }
            *sent_mb = sent_value * multiplier;
        }
        g_free(lower_sent);
    }
    
    g_strfreev(parts);
}
static void calculate_total_network_io(gchar* output, gdouble* total_received_mb, gdouble* total_sent_mb) {
    *total_received_mb = 0.0;
    *total_sent_mb = 0.0;
    
    if (output == NULL || strlen(output) == 0) {
        return;
    }
    
    gchar** lines = g_strsplit(output, "\n", -1);
    
    for (gint i = 0; lines[i] != NULL; i++) {
        gchar* line = g_strstrip(lines[i]);
        if (strlen(line) == 0) continue;
        
        gdouble received_mb = 0.0;
        gdouble sent_mb = 0.0;
        parse_docker_netio(line, &received_mb, &sent_mb);
        
        *total_received_mb += received_mb;
        *total_sent_mb += sent_mb;
    }
    
    g_strfreev(lines);
}
static void calculate_total_disk_io(gchar* output, gdouble* total_read_mb, gdouble* total_write_mb) {
    *total_read_mb = 0.0;
    *total_write_mb = 0.0;
    
    if (output == NULL || strlen(output) == 0) {
        return;
    }
    
    gchar** lines = g_strsplit(output, "\n", -1);
    
    for (gint i = 0; lines[i] != NULL; i++) {
        gchar* line = g_strstrip(lines[i]);
        if (strlen(line) == 0) continue;
        
        gdouble read_mb = 0.0;
        gdouble write_mb = 0.0;
        parse_docker_blockio(line, &read_mb, &write_mb);
        
        *total_read_mb += read_mb;
        *total_write_mb += write_mb;
    }
    
    g_strfreev(lines);
}
static void on_memory_chart_stats_received(gchar* output, gpointer user_data) {
    MemoryChartData* chart = (MemoryChartData*)user_data;

    if (chart == NULL || chart->value_label == NULL) {
        g_free(output);
        return;
    }

    gdouble total_memory_used = calculate_total_memory_usage(output);
    if (chart->num_points < CHART_MAX_POINTS) {
        chart->points[chart->num_points] = total_memory_used;
        chart->num_points++;
    } else {
        memmove(chart->points, chart->points + 1, (CHART_MAX_POINTS - 1) * sizeof(gdouble));
        chart->points[CHART_MAX_POINTS - 1] = total_memory_used;
    }
    gchar* used_value;
    gchar* used_unit;
    format_memory(total_memory_used, &used_value, &used_unit);

    gchar* total_value;
    gchar* total_unit;
    format_memory(chart->system_total_memory, &total_value, &total_unit);

    gchar* memory_text = g_strdup_printf("%s%s / %s%s",
                                         used_value, used_unit,
                                         total_value, total_unit);
    gtk_label_set_text(GTK_LABEL(chart->value_label), memory_text);

    g_free(memory_text);
    g_free(used_value);
    g_free(used_unit);
    g_free(total_value);
    g_free(total_unit);
    chart->anim_offset = 0.0;
    chart->total_updates++;
    if (chart->drawing_area) {
        gtk_widget_queue_draw(chart->drawing_area);
    }

    g_free(output);
}
static void on_diskio_chart_stats_received(gchar* output, gpointer user_data) {
    DiskIOChartData* chart = (DiskIOChartData*)user_data;

    if (chart == NULL || chart->value_label == NULL) {
        g_free(output);
        return;
    }

    gdouble total_read_mb = 0.0;
    gdouble total_write_mb = 0.0;
    calculate_total_disk_io(output, &total_read_mb, &total_write_mb);
    if (chart->num_points < CHART_MAX_POINTS) {
        chart->read_points[chart->num_points] = total_read_mb;
        chart->write_points[chart->num_points] = total_write_mb;
        chart->num_points++;
    } else {
        memmove(chart->read_points, chart->read_points + 1, (CHART_MAX_POINTS - 1) * sizeof(gdouble));
        memmove(chart->write_points, chart->write_points + 1, (CHART_MAX_POINTS - 1) * sizeof(gdouble));
        chart->read_points[CHART_MAX_POINTS - 1] = total_read_mb;
        chart->write_points[CHART_MAX_POINTS - 1] = total_write_mb;
    }
    gchar* diskio_text = g_strdup_printf("%.2f MB/s read / %.2f MB/s write", 
                                         total_read_mb, total_write_mb);
    gtk_label_set_text(GTK_LABEL(chart->value_label), diskio_text);
    g_free(diskio_text);
    chart->anim_offset = 0.0;
    chart->total_updates++;
    if (chart->drawing_area) {
        gtk_widget_queue_draw(chart->drawing_area);
    }

    g_free(output);
}
static void update_memory_chart_stats(MemoryChartData* chart) {
    if (chart == NULL) return;
    chart->system_total_memory = get_system_total_memory();
    execute_command_async("docker stats --no-stream --format '{{.MemUsage}}'",
                          on_memory_chart_stats_received, chart);
}
static gboolean update_memory_chart_stats_timer(gpointer user_data) {
    update_memory_chart_stats((MemoryChartData*)user_data);
    return TRUE;
}
static gboolean cpu_chart_anim_tick(gpointer user_data) {
    CpuChartData* chart = (CpuChartData*)user_data;
    chart->anim_offset += (gdouble)CHART_ANIM_INTERVAL_MS / CHART_DATA_INTERVAL_MS;
    if (chart->anim_offset > 1.0) chart->anim_offset = 1.0;
    if (chart->drawing_area) {
        gtk_widget_queue_draw(chart->drawing_area);
    }
    return TRUE;
}
static gboolean memory_chart_anim_tick(gpointer user_data) {
    MemoryChartData* chart = (MemoryChartData*)user_data;
    chart->anim_offset += (gdouble)CHART_ANIM_INTERVAL_MS / CHART_DATA_INTERVAL_MS;
    if (chart->anim_offset > 1.0) chart->anim_offset = 1.0;
    if (chart->drawing_area) {
        gtk_widget_queue_draw(chart->drawing_area);
    }
    return TRUE;
}
static void update_diskio_chart_stats(DiskIOChartData* chart) {
    if (chart == NULL) return;
    execute_command_async("docker stats --no-stream --format '{{.BlockIO}}'",
                          on_diskio_chart_stats_received, chart);
}
static gboolean update_diskio_chart_stats_timer(gpointer user_data) {
    update_diskio_chart_stats((DiskIOChartData*)user_data);
    return TRUE;
}
static gboolean diskio_chart_anim_tick(gpointer user_data) {
    DiskIOChartData* chart = (DiskIOChartData*)user_data;
    chart->anim_offset += (gdouble)CHART_ANIM_INTERVAL_MS / CHART_DATA_INTERVAL_MS;
    if (chart->anim_offset > 1.0) chart->anim_offset = 1.0;
    if (chart->drawing_area) {
        gtk_widget_queue_draw(chart->drawing_area);
    }
    return TRUE;
}
static gboolean on_networkio_chart_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    NetworkIOChartData *chart = (NetworkIOChartData*)user_data;

    gint width = gtk_widget_get_allocated_width(widget);
    gint height = gtk_widget_get_allocated_height(widget);
    gdouble slot_width = (gdouble)width / (CHART_MAX_POINTS - 1);
    gdouble scroll_total_px = (chart->total_updates + chart->anim_offset) * slot_width;
    draw_chart_grid(cr, width, height, scroll_total_px);

    if (chart->num_points < 2) return FALSE;
    gdouble max_val = 1.0;
    for (gint p = 0; p < chart->num_points; p++) {
        if (chart->received_points[p] > max_val) {
            max_val = chart->received_points[p];
        }
        if (chart->sent_points[p] > max_val) {
            max_val = chart->sent_points[p];
        }
    }
    max_val *= 1.1;  // Adiciona 10% de margem no topo
    if (max_val <= 0) max_val = 1.0;

    gint n = chart->num_points;
    gdouble scroll_px = chart->anim_offset * slot_width;
    gdouble x_vals[CHART_MAX_POINTS + 1];
    gdouble received_y_vals[CHART_MAX_POINTS + 1];
    gdouble sent_y_vals[CHART_MAX_POINTS + 1];

    for (gint p = 0; p < n; p++) {
        gdouble base_x = (CHART_MAX_POINTS - 1 - (n - 1 - p)) * slot_width;
        x_vals[p] = base_x - scroll_px;
        received_y_vals[p] = height - (chart->received_points[p] / max_val) * height;
        sent_y_vals[p] = height - (chart->sent_points[p] / max_val) * height;
        if (received_y_vals[p] < 0) received_y_vals[p] = 0;
        if (received_y_vals[p] > height) received_y_vals[p] = height;
        if (sent_y_vals[p] < 0) sent_y_vals[p] = 0;
        if (sent_y_vals[p] > height) sent_y_vals[p] = height;
    }
    gint draw_n = n;
    if (chart->anim_offset > 0.01) {
        x_vals[n] = (gdouble)width;
        received_y_vals[n] = received_y_vals[n - 1];
        sent_y_vals[n] = sent_y_vals[n - 1];
        draw_n = n + 1;
    }
    cairo_save(cr);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_clip(cr);
    cairo_set_source_rgba(cr, 0.2, 0.8, 0.4, 0.1);
    draw_smooth_line_points(cr, x_vals, received_y_vals, draw_n);
    cairo_line_to(cr, x_vals[draw_n - 1], height);
    cairo_line_to(cr, x_vals[0], height);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.2, 0.8, 0.4, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    draw_smooth_line_points(cr, x_vals, received_y_vals, draw_n);
    cairo_stroke(cr);
    cairo_set_source_rgba(cr, 0.6, 0.4, 0.8, 0.1);
    draw_smooth_line_points(cr, x_vals, sent_y_vals, draw_n);
    cairo_line_to(cr, x_vals[draw_n - 1], height);
    cairo_line_to(cr, x_vals[0], height);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.6, 0.4, 0.8, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    draw_smooth_line_points(cr, x_vals, sent_y_vals, draw_n);
    cairo_stroke(cr);

    cairo_restore(cr);
    return FALSE;
}
static void on_networkio_chart_stats_received(gchar* output, gpointer user_data) {
    NetworkIOChartData* chart = (NetworkIOChartData*)user_data;

    if (chart == NULL || chart->value_label == NULL) {
        g_free(output);
        return;
    }

    gdouble total_received_mb = 0.0;
    gdouble total_sent_mb = 0.0;
    calculate_total_network_io(output, &total_received_mb, &total_sent_mb);
    if (chart->num_points < CHART_MAX_POINTS) {
        chart->received_points[chart->num_points] = total_received_mb;
        chart->sent_points[chart->num_points] = total_sent_mb;
        chart->num_points++;
    } else {
        memmove(chart->received_points, chart->received_points + 1, (CHART_MAX_POINTS - 1) * sizeof(gdouble));
        memmove(chart->sent_points, chart->sent_points + 1, (CHART_MAX_POINTS - 1) * sizeof(gdouble));
        chart->received_points[CHART_MAX_POINTS - 1] = total_received_mb;
        chart->sent_points[CHART_MAX_POINTS - 1] = total_sent_mb;
    }
    gchar* networkio_text = g_strdup_printf("%.2f MB/s received / %.2f MB/s sent", 
                                         total_received_mb, total_sent_mb);
    gtk_label_set_text(GTK_LABEL(chart->value_label), networkio_text);
    g_free(networkio_text);
    chart->anim_offset = 0.0;
    chart->total_updates++;
    if (chart->drawing_area) {
        gtk_widget_queue_draw(chart->drawing_area);
    }

    g_free(output);
}
static void update_networkio_chart_stats(NetworkIOChartData* chart) {
    if (chart == NULL) return;
    execute_command_async("docker stats --no-stream --format '{{.NetIO}}'",
                          on_networkio_chart_stats_received, chart);
}
static gboolean update_networkio_chart_stats_timer(gpointer user_data) {
    update_networkio_chart_stats((NetworkIOChartData*)user_data);
    return TRUE;
}
static gboolean networkio_chart_anim_tick(gpointer user_data) {
    NetworkIOChartData* chart = (NetworkIOChartData*)user_data;
    chart->anim_offset += (gdouble)CHART_ANIM_INTERVAL_MS / CHART_DATA_INTERVAL_MS;
    if (chart->anim_offset > 1.0) chart->anim_offset = 1.0;
    if (chart->drawing_area) {
        gtk_widget_queue_draw(chart->drawing_area);
    }
    return TRUE;
}
static void on_about_clicked(GtkButton *button, gpointer user_data) {
    GtkWindow *window = GTK_WINDOW(user_data);
    GtkWidget *about_dialog;
    about_dialog = gtk_about_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(about_dialog), window);
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about_dialog), "Dodo");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(about_dialog), "Version 1.0.0");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about_dialog), 
                                  "A GTK3 application to manage Docker containers");
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about_dialog), 
                                   "Copyright © 2026 Davi Bomfim Santiago");
    g_signal_connect(about_dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show_all(about_dialog);
}

GtkWidget* create_main_window(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *header_bar;
    GtkWidget *stack;
    GtkWidget *switcher_box;
    GtkWidget *container_view;
    GtkWidget *images_view;
    GtkWidget *networks_view;
    GtkWidget *volumes_view;
    GtkWidget *box;
    GtkWidget *containers_button;
    GtkWidget *images_button;
    GtkWidget *networks_button;
    GtkWidget *volumes_button;
    GtkWidget *menu_button;
    GtkWidget *popover;
    GtkWidget *refresh_button;
    GtkWidget *about_button;
    TableStoresData *stores_data;
    gtk_init(&argc, &argv);
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Dodo");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "Dodo");
    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const gchar *css = 
        "frame#metric-card {"
        "  background-color: mix(@theme_bg_color, @theme_fg_color, 0.05);"
        "  border-radius: 8px;"
        "}"
        "frame#metric-card border {"
        "  border: 1px solid alpha(@borders, 0.3);"
        "  border-radius: 8px;"
        "  box-shadow: none;"
        "}"
        "button.suggested-action {"
        "  outline: none;"
        "  border: none;"
        "  box-shadow: none;"
        "}"
        "button.suggested-action:focus {"
        "  outline: none;"
        "  border: none;"
        "  box-shadow: none;"
        "}";
    gtk_css_provider_load_from_data(css_provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);
    stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 300);
    container_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* metrics_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(metrics_box), TRUE);  // Distribui espaço igualmente
    gtk_widget_set_margin_start(metrics_box, 12);
    gtk_widget_set_margin_end(metrics_box, 12);
    gtk_widget_set_margin_top(metrics_box, 12);
    gtk_widget_set_margin_bottom(metrics_box, 12);
    GtkWidget* cpu_card = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(cpu_card), GTK_SHADOW_NONE);
    gtk_widget_set_margin_start(cpu_card, 0);
    gtk_widget_set_margin_end(cpu_card, 6);
    gtk_widget_set_hexpand(cpu_card, TRUE);  // Permite expansão horizontal igual
    gtk_widget_set_margin_top(cpu_card, 0);
    gtk_widget_set_margin_bottom(cpu_card, 0);
    gtk_widget_set_name(cpu_card, "metric-card");
    
    GtkWidget* cpu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(cpu_box, 16);
    gtk_widget_set_margin_end(cpu_box, 16);
    gtk_widget_set_margin_top(cpu_box, 16);
    gtk_widget_set_margin_bottom(cpu_box, 16);
    
    GtkWidget* cpu_label = gtk_label_new("CPU usage");
    gtk_widget_set_halign(cpu_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(cpu_label), 0.0);
    PangoAttrList* attr_list = pango_attr_list_new();
    PangoAttribute* attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(attr_list, attr);
    gtk_label_set_attributes(GTK_LABEL(cpu_label), attr_list);
    pango_attr_list_unref(attr_list);
    CpuChartData* cpu_chart = g_new0(CpuChartData, 1);
    GtkWidget* cpu_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(cpu_drawing_area, -1, CHART_HEIGHT);
    cpu_chart->drawing_area = cpu_drawing_area;
    g_signal_connect(cpu_drawing_area, "draw", G_CALLBACK(on_cpu_chart_draw), cpu_chart);
    GtkWidget* cpu_percent_label = gtk_label_new("0.0%");
    gtk_widget_set_halign(cpu_percent_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(cpu_percent_label), 0.0);
    cpu_chart->value_label = cpu_percent_label;
    
    gtk_box_pack_start(GTK_BOX(cpu_box), cpu_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cpu_box), cpu_drawing_area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(cpu_box), cpu_percent_label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(cpu_card), cpu_box);
    gtk_box_pack_start(GTK_BOX(metrics_box), cpu_card, TRUE, TRUE, 0);
    update_cpu_chart_stats(cpu_chart);
    g_timeout_add(CHART_DATA_INTERVAL_MS, update_cpu_chart_stats_timer, cpu_chart);
    g_timeout_add(CHART_ANIM_INTERVAL_MS, cpu_chart_anim_tick, cpu_chart);
    GtkWidget* memory_card = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(memory_card), GTK_SHADOW_NONE);
    gtk_widget_set_margin_start(memory_card, 6);
    gtk_widget_set_margin_end(memory_card, 6);
    gtk_widget_set_hexpand(memory_card, TRUE);  // Permite expansão horizontal igual
    gtk_widget_set_margin_top(memory_card, 0);
    gtk_widget_set_margin_bottom(memory_card, 0);
    gtk_widget_set_name(memory_card, "metric-card");
    
    GtkWidget* memory_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(memory_box, 16);
    gtk_widget_set_margin_end(memory_box, 16);
    gtk_widget_set_margin_top(memory_box, 16);
    gtk_widget_set_margin_bottom(memory_box, 16);
    
    GtkWidget* memory_label = gtk_label_new("Memory usage");
    gtk_widget_set_halign(memory_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(memory_label), 0.0);
    attr_list = pango_attr_list_new();
    attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(attr_list, attr);
    gtk_label_set_attributes(GTK_LABEL(memory_label), attr_list);
    pango_attr_list_unref(attr_list);
    MemoryChartData* memory_chart = g_new0(MemoryChartData, 1);
    memory_chart->system_total_memory = get_system_total_memory();
    GtkWidget* memory_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(memory_drawing_area, -1, CHART_HEIGHT);
    memory_chart->drawing_area = memory_drawing_area;
    g_signal_connect(memory_drawing_area, "draw", G_CALLBACK(on_memory_chart_draw), memory_chart);
    GtkWidget* memory_usage_label = gtk_label_new("0.00MB / 0.00GB");
    gtk_widget_set_halign(memory_usage_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(memory_usage_label), 0.0);
    memory_chart->value_label = memory_usage_label;
    
    gtk_box_pack_start(GTK_BOX(memory_box), memory_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(memory_box), memory_drawing_area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(memory_box), memory_usage_label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(memory_card), memory_box);
    gtk_box_pack_start(GTK_BOX(metrics_box), memory_card, TRUE, TRUE, 0);
    update_memory_chart_stats(memory_chart);
    g_timeout_add(CHART_DATA_INTERVAL_MS, update_memory_chart_stats_timer, memory_chart);
    g_timeout_add(CHART_ANIM_INTERVAL_MS, memory_chart_anim_tick, memory_chart);
    GtkWidget* diskio_card = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(diskio_card), GTK_SHADOW_NONE);
    gtk_widget_set_margin_start(diskio_card, 6);
    gtk_widget_set_margin_end(diskio_card, 6);
    gtk_widget_set_hexpand(diskio_card, TRUE);  // Permite expansão horizontal igual
    gtk_widget_set_margin_top(diskio_card, 0);
    gtk_widget_set_margin_bottom(diskio_card, 0);
    gtk_widget_set_name(diskio_card, "metric-card");
    
    GtkWidget* diskio_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(diskio_box, 16);
    gtk_widget_set_margin_end(diskio_box, 16);
    gtk_widget_set_margin_top(diskio_box, 16);
    gtk_widget_set_margin_bottom(diskio_box, 16);
    
    GtkWidget* diskio_label = gtk_label_new("Disk I/O");
    gtk_widget_set_halign(diskio_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(diskio_label), 0.0);
    attr_list = pango_attr_list_new();
    attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(attr_list, attr);
    gtk_label_set_attributes(GTK_LABEL(diskio_label), attr_list);
    pango_attr_list_unref(attr_list);
    DiskIOChartData* diskio_chart = g_new0(DiskIOChartData, 1);
    GtkWidget* diskio_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(diskio_drawing_area, -1, CHART_HEIGHT);
    diskio_chart->drawing_area = diskio_drawing_area;
    g_signal_connect(diskio_drawing_area, "draw", G_CALLBACK(on_diskio_chart_draw), diskio_chart);
    GtkWidget* legend_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(legend_box, 4);
    gtk_widget_set_margin_bottom(legend_box, 4);
    GtkWidget* read_legend = create_legend_item("Read", 0.2, 0.6, 1.0);  // Azul
    GtkWidget* write_legend = create_legend_item("Write", 1.0, 0.6, 0.2);  // Laranja
    gtk_box_pack_start(GTK_BOX(legend_box), read_legend, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(legend_box), write_legend, FALSE, FALSE, 0);
    GtkWidget* diskio_usage_label = gtk_label_new("0.00 MB/s read / 0.00 MB/s write");
    gtk_widget_set_halign(diskio_usage_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(diskio_usage_label), 0.0);
    diskio_chart->value_label = diskio_usage_label;
    
    gtk_box_pack_start(GTK_BOX(diskio_box), diskio_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(diskio_box), diskio_drawing_area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(diskio_box), legend_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(diskio_box), diskio_usage_label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(diskio_card), diskio_box);
    gtk_box_pack_start(GTK_BOX(metrics_box), diskio_card, TRUE, TRUE, 0);
    update_diskio_chart_stats(diskio_chart);
    g_timeout_add(CHART_DATA_INTERVAL_MS, update_diskio_chart_stats_timer, diskio_chart);
    g_timeout_add(CHART_ANIM_INTERVAL_MS, diskio_chart_anim_tick, diskio_chart);
    GtkWidget* networkio_card = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(networkio_card), GTK_SHADOW_NONE);
    gtk_widget_set_margin_start(networkio_card, 6);
    gtk_widget_set_margin_end(networkio_card, 0);
    gtk_widget_set_hexpand(networkio_card, TRUE);  // Permite expansão horizontal igual
    gtk_widget_set_margin_top(networkio_card, 0);
    gtk_widget_set_margin_bottom(networkio_card, 0);
    gtk_widget_set_name(networkio_card, "metric-card");
    
    GtkWidget* networkio_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(networkio_box, 16);
    gtk_widget_set_margin_end(networkio_box, 16);
    gtk_widget_set_margin_top(networkio_box, 16);
    gtk_widget_set_margin_bottom(networkio_box, 16);
    
    GtkWidget* networkio_label = gtk_label_new("Network I/O");
    gtk_widget_set_halign(networkio_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(networkio_label), 0.0);
    attr_list = pango_attr_list_new();
    attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    pango_attr_list_insert(attr_list, attr);
    gtk_label_set_attributes(GTK_LABEL(networkio_label), attr_list);
    pango_attr_list_unref(attr_list);
    NetworkIOChartData* networkio_chart = g_new0(NetworkIOChartData, 1);
    GtkWidget* networkio_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(networkio_drawing_area, -1, CHART_HEIGHT);
    networkio_chart->drawing_area = networkio_drawing_area;
    g_signal_connect(networkio_drawing_area, "draw", G_CALLBACK(on_networkio_chart_draw), networkio_chart);
    GtkWidget* networkio_legend_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_top(networkio_legend_box, 4);
    gtk_widget_set_margin_bottom(networkio_legend_box, 4);
    GtkWidget* received_legend = create_legend_item("Received", 0.2, 0.8, 0.4);  // Verde
    GtkWidget* sent_legend = create_legend_item("Sent", 0.6, 0.4, 0.8);  // Roxo
    gtk_box_pack_start(GTK_BOX(networkio_legend_box), received_legend, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(networkio_legend_box), sent_legend, FALSE, FALSE, 0);
    GtkWidget* networkio_usage_label = gtk_label_new("0.00 MB/s received / 0.00 MB/s sent");
    gtk_widget_set_halign(networkio_usage_label, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(networkio_usage_label), 0.0);
    networkio_chart->value_label = networkio_usage_label;
    
    gtk_box_pack_start(GTK_BOX(networkio_box), networkio_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(networkio_box), networkio_drawing_area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(networkio_box), networkio_legend_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(networkio_box), networkio_usage_label, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(networkio_card), networkio_box);
    gtk_box_pack_start(GTK_BOX(metrics_box), networkio_card, TRUE, TRUE, 0);
    update_networkio_chart_stats(networkio_chart);
    g_timeout_add(CHART_DATA_INTERVAL_MS, update_networkio_chart_stats_timer, networkio_chart);
    g_timeout_add(CHART_ANIM_INTERVAL_MS, networkio_chart_anim_tick, networkio_chart);
    gtk_box_pack_start(GTK_BOX(container_view), metrics_box, FALSE, FALSE, 0);
    
    GtkWidget* containers_table = create_containers_table();
    gtk_box_pack_start(GTK_BOX(container_view), containers_table, TRUE, TRUE, 0);
    gtk_stack_add_titled(GTK_STACK(stack), container_view, "containers", "Containers");
    images_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* images_table = create_images_table();
    gtk_box_pack_start(GTK_BOX(images_view), images_table, TRUE, TRUE, 0);
    gtk_stack_add_titled(GTK_STACK(stack), images_view, "images", "Images");
    networks_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* networks_table = create_networks_table();
    gtk_box_pack_start(GTK_BOX(networks_view), networks_table, TRUE, TRUE, 0);
    gtk_stack_add_titled(GTK_STACK(stack), networks_view, "networks", "Networks");
    volumes_view = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget* volumes_table = create_volumes_table();
    gtk_box_pack_start(GTK_BOX(volumes_view), volumes_table, TRUE, TRUE, 0);
    gtk_stack_add_titled(GTK_STACK(stack), volumes_view, "volumes", "Volumes");
    stores_data = g_new(TableStoresData, 1);
    stores_data->containers_store = get_store_from_scrolled_window(containers_table, TRUE);
    stores_data->containers_tree_view = get_tree_view_from_widget(containers_table);
    stores_data->images_store = get_store_from_scrolled_window(images_table, FALSE);
    stores_data->networks_store = get_store_from_scrolled_window(networks_table, FALSE);
    stores_data->volumes_store = get_store_from_scrolled_window(volumes_table, FALSE);
    if (stores_data->containers_store) {
        g_object_ref(stores_data->containers_store);
    }
    if (stores_data->images_store) {
        g_object_ref(stores_data->images_store);
    }
    if (stores_data->networks_store) {
        g_object_ref(stores_data->networks_store);
    }
    if (stores_data->volumes_store) {
        g_object_ref(stores_data->volumes_store);
    }
    if (stores_data->containers_store) {
        g_object_set_data_full(G_OBJECT(window), "containers-store", 
                              g_object_ref(stores_data->containers_store), g_object_unref);
    }
    if (stores_data->containers_tree_view) {
        g_object_set_data(G_OBJECT(window), "containers-tree-view", stores_data->containers_tree_view);
    }
    switcher_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_set_homogeneous(GTK_BOX(switcher_box), TRUE);
    containers_button = create_view_button("containers", "Containers", "containers", GTK_STACK(stack));
    gtk_box_pack_start(GTK_BOX(switcher_box), containers_button, TRUE, TRUE, 0);
    images_button = create_view_button("images", "Images", "images", GTK_STACK(stack));
    gtk_box_pack_start(GTK_BOX(switcher_box), images_button, TRUE, TRUE, 0);
    networks_button = create_view_button("networks", "Networks", "networks", GTK_STACK(stack));
    gtk_box_pack_start(GTK_BOX(switcher_box), networks_button, TRUE, TRUE, 0);
    volumes_button = create_view_button("volumes", "Volumes", "volumes", GTK_STACK(stack));
    gtk_box_pack_start(GTK_BOX(switcher_box), volumes_button, TRUE, TRUE, 0);
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(header_bar), switcher_box);
    ThemeIconsData *theme_data = g_new(ThemeIconsData, 1);
    theme_data->buttons[0] = containers_button;
    theme_data->buttons[1] = images_button;
    theme_data->buttons[2] = networks_button;
    theme_data->buttons[3] = volumes_button;
    theme_data->count = 4;
    GtkSettings *gtk_settings = gtk_settings_get_default();
    if (gtk_settings) {
        g_signal_connect(gtk_settings, "notify::gtk-theme-name",
                         G_CALLBACK(on_gtk_theme_name_changed), theme_data);
    }
    GSettingsSchemaSource *schema_source = g_settings_schema_source_get_default();
    if (schema_source) {
        GSettingsSchema *iface_schema = g_settings_schema_source_lookup(
            schema_source, "org.gnome.desktop.interface", TRUE);
        if (iface_schema) {
            if (g_settings_schema_has_key(iface_schema, "color-scheme")) {
                GSettings *iface_settings = g_settings_new("org.gnome.desktop.interface");
                g_signal_connect(iface_settings, "changed::color-scheme",
                                 G_CALLBACK(on_color_scheme_changed), theme_data);
                g_object_set_data_full(G_OBJECT(window), "iface-gsettings",
                                       iface_settings, g_object_unref);
            }
            g_settings_schema_unref(iface_schema);
        }
    }
    menu_button = gtk_menu_button_new();
    gtk_menu_button_set_direction(GTK_MENU_BUTTON(menu_button), GTK_ARROW_DOWN);
    GtkWidget *menu_icon = gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(menu_button), menu_icon);
    gtk_button_set_always_show_image(GTK_BUTTON(menu_button), TRUE);
    popover = gtk_popover_new(menu_button);
    gtk_menu_button_set_popover(GTK_MENU_BUTTON(menu_button), popover);
    GtkWidget *popover_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(popover), popover_box);
    refresh_button = gtk_model_button_new();
    g_object_set(G_OBJECT(refresh_button), 
                 "text", "Refresh",
                 "icon-name", NULL,
                 NULL);
    gtk_widget_set_halign(refresh_button, GTK_ALIGN_FILL);
    gtk_widget_set_margin_start(refresh_button, 6);
    gtk_widget_set_margin_end(refresh_button, 6);
    gtk_widget_set_margin_top(refresh_button, 6);
    gtk_widget_set_margin_bottom(refresh_button, 0);
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_clicked), stores_data);
    gtk_container_add(GTK_CONTAINER(popover_box), refresh_button);
    about_button = gtk_model_button_new();
    g_object_set(G_OBJECT(about_button), 
                 "text", "About Dodo",
                 "icon-name", NULL,
                 NULL);
    gtk_widget_set_halign(about_button, GTK_ALIGN_FILL);
    gtk_widget_set_margin_start(about_button, 6);
    gtk_widget_set_margin_end(about_button, 6);
    gtk_widget_set_margin_top(about_button, 6);
    gtk_widget_set_margin_bottom(about_button, 6);
    g_signal_connect(about_button, "clicked", G_CALLBACK(on_about_clicked), window);
    gtk_container_add(GTK_CONTAINER(popover_box), about_button);
    gtk_widget_show(popover_box);
    gtk_widget_show(refresh_button);
    gtk_widget_show(about_button);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), menu_button);
    ViewSwitcherData *switcher_data = g_new(ViewSwitcherData, 1);
    switcher_data->stack = GTK_STACK(stack);
    switcher_data->containers_button = containers_button;
    switcher_data->images_button = images_button;
    switcher_data->networks_button = networks_button;
    switcher_data->volumes_button = volumes_button;
    g_signal_connect(stack, "notify::visible-child-name", G_CALLBACK(on_stack_visible_child_changed), switcher_data);
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "containers");
    gtk_style_context_add_class(gtk_widget_get_style_context(containers_button), "suggested-action");
    update_button_icon(containers_button);
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(box), stack, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_close), NULL);
    gtk_widget_show_all(window);
    
    return window;
}
