#ifndef NETWORK_H
#define NETWORK_H

#include <gtk/gtk.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Populates a GtkListStore with Docker networks (synchronous).
 * 
 * @param store The GtkListStore to populate (must have 4 G_TYPE_STRING columns)
 */
void populate_docker_networks(GtkListStore* store);

/**
 * Populates a GtkListStore with Docker networks asynchronously.
 * The store is cleared and repopulated when data is ready.
 * 
 * @param store The GtkListStore to populate
 */
void populate_docker_networks_async(GtkListStore* store);

/**
 * Refreshes the networks table by clearing and repopulating (synchronous).
 * 
 * @param store The GtkListStore to update
 */
void refresh_networks_table(GtkListStore* store);

/**
 * Refreshes the networks table asynchronously.
 * 
 * @param store The GtkListStore to update
 */
void refresh_networks_table_async(GtkListStore* store);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_H
