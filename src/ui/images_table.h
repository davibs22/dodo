#ifndef IMAGES_TABLE_H
#define IMAGES_TABLE_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Cria e retorna um widget ScrolledWindow contendo a tabela de imagens do Docker.
 * 
 * @return GtkWidget* ScrolledWindow contendo a TreeView de imagens
 */
GtkWidget* create_images_table(void);

#ifdef __cplusplus
}
#endif

#endif // IMAGES_TABLE_H
