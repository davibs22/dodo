#ifndef NETWORKS_TABLE_H
#define NETWORKS_TABLE_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates and returns a ScrolledWindow widget containing the Docker networks table.
 * 
 * @return GtkWidget* ScrolledWindow containing the networks TreeView
 */
GtkWidget* create_networks_table(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORKS_TABLE_H
