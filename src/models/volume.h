#ifndef VOLUME_H
#define VOLUME_H

#include <gtk/gtk.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Populates a GtkListStore with Docker volumes (synchronous).
 *
 * @param store The GtkListStore to populate (must have 4 G_TYPE_STRING columns)
 */
void populate_docker_volumes(GtkListStore* store);

/**
 * Populates a GtkListStore with Docker volumes asynchronously.
 * The store is cleared and repopulated when data is ready.
 *
 * @param store The GtkListStore to populate
 */
void populate_docker_volumes_async(GtkListStore* store);

/**
 * Refreshes the volumes table by clearing and repopulating (synchronous).
 *
 * @param store The GtkListStore to update
 */
void refresh_volumes_table(GtkListStore* store);

/**
 * Refreshes the volumes table asynchronously.
 *
 * @param store The GtkListStore to update
 */
void refresh_volumes_table_async(GtkListStore* store);

#ifdef __cplusplus
}
#endif

#endif // VOLUME_H
