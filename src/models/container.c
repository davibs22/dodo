#include "container.h"
#include "../docker/docker_command.h"
#include "../utils/status_utils.h"
#include <string.h>
#include <glib.h>
#include <gio/gio.h>
typedef struct {
    gchar* id;
    gchar* image;
    gchar* command;
    gchar* created;
    gchar* status;
    gchar* ports;
    gchar* names;
    gchar* compose_project;
} ContainerInfo;
static gchar* get_compose_project(const gchar* container_id) {
    gchar* command = g_strdup_printf("docker inspect --format '{{index .Config.Labels \"com.docker.compose.project\"}}' %s", container_id);
    gchar* output = execute_command(command);
    g_free(command);
    
    if (output && strlen(output) > 0 && strlen(output) < 200) {
        g_strstrip(output);
        if (strlen(output) > 0 && strcmp(output, "<no value>") != 0) {
            return output;
        }
    }
    
    if (output) g_free(output);
    return NULL;
}
static void free_container_info(ContainerInfo* info) {
    if (info) {
        g_free(info->id);
        g_free(info->image);
        g_free(info->command);
        g_free(info->created);
        g_free(info->status);
        g_free(info->ports);
        g_free(info->names);
        g_free(info->compose_project);
        g_free(info);
    }
}
static void get_group_status(GPtrArray* containers, gchar** status_text, gchar** color) {
    if (containers == NULL || containers->len == 0) {
        *status_text = g_strdup("");
        *color = NULL;
        return;
    }
    
    guint running_count = 0;
    guint total_count = containers->len;
    
    for (guint i = 0; i < containers->len; i++) {
        ContainerInfo* info = g_ptr_array_index(containers, i);
        if (info->status && g_str_has_prefix(info->status, "Up")) {
            running_count++;
        }
    }
    
    if (running_count == 0) {
        *status_text = g_strdup_printf("All Stopped (%d)", total_count);
        *color = NULL; // Cor padrão
    } else if (running_count == total_count) {
        *status_text = g_strdup_printf("All Running (%d)", total_count);
        *color = g_strdup("#00AA00"); // Verde
    } else {
        *status_text = g_strdup_printf("%d/%d Running", running_count, total_count);
        *color = g_strdup("#FFAA00"); // Laranja/Amarelo para parcial
    }
}

typedef struct {
    GHashTable* project_groups;         // gchar* → GPtrArray* de ContainerInfo*
    GPtrArray* standalone_containers;   // GPtrArray de ContainerInfo*
    gboolean has_error;
} ContainersCollectedData;
static void free_collected_data(ContainersCollectedData* data) {
    if (data == NULL) return;
    
    if (data->project_groups) {
        g_hash_table_destroy(data->project_groups);
    }
    if (data->standalone_containers) {
        g_ptr_array_unref(data->standalone_containers);
    }
    g_free(data);
}
static void destroy_container_array(gpointer data) {
    GPtrArray* array = (GPtrArray*)data;
    g_ptr_array_unref(array);
}

static ContainersCollectedData* collect_containers_data(void) {
    ContainersCollectedData* result = g_new0(ContainersCollectedData, 1);
    
    gchar* output = execute_command("docker container ls -a --format '{{.ID}}\t{{.Image}}\t{{.Command}}\t{{.CreatedAt}}\t{{.Status}}\t{{.Ports}}\t{{.Names}}'");
    
    if (output == NULL) {
        result->has_error = TRUE;
        return result;
    }
    
    gchar** lines = g_strsplit(output, "\n", -1);
    g_free(output);
    result->project_groups = g_hash_table_new_full(
        g_str_hash, g_str_equal,
        g_free,                     // key destroy
        destroy_container_array     // value destroy (libera GPtrArray que libera ContainerInfo)
    );
    result->standalone_containers = g_ptr_array_new_with_free_func((GDestroyNotify)free_container_info);
    for (gint i = 0; lines[i] != NULL; i++) {
        if (strlen(lines[i]) == 0) continue;
        
        gchar** fields = g_strsplit(lines[i], "\t", -1);
        if (g_strv_length(fields) >= 7) {
            ContainerInfo* info = g_new0(ContainerInfo, 1);
            info->id = g_strdup(fields[0] ? fields[0] : "");
            info->image = g_strdup(fields[1] ? fields[1] : "");
            info->command = g_strdup(fields[2] ? fields[2] : "");
            info->created = g_strdup(fields[3] ? fields[3] : "");
            info->status = g_strdup(fields[4] ? fields[4] : "");
            info->ports = g_strdup(fields[5] ? fields[5] : "");
            info->names = g_strdup(fields[6] ? fields[6] : "");
            info->compose_project = get_compose_project(info->id);
            
            if (info->compose_project && strlen(info->compose_project) > 0) {
                GPtrArray* group = g_hash_table_lookup(result->project_groups, info->compose_project);
                if (group == NULL) {
                    group = g_ptr_array_new_with_free_func((GDestroyNotify)free_container_info);
                    g_hash_table_insert(result->project_groups, g_strdup(info->compose_project), group);
                }
                g_ptr_array_add(group, info);
            } else {
                g_free(info->compose_project);
                info->compose_project = NULL;
                g_ptr_array_add(result->standalone_containers, info);
            }
        }
        g_strfreev(fields);
    }
    
    g_strfreev(lines);
    return result;
}

static void apply_data_to_store(GtkTreeStore* store, ContainersCollectedData* data) {
    gtk_tree_store_clear(store);
    
    if (data->has_error) {
        GtkTreeIter iter;
        gtk_tree_store_append(store, &iter, NULL);
        gtk_tree_store_set(store, &iter,
                           0, "⚠ Erro",
                           1, NULL,
                           2, "Erro ao executar docker container ls -a",
                           3, "", 4, "", 5, "", 6, "", 7, "", 8, "", 9, "", -1);
        return;
    }
    GHashTableIter hash_iter;
    gpointer key, value;
    
    g_hash_table_iter_init(&hash_iter, data->project_groups);
    while (g_hash_table_iter_next(&hash_iter, &key, &value)) {
        gchar* project_name = (gchar*)key;
        GPtrArray* containers = (GPtrArray*)value;
        gchar* group_status;
        gchar* group_color;
        get_group_status(containers, &group_status, &group_color);
        GtkTreeIter group_iter;
        gtk_tree_store_append(store, &group_iter, NULL);
        gtk_tree_store_set(store, &group_iter,
                           0, group_status,
                           1, group_color,
                           2, "",
                           3, "",
                           4, "",
                           5, "",
                           6, "",
                           7, "",
                           8, project_name,
                           9, "⬢",
                           -1);
        
        g_free(group_status);
        if (group_color) g_free(group_color);
        for (guint j = 0; j < containers->len; j++) {
            ContainerInfo* info = g_ptr_array_index(containers, j);
            GtkTreeIter child_iter;
            gchar* running_status;
            gchar* color;
            
            get_running_status_and_color(info->status, &running_status, &color);
            
            gtk_tree_store_append(store, &child_iter, &group_iter);
            gtk_tree_store_set(store, &child_iter,
                               0, running_status,
                               1, color,
                               2, info->id,
                               3, info->image,
                               4, info->command,
                               5, info->created,
                               6, info->status,
                               7, info->ports,
                               8, info->names,
                               9, "",
                               -1);
            
            g_free(running_status);
            if (color) g_free(color);
        }
    }
    if (data->standalone_containers->len > 0) {
        gchar* group_status;
        gchar* group_color;
        get_group_status(data->standalone_containers, &group_status, &group_color);
        
        GtkTreeIter others_iter;
        gtk_tree_store_append(store, &others_iter, NULL);
        gtk_tree_store_set(store, &others_iter,
                           0, group_status,
                           1, group_color,
                           2, "",
                           3, "",
                           4, "",
                           5, "",
                           6, "",
                           7, "",
                           8, "individual containers",
                           9, "⬢",
                           -1);
        
        g_free(group_status);
        if (group_color) g_free(group_color);
        
        for (guint j = 0; j < data->standalone_containers->len; j++) {
            ContainerInfo* info = g_ptr_array_index(data->standalone_containers, j);
            GtkTreeIter child_iter;
            gchar* running_status;
            gchar* color;
            
            get_running_status_and_color(info->status, &running_status, &color);
            
            gtk_tree_store_append(store, &child_iter, &others_iter);
            gtk_tree_store_set(store, &child_iter,
                               0, running_status,
                               1, color,
                               2, info->id,
                               3, info->image,
                               4, info->command,
                               5, info->created,
                               6, info->status,
                               7, info->ports,
                               8, info->names,
                               9, "",
                               -1);
            
            g_free(running_status);
            if (color) g_free(color);
        }
    }
}

void populate_docker_containers(GtkTreeStore* store) {
    ContainersCollectedData* data = collect_containers_data();
    apply_data_to_store(store, data);
    free_collected_data(data);
}

void refresh_containers_table(GtkTreeStore* store) {
    gtk_tree_store_clear(store);
    populate_docker_containers(store);
}
typedef struct {
    GHashTable* expanded_projects;  // gchar* (project_name) → gboolean
} ExpansionState;
static void convert_view_iter_to_store_iter(GtkTreeModel* view_model, GtkTreeIter* view_iter, GtkTreeIter* store_iter) {
    GtkTreeIter current_iter = *view_iter;
    GtkTreeModel* current_model = view_model;
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
static ExpansionState* collect_expansion_state(GtkWidget* tree_view, GtkTreeStore* store) {
    ExpansionState* state = g_new0(ExpansionState, 1);
    state->expanded_projects = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    
    if (!tree_view || !GTK_IS_TREE_VIEW(tree_view)) {
        return state;
    }
    
    GtkTreeModel* view_model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));
    if (!view_model) {
        return state;
    }
    GtkTreeIter view_iter;
    if (gtk_tree_model_get_iter_first(view_model, &view_iter)) {
        do {
            if (gtk_tree_model_iter_has_child(view_model, &view_iter)) {
                GtkTreePath* view_path = gtk_tree_model_get_path(view_model, &view_iter);
                gboolean expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(tree_view), view_path);
                
                if (expanded) {
                    GtkTreeIter store_iter;
                    convert_view_iter_to_store_iter(view_model, &view_iter, &store_iter);
                    gchar* project_name = NULL;
                    gtk_tree_model_get(GTK_TREE_MODEL(store), &store_iter, 8, &project_name, -1);
                    
                    if (project_name && strlen(project_name) > 0) {
                        g_hash_table_insert(state->expanded_projects, 
                                           g_strdup(project_name), 
                                           GINT_TO_POINTER(TRUE));
                    }
                    if (project_name) g_free(project_name);
                }
                
                gtk_tree_path_free(view_path);
            }
        } while (gtk_tree_model_iter_next(view_model, &view_iter));
    }
    
    return state;
}
static void restore_expansion_state(GtkWidget* tree_view, GtkTreeStore* store, ExpansionState* state) {
    if (!tree_view || !GTK_IS_TREE_VIEW(tree_view) || !state || !state->expanded_projects) {
        return;
    }
    
    GtkTreeModel* view_model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));
    if (!view_model) {
        return;
    }
    GtkTreeIter view_iter;
    if (gtk_tree_model_get_iter_first(view_model, &view_iter)) {
        do {
            if (gtk_tree_model_iter_has_child(view_model, &view_iter)) {
                GtkTreeIter store_iter;
                convert_view_iter_to_store_iter(view_model, &view_iter, &store_iter);
                gchar* project_name = NULL;
                gtk_tree_model_get(GTK_TREE_MODEL(store), &store_iter, 8, &project_name, -1);
                
                if (project_name && strlen(project_name) > 0) {
                    if (g_hash_table_lookup(state->expanded_projects, project_name)) {
                        GtkTreePath* view_path = gtk_tree_model_get_path(view_model, &view_iter);
                        gtk_tree_view_expand_row(GTK_TREE_VIEW(tree_view), view_path, FALSE);
                        gtk_tree_path_free(view_path);
                    }
                }
                
                if (project_name) g_free(project_name);
            }
        } while (gtk_tree_model_iter_next(view_model, &view_iter));
    }
}
static void free_expansion_state(ExpansionState* state) {
    if (state) {
        if (state->expanded_projects) {
            g_hash_table_destroy(state->expanded_projects);
        }
        g_free(state);
    }
}
typedef struct {
    GtkTreeStore* store;
    GtkWidget* tree_view;
    ExpansionState* expansion_state;
} RefreshContainersData;
static void collect_containers_task_func(GTask* task,
                                          gpointer source_object,
                                          gpointer task_data,
                                          GCancellable* cancellable) {
    ContainersCollectedData* data = collect_containers_data();
    g_task_return_pointer(task, data, (GDestroyNotify)free_collected_data);
}
static void on_containers_collected(GObject* source_object,
                                     GAsyncResult* res,
                                     gpointer user_data) {
    RefreshContainersData* refresh_data = (RefreshContainersData*)user_data;
    GtkTreeStore* store = refresh_data->store;
    GError* error = NULL;
    
    ContainersCollectedData* data = g_task_propagate_pointer(G_TASK(res), &error);
    
    if (data == NULL) {
        if (error) {
            g_warning("populate_docker_containers_async: %s", error->message);
            g_error_free(error);
        }
        free_expansion_state(refresh_data->expansion_state);
        g_object_unref(store);
        g_free(refresh_data);
        return;
    }
    apply_data_to_store(store, data);
    restore_expansion_state(refresh_data->tree_view, store, refresh_data->expansion_state);
    free_collected_data(data);
    free_expansion_state(refresh_data->expansion_state);
    g_object_unref(store);
    g_free(refresh_data);
}

void populate_docker_containers_async(GtkTreeStore* store) {
    g_object_ref(store);
    RefreshContainersData* refresh_data = g_new(RefreshContainersData, 1);
    refresh_data->store = store;
    refresh_data->tree_view = NULL;
    refresh_data->expansion_state = NULL;
    
    GTask* task = g_task_new(NULL, NULL, on_containers_collected, refresh_data);
    g_task_run_in_thread(task, collect_containers_task_func);
    g_object_unref(task);
}

void refresh_containers_table_async(GtkTreeStore* store, GtkWidget* tree_view) {
    ExpansionState* expansion_state = NULL;
    if (tree_view) {
        expansion_state = collect_expansion_state(tree_view, store);
    }
    g_object_ref(store);
    RefreshContainersData* refresh_data = g_new(RefreshContainersData, 1);
    refresh_data->store = store;
    refresh_data->tree_view = tree_view;
    refresh_data->expansion_state = expansion_state;
    
    GTask* task = g_task_new(NULL, NULL, on_containers_collected, refresh_data);
    g_task_run_in_thread(task, collect_containers_task_func);
    g_object_unref(task);
}
