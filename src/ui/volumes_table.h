#ifndef VOLUMES_TABLE_H
#define VOLUMES_TABLE_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Cria e retorna um widget ScrolledWindow contendo a tabela de volumes do Docker.
 *
 * @return GtkWidget* ScrolledWindow contendo a TreeView de volumes
 */
GtkWidget* create_volumes_table(void);

#ifdef __cplusplus
}
#endif

#endif // VOLUMES_TABLE_H

