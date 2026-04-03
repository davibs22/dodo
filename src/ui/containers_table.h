#ifndef CONTAINERS_TABLE_H
#define CONTAINERS_TABLE_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Cria e retorna um widget ScrolledWindow contendo a tabela de containers do Docker.
 * A tabela inclui menu de contexto (botão direito) para controlar os containers.
 * 
 * @return GtkWidget* ScrolledWindow contendo a TreeView de containers
 */
GtkWidget* create_containers_table(void);

#ifdef __cplusplus
}
#endif

#endif // CONTAINERS_TABLE_H
