#ifndef IMAGE_H
#define IMAGE_H

#include <gtk/gtk.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Populates a GtkListStore with Docker images (synchronous).
 * 
 * @param store The GtkListStore to populate (must have 5 G_TYPE_STRING columns)
 */
void populate_docker_images(GtkListStore* store);

/**
 * Populates a GtkListStore with Docker images asynchronously.
 * The store is cleared and repopulated when data is ready.
 * 
 * @param store The GtkListStore to populate
 */
void populate_docker_images_async(GtkListStore* store);

/**
 * Refreshes the images table by clearing and repopulating (synchronous).
 * 
 * @param store The GtkListStore to update
 */
void refresh_images_table(GtkListStore* store);

/**
 * Refreshes the images table asynchronously.
 * 
 * @param store The GtkListStore to update
 */
void refresh_images_table_async(GtkListStore* store);

#ifdef __cplusplus
}
#endif

#endif // IMAGE_H
