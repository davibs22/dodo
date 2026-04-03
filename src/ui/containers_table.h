#ifndef CONTAINERS_TABLE_H
#define CONTAINERS_TABLE_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates and returns a ScrolledWindow widget containing the Docker containers table.
 * The table includes a context menu (right-click) to control containers.
 * 
 * @return GtkWidget* ScrolledWindow containing the containers TreeView
 */
GtkWidget* create_containers_table(void);

#ifdef __cplusplus
}
#endif

#endif // CONTAINERS_TABLE_H
