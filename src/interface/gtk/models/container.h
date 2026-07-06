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
 * Store updates occur on the GTK main thread using incremental sync.
 * 
 * @param store The GtkTreeStore to populate (must have 10 G_TYPE_STRING columns)
 */
void populate_docker_containers_async(GtkTreeStore* store);

/**
 * Refreshes the containers table incrementally (preserves scroll and expansion).
 * 
 * @param store The GtkTreeStore to update
 */
void refresh_containers_table(GtkTreeStore* store);

/**
 * Refreshes the containers table ASYNCHRONOUSLY with incremental sync.
 * 
 * @param store The GtkTreeStore to update
 * @param tree_view The TreeView GtkWidget (may be NULL; expansion state won't be preserved)
 */
void refresh_containers_table_async(GtkTreeStore* store, GtkWidget* tree_view);

#ifdef __cplusplus
}
#endif

#endif // CONTAINER_H
