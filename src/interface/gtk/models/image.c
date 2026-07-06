#include "image.h"
#include "core/repository/image_repo.h"
#include <string.h>

#define INITIAL_LOAD_DONE_KEY "dodo-initial-load-done"

static void populate_store_from_list(GtkListStore* store, DodoImageList* list) {
    gtk_list_store_clear(store);

    if (list->has_error) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, "Error running docker images",
                           1, "", 2, "", 3, "", 4, "", -1);
        return;
    }

    for (guint i = 0; i < list->images->len; i++) {
        DodoImage* image = g_ptr_array_index(list->images, i);
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, image->repository,
                           1, image->tag,
                           2, image->id,
                           3, image->created,
                           4, image->size,
                           -1);
    }
}

void populate_docker_images(GtkListStore* store) {
    DodoImageList* list = dodo_image_list_fetch();
    populate_store_from_list(store, list);
    dodo_image_list_free(list);
}

typedef struct {
    GtkListStore* store;
} ImageFetchData;

static void collect_images_task_func(GTask* task,
                                     gpointer source_object,
                                     gpointer task_data,
                                     GCancellable* cancellable) {
    (void)source_object;
    (void)task_data;
    (void)cancellable;
    DodoImageList* list = dodo_image_list_fetch();
    g_task_return_pointer(task, list, (GDestroyNotify)dodo_image_list_free);
}

static void on_images_collected(GObject* source_object,
                                GAsyncResult* res,
                                gpointer user_data) {
    (void)source_object;
    ImageFetchData* fetch_data = (ImageFetchData*)user_data;
    GError* error = NULL;
    DodoImageList* list = g_task_propagate_pointer(G_TASK(res), &error);

    if (list) {
        populate_store_from_list(fetch_data->store, list);
        dodo_image_list_free(list);
    } else if (error) {
        g_warning("populate_docker_images_async: %s", error->message);
        g_error_free(error);
    }

    g_object_set_data(G_OBJECT(fetch_data->store), INITIAL_LOAD_DONE_KEY, GINT_TO_POINTER(TRUE));
    g_object_unref(fetch_data->store);
    g_free(fetch_data);
}

static void fetch_images_async(GtkListStore* store) {
    g_object_set_data(G_OBJECT(store), INITIAL_LOAD_DONE_KEY, GINT_TO_POINTER(FALSE));
    g_object_ref(store);
    ImageFetchData* fetch_data = g_new(ImageFetchData, 1);
    fetch_data->store = store;

    GTask* task = g_task_new(NULL, NULL, on_images_collected, fetch_data);
    g_task_run_in_thread(task, collect_images_task_func);
    g_object_unref(task);
}

void populate_docker_images_async(GtkListStore* store) {
    fetch_images_async(store);
}

void refresh_images_table(GtkListStore* store) {
    gtk_list_store_clear(store);
    populate_docker_images(store);
}

void refresh_images_table_async(GtkListStore* store) {
    fetch_images_async(store);
}
