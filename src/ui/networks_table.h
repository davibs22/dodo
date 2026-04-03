#ifndef NETWORKS_TABLE_H
#define NETWORKS_TABLE_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Cria e retorna um widget ScrolledWindow contendo a tabela de networks do Docker.
 * 
 * @return GtkWidget* ScrolledWindow contendo a TreeView de networks
 */
GtkWidget* create_networks_table(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORKS_TABLE_H
