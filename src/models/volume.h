#ifndef VOLUME_H
#define VOLUME_H

#include <gtk/gtk.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Popula um GtkListStore com os volumes do Docker (síncrono).
 *
 * @param store O GtkListStore a ser populado (deve ter 4 colunas G_TYPE_STRING)
 */
void populate_docker_volumes(GtkListStore* store);

/**
 * Popula um GtkListStore com os volumes do Docker de forma assíncrona.
 * O store é limpo e repopulado quando os dados ficam prontos.
 *
 * @param store O GtkListStore a ser populado
 */
void populate_docker_volumes_async(GtkListStore* store);

/**
 * Atualiza a tabela de volumes limpando e repopulando (síncrono).
 *
 * @param store O GtkListStore a ser atualizado
 */
void refresh_volumes_table(GtkListStore* store);

/**
 * Atualiza a tabela de volumes de forma assíncrona.
 *
 * @param store O GtkListStore a ser atualizado
 */
void refresh_volumes_table_async(GtkListStore* store);

#ifdef __cplusplus
}
#endif

#endif // VOLUME_H
