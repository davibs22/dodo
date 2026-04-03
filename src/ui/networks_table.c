#include "networks_table.h"
#include "../models/network.h"
#include "../docker/docker_command.h"
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <unistd.h>
#include <stdio.h>
typedef struct {
    GtkListStore* store;
    GtkWidget* tree_view;
    GtkWindow* parent_window;
    gchar* network_name;
} RemoveNetworkData;
typedef struct {
    GtkWindow* parent_window;
    gchar* network_name;
} InspectNetworkData;
static gboolean list_filter_func(GtkTreeModel* model, GtkTreeIter* iter, gpointer data) {
    GtkEntry* search_entry = GTK_ENTRY(data);
    const gchar* search_text = gtk_entry_get_text(search_entry);
    
    if (!search_text || strlen(search_text) == 0) {
        return TRUE;
    }
    
    gchar* search_lower = g_ascii_strdown(search_text, -1);
    gboolean visible = FALSE;
    
    gint n_columns = gtk_tree_model_get_n_columns(model);
    for (gint i = 0; i < n_columns && !visible; i++) {
        if (gtk_tree_model_get_column_type(model, i) == G_TYPE_STRING) {
            gchar* value = NULL;
            gtk_tree_model_get(model, iter, i, &value, -1);
            if (value) {
                gchar* value_lower = g_ascii_strdown(value, -1);
                visible = (g_strstr_len(value_lower, -1, search_lower) != NULL);
                g_free(value_lower);
                g_free(value);
            }
        }
    }
    
    g_free(search_lower);
    return visible;
}
static void on_search_changed(GtkSearchEntry* entry, gpointer user_data) {
    GtkTreeModelFilter* filter = GTK_TREE_MODEL_FILTER(user_data);
    gtk_tree_model_filter_refilter(filter);
}
static void show_message_dialog(GtkWindow* parent, GtkMessageType type, const gchar* title, const gchar* message) {
    GtkWidget* dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, type, GTK_BUTTONS_OK, "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
static void on_network_removed(gchar* output, gpointer user_data) {
    RemoveNetworkData* data = (RemoveNetworkData*)user_data;
    gboolean success = TRUE;
    if (!output) {
        success = FALSE;
    } else {
        gchar* trimmed_output = g_strstrip(g_strdup(output));
        if (strlen(trimmed_output) == 0) {
            success = TRUE;
        } else {
            gchar* lower_output = g_ascii_strdown(trimmed_output, -1);
            if (g_strrstr(lower_output, "error") || 
                g_strrstr(lower_output, "cannot") ||
                g_strrstr(lower_output, "failed") ||
                g_strrstr(lower_output, "denied") ||
                g_strrstr(lower_output, "permission") ||
                g_strrstr(lower_output, "not found")) {
                success = FALSE;
            } else {
                success = TRUE;
            }
            g_free(lower_output);
        }
        g_free(trimmed_output);
    }
    if (data && data->parent_window) {
        if (success) {
            gchar* message = g_strdup_printf("Network '%s' removed successfully.", data->network_name ? data->network_name : "unknown");
            show_message_dialog(data->parent_window, GTK_MESSAGE_INFO, "Success", message);
            g_free(message);
        } else {
            gchar* error_msg = output ? g_strdup(output) : g_strdup("Unknown error while removing network.");
            gchar* message = g_strdup_printf("Error ao remover rede '%s':\n%s", 
                                                  data->network_name ? data->network_name : "unknown", 
                                                  error_msg);
            show_message_dialog(data->parent_window, GTK_MESSAGE_ERROR, "Error", message);
            g_free(message);
            g_free(error_msg);
        }
    }
    if (data && data->store) {
        refresh_networks_table_async(data->store);
    }
    if (data) {
        g_free(data->network_name);
        g_free(data);
    }
    if (output) {
        g_free(output);
    }
}
static void on_confirm_dialog_response(GtkDialog* dialog, gint response_id, gpointer user_data) {
    RemoveNetworkData* confirm_data = (RemoveNetworkData*)user_data;
    
    if (response_id == GTK_RESPONSE_YES) {
        if (!confirm_data || !confirm_data->tree_view || !confirm_data->store) {
            g_free(confirm_data->network_name);
            g_free(confirm_data);
            gtk_widget_destroy(GTK_WIDGET(dialog));
            return;
        }
        gchar* command = g_strdup_printf("docker network rm %s", confirm_data->network_name);
        execute_command_async(command, on_network_removed, confirm_data);
        
        g_free(command);
    } else {
        g_free(confirm_data->network_name);
        g_free(confirm_data);
    }
    
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void on_remove_network_clicked(GtkMenuItem* menu_item, gpointer user_data) {
    RemoveNetworkData* data = (RemoveNetworkData*)user_data;
    if (!data || !data->tree_view || !data->store) {
        return;
    }
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return; // No row selected
    }
    gchar* network_name = NULL;
    gtk_tree_model_get(model, &iter, 1, &network_name, -1);
    
    if (!network_name || strlen(network_name) == 0 || g_strcmp0(network_name, "<none>") == 0) {
        g_free(network_name);
        gtk_tree_model_get(model, &iter, 0, &network_name, -1);
    }
    
    if (!network_name || strlen(network_name) == 0) {
        g_free(network_name);
        return;
    }
    GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
    GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
    gchar* message = g_strdup_printf("Are you sure you want to remove network '%s'?\n\nThis action cannot be undone.", network_name);
    GtkWidget* dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL, 
                                            GTK_MESSAGE_QUESTION, 
                                            GTK_BUTTONS_YES_NO, 
                                            "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Removal");
    g_free(message);
    RemoveNetworkData* confirm_data = g_new(RemoveNetworkData, 1);
    confirm_data->store = data->store;
    confirm_data->tree_view = data->tree_view;
    confirm_data->parent_window = parent_window;
    confirm_data->network_name = g_strdup(network_name);
    g_signal_connect(dialog, "response", G_CALLBACK(on_confirm_dialog_response), confirm_data);
    gtk_widget_show_all(dialog);
    
    g_free(network_name);
}
static gchar* format_json_simple(const gchar* json) {
    if (!json) return NULL;
    
    gint indent = 0;
    GString* formatted = g_string_new("");
    gboolean in_string = FALSE;
    gboolean escape_next = FALSE;
    
    for (const gchar* p = json; *p; p++) {
        if (escape_next) {
            g_string_append_c(formatted, *p);
            escape_next = FALSE;
            continue;
        }
        
        if (*p == '\\') {
            escape_next = TRUE;
            g_string_append_c(formatted, *p);
            continue;
        }
        
        if (*p == '"') {
            in_string = !in_string;
            g_string_append_c(formatted, *p);
            continue;
        }
        
        if (in_string) {
            g_string_append_c(formatted, *p);
            continue;
        }
        
        switch (*p) {
            case '{':
            case '[':
                g_string_append_c(formatted, *p);
                g_string_append_c(formatted, '\n');
                indent++;
                for (gint i = 0; i < indent; i++) {
                    g_string_append(formatted, "  ");
                }
                break;
            case '}':
            case ']':
                g_string_append_c(formatted, '\n');
                indent--;
                for (gint i = 0; i < indent; i++) {
                    g_string_append(formatted, "  ");
                }
                g_string_append_c(formatted, *p);
                break;
            case ',':
                g_string_append_c(formatted, *p);
                g_string_append_c(formatted, '\n');
                for (gint i = 0; i < indent; i++) {
                    g_string_append(formatted, "  ");
                }
                break;
            case ':':
                g_string_append(formatted, ": ");
                break;
            case ' ':
            case '\n':
            case '\t':
                break;
            default:
                g_string_append_c(formatted, *p);
                break;
        }
    }
    
    return g_string_free(formatted, FALSE);
}
static void apply_json_syntax_highlighting(GtkTextBuffer* buffer, const gchar* json_text) {
    if (!buffer || !json_text) return;
    gtk_text_buffer_set_text(buffer, json_text, -1);
    GtkTextTag* tag_key = gtk_text_buffer_create_tag(buffer, "json_key",
        "foreground", "#268bd2",  // Blue for keys
        NULL);
    GtkTextTag* tag_string = gtk_text_buffer_create_tag(buffer, "json_string",
        "foreground", "#2aa198",  // Cyan for strings
        NULL);
    GtkTextTag* tag_number = gtk_text_buffer_create_tag(buffer, "json_number",
        "foreground", "#d33682",  // Magenta for numbers
        NULL);
    GtkTextTag* tag_boolean = gtk_text_buffer_create_tag(buffer, "json_boolean",
        "foreground", "#859900",  // Green for booleans
        NULL);
    GtkTextTag* tag_null = gtk_text_buffer_create_tag(buffer, "json_null",
        "foreground", "#dc322f",  // Red for null
        NULL);
    GtkTextTag* tag_delimiter = gtk_text_buffer_create_tag(buffer, "json_delimiter",
        "foreground", "#586e75",  // Gray for delimiters
        "weight", PANGO_WEIGHT_BOLD,
        NULL);
    
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gchar* text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    
    if (!text) return;
    
    gint len = strlen(text);
    gboolean in_string = FALSE;
    gboolean escape_next = FALSE;
    gint key_start = -1;
    gint value_start = -1;
    gboolean in_key = FALSE;
    
    for (gint i = 0; i < len; i++) {
        gchar c = text[i];
        
        if (escape_next) {
            escape_next = FALSE;
            continue;
        }
        
        if (c == '\\') {
            escape_next = TRUE;
            continue;
        }
        
        if (c == '"') {
            if (!in_string) {
                in_string = TRUE;
                in_key = FALSE;
                for (gint j = i + 1; j < len; j++) {
                    if (text[j] == ' ' || text[j] == '\t' || text[j] == '\n') continue;
                    if (text[j] == ':') {
                        in_key = TRUE;
                        break;
                    }
                    break;
                }
                key_start = i;
            } else {
                in_string = FALSE;
                if (in_key && key_start >= 0) {
                    gtk_text_buffer_get_iter_at_offset(buffer, &start, key_start);
                    gtk_text_buffer_get_iter_at_offset(buffer, &end, i + 1);
                    gtk_text_buffer_apply_tag(buffer, tag_key, &start, &end);
                } else if (key_start >= 0) {
                    gtk_text_buffer_get_iter_at_offset(buffer, &start, key_start);
                    gtk_text_buffer_get_iter_at_offset(buffer, &end, i + 1);
                    gtk_text_buffer_apply_tag(buffer, tag_string, &start, &end);
                }
                key_start = -1;
                in_key = FALSE;
            }
            continue;
        }
        
        if (in_string) continue;
        if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',') {
            gtk_text_buffer_get_iter_at_offset(buffer, &start, i);
            gtk_text_buffer_get_iter_at_offset(buffer, &end, i + 1);
            gtk_text_buffer_apply_tag(buffer, tag_delimiter, &start, &end);
            continue;
        }
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.') {
            gint num_start = i;
            gint num_end = i + 1;
            gboolean is_number = TRUE;
            if (i > 0) {
                gchar prev = text[i - 1];
                if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') || prev == '_') {
                    is_number = FALSE;
                }
            }
            if (is_number) {
                while (num_end < len && 
                       ((text[num_end] >= '0' && text[num_end] <= '9') || 
                        text[num_end] == '.' || text[num_end] == 'e' || 
                        text[num_end] == 'E' || text[num_end] == '-' || text[num_end] == '+')) {
                    num_end++;
                }
                if (num_end < len) {
                    gchar next = text[num_end];
                    if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || next == '_') {
                        is_number = FALSE;
                    }
                }
                if (is_number) {
                    gtk_text_buffer_get_iter_at_offset(buffer, &start, num_start);
                    gtk_text_buffer_get_iter_at_offset(buffer, &end, num_end);
                    gtk_text_buffer_apply_tag(buffer, tag_number, &start, &end);
                    i = num_end - 1; // Adjust index
                }
            }
            continue;
        }
        if (c == 't' && i + 3 < len && strncmp(&text[i], "true", 4) == 0) {
            if ((i == 0 || !((text[i-1] >= 'a' && text[i-1] <= 'z') || 
                              (text[i-1] >= 'A' && text[i-1] <= 'Z') || text[i-1] == '_')) &&
                (i + 4 >= len || !((text[i+4] >= 'a' && text[i+4] <= 'z') || 
                                    (text[i+4] >= 'A' && text[i+4] <= 'Z') || text[i+4] == '_'))) {
                gtk_text_buffer_get_iter_at_offset(buffer, &start, i);
                gtk_text_buffer_get_iter_at_offset(buffer, &end, i + 4);
                gtk_text_buffer_apply_tag(buffer, tag_boolean, &start, &end);
                i += 3; // Skip processed characters
            }
            continue;
        }
        
        if (c == 'f' && i + 4 < len && strncmp(&text[i], "false", 5) == 0) {
            if ((i == 0 || !((text[i-1] >= 'a' && text[i-1] <= 'z') || 
                              (text[i-1] >= 'A' && text[i-1] <= 'Z') || text[i-1] == '_')) &&
                (i + 5 >= len || !((text[i+5] >= 'a' && text[i+5] <= 'z') || 
                                    (text[i+5] >= 'A' && text[i+5] <= 'Z') || text[i+5] == '_'))) {
                gtk_text_buffer_get_iter_at_offset(buffer, &start, i);
                gtk_text_buffer_get_iter_at_offset(buffer, &end, i + 5);
                gtk_text_buffer_apply_tag(buffer, tag_boolean, &start, &end);
                i += 4;
            }
            continue;
        }
        
        if (c == 'n' && i + 3 < len && strncmp(&text[i], "null", 4) == 0) {
            if ((i == 0 || !((text[i-1] >= 'a' && text[i-1] <= 'z') || 
                              (text[i-1] >= 'A' && text[i-1] <= 'Z') || text[i-1] == '_')) &&
                (i + 4 >= len || !((text[i+4] >= 'a' && text[i+4] <= 'z') || 
                                    (text[i+4] >= 'A' && text[i+4] <= 'Z') || text[i+4] == '_'))) {
                gtk_text_buffer_get_iter_at_offset(buffer, &start, i);
                gtk_text_buffer_get_iter_at_offset(buffer, &end, i + 4);
                gtk_text_buffer_apply_tag(buffer, tag_null, &start, &end);
                i += 3;
            }
            continue;
        }
    }
    
    g_free(text);
}
static void on_network_inspect_complete(gchar* output, gpointer user_data) {
    InspectNetworkData* data = (InspectNetworkData*)user_data;
    
    if (!data || !data->parent_window) {
        if (data) {
            g_free(data->network_name);
            g_free(data);
        }
        if (output) g_free(output);
        return;
    }
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Network Details",
        data->parent_window,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close",
        GTK_RESPONSE_CLOSE,
        NULL
    );
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 700, 500);
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 12);
    GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 400);
    GtkWidget* text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    if (output && strlen(output) > 0) {
        gchar* formatted_output = NULL;
        gchar* jq_check = execute_command("which jq >/dev/null 2>&1 && echo 'ok'");
        gboolean jq_available = (jq_check && g_strcmp0(g_strstrip(jq_check), "ok") == 0);
        g_free(jq_check);
        
        if (jq_available) {
            gchar* temp_file = g_strdup_printf("/tmp/docker_network_inspect_%d.json", getpid());
            FILE* fp = fopen(temp_file, "w");
            if (fp) {
                fputs(output, fp);
                fclose(fp);
                
                gchar* jq_command = g_strdup_printf("jq . %s 2>/dev/null", temp_file);
                gchar* jq_output = execute_command(jq_command);
                g_free(jq_command);
                unlink(temp_file);
                
                if (jq_output && strlen(jq_output) > 0 && !g_strrstr(jq_output, "error")) {
                    formatted_output = jq_output;
                } else {
                    if (jq_output) g_free(jq_output);
                    formatted_output = format_json_simple(output);
                    if (!formatted_output) {
                        formatted_output = g_strdup(output);
                    }
                }
            } else {
                formatted_output = format_json_simple(output);
                if (!formatted_output) {
                    formatted_output = g_strdup(output);
                }
            }
            g_free(temp_file);
        } else {
            formatted_output = format_json_simple(output);
            if (!formatted_output) {
                formatted_output = g_strdup(output);
            }
        }
        apply_json_syntax_highlighting(buffer, formatted_output);
        g_free(formatted_output);
    } else {
        gchar* error_msg = g_strdup_printf("Error ao obter detalhes da rede '%s'.\n\n%s",
                                           data->network_name ? data->network_name : "unknown",
                                           output ? output : "Error desconhecido.");
        gtk_text_buffer_set_text(buffer, error_msg, -1);
        g_free(error_msg);
    }
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled);
    g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_widget_show_all(dialog);
    g_free(data->network_name);
    g_free(data);
    if (output) g_free(output);
}
static void on_inspect_network_clicked(GtkMenuItem* menu_item, gpointer user_data) {
    RemoveNetworkData* data = (RemoveNetworkData*)user_data;
    if (!data || !data->tree_view) {
        return;
    }
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return; // No row selected
    }
    gchar* network_name = NULL;
    gtk_tree_model_get(model, &iter, 1, &network_name, -1);
    
    if (!network_name || strlen(network_name) == 0 || g_strcmp0(network_name, "<none>") == 0) {
        g_free(network_name);
        gtk_tree_model_get(model, &iter, 0, &network_name, -1);
    }
    
    if (!network_name || strlen(network_name) == 0) {
        g_free(network_name);
        return;
    }
    GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
    GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
    InspectNetworkData* inspect_data = g_new(InspectNetworkData, 1);
    inspect_data->parent_window = parent_window;
    inspect_data->network_name = g_strdup(network_name);
    gchar* command = g_strdup_printf("docker network inspect %s", network_name);
    execute_command_async(command, on_network_inspect_complete, inspect_data);
    
    g_free(command);
    g_free(network_name);
}
static gboolean on_button_press_event(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) { // Right button
        GtkWidget* menu = GTK_WIDGET(user_data);
        GtkTreeView* tree_view = GTK_TREE_VIEW(widget);
        GtkTreePath* path = NULL;
        if (gtk_tree_view_get_path_at_pos(tree_view, (gint)event->x, (gint)event->y,
                                          &path, NULL, NULL, NULL)) {
            GtkTreeSelection* selection = gtk_tree_view_get_selection(tree_view);
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_path_free(path);
            gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
            return TRUE;
        }
    }
    return FALSE;
}

GtkWidget* create_networks_table(void) {
    GtkListStore* store = gtk_list_store_new(4,
                                              G_TYPE_STRING,  // NETWORK ID
                                              G_TYPE_STRING,  // NAME
                                              G_TYPE_STRING,  // DRIVER
                                              G_TYPE_STRING); // SCOPE
    populate_docker_networks_async(store);
    GtkWidget* search_entry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Filter networks...");
    gtk_widget_set_margin_start(search_entry, 6);
    gtk_widget_set_margin_end(search_entry, 6);
    gtk_widget_set_margin_top(search_entry, 6);
    gtk_widget_set_margin_bottom(search_entry, 4);
    GtkTreeModelFilter* filter = GTK_TREE_MODEL_FILTER(
        gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL));
    gtk_tree_model_filter_set_visible_func(filter, list_filter_func, search_entry, NULL);
    g_signal_connect(search_entry, "search-changed", G_CALLBACK(on_search_changed), filter);
    GtkTreeModel* sort_model = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(filter));
    g_object_unref(filter); // sort_model keeps the reference
    GtkWidget* tree_view = gtk_tree_view_new_with_model(sort_model);
    g_object_unref(sort_model); // TreeView keeps the reference
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(
        "NETWORK ID", renderer, "text", 0, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "NAME", renderer, "text", 1, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 1);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "DRIVER", renderer, "text", 2, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 2);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "SCOPE", renderer, "text", 3, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 3);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    GtkWidget* context_menu = gtk_menu_new();
    RemoveNetworkData* menu_data = g_new(RemoveNetworkData, 1);
    menu_data->store = store;
    menu_data->tree_view = tree_view;
    g_object_set_data_full(G_OBJECT(tree_view), "networks-store", 
                          g_object_ref(store), g_object_unref);
    GtkWidget* inspect_item = gtk_menu_item_new_with_label("View details");
    g_signal_connect(inspect_item, "activate", G_CALLBACK(on_inspect_network_clicked), menu_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(context_menu), inspect_item);
    gtk_widget_show(inspect_item);
    GtkWidget* separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(context_menu), separator);
    gtk_widget_show(separator);
    GtkWidget* remove_item = gtk_menu_item_new_with_label("Remove");
    g_signal_connect(remove_item, "activate", G_CALLBACK(on_remove_network_clicked), menu_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(context_menu), remove_item);
    gtk_widget_show(remove_item);
    g_signal_connect(tree_view, "button-press-event", G_CALLBACK(on_button_press_event), context_menu);
    g_signal_connect_swapped(tree_view, "destroy", G_CALLBACK(g_free), menu_data);
    GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), tree_view);
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), search_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    
    return vbox;
}
