#ifndef NETWORK_H
#define NETWORK_H

#include <gtk/gtk.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Popula um GtkListStore com as networks do Docker (síncrono).
 * 
 * @param store O GtkListStore a ser populado (deve ter 4 colunas G_TYPE_STRING)
 */
void populate_docker_networks(GtkListStore* store);

/**
 * Popula um GtkListStore com as networks do Docker de forma assíncrona.
 * O store é limpo e repopulado quando os dados ficam prontos.
 * 
 * @param store O GtkListStore a ser populado
 */
void populate_docker_networks_async(GtkListStore* store);

/**
 * Atualiza a tabela de networks limpando e repopulando (síncrono).
 * 
 * @param store O GtkListStore a ser atualizado
 */
void refresh_networks_table(GtkListStore* store);

/**
 * Atualiza a tabela de networks de forma assíncrona.
 * 
 * @param store O GtkListStore a ser atualizado
 */
void refresh_networks_table_async(GtkListStore* store);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_H
