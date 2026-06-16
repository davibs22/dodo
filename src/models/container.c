#include "container.h"
#include "../docker/docker_command.h"
#include "../utils/status_utils.h"
#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define INITIAL_LOAD_DONE_KEY "dodo-initial-load-done"
#define STANDALONE_GROUP_NAME "individual containers"
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
        *color = NULL; // Default color
    } else if (running_count == total_count) {
        *status_text = g_strdup_printf("All Running (%d)", total_count);
        *color = g_strdup("#00AA00"); // Green
    } else {
        *status_text = g_strdup_printf("%d/%d Running", running_count, total_count);
        *color = g_strdup("#FFAA00"); // Orange/Yellow for partial
    }
}

typedef struct {
    GHashTable* project_groups;         // gchar* -> GPtrArray* of ContainerInfo*
    GPtrArray* standalone_containers;   // GPtrArray of ContainerInfo*
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
        destroy_container_array     // value destroy (frees GPtrArray and ContainerInfo)
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

static void set_container_row(GtkTreeStore* store, GtkTreeIter* iter, ContainerInfo* info) {
    gchar* running_status;
    gchar* color;

    get_running_status_and_color(info->status, &running_status, &color);
    gtk_tree_store_set(store, iter,
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

static void set_group_row(GtkTreeStore* store, GtkTreeIter* iter, const gchar* group_name, GPtrArray* containers) {
    gchar* group_status;
    gchar* group_color;

    get_group_status(containers, &group_status, &group_color);
    gtk_tree_store_set(store, iter,
                       0, group_status,
                       1, group_color,
                       2, "",
                       3, "",
                       4, "",
                       5, "",
                       6, "",
                       7, "",
                       8, group_name,
                       9, "⬢",
                       -1);
    g_free(group_status);
    if (group_color) g_free(group_color);
}

static gboolean find_group_iter(GtkTreeStore* store, const gchar* group_name, GtkTreeIter* group_iter) {
    GtkTreeIter iter;

    if (!group_name || !gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
        return FALSE;
    }

    do {
        if (!gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter)) {
            continue;
        }

        gchar* name = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 8, &name, -1);
        gboolean match = name && g_strcmp0(name, group_name) == 0;
        g_free(name);

        if (match) {
            *group_iter = iter;
            return TRUE;
        }
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));

    return FALSE;
}

static gboolean find_container_iter(GtkTreeStore* store, const gchar* container_id,
                                    GtkTreeIter* container_iter, gchar** group_name) {
    GtkTreeIter group_iter;

    if (!container_id || !gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &group_iter)) {
        return FALSE;
    }

    do {
        if (!gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &group_iter)) {
            continue;
        }

        gchar* current_group = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &group_iter, 8, &current_group, -1);

        GtkTreeIter child_iter;
        if (gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &child_iter, &group_iter)) {
            do {
                gchar* current_id = NULL;
                gtk_tree_model_get(GTK_TREE_MODEL(store), &child_iter, 2, &current_id, -1);

                if (current_id && g_strcmp0(current_id, container_id) == 0) {
                    *container_iter = child_iter;
                    if (group_name) {
                        *group_name = current_group;
                    } else {
                        g_free(current_group);
                    }
                    g_free(current_id);
                    return TRUE;
                }

                g_free(current_id);
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &child_iter));
        }

        g_free(current_group);
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &group_iter));

    return FALSE;
}

static GtkTreeIter ensure_group_iter(GtkTreeStore* store, const gchar* group_name, GPtrArray* containers) {
    GtkTreeIter group_iter;

    if (find_group_iter(store, group_name, &group_iter)) {
        set_group_row(store, &group_iter, group_name, containers);
        return group_iter;
    }

    gtk_tree_store_append(store, &group_iter, NULL);
    set_group_row(store, &group_iter, group_name, containers);
    return group_iter;
}

static void upsert_container_row(GtkTreeStore* store, GtkTreeIter* group_iter,
                                 const gchar* group_name, ContainerInfo* info) {
    GtkTreeIter container_iter;
    gchar* current_group = NULL;

    if (find_container_iter(store, info->id, &container_iter, &current_group)) {
        if (g_strcmp0(current_group, group_name) != 0) {
            gtk_tree_store_remove(store, &container_iter);
            gtk_tree_store_append(store, &container_iter, group_iter);
            set_container_row(store, &container_iter, info);
        } else {
            set_container_row(store, &container_iter, info);
        }
        g_free(current_group);
        return;
    }

    g_free(current_group);
    gtk_tree_store_append(store, &container_iter, group_iter);
    set_container_row(store, &container_iter, info);
}

static void collect_container_ids(ContainersCollectedData* data, GHashTable* ids) {
    GHashTableIter hash_iter;
    gpointer key, value;

    g_hash_table_iter_init(&hash_iter, data->project_groups);
    while (g_hash_table_iter_next(&hash_iter, &key, &value)) {
        GPtrArray* containers = (GPtrArray*)value;
        for (guint i = 0; i < containers->len; i++) {
            ContainerInfo* info = g_ptr_array_index(containers, i);
            if (info->id && info->id[0] != '\0') {
                g_hash_table_add(ids, info->id);
            }
        }
    }

    for (guint i = 0; i < data->standalone_containers->len; i++) {
        ContainerInfo* info = g_ptr_array_index(data->standalone_containers, i);
        if (info->id && info->id[0] != '\0') {
            g_hash_table_add(ids, info->id);
        }
    }
}

static void remove_stale_containers(GtkTreeStore* store, GHashTable* valid_ids) {
    GtkTreeModel* model = GTK_TREE_MODEL(store);
    GtkTreeIter group_iter;

    if (!gtk_tree_model_get_iter_first(model, &group_iter)) {
        return;
    }

    do {
        GtkTreeIter child_iter;
        gboolean valid = gtk_tree_model_iter_children(model, &child_iter, &group_iter);

        while (valid) {
            gchar* container_id = NULL;
            gtk_tree_model_get(model, &child_iter, 2, &container_id, -1);

            gboolean remove = !container_id || container_id[0] == '\0'
                              || !g_hash_table_contains(valid_ids, container_id);
            g_free(container_id);

            if (remove) {
                valid = gtk_tree_store_remove(store, &child_iter);
            } else {
                valid = gtk_tree_model_iter_next(model, &child_iter);
            }
        }
    } while (gtk_tree_model_iter_next(model, &group_iter));
}

static void remove_empty_groups(GtkTreeStore* store) {
    GtkTreeModel* model = GTK_TREE_MODEL(store);
    GtkTreeIter group_iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &group_iter);

    while (valid) {
        if (gtk_tree_model_iter_has_child(model, &group_iter)) {
            valid = gtk_tree_model_iter_next(model, &group_iter);
        } else {
            valid = gtk_tree_store_remove(store, &group_iter);
        }
    }
}

static void show_store_error(GtkTreeStore* store) {
    GtkTreeIter iter;

    gtk_tree_store_clear(store);
    gtk_tree_store_append(store, &iter, NULL);
    gtk_tree_store_set(store, &iter,
                       0, "⚠ Error",
                       1, NULL,
                       2, "Error running docker container ls -a",
                       3, "", 4, "", 5, "", 6, "", 7, "", 8, "", 9, "", -1);
}

static void sync_data_to_store(GtkTreeStore* store, ContainersCollectedData* data) {
    if (data->has_error) {
        show_store_error(store);
        return;
    }

    GHashTable* valid_ids = g_hash_table_new(g_str_hash, g_str_equal);
    collect_container_ids(data, valid_ids);
    remove_stale_containers(store, valid_ids);

    GHashTableIter hash_iter;
    gpointer key, value;

    g_hash_table_iter_init(&hash_iter, data->project_groups);
    while (g_hash_table_iter_next(&hash_iter, &key, &value)) {
        gchar* project_name = (gchar*)key;
        GPtrArray* containers = (GPtrArray*)value;
        GtkTreeIter group_iter = ensure_group_iter(store, project_name, containers);

        for (guint i = 0; i < containers->len; i++) {
            ContainerInfo* info = g_ptr_array_index(containers, i);
            upsert_container_row(store, &group_iter, project_name, info);
        }
    }

    if (data->standalone_containers->len > 0) {
        GtkTreeIter group_iter = ensure_group_iter(store, STANDALONE_GROUP_NAME, data->standalone_containers);

        for (guint i = 0; i < data->standalone_containers->len; i++) {
            ContainerInfo* info = g_ptr_array_index(data->standalone_containers, i);
            upsert_container_row(store, &group_iter, STANDALONE_GROUP_NAME, info);
        }
    } else {
        GtkTreeIter standalone_iter;
        if (find_group_iter(store, STANDALONE_GROUP_NAME, &standalone_iter)) {
            gtk_tree_store_remove(store, &standalone_iter);
        }
    }

    remove_empty_groups(store);
    g_hash_table_destroy(valid_ids);
}

void populate_docker_containers(GtkTreeStore* store) {
    ContainersCollectedData* data = collect_containers_data();
    sync_data_to_store(store, data);
    free_collected_data(data);
}

void refresh_containers_table(GtkTreeStore* store) {
    ContainersCollectedData* data = collect_containers_data();
    sync_data_to_store(store, data);
    free_collected_data(data);
}
typedef struct {
    GtkTreeStore* store;
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
        g_object_set_data(G_OBJECT(store), INITIAL_LOAD_DONE_KEY, GINT_TO_POINTER(TRUE));
        if (error) {
            g_warning("populate_docker_containers_async: %s", error->message);
            g_error_free(error);
        }
        g_object_unref(store);
        g_free(refresh_data);
        return;
    }
    sync_data_to_store(store, data);
    g_object_set_data(G_OBJECT(store), INITIAL_LOAD_DONE_KEY, GINT_TO_POINTER(TRUE));
    free_collected_data(data);
    g_object_unref(store);
    g_free(refresh_data);
}

void populate_docker_containers_async(GtkTreeStore* store) {
    g_object_set_data(G_OBJECT(store), INITIAL_LOAD_DONE_KEY, GINT_TO_POINTER(FALSE));
    g_object_ref(store);
    RefreshContainersData* refresh_data = g_new(RefreshContainersData, 1);
    refresh_data->store = store;

    GTask* task = g_task_new(NULL, NULL, on_containers_collected, refresh_data);
    g_task_run_in_thread(task, collect_containers_task_func);
    g_object_unref(task);
}

void refresh_containers_table_async(GtkTreeStore* store, GtkWidget* tree_view) {
    (void)tree_view;
    g_object_ref(store);
    RefreshContainersData* refresh_data = g_new(RefreshContainersData, 1);
    refresh_data->store = store;
    
    GTask* task = g_task_new(NULL, NULL, on_containers_collected, refresh_data);
    g_task_run_in_thread(task, collect_containers_task_func);
    g_object_unref(task);
}
