#include "images_table.h"
#include "../models/image.h"
#include "../models/container.h"
#include "../docker/docker_command.h"
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <unistd.h>
#include <stdio.h>
#include <glib/gstdio.h>
typedef struct {
    GtkListStore* store;
    GtkWidget* tree_view;
    GtkWindow* parent_window;
    gchar* image_id;
    gchar* image_name;
} RemoveImageData;
typedef struct {
    GtkWindow* parent_window;
    gchar* image_id;
} InspectImageData;
typedef struct {
    GtkWindow* parent_window;
    GtkListStore* store;
    gchar* image_id;
    gchar* image_name;
    GtkWidget* loading_dialog;  // Loading dialog during export
} ExportImageData;
typedef struct {
    GtkWindow* parent_window;
    GtkListStore* store;
} ImportImageData;
typedef struct {
    GtkWindow* parent_window;
    gchar* image_id;
    gchar* image_name;
    GtkTreeStore* containers_store;  // Containers table store to refresh
    GtkWidget* containers_tree_view; // Containers table tree view
} CreateContainerData;
static gboolean list_filter_func(GtkTreeModel* model, GtkTreeIter* iter, gpointer data) {
    GtkEntry* search_entry = GTK_ENTRY(data);
    const gchar* search_text = gtk_entry_get_text(search_entry);
    
    if (!search_text || strlen(search_text) == 0) {
        return TRUE; // Show all if there is no search text
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
static void on_image_removed(gchar* output, gpointer user_data) {
    RemoveImageData* data = (RemoveImageData*)user_data;
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
                g_strrstr(lower_output, "not found") ||
                g_strrstr(lower_output, "is being used")) {
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
            gchar* message = g_strdup_printf("Image '%s' removed successfully.", 
                                             data->image_name ? data->image_name : data->image_id ? data->image_id : "unknown");
            show_message_dialog(data->parent_window, GTK_MESSAGE_INFO, "Success", message);
            g_free(message);
        } else {
            gchar* error_msg = output ? g_strdup(output) : g_strdup("Unknown error while removing image.");
            gchar* message = g_strdup_printf("Error removing image '%s':\n%s", 
                                                  data->image_name ? data->image_name : data->image_id ? data->image_id : "unknown", 
                                                  error_msg);
            show_message_dialog(data->parent_window, GTK_MESSAGE_ERROR, "Error", message);
            g_free(message);
            g_free(error_msg);
        }
    }
    if (data && data->store) {
        refresh_images_table_async(data->store);
    }
    if (data) {
        g_free(data->image_id);
        g_free(data->image_name);
        g_free(data);
    }
    if (output) {
        g_free(output);
    }
}
static void on_confirm_image_dialog_response(GtkDialog* dialog, gint response_id, gpointer user_data) {
    RemoveImageData* confirm_data = (RemoveImageData*)user_data;
    
    if (response_id == GTK_RESPONSE_YES) {
        if (!confirm_data || !confirm_data->tree_view || !confirm_data->store) {
            g_free(confirm_data->image_id);
            g_free(confirm_data->image_name);
            g_free(confirm_data);
            gtk_widget_destroy(GTK_WIDGET(dialog));
            return;
        }
        gchar* command = g_strdup_printf("docker rmi -f %s", confirm_data->image_id);
        execute_command_async(command, on_image_removed, confirm_data);
        
        g_free(command);
    } else {
        g_free(confirm_data->image_id);
        g_free(confirm_data->image_name);
        g_free(confirm_data);
    }
    
    gtk_widget_destroy(GTK_WIDGET(dialog));
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
static void on_image_inspect_complete(gchar* output, gpointer user_data) {
    InspectImageData* data = (InspectImageData*)user_data;
    
    if (!data || !data->parent_window) {
        if (data) {
            g_free(data->image_id);
            g_free(data);
        }
        if (output) g_free(output);
        return;
    }
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Image Details",
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
            gchar* temp_file = g_strdup_printf("/tmp/docker_image_inspect_%d.json", getpid());
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
        gchar* error_msg = g_strdup_printf("Error fetching details for image '%s'.\n\n%s",
                                           data->image_id ? data->image_id : "unknown",
                                           output ? output : "Unknown error.");
        gtk_text_buffer_set_text(buffer, error_msg, -1);
        g_free(error_msg);
    }
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled);
    g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_widget_show_all(dialog);
    g_free(data->image_id);
    g_free(data);
    if (output) g_free(output);
}
static GtkWidget* create_loading_dialog(GtkWindow* parent, const gchar* message) {
    GtkWidget* dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Exporting Image");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 120);
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 20);
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_add(GTK_CONTAINER(content_area), vbox);
    GtkWidget* spinner = gtk_spinner_new();
    gtk_spinner_start(GTK_SPINNER(spinner));
    gtk_box_pack_start(GTK_BOX(vbox), spinner, FALSE, FALSE, 0);
    GtkWidget* label = gtk_label_new(message);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    
    gtk_widget_show_all(dialog);
    
    return dialog;
}
static void on_image_exported(gchar* output, gpointer user_data) {
    ExportImageData* data = (ExportImageData*)user_data;
    if (data && data->loading_dialog) {
        gtk_widget_destroy(data->loading_dialog);
        data->loading_dialog = NULL;
    }
    gboolean success = TRUE;
    if (!output) {
        success = FALSE;
    } else {
        gchar* trimmed_output = g_strstrip(g_strdup(output));
        if (strlen(trimmed_output) > 0) {
            gchar* lower_output = g_ascii_strdown(trimmed_output, -1);
            if (g_strrstr(lower_output, "error") || 
                g_strrstr(lower_output, "cannot") ||
                g_strrstr(lower_output, "failed") ||
                g_strrstr(lower_output, "denied") ||
                g_strrstr(lower_output, "permission") ||
                g_strrstr(lower_output, "not found")) {
                success = FALSE;
            }
            g_free(lower_output);
        }
        g_free(trimmed_output);
    }
    if (data && data->parent_window) {
        if (success) {
            gchar* message = g_strdup_printf("Image '%s' exported successfully.", 
                                           data->image_name ? data->image_name : data->image_id ? data->image_id : "unknown");
            show_message_dialog(data->parent_window, GTK_MESSAGE_INFO, "Success", message);
            g_free(message);
        } else {
            gchar* error_msg = output ? g_strdup(output) : g_strdup("Unknown error while exporting the image.");
            gchar* message = g_strdup_printf("Error exporting image '%s':\n%s", 
                                              data->image_name ? data->image_name : data->image_id ? data->image_id : "unknown", 
                                              error_msg);
            show_message_dialog(data->parent_window, GTK_MESSAGE_ERROR, "Error", message);
            g_free(message);
            g_free(error_msg);
        }
    }
    if (data) {
        g_free(data->image_id);
        g_free(data->image_name);
        g_free(data);
    }
    if (output) {
        g_free(output);
    }
}
static void on_image_imported(gchar* output, gpointer user_data) {
    ImportImageData* data = (ImportImageData*)user_data;
    gboolean success = TRUE;
    if (!output) {
        success = FALSE;
    } else {
        gchar* trimmed_output = g_strstrip(g_strdup(output));
        if (strlen(trimmed_output) > 0) {
            gchar* lower_output = g_ascii_strdown(trimmed_output, -1);
            if (g_strrstr(lower_output, "error") || 
                g_strrstr(lower_output, "cannot") ||
                g_strrstr(lower_output, "failed") ||
                g_strrstr(lower_output, "denied") ||
                g_strrstr(lower_output, "permission") ||
                g_strrstr(lower_output, "not found") ||
                g_strrstr(lower_output, "invalid")) {
                success = FALSE;
            }
            g_free(lower_output);
        }
        g_free(trimmed_output);
    }
    if (data && data->parent_window) {
        if (success) {
            show_message_dialog(data->parent_window, GTK_MESSAGE_INFO, "Success", "Image imported successfully.");
        } else {
            gchar* error_msg = output ? g_strdup(output) : g_strdup("Unknown error while importing the image.");
            gchar* message = g_strdup_printf("Error importing image:\n%s", error_msg);
            show_message_dialog(data->parent_window, GTK_MESSAGE_ERROR, "Error", message);
            g_free(message);
            g_free(error_msg);
        }
    }
    if (data && data->store) {
        refresh_images_table_async(data->store);
    }
    if (data) {
        g_free(data);
    }
    if (output) {
        g_free(output);
    }
}
static void on_export_file_dialog_response(GtkDialog* dialog, gint response_id, gpointer user_data) {
    ExportImageData* data = (ExportImageData*)user_data;
    
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog);
        gchar* filename = gtk_file_chooser_get_filename(chooser);
        
        if (filename) {
            gchar* loading_message = g_strdup_printf("Exporting image '%s'...\nThis may take a few minutes.", 
                                                     data->image_name ? data->image_name : data->image_id ? data->image_id : "unknown");
            data->loading_dialog = create_loading_dialog(data->parent_window, loading_message);
            g_free(loading_message);
            gchar* escaped_filename = g_shell_quote(filename);
            gchar* command = g_strdup_printf("docker save %s -o %s", data->image_id, escaped_filename);
            execute_command_async(command, on_image_exported, data);
            
            g_free(command);
            g_free(escaped_filename);
            g_free(filename);
        } else {
            g_free(data->image_id);
            g_free(data->image_name);
            g_free(data);
        }
    } else {
        g_free(data->image_id);
        g_free(data->image_name);
        g_free(data);
    }
    
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void on_import_file_dialog_response(GtkDialog* dialog, gint response_id, gpointer user_data) {
    ImportImageData* data = (ImportImageData*)user_data;
    
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser* chooser = GTK_FILE_CHOOSER(dialog);
        gchar* filename = gtk_file_chooser_get_filename(chooser);
        
        if (filename) {
            if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
                gchar* message = g_strdup_printf("File '%s' does not exist.", filename);
                show_message_dialog(data->parent_window, GTK_MESSAGE_ERROR, "Error", message);
                g_free(message);
                g_free(filename);
                g_free(data);
                gtk_widget_destroy(GTK_WIDGET(dialog));
                return;
            }
            gchar* escaped_filename = g_shell_quote(filename);
            gchar* command = g_strdup_printf("docker load -i %s", escaped_filename);
            execute_command_async(command, on_image_imported, data);
            
            g_free(command);
            g_free(escaped_filename);
            g_free(filename);
        } else {
            g_free(data);
        }
    } else {
        g_free(data);
    }
    
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void on_export_image_clicked(GtkMenuItem* menu_item, gpointer user_data) {
    RemoveImageData* data = (RemoveImageData*)user_data;
    if (!data || !data->tree_view || !data->store) {
        return;
    }
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return; // No row selected
    }
    gchar* image_id = NULL;
    gchar* repository = NULL;
    gchar* tag = NULL;
    gtk_tree_model_get(model, &iter, 
                       0, &repository,
                       1, &tag,
                       2, &image_id, -1);
    
    if (!image_id || strlen(image_id) == 0) {
        g_free(image_id);
        g_free(repository);
        g_free(tag);
        return;
    }
    gchar* image_name = NULL;
    if (repository && tag && strlen(repository) > 0 && strlen(tag) > 0) {
        image_name = g_strdup_printf("%s:%s", repository, tag);
    } else if (repository && strlen(repository) > 0) {
        image_name = g_strdup(repository);
    } else {
        image_name = g_strdup(image_id);
    }
    GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
    GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Export Image",
        parent_window,
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Save", GTK_RESPONSE_ACCEPT,
        NULL
    );
    
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    gchar* default_filename = g_strdup_printf("%s.tar", image_name);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), default_filename);
    g_free(default_filename);
    ExportImageData* export_data = g_new0(ExportImageData, 1);
    export_data->parent_window = parent_window;
    export_data->store = data->store;
    export_data->image_id = g_strdup(image_id);
    export_data->image_name = g_strdup(image_name);
    export_data->loading_dialog = NULL;  // Created when export starts
    g_signal_connect(dialog, "response", G_CALLBACK(on_export_file_dialog_response), export_data);
    gtk_widget_show_all(dialog);
    
    g_free(image_id);
    g_free(repository);
    g_free(tag);
    g_free(image_name);
}
static void on_container_created(gchar* output, gpointer user_data) {
    CreateContainerData* data = (CreateContainerData*)user_data;
    gboolean success = TRUE;
    gchar* container_id = NULL;
    
    if (!output) {
        success = FALSE;
    } else {
        gchar* trimmed_output = g_strstrip(g_strdup(output));
        if (strlen(trimmed_output) == 0) {
            success = FALSE;
        } else {
            gchar* lower_output = g_ascii_strdown(trimmed_output, -1);
            if (g_strrstr(lower_output, "error") || 
                g_strrstr(lower_output, "cannot") ||
                g_strrstr(lower_output, "failed") ||
                g_strrstr(lower_output, "denied") ||
                g_strrstr(lower_output, "permission") ||
                g_strrstr(lower_output, "not found") ||
                g_strrstr(lower_output, "invalid") ||
                g_strrstr(lower_output, "already in use")) {
                success = FALSE;
            } else {
                gchar* id = g_strstrip(g_strdup(trimmed_output));
                if (strlen(id) >= 12) {
                    container_id = g_strndup(id, 12);
                }
                g_free(id);
            }
            g_free(lower_output);
        }
        g_free(trimmed_output);
    }
    if (data && data->parent_window) {
        if (success) {
            gchar* container_id_str = container_id ? g_strdup_printf(" (ID: %s)", container_id) : g_strdup("");
            gchar* message = g_strdup_printf("Container created successfully from image '%s'%s", 
                                           data->image_name ? data->image_name : data->image_id ? data->image_id : "unknown",
                                           container_id_str);
            show_message_dialog(data->parent_window, GTK_MESSAGE_INFO, "Success", message);
            g_free(message);
            g_free(container_id_str);
        } else {
            gchar* error_msg = output ? g_strdup(output) : g_strdup("Unknown error while creating the container.");
            gchar* message = g_strdup_printf("Error creating container from image '%s':\n%s", 
                                              data->image_name ? data->image_name : data->image_id ? data->image_id : "unknown", 
                                              error_msg);
            show_message_dialog(data->parent_window, GTK_MESSAGE_ERROR, "Error", message);
            g_free(message);
            g_free(error_msg);
        }
    }
    if (data && data->containers_store) {
        refresh_containers_table_async(data->containers_store, data->containers_tree_view);
    }
    if (data) {
        g_free(data->image_id);
        g_free(data->image_name);
        g_free(data);
    }
    
    if (container_id) g_free(container_id);
    if (output) {
        g_free(output);
    }
}
static void on_create_container_dialog_response(GtkDialog* dialog, gint response_id, gpointer user_data) {
    CreateContainerData* data = (CreateContainerData*)user_data;
    
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GtkWidget* name_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "name-entry"));
        GtkWidget* ports_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "ports-entry"));
        GtkWidget* env_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "env-entry"));
        GtkWidget* command_entry = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "command-entry"));
        GtkWidget* interactive_check = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "interactive-check"));
        GtkWidget* detached_check = GTK_WIDGET(g_object_get_data(G_OBJECT(dialog), "detached-check"));
        GString* command = g_string_new("docker run");
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(detached_check))) {
            g_string_append(command, " -d");
        }
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(interactive_check)) && 
            !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(detached_check))) {
            g_string_append(command, " -it");
        }
        const gchar* name = gtk_entry_get_text(GTK_ENTRY(name_entry));
        if (name && strlen(name) > 0) {
            gchar* escaped_name = g_shell_quote(name);
            g_string_append_printf(command, " --name %s", escaped_name);
            g_free(escaped_name);
        }
        const gchar* ports = gtk_entry_get_text(GTK_ENTRY(ports_entry));
        if (ports && strlen(ports) > 0) {
            gchar** port_list = g_strsplit_set(ports, ", \n", -1);
            for (gint i = 0; port_list[i] != NULL; i++) {
                gchar* port = g_strstrip(port_list[i]);
                if (strlen(port) > 0) {
                    gchar* escaped_port = g_shell_quote(port);
                    g_string_append_printf(command, " -p %s", escaped_port);
                    g_free(escaped_port);
                }
            }
            g_strfreev(port_list);
        }
        const gchar* env = gtk_entry_get_text(GTK_ENTRY(env_entry));
        if (env && strlen(env) > 0) {
            gchar** env_list = g_strsplit_set(env, ", \n", -1);
            for (gint i = 0; env_list[i] != NULL; i++) {
                gchar* env_var = g_strstrip(env_list[i]);
                if (strlen(env_var) > 0) {
                    gchar* escaped_env = g_shell_quote(env_var);
                    g_string_append_printf(command, " -e %s", escaped_env);
                    g_free(escaped_env);
                }
            }
            g_strfreev(env_list);
        }
        gchar* escaped_image = g_shell_quote(data->image_id);
        g_string_append_printf(command, " %s", escaped_image);
        g_free(escaped_image);
        const gchar* cmd = gtk_entry_get_text(GTK_ENTRY(command_entry));
        if (cmd && strlen(cmd) > 0) {
            gchar* escaped_cmd = g_shell_quote(cmd);
            g_string_append_printf(command, " %s", escaped_cmd);
            g_free(escaped_cmd);
        }
        execute_command_async(command->str, on_container_created, data);
        
        g_string_free(command, TRUE);
    } else {
        g_free(data->image_id);
        g_free(data->image_name);
        g_free(data);
    }
    
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void on_create_container_clicked(GtkMenuItem* menu_item, gpointer user_data) {
    RemoveImageData* data = (RemoveImageData*)user_data;
    if (!data || !data->tree_view || !data->store) {
        return;
    }
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return; // No row selected
    }
    gchar* image_id = NULL;
    gchar* repository = NULL;
    gchar* tag = NULL;
    gtk_tree_model_get(model, &iter, 
                       0, &repository,
                       1, &tag,
                       2, &image_id, -1);
    
    if (!image_id || strlen(image_id) == 0) {
        g_free(image_id);
        g_free(repository);
        g_free(tag);
        return;
    }
    gchar* image_name = NULL;
    if (repository && tag && strlen(repository) > 0 && strlen(tag) > 0) {
        image_name = g_strdup_printf("%s:%s", repository, tag);
    } else if (repository && strlen(repository) > 0) {
        image_name = g_strdup(repository);
    } else {
        image_name = g_strdup(image_id);
    }
    GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
    GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
    GtkTreeStore* containers_store = NULL;
    GtkWidget* containers_tree_view = NULL;
    GtkWidget* window = gtk_widget_get_toplevel(data->tree_view);
    if (GTK_IS_WINDOW(window)) {
        containers_store = GTK_TREE_STORE(g_object_get_data(G_OBJECT(window), "containers-store"));
        containers_tree_view = GTK_WIDGET(g_object_get_data(G_OBJECT(window), "containers-tree-view"));
    }
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Create Container",
        parent_window,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Create", GTK_RESPONSE_ACCEPT,
        NULL
    );
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    gtk_window_set_resizable(GTK_WINDOW(dialog), TRUE);
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 12);
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), FALSE);
    
    gint row = 0;
    GtkWidget* image_label = gtk_label_new("Image:");
    gtk_label_set_xalign(GTK_LABEL(image_label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), image_label, 0, row, 1, 1);
    
    GtkWidget* image_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(image_entry), image_name);
    gtk_widget_set_sensitive(image_entry, FALSE);
    gtk_grid_attach(GTK_GRID(grid), image_entry, 1, row, 1, 1);
    row++;
    GtkWidget* name_label = gtk_label_new("Container Name:");
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), name_label, 0, row, 1, 1);
    
    GtkWidget* name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(name_entry), "Optional");
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, row, 1, 1);
    g_object_set_data(G_OBJECT(dialog), "name-entry", name_entry);
    row++;
    GtkWidget* ports_label = gtk_label_new("Ports (host:container):");
    gtk_label_set_xalign(GTK_LABEL(ports_label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), ports_label, 0, row, 1, 1);
    
    GtkWidget* ports_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ports_entry), "e.g.: 8080:80, 3306:3306");
    gtk_grid_attach(GTK_GRID(grid), ports_entry, 1, row, 1, 1);
    g_object_set_data(G_OBJECT(dialog), "ports-entry", ports_entry);
    row++;
    GtkWidget* env_label = gtk_label_new("Environment Variables:");
    gtk_label_set_xalign(GTK_LABEL(env_label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), env_label, 0, row, 1, 1);
    
    GtkWidget* env_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(env_entry), "e.g.: VAR1=value1, VAR2=value2");
    gtk_grid_attach(GTK_GRID(grid), env_entry, 1, row, 1, 1);
    g_object_set_data(G_OBJECT(dialog), "env-entry", env_entry);
    row++;
    GtkWidget* command_label = gtk_label_new("Command:");
    gtk_label_set_xalign(GTK_LABEL(command_label), 0.0);
    gtk_grid_attach(GTK_GRID(grid), command_label, 0, row, 1, 1);
    
    GtkWidget* command_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(command_entry), "Optional - command to run in the container");
    gtk_grid_attach(GTK_GRID(grid), command_entry, 1, row, 1, 1);
    g_object_set_data(G_OBJECT(dialog), "command-entry", command_entry);
    row++;
    GtkWidget* interactive_check = gtk_check_button_new_with_label("Interactive Mode (-it)");
    gtk_grid_attach(GTK_GRID(grid), interactive_check, 1, row, 1, 1);
    g_object_set_data(G_OBJECT(dialog), "interactive-check", interactive_check);
    row++;
    GtkWidget* detached_check = gtk_check_button_new_with_label("Detached Mode (-d) - Run in background");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(detached_check), TRUE); // Default: detached
    gtk_grid_attach(GTK_GRID(grid), detached_check, 1, row, 1, 1);
    g_object_set_data(G_OBJECT(dialog), "detached-check", detached_check);
    row++;
    gtk_container_add(GTK_CONTAINER(content_area), grid);
    CreateContainerData* create_data = g_new0(CreateContainerData, 1);
    create_data->parent_window = parent_window;
    create_data->image_id = g_strdup(image_id);
    create_data->image_name = g_strdup(image_name);
    create_data->containers_store = containers_store;
    create_data->containers_tree_view = containers_tree_view;
    g_signal_connect(dialog, "response", G_CALLBACK(on_create_container_dialog_response), create_data);
    gtk_widget_show_all(dialog);
    
    g_free(image_id);
    g_free(repository);
    g_free(tag);
    g_free(image_name);
}
static void on_import_image_clicked(GtkMenuItem* menu_item, gpointer user_data) {
    RemoveImageData* data = (RemoveImageData*)user_data;
    if (!data || !data->tree_view || !data->store) {
        return;
    }
    GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
    GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
    GtkWidget* dialog = gtk_file_chooser_dialog_new(
        "Import Image",
        parent_window,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        NULL
    );
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Docker image files (*.tar)");
    gtk_file_filter_add_pattern(filter, "*.tar");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "All files");
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    ImportImageData* import_data = g_new(ImportImageData, 1);
    import_data->parent_window = parent_window;
    import_data->store = data->store;
    g_signal_connect(dialog, "response", G_CALLBACK(on_import_file_dialog_response), import_data);
    gtk_widget_show_all(dialog);
}
static void on_inspect_image_clicked(GtkMenuItem* menu_item, gpointer user_data) {
    RemoveImageData* data = (RemoveImageData*)user_data;
    if (!data || !data->tree_view) {
        return;
    }
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return; // No row selected
    }
    gchar* image_id = NULL;
    gtk_tree_model_get(model, &iter, 2, &image_id, -1);
    
    if (!image_id || strlen(image_id) == 0) {
        g_free(image_id);
        return;
    }
    GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
    GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
    InspectImageData* inspect_data = g_new(InspectImageData, 1);
    inspect_data->parent_window = parent_window;
    inspect_data->image_id = g_strdup(image_id);
    gchar* command = g_strdup_printf("docker image inspect %s", image_id);
    execute_command_async(command, on_image_inspect_complete, inspect_data);
    
    g_free(command);
    g_free(image_id);
}
static void on_remove_image_clicked(GtkMenuItem* menu_item, gpointer user_data) {
    RemoveImageData* data = (RemoveImageData*)user_data;
    if (!data || !data->tree_view || !data->store) {
        return;
    }
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return; // No row selected
    }
    gchar* image_id = NULL;
    gchar* repository = NULL;
    gchar* tag = NULL;
    gtk_tree_model_get(model, &iter, 
                       0, &repository,
                       1, &tag,
                       2, &image_id, -1);
    
    if (!image_id || strlen(image_id) == 0) {
        g_free(image_id);
        g_free(repository);
        g_free(tag);
        return;
    }
    gchar* image_name = NULL;
    if (repository && tag && strlen(repository) > 0 && strlen(tag) > 0) {
        image_name = g_strdup_printf("%s:%s", repository, tag);
    } else if (repository && strlen(repository) > 0) {
        image_name = g_strdup(repository);
    } else {
        image_name = g_strdup(image_id);
    }
    GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
    GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
    gchar* message = g_strdup_printf("Are you sure you want to remove image '%s'?\n\nThis action cannot be undone.", image_name);
    GtkWidget* dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL, 
                                            GTK_MESSAGE_QUESTION, 
                                            GTK_BUTTONS_YES_NO, 
                                            "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Removal");
    g_free(message);
    RemoveImageData* confirm_data = g_new(RemoveImageData, 1);
    confirm_data->store = data->store;
    confirm_data->tree_view = data->tree_view;
    confirm_data->parent_window = parent_window;
    confirm_data->image_id = g_strdup(image_id);
    confirm_data->image_name = g_strdup(image_name);
    g_signal_connect(dialog, "response", G_CALLBACK(on_confirm_image_dialog_response), confirm_data);
    gtk_widget_show_all(dialog);
    
    g_free(image_id);
    g_free(repository);
    g_free(tag);
    g_free(image_name);
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

GtkWidget* create_images_table(void) {
    GtkListStore* store = gtk_list_store_new(5,
                                              G_TYPE_STRING,  // REPOSITORY
                                              G_TYPE_STRING,  // TAG
                                              G_TYPE_STRING,  // IMAGE ID
                                              G_TYPE_STRING,  // CREATED
                                              G_TYPE_STRING); // SIZE
    populate_docker_images_async(store);
    GtkWidget* search_entry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Filter images...");
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
        "REPOSITORY", renderer, "text", 0, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "TAG", renderer, "text", 1, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 1);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "IMAGE ID", renderer, "text", 2, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 2);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "CREATED", renderer, "text", 3, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 3);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "SIZE", renderer, "text", 4, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 4);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    GtkWidget* context_menu = gtk_menu_new();
    RemoveImageData* menu_data = g_new(RemoveImageData, 1);
    menu_data->store = store;
    menu_data->tree_view = tree_view;
    g_object_set_data_full(G_OBJECT(tree_view), "images-store", 
                          g_object_ref(store), g_object_unref);
    GtkWidget* inspect_item = gtk_menu_item_new_with_label("View details");
    g_signal_connect(inspect_item, "activate", G_CALLBACK(on_inspect_image_clicked), menu_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(context_menu), inspect_item);
    gtk_widget_show(inspect_item);
    GtkWidget* separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(context_menu), separator);
    gtk_widget_show(separator);
    GtkWidget* create_container_item = gtk_menu_item_new_with_label("Create container");
    g_signal_connect(create_container_item, "activate", G_CALLBACK(on_create_container_clicked), menu_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(context_menu), create_container_item);
    gtk_widget_show(create_container_item);
    separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(context_menu), separator);
    gtk_widget_show(separator);
    GtkWidget* export_item = gtk_menu_item_new_with_label("Export");
    g_signal_connect(export_item, "activate", G_CALLBACK(on_export_image_clicked), menu_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(context_menu), export_item);
    gtk_widget_show(export_item);
    GtkWidget* import_item = gtk_menu_item_new_with_label("Import");
    g_signal_connect(import_item, "activate", G_CALLBACK(on_import_image_clicked), menu_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(context_menu), import_item);
    gtk_widget_show(import_item);
    separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(context_menu), separator);
    gtk_widget_show(separator);
    GtkWidget* remove_item = gtk_menu_item_new_with_label("Remove");
    g_signal_connect(remove_item, "activate", G_CALLBACK(on_remove_image_clicked), menu_data);
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
