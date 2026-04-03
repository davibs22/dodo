#ifndef IMAGES_TABLE_H
#define IMAGES_TABLE_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates and returns a ScrolledWindow widget containing the Docker images table.
 * 
 * @return GtkWidget* ScrolledWindow containing the images TreeView
 */
GtkWidget* create_images_table(void);

#ifdef __cplusplus
}
#endif

#endif // IMAGES_TABLE_H
