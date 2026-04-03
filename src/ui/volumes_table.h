#ifndef VOLUMES_TABLE_H
#define VOLUMES_TABLE_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates and returns a ScrolledWindow widget containing the Docker volumes table.
 *
 * @return GtkWidget* ScrolledWindow containing the volumes TreeView
 */
GtkWidget* create_volumes_table(void);

#ifdef __cplusplus
}
#endif

#endif // VOLUMES_TABLE_H

