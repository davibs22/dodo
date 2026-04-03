#include "containers_table.h"
#include "../models/container.h"
#include "../docker/docker_command.h"
#include "../utils/status_utils.h"
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <unistd.h>
#include <stdio.h>
#include <gio/gio.h>
typedef struct {
    GtkTreeStore* store;
    GtkTreeModelFilter* filter;
    GtkWidget* tree_view;
    GtkWindow* parent_window;
    gchar* project_name;
} ContainerTableData;
typedef struct {
    GtkTreeStore* store;
    GtkWidget* tree_view;
    GtkWindow* parent_window;
    gchar* project_name;
} ComposeDownData;
typedef struct {
    GtkWindow* parent_window;
    gchar* container_id;
} InspectContainerData;
typedef struct {
    GtkWindow* dialog;
    GtkTextView* text_view;
    GtkTextBuffer* text_buffer;
    CommandStream* stream;
    gchar* container_id;
    gchar* container_name;
    gboolean is_streaming;
    gboolean is_destroyed; // Flag indicating dialog was destroyed
} LogsWindowData;
typedef struct {
    GtkTreeStore* store;
    GtkWidget* tree_view;
    GtkWindow* parent_window;
    gchar* container_id;
    gchar* container_name;
    gboolean is_running;
} RemoveContainerData;
static void convert_to_store_iter(GtkTreeModel* model, GtkTreeIter* model_iter, GtkTreeIter* store_iter) {
    GtkTreeIter current_iter = *model_iter;
    GtkTreeModel* current_model = model;
    if (GTK_IS_TREE_MODEL_SORT(current_model)) {
        GtkTreeIter sort_child_iter;
        gtk_tree_model_sort_convert_iter_to_child_iter(
            GTK_TREE_MODEL_SORT(current_model), &sort_child_iter, &current_iter);
        current_model = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(current_model));
        current_iter = sort_child_iter;
    }
    if (GTK_IS_TREE_MODEL_FILTER(current_model)) {
        GtkTreeIter filter_child_iter;
        gtk_tree_model_filter_convert_iter_to_child_iter(
            GTK_TREE_MODEL_FILTER(current_model), &filter_child_iter, &current_iter);
        current_iter = filter_child_iter;
    }
    
    *store_iter = current_iter;
}
static gboolean row_matches_search(GtkTreeModel* model, GtkTreeIter* iter, const gchar* search_lower) {
    gint n_columns = gtk_tree_model_get_n_columns(model);
    gboolean matches = FALSE;
    
    for (gint i = 0; i < n_columns && !matches; i++) {
        if (gtk_tree_model_get_column_type(model, i) == G_TYPE_STRING) {
            gchar* value = NULL;
            gtk_tree_model_get(model, iter, i, &value, -1);
            if (value) {
                gchar* value_lower = g_ascii_strdown(value, -1);
                matches = (g_strstr_len(value_lower, -1, search_lower) != NULL);
                g_free(value_lower);
                g_free(value);
            }
        }
    }
    
    return matches;
}
static gboolean containers_filter_func(GtkTreeModel* model, GtkTreeIter* iter, gpointer data) {
    GtkEntry* search_entry = GTK_ENTRY(data);
    const gchar* search_text = gtk_entry_get_text(search_entry);
    
    if (!search_text || strlen(search_text) == 0) {
        return TRUE; // Show all if there is no search text
    }
    
    gchar* search_lower = g_ascii_strdown(search_text, -1);
    gboolean visible = FALSE;
    if (row_matches_search(model, iter, search_lower)) {
        visible = TRUE;
    }
    if (!visible && gtk_tree_model_iter_has_child(model, iter)) {
        GtkTreeIter child;
        if (gtk_tree_model_iter_children(model, &child, iter)) {
            do {
                if (row_matches_search(model, &child, search_lower)) {
                    visible = TRUE;
                    break;
                }
            } while (gtk_tree_model_iter_next(model, &child));
        }
    }
    if (!visible) {
        GtkTreeIter parent;
        if (gtk_tree_model_iter_parent(model, &parent, iter)) {
            if (row_matches_search(model, &parent, search_lower)) {
                visible = TRUE;
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
static void set_container_loading_status(GtkTreeStore* store, GtkTreeIter* iter) {
    gchar* loading_status;
    gchar* loading_color;
    get_loading_status_and_color(&loading_status, &loading_color);
    
    gtk_tree_store_set(store, iter,
                      0, loading_status,
                      1, loading_color,
                      -1);
    
    g_free(loading_status);
    if (loading_color) g_free(loading_color);
}
static void set_group_loading_status(GtkTreeStore* store, GtkTreeIter* iter) {
    gchar* loading_status = g_strdup("\xe2\x9f\xb3 Loading");
    gchar* loading_color = g_strdup("#FFAA00");
    
    gtk_tree_store_set(store, iter,
                      0, loading_status,
                      1, loading_color,
                      -1);
    
    g_free(loading_status);
    g_free(loading_color);
}
static void on_confirm_remove_container_response(GtkDialog* dialog, gint response_id, gpointer user_data);
static void on_docker_action_complete(gchar* output, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    g_free(output); // No need for command action output
    refresh_containers_table_async(data->store, data->tree_view);
}
static void on_container_remove_complete(gchar* output, gpointer user_data) {
    RemoveContainerData* data = (RemoveContainerData*)user_data;
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
                g_strrstr(lower_output, "is running")) {
                success = FALSE;
            }
            g_free(lower_output);
        }
        g_free(trimmed_output);
    }
    if (data && data->parent_window) {
        if (success) {
            gchar* message = g_strdup_printf("Container '%s' was removed successfully.", 
                                             data->container_name ? data->container_name : data->container_id);
            show_message_dialog(data->parent_window, GTK_MESSAGE_INFO, "Success", message);
            g_free(message);
        } else {
            gchar* error_msg = output ? g_strdup(output) : g_strdup("Unknown error while removing container.");
            gchar* message = g_strdup_printf("Error ao remover container '%s':\n%s", 
                                             data->container_name ? data->container_name : data->container_id,
                                             error_msg);
            show_message_dialog(data->parent_window, GTK_MESSAGE_ERROR, "Error", message);
            g_free(message);
            g_free(error_msg);
        }
    }
    if (data && data->store) {
        refresh_containers_table_async(data->store, data->tree_view);
    }
    if (data) {
        g_free(data->container_id);
        g_free(data->container_name);
        g_free(data);
    }
    if (output) {
        g_free(output);
    }
}
static void on_compose_down_complete(gchar* output, gpointer user_data) {
    ComposeDownData* data = (ComposeDownData*)user_data;
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
                g_strrstr(lower_output, "permission")) {
                success = FALSE;
            }
            g_free(lower_output);
        }
        g_free(trimmed_output);
    }
    if (data && data->parent_window) {
        if (success) {
            gchar* message = g_strdup_printf("Compose project '%s' was removed successfully.", 
                                             data->project_name ? data->project_name : "unknown");
            show_message_dialog(data->parent_window, GTK_MESSAGE_INFO, "Success", message);
            g_free(message);
        } else {
            gchar* error_msg = output ? g_strdup(output) : g_strdup("Error unknown ao executar compose down.");
            gchar* message = g_strdup_printf("Error ao executar compose down no projeto '%s':\n%s", 
                                                  data->project_name ? data->project_name : "unknown", 
                                                  error_msg);
            show_message_dialog(data->parent_window, GTK_MESSAGE_ERROR, "Error", message);
            g_free(message);
            g_free(error_msg);
        }
    }
    if (data && data->store) {
        refresh_containers_table_async(data->store, data->tree_view);
    }
    if (data) {
        g_free(data->project_name);
        g_free(data);
    }
    if (output) {
        g_free(output);
    }
}
static void on_stop_container(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (gtk_tree_model_iter_has_child(model, &iter)) {
            return;
        }
        
        gchar* container_id;
        gtk_tree_model_get(model, &iter, 2, &container_id, -1);
        
        if (container_id && strlen(container_id) > 0) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_container_loading_status(data->store, &store_iter);
            
            gchar* command = g_strdup_printf("docker stop %s", container_id);
            execute_command_async(command, on_docker_action_complete, data);
            g_free(command);
        }
        
        if (container_id) g_free(container_id);
    }
}
static void on_restart_container(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (gtk_tree_model_iter_has_child(model, &iter)) {
            return;
        }
        
        gchar* container_id;
        gtk_tree_model_get(model, &iter, 2, &container_id, -1);
        
        if (container_id && strlen(container_id) > 0) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_container_loading_status(data->store, &store_iter);
            
            gchar* command = g_strdup_printf("docker restart %s", container_id);
            execute_command_async(command, on_docker_action_complete, data);
            g_free(command);
        }
        
        if (container_id) g_free(container_id);
    }
}
static void on_start_container(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (gtk_tree_model_iter_has_child(model, &iter)) {
            return;
        }
        
        gchar* container_id;
        gtk_tree_model_get(model, &iter, 2, &container_id, -1);
        
        if (container_id && strlen(container_id) > 0) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_container_loading_status(data->store, &store_iter);
            
            gchar* command = g_strdup_printf("docker start %s", container_id);
            execute_command_async(command, on_docker_action_complete, data);
            g_free(command);
        }
        
        if (container_id) g_free(container_id);
    }
}
static void on_pause_container(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (gtk_tree_model_iter_has_child(model, &iter)) {
            return;
        }
        
        gchar* container_id;
        gtk_tree_model_get(model, &iter, 2, &container_id, -1);
        
        if (container_id && strlen(container_id) > 0) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_container_loading_status(data->store, &store_iter);
            
            gchar* command = g_strdup_printf("docker pause %s", container_id);
            execute_command_async(command, on_docker_action_complete, data);
            g_free(command);
        }
        
        if (container_id) g_free(container_id);
    }
}
static void on_unpause_container(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (gtk_tree_model_iter_has_child(model, &iter)) {
            return;
        }
        
        gchar* container_id;
        gtk_tree_model_get(model, &iter, 2, &container_id, -1);
        
        if (container_id && strlen(container_id) > 0) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_container_loading_status(data->store, &store_iter);
            
            gchar* command = g_strdup_printf("docker unpause %s", container_id);
            execute_command_async(command, on_docker_action_complete, data);
            g_free(command);
        }
        
        if (container_id) g_free(container_id);
    }
}
static void on_remove_container(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        if (gtk_tree_model_iter_has_child(model, &iter)) {
            return; // Do not remove groups
        }
        
        gchar* container_id = NULL;
        gchar* container_name = NULL;
        gchar* status_text = NULL;
        gboolean is_running = FALSE;
        
        gtk_tree_model_get(model, &iter, 2, &container_id, 8, &container_name, 0, &status_text, -1);
        
        if (container_id && strlen(container_id) > 0) {
            if (status_text && g_str_has_prefix(status_text, "\xe2\x97\x8f")) {
                is_running = TRUE;
            }
            GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
            GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
            gchar* display_name = container_name && strlen(container_name) > 0 ? container_name : container_id;
            gchar* message;
            if (is_running) {
                message = g_strdup_printf(
                    "Are you sure you want to remove container '%s'?\n\n"
                    "The container is running and will be force-stopped and removed. "
                    "This action cannot be undone.",
                    display_name
                );
            } else {
                message = g_strdup_printf(
                    "Are you sure you want to remove container '%s'?\n\n"
                    "This action cannot be undone.",
                    display_name
                );
            }
            GtkWidget* dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL, 
                                                    GTK_MESSAGE_QUESTION, 
                                                    GTK_BUTTONS_YES_NO, 
                                                    "%s", message);
            gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Container Removal");
            g_free(message);
            RemoveContainerData* confirm_data = g_new(RemoveContainerData, 1);
            confirm_data->store = data->store;
            confirm_data->tree_view = data->tree_view;
            confirm_data->parent_window = parent_window;
            confirm_data->container_id = g_strdup(container_id);
            confirm_data->container_name = container_name ? g_strdup(container_name) : NULL;
            confirm_data->is_running = is_running;
            g_signal_connect(dialog, "response", G_CALLBACK(on_confirm_remove_container_response), confirm_data);
            gtk_widget_show_all(dialog);
        }
        
        if (container_id) g_free(container_id);
        if (container_name) g_free(container_name);
        if (status_text) g_free(status_text);
    }
}
static void on_confirm_remove_container_response(GtkDialog* dialog, gint response_id, gpointer user_data) {
    RemoveContainerData* confirm_data = (RemoveContainerData*)user_data;
    
    if (response_id == GTK_RESPONSE_YES) {
        if (!confirm_data || !confirm_data->tree_view || !confirm_data->store || !confirm_data->container_id) {
            if (confirm_data) {
                g_free(confirm_data->container_id);
                g_free(confirm_data->container_name);
                g_free(confirm_data);
            }
            gtk_widget_destroy(GTK_WIDGET(dialog));
            return;
        }
        GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(confirm_data->tree_view));
        GtkTreeModel* model;
        GtkTreeIter iter;
        
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_container_loading_status(confirm_data->store, &store_iter);
        }
        gchar* command;
        if (confirm_data->is_running) {
            command = g_strdup_printf("docker rm -f %s", confirm_data->container_id);
        } else {
            command = g_strdup_printf("docker rm %s", confirm_data->container_id);
        }
        execute_command_async(command, on_container_remove_complete, confirm_data);
        
        g_free(command);
    } else {
        if (confirm_data) {
            g_free(confirm_data->container_id);
            g_free(confirm_data->container_name);
            g_free(confirm_data);
        }
    }
    
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void on_confirm_compose_down_response(GtkDialog* dialog, gint response_id, gpointer user_data) {
    ComposeDownData* confirm_data = (ComposeDownData*)user_data;
    
    if (response_id == GTK_RESPONSE_YES) {
        if (!confirm_data || !confirm_data->tree_view || !confirm_data->store) {
            g_free(confirm_data->project_name);
            g_free(confirm_data);
            gtk_widget_destroy(GTK_WIDGET(dialog));
            return;
        }
        GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(confirm_data->tree_view));
        GtkTreeModel* model;
        GtkTreeIter iter;
        
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_group_loading_status(confirm_data->store, &store_iter);
        }
        gchar* command = g_strdup_printf("docker compose -p %s down", confirm_data->project_name);
        execute_command_async(command, on_compose_down_complete, confirm_data);
        
        g_free(command);
    } else {
        g_free(confirm_data->project_name);
        g_free(confirm_data);
    }
    
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
static void on_compose_down(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar* project_name;
        gtk_tree_model_get(model, &iter, 8, &project_name, -1);
        
        if (project_name && strlen(project_name) > 0) {
            GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
            GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
            gchar* message = g_strdup_printf("Are you sure you want to run 'compose down' on project '%s'?\n\nThis will stop and remove all containers, networks, and volumes for this project. This action cannot be undone.", project_name);
            GtkWidget* dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL, 
                                                    GTK_MESSAGE_QUESTION, 
                                                    GTK_BUTTONS_YES_NO, 
                                                    "%s", message);
            gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Compose Down");
            g_free(message);
            ComposeDownData* confirm_data = g_new(ComposeDownData, 1);
            confirm_data->store = data->store;
            confirm_data->tree_view = data->tree_view;
            confirm_data->parent_window = parent_window;
            confirm_data->project_name = g_strdup(project_name);
            g_signal_connect(dialog, "response", G_CALLBACK(on_confirm_compose_down_response), confirm_data);
            gtk_widget_show_all(dialog);
        }
        
        if (project_name) g_free(project_name);
    }
}
static void on_compose_stop(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar* project_name;
        gtk_tree_model_get(model, &iter, 8, &project_name, -1);
        
        if (project_name && strlen(project_name) > 0) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_group_loading_status(data->store, &store_iter);
            
            gchar* command = g_strdup_printf("docker compose -p %s stop", project_name);
            execute_command_async(command, on_docker_action_complete, data);
            g_free(command);
        }
        
        if (project_name) g_free(project_name);
    }
}
static void on_compose_start(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar* project_name;
        gtk_tree_model_get(model, &iter, 8, &project_name, -1);
        
        if (project_name && strlen(project_name) > 0) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_group_loading_status(data->store, &store_iter);
            
            gchar* command = g_strdup_printf("docker compose -p %s start", project_name);
            execute_command_async(command, on_docker_action_complete, data);
            g_free(command);
        }
        
        if (project_name) g_free(project_name);
    }
}
static void on_compose_restart(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar* project_name;
        gtk_tree_model_get(model, &iter, 8, &project_name, -1);
        
        if (project_name && strlen(project_name) > 0) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_group_loading_status(data->store, &store_iter);
            
            gchar* command = g_strdup_printf("docker compose -p %s restart", project_name);
            execute_command_async(command, on_docker_action_complete, data);
            g_free(command);
        }
        
        if (project_name) g_free(project_name);
    }
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
static void on_container_inspect_complete(gchar* output, gpointer user_data) {
    InspectContainerData* data = (InspectContainerData*)user_data;
    
    if (!data || !data->parent_window) {
        if (data) {
            g_free(data->container_id);
            g_free(data);
        }
        if (output) g_free(output);
        return;
    }
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Container Details",
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
            gchar* temp_file = g_strdup_printf("/tmp/docker_container_inspect_%d.json", getpid());
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
        gchar* error_msg = g_strdup_printf("Error ao obter detalhes do container '%s'.\n\n%s",
                                           data->container_id ? data->container_id : "unknown",
                                           output ? output : "Error unknown.");
        gtk_text_buffer_set_text(buffer, error_msg, -1);
        g_free(error_msg);
    }
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled);
    g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);
    gtk_widget_show_all(dialog);
    g_free(data->container_id);
    g_free(data);
    if (output) g_free(output);
}
static void on_logs_chunk_received(gchar* chunk, gpointer user_data) {
    LogsWindowData* data = (LogsWindowData*)user_data;
    if (!data || data->is_destroyed || !data->text_buffer || !GTK_IS_TEXT_BUFFER(data->text_buffer)) {
        if (chunk) g_free(chunk);
        return;
    }
    
    if (chunk) {
        GtkTextIter end_iter;
        gtk_text_buffer_get_end_iter(data->text_buffer, &end_iter);
        gtk_text_buffer_insert(data->text_buffer, &end_iter, chunk, -1);
        if (data->text_view && GTK_IS_TEXT_VIEW(data->text_view)) {
            gtk_text_view_scroll_to_mark(data->text_view,
                                         gtk_text_buffer_get_mark(data->text_buffer, "insert"),
                                         0.0, FALSE, 0.0, 0.0);
        }
        
        g_free(chunk);
    } else {
        data->is_streaming = FALSE;
        if (data->stream) {
            command_stream_stop(data->stream);
            data->stream = NULL;
        }
    }
}
static void on_logs_dialog_destroy(GtkWidget* widget, gpointer user_data) {
    LogsWindowData* data = (LogsWindowData*)user_data;
    
    if (data) {
        data->is_destroyed = TRUE;
        if (data->is_streaming && data->stream) {
            if (data->stream) {
                data->stream->user_data = NULL;
                data->stream->callback = NULL;
            }
            command_stream_stop(data->stream);
            data->stream = NULL;
        }
        data->dialog = NULL;
        data->text_view = NULL;
        data->text_buffer = NULL;
        if (data->container_id) g_free(data->container_id);
        if (data->container_name) g_free(data->container_name);
        g_free(data);
    }
}
static void show_container_logs(GtkWindow* parent_window, const gchar* container_id, const gchar* container_name) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Container Logs",
        parent_window,
        GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close",
        GTK_RESPONSE_CLOSE,
        NULL
    );
    
    gtk_window_set_default_size(GTK_WINDOW(dialog), 900, 600);
    LogsWindowData* data = g_new0(LogsWindowData, 1);
    data->dialog = GTK_WINDOW(dialog);
    data->container_id = g_strdup(container_id);
    data->container_name = container_name ? g_strdup(container_name) : NULL;
    data->is_streaming = FALSE;
    data->is_destroyed = FALSE;
    GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content_area), 12);
    GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled), 500);
    GtkWidget* text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    data->text_view = GTK_TEXT_VIEW(text_view);
    data->text_buffer = buffer;
    gchar* initial_command = g_strdup_printf("docker logs --tail 100 %s", container_id);
    gchar* initial_output = execute_command(initial_command);
    g_free(initial_command);
    
    if (initial_output && strlen(initial_output) > 0) {
        gtk_text_buffer_set_text(buffer, initial_output, -1);
        GtkTextIter end_iter;
        gtk_text_buffer_get_end_iter(buffer, &end_iter);
        gtk_text_buffer_place_cursor(buffer, &end_iter);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(text_view),
                                     gtk_text_buffer_get_mark(buffer, "insert"),
                                     0.0, FALSE, 0.0, 0.0);
    }
    if (initial_output) g_free(initial_output);
    gtk_container_add(GTK_CONTAINER(scrolled), text_view);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    g_signal_connect(dialog, "destroy", G_CALLBACK(on_logs_dialog_destroy), data);
    gchar* stream_command = g_strdup_printf("docker logs -f --tail 0 %s", container_id);
    data->stream = execute_command_stream(stream_command, on_logs_chunk_received, data);
    data->is_streaming = (data->stream != NULL);
    g_free(stream_command);
    gtk_widget_show_all(dialog);
}
static void on_view_logs_clicked(GtkMenuItem* menu_item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    if (!data || !data->tree_view) {
        return;
    }
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return; // No row selected
    }
    if (gtk_tree_model_iter_has_child(model, &iter)) {
        return; // Do not show group logs
    }
    gchar* container_id = NULL;
    gchar* container_name = NULL;
    gtk_tree_model_get(model, &iter, 2, &container_id, 8, &container_name, -1);
    
    if (!container_id || strlen(container_id) == 0) {
        if (container_id) g_free(container_id);
        if (container_name) g_free(container_name);
        return;
    }
    GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
    GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
    show_container_logs(parent_window, container_id, container_name);
    
    if (container_id) g_free(container_id);
    if (container_name) g_free(container_name);
}
static void on_inspect_container_clicked(GtkMenuItem* menu_item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    if (!data || !data->tree_view) {
        return;
    }
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return; // No row selected
    }
    if (gtk_tree_model_iter_has_child(model, &iter)) {
        return; // Do not inspect groups
    }
    gchar* container_id = NULL;
    gtk_tree_model_get(model, &iter, 2, &container_id, -1);
    
    if (!container_id || strlen(container_id) == 0) {
        g_free(container_id);
        return;
    }
    GtkWidget* toplevel = gtk_widget_get_toplevel(data->tree_view);
    GtkWindow* parent_window = GTK_IS_WINDOW(toplevel) ? GTK_WINDOW(toplevel) : NULL;
    InspectContainerData* inspect_data = g_new(InspectContainerData, 1);
    inspect_data->parent_window = parent_window;
    inspect_data->container_id = g_strdup(container_id);
    gchar* command = g_strdup_printf("docker inspect %s", container_id);
    execute_command_async(command, on_container_inspect_complete, inspect_data);
    
    g_free(command);
    g_free(container_id);
}
static void on_compose_up(GtkMenuItem* item, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeModel* model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gchar* project_name;
        gtk_tree_model_get(model, &iter, 8, &project_name, -1);
        
        if (project_name && strlen(project_name) > 0) {
            GtkTreeIter store_iter;
            convert_to_store_iter(model, &iter, &store_iter);
            set_group_loading_status(data->store, &store_iter);
            
            gchar* command = g_strdup_printf("docker compose -p %s up -d", project_name);
            execute_command_async(command, on_docker_action_complete, data);
            g_free(command);
        }
        
        if (project_name) g_free(project_name);
    }
}
static gboolean on_button_press_event(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
    ContainerTableData* data = (ContainerTableData*)user_data;
    
    if (event->button == GDK_BUTTON_SECONDARY) {
        GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
        GtkTreePath* path;
        
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget),
                                          (gint)event->x,
                                          (gint)event->y,
                                          &path, NULL, NULL, NULL)) {
            gtk_tree_selection_unselect_all(selection);
            gtk_tree_selection_select_path(selection, path);
            
            GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
            GtkTreeIter iter;
            gboolean is_running = FALSE;
            gboolean is_group = FALSE;
            gchar* project_name = NULL;
            
            if (gtk_tree_model_get_iter(model, &iter, path)) {
                if (gtk_tree_model_iter_has_child(model, &iter)) {
                    is_group = TRUE;
                    gtk_tree_model_get(model, &iter, 8, &project_name, -1);
                } else {
                    gchar* status_text;
                    gtk_tree_model_get(model, &iter, 0, &status_text, -1);
                    if (status_text && g_str_has_prefix(status_text, "\xe2\x97\x8f")) {
                        is_running = TRUE;
                    }
                    if (status_text) g_free(status_text);
                }
            }
            
            gtk_tree_path_free(path);
            
            GtkWidget* menu = gtk_menu_new();
            
            if (is_group && project_name && strlen(project_name) > 0 && 
                strcmp(project_name, "individual containers") != 0) {
                gchar* status_text;
                gtk_tree_model_get(model, &iter, 0, &status_text, -1);
                gboolean is_all_running = (status_text && g_str_has_prefix(status_text, "All Running"));
                gboolean is_all_stopped = (status_text && g_str_has_prefix(status_text, "All Stopped"));
                if (status_text) g_free(status_text);
                
                if (is_all_running) {
                    GtkWidget* stop_item = gtk_menu_item_new_with_label("Compose Stop");
                    g_signal_connect(stop_item, "activate", G_CALLBACK(on_compose_stop), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), stop_item);
                    
                    GtkWidget* restart_item = gtk_menu_item_new_with_label("Compose Restart");
                    g_signal_connect(restart_item, "activate", G_CALLBACK(on_compose_restart), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), restart_item);
                } else if (is_all_stopped) {
                    GtkWidget* start_item = gtk_menu_item_new_with_label("Compose Start");
                    g_signal_connect(start_item, "activate", G_CALLBACK(on_compose_start), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), start_item);
                    
                    GtkWidget* up_item = gtk_menu_item_new_with_label("Compose Up");
                    g_signal_connect(up_item, "activate", G_CALLBACK(on_compose_up), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), up_item);
                } else {
                    GtkWidget* stop_item = gtk_menu_item_new_with_label("Compose Stop");
                    g_signal_connect(stop_item, "activate", G_CALLBACK(on_compose_stop), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), stop_item);
                    
                    GtkWidget* restart_item = gtk_menu_item_new_with_label("Compose Restart");
                    g_signal_connect(restart_item, "activate", G_CALLBACK(on_compose_restart), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), restart_item);
                }
                
                GtkWidget* separator = gtk_separator_menu_item_new();
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
                
                GtkWidget* down_item = gtk_menu_item_new_with_label("Compose Down");
                g_signal_connect(down_item, "activate", G_CALLBACK(on_compose_down), data);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), down_item);
            } else if (!is_group) {
                GtkWidget* logs_item = gtk_menu_item_new_with_label("View logs");
                g_signal_connect(logs_item, "activate", G_CALLBACK(on_view_logs_clicked), data);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), logs_item);
                GtkWidget* inspect_item = gtk_menu_item_new_with_label("View details");
                g_signal_connect(inspect_item, "activate", G_CALLBACK(on_inspect_container_clicked), data);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), inspect_item);
                GtkWidget* separator = gtk_separator_menu_item_new();
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
                gchar* status_full = NULL;
                gtk_tree_model_get(model, &iter, 6, &status_full, -1);
                gboolean is_paused = (status_full && g_strstr_len(status_full, -1, "(Paused)") != NULL);
                
                if (is_paused) {
                    GtkWidget* unpause_item = gtk_menu_item_new_with_label("Unpause");
                    g_signal_connect(unpause_item, "activate", G_CALLBACK(on_unpause_container), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), unpause_item);
                } else if (is_running) {
                    GtkWidget* stop_item = gtk_menu_item_new_with_label("Stop");
                    g_signal_connect(stop_item, "activate", G_CALLBACK(on_stop_container), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), stop_item);
                    
                    GtkWidget* restart_item = gtk_menu_item_new_with_label("Restart");
                    g_signal_connect(restart_item, "activate", G_CALLBACK(on_restart_container), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), restart_item);
                    
                    GtkWidget* pause_item = gtk_menu_item_new_with_label("Pause");
                    g_signal_connect(pause_item, "activate", G_CALLBACK(on_pause_container), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), pause_item);
                } else {
                    GtkWidget* start_item = gtk_menu_item_new_with_label("Start");
                    g_signal_connect(start_item, "activate", G_CALLBACK(on_start_container), data);
                    gtk_menu_shell_append(GTK_MENU_SHELL(menu), start_item);
                }
                
                if (status_full) g_free(status_full);
                GtkWidget* separator2 = gtk_separator_menu_item_new();
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator2);
                GtkWidget* remove_item = gtk_menu_item_new_with_label("Remove");
                g_signal_connect(remove_item, "activate", G_CALLBACK(on_remove_container), data);
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), remove_item);
            }
            
            if (project_name) g_free(project_name);
            
            gtk_widget_show_all(menu);
            gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event);
            
            return TRUE;
        }
    }
    
    return FALSE;
}

GtkWidget* create_containers_table(void) {
    GtkTreeStore* store = gtk_tree_store_new(10,
                                              G_TYPE_STRING,  // STATUS (running/stopped)
                                              G_TYPE_STRING,  // COLOR (foreground)
                                              G_TYPE_STRING,  // CONTAINER ID
                                              G_TYPE_STRING,  // IMAGE
                                              G_TYPE_STRING,  // COMMAND
                                              G_TYPE_STRING,  // CREATED
                                              G_TYPE_STRING,  // STATUS (full)
                                              G_TYPE_STRING,  // PORTS
                                              G_TYPE_STRING,  // NAMES
                                              G_TYPE_STRING); // COMPOSE ICON
    populate_docker_containers_async(store);
    GtkWidget* search_entry = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Filter containers...");
    gtk_widget_set_margin_start(search_entry, 6);
    gtk_widget_set_margin_end(search_entry, 6);
    gtk_widget_set_margin_top(search_entry, 6);
    gtk_widget_set_margin_bottom(search_entry, 4);
    GtkTreeModelFilter* filter = GTK_TREE_MODEL_FILTER(
        gtk_tree_model_filter_new(GTK_TREE_MODEL(store), NULL));
    gtk_tree_model_filter_set_visible_func(filter, containers_filter_func, search_entry, NULL);
    g_signal_connect(search_entry, "search-changed", G_CALLBACK(on_search_changed), filter);
    GtkTreeModel* sort_model = gtk_tree_model_sort_new_with_model(GTK_TREE_MODEL(filter));
    g_object_unref(filter); // sort_model keeps the reference
    GtkWidget* tree_view = gtk_tree_view_new_with_model(sort_model);
    g_object_unref(sort_model); // TreeView keeps the reference
    ContainerTableData* data = g_new(ContainerTableData, 1);
    data->store = store;
    data->filter = filter;
    data->tree_view = tree_view;
    g_object_ref(store);
    g_signal_connect(tree_view, "button-press-event", G_CALLBACK(on_button_press_event), data);
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* compose_column = gtk_tree_view_column_new_with_attributes(
        "", renderer, "text", 9, NULL);
    gtk_tree_view_column_set_resizable(compose_column, FALSE);
    gtk_tree_view_column_set_sizing(compose_column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(compose_column, 45);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), compose_column);
    renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(
        "STATUS", renderer, "text", 0, "foreground", 1, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "NAMES", renderer, "text", 8, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 8);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "CONTAINER ID", renderer, "text", 2, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 2);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "IMAGE", renderer, "text", 3, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 3);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "COMMAND", renderer, "text", 4, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 4);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "CREATED", renderer, "text", 5, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 5);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "STATUS", renderer, "text", 6, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 6);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
        "PORTS", renderer, "text", 7, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, 7);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_set_expander_column(GTK_TREE_VIEW(tree_view), compose_column);
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
