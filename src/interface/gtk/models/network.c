#include "network.h"
#include "core/repository/network_repo.h"
#include <string.h>

#define INITIAL_LOAD_DONE_KEY "dodo-initial-load-done"

static void populate_store_from_list(GtkListStore* store, DodoNetworkList* list) {
    gtk_list_store_clear(store);

    if (list->has_error) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, "Error running docker network ls",
                           1, "", 2, "", 3, "", -1);
        return;
    }

    for (guint i = 0; i < list->networks->len; i++) {
        DodoNetwork* network = g_ptr_array_index(list->networks, i);
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, network->id,
                           1, network->name,
                           2, network->driver,
                           3, network->scope,
                           -1);
    }
}

void populate_docker_networks(GtkListStore* store) {
    DodoNetworkList* list = dodo_network_list_fetch();
    populate_store_from_list(store, list);
    dodo_network_list_free(list);
}

typedef struct {
    GtkListStore* store;
} NetworkFetchData;

static void collect_networks_task_func(GTask* task,
                                       gpointer source_object,
                                       gpointer task_data,
                                       GCancellable* cancellable) {
    (void)source_object;
    (void)task_data;
    (void)cancellable;
    DodoNetworkList* list = dodo_network_list_fetch();
    g_task_return_pointer(task, list, (GDestroyNotify)dodo_network_list_free);
}

static void on_networks_collected(GObject* source_object,
                                  GAsyncResult* res,
                                  gpointer user_data) {
    (void)source_object;
    NetworkFetchData* fetch_data = (NetworkFetchData*)user_data;
    GError* error = NULL;
    DodoNetworkList* list = g_task_propagate_pointer(G_TASK(res), &error);

    if (list) {
        populate_store_from_list(fetch_data->store, list);
        dodo_network_list_free(list);
    } else if (error) {
        g_warning("populate_docker_networks_async: %s", error->message);
        g_error_free(error);
    }

    g_object_set_data(G_OBJECT(fetch_data->store), INITIAL_LOAD_DONE_KEY, GINT_TO_POINTER(TRUE));
    g_object_unref(fetch_data->store);
    g_free(fetch_data);
}

static void fetch_networks_async(GtkListStore* store) {
    g_object_set_data(G_OBJECT(store), INITIAL_LOAD_DONE_KEY, GINT_TO_POINTER(FALSE));
    g_object_ref(store);
    NetworkFetchData* fetch_data = g_new(NetworkFetchData, 1);
    fetch_data->store = store;

    GTask* task = g_task_new(NULL, NULL, on_networks_collected, fetch_data);
    g_task_run_in_thread(task, collect_networks_task_func);
    g_object_unref(task);
}

void populate_docker_networks_async(GtkListStore* store) {
    fetch_networks_async(store);
}

void refresh_networks_table(GtkListStore* store) {
    gtk_list_store_clear(store);
    populate_docker_networks(store);
}

void refresh_networks_table_async(GtkListStore* store) {
    fetch_networks_async(store);
}
