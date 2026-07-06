#include "container.h"
#include "core/repository/container_repo.h"
#include "core/container_state.h"
#include "../utils/status_utils.h"
#include <string.h>

#define INITIAL_LOAD_DONE_KEY "dodo-initial-load-done"

static void get_group_status(GPtrArray* containers, gchar** status_text, gchar** color) {
    gchar* summary = NULL;
    guint running_count = 0;
    guint total_count = 0;

    dodo_compose_group_summary(containers, &summary, &running_count, &total_count);

    if (status_text) {
        *status_text = summary ? summary : g_strdup("");
    } else if (summary) {
        g_free(summary);
    }

    if (color) {
        if (total_count == 0) {
            *color = NULL;
        } else if (running_count == 0) {
            *color = NULL;
        } else if (running_count == total_count) {
            *color = g_strdup("#00AA00");
        } else {
            *color = g_strdup("#FFAA00");
        }
    }
}

static void set_container_row(GtkTreeStore* store, GtkTreeIter* iter, DodoContainer* info) {
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
                                 const gchar* group_name, DodoContainer* info) {
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

static void collect_container_ids(DodoContainerList* data, GHashTable* ids) {
    GHashTableIter hash_iter;
    gpointer key, value;

    g_hash_table_iter_init(&hash_iter, data->project_groups);
    while (g_hash_table_iter_next(&hash_iter, &key, &value)) {
        GPtrArray* containers = (GPtrArray*)value;
        for (guint i = 0; i < containers->len; i++) {
            DodoContainer* info = g_ptr_array_index(containers, i);
            if (info->id && info->id[0] != '\0') {
                g_hash_table_add(ids, info->id);
            }
        }
    }

    for (guint i = 0; i < data->standalone_containers->len; i++) {
        DodoContainer* info = g_ptr_array_index(data->standalone_containers, i);
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

static void sync_data_to_store(GtkTreeStore* store, DodoContainerList* data) {
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
            DodoContainer* info = g_ptr_array_index(containers, i);
            upsert_container_row(store, &group_iter, project_name, info);
        }
    }

    if (data->standalone_containers->len > 0) {
        GtkTreeIter group_iter = ensure_group_iter(store, DODO_STANDALONE_GROUP_NAME, data->standalone_containers);

        for (guint i = 0; i < data->standalone_containers->len; i++) {
            DodoContainer* info = g_ptr_array_index(data->standalone_containers, i);
            upsert_container_row(store, &group_iter, DODO_STANDALONE_GROUP_NAME, info);
        }
    } else {
        GtkTreeIter standalone_iter;
        if (find_group_iter(store, DODO_STANDALONE_GROUP_NAME, &standalone_iter)) {
            gtk_tree_store_remove(store, &standalone_iter);
        }
    }

    remove_empty_groups(store);
    g_hash_table_destroy(valid_ids);
}

void populate_docker_containers(GtkTreeStore* store) {
    DodoContainerList* data = dodo_container_list_fetch();
    sync_data_to_store(store, data);
    dodo_container_list_free(data);
}

void refresh_containers_table(GtkTreeStore* store) {
    DodoContainerList* data = dodo_container_list_fetch();
    sync_data_to_store(store, data);
    dodo_container_list_free(data);
}

typedef struct {
    GtkTreeStore* store;
} RefreshContainersData;

static void collect_containers_task_func(GTask* task,
                                          gpointer source_object,
                                          gpointer task_data,
                                          GCancellable* cancellable) {
    (void)source_object;
    (void)task_data;
    (void)cancellable;
    DodoContainerList* data = dodo_container_list_fetch();
    g_task_return_pointer(task, data, (GDestroyNotify)dodo_container_list_free);
}

static void on_containers_collected(GObject* source_object,
                                     GAsyncResult* res,
                                     gpointer user_data) {
    (void)source_object;
    RefreshContainersData* refresh_data = (RefreshContainersData*)user_data;
    GtkTreeStore* store = refresh_data->store;
    GError* error = NULL;

    DodoContainerList* data = g_task_propagate_pointer(G_TASK(res), &error);

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
    dodo_container_list_free(data);
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
