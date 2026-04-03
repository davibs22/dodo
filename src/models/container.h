#ifndef CONTAINER_H
#define CONTAINER_H

#include <gtk/gtk.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Populates a GtkTreeStore with Docker containers grouped by docker compose.
 * WARNING: This function is SYNCHRONOUS and blocks the calling thread.
 * Prefer using populate_docker_containers_async() to keep the UI responsive.
 * 
 * @param store The GtkTreeStore to populate (must have 10 G_TYPE_STRING columns)
 */
void populate_docker_containers(GtkTreeStore* store);

/**
 * Populates a GtkTreeStore ASYNCHRONOUSLY.
 * Data collection (Docker commands) runs on a separate thread.
 * Store updates occur on the GTK main thread.
 * The store is cleared and repopulated when data is ready.
 * 
 * @param store The GtkTreeStore to populate (must have 10 G_TYPE_STRING columns)
 */
void populate_docker_containers_async(GtkTreeStore* store);

/**
 * Refreshes the containers table by clearing and repopulating SYNCHRONOUSLY.
 * 
 * @param store The GtkTreeStore to update
 */
void refresh_containers_table(GtkTreeStore* store);

/**
 * Refreshes the containers table ASYNCHRONOUSLY.
 * Collection runs in the background and the store is updated on the main thread.
 * Preserves compose group expansion state if tree_view is provided.
 * 
 * @param store The GtkTreeStore to update
 * @param tree_view The TreeView GtkWidget (may be NULL; expansion state won't be preserved)
 */
void refresh_containers_table_async(GtkTreeStore* store, GtkWidget* tree_view);

#ifdef __cplusplus
}
#endif

#endif // CONTAINER_H
