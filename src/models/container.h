#ifndef CONTAINER_H
#define CONTAINER_H

#include <gtk/gtk.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Popula um GtkTreeStore com os containers do Docker agrupados por docker compose.
 * ATENÇÃO: Esta função é SÍNCRONA e bloqueia a thread chamadora.
 * Prefira usar populate_docker_containers_async() para manter a UI responsiva.
 * 
 * @param store O GtkTreeStore a ser populado (deve ter 10 colunas G_TYPE_STRING)
 */
void populate_docker_containers(GtkTreeStore* store);

/**
 * Popula um GtkTreeStore de forma ASSÍNCRONA.
 * A coleta de dados (comandos Docker) roda em thread separada.
 * A atualização do store é feita na thread principal do GTK.
 * O store é limpo e repopulado quando os dados ficam prontos.
 * 
 * @param store O GtkTreeStore a ser populado (deve ter 10 colunas G_TYPE_STRING)
 */
void populate_docker_containers_async(GtkTreeStore* store);

/**
 * Atualiza a tabela de containers limpando e repopulando de forma SÍNCRONA.
 * 
 * @param store O GtkTreeStore a ser atualizado
 */
void refresh_containers_table(GtkTreeStore* store);

/**
 * Atualiza a tabela de containers de forma ASSÍNCRONA.
 * A coleta roda em background e o store é atualizado na thread principal.
 * Preserva o estado de expansão dos grupos compose se tree_view for fornecido.
 * 
 * @param store O GtkTreeStore a ser atualizado
 * @param tree_view O GtkWidget da TreeView (pode ser NULL, mas o estado de expansão não será preservado)
 */
void refresh_containers_table_async(GtkTreeStore* store, GtkWidget* tree_view);

#ifdef __cplusplus
}
#endif

#endif // CONTAINER_H
