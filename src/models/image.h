#ifndef IMAGE_H
#define IMAGE_H

#include <gtk/gtk.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Popula um GtkListStore com as imagens do Docker (síncrono).
 * 
 * @param store O GtkListStore a ser populado (deve ter 5 colunas G_TYPE_STRING)
 */
void populate_docker_images(GtkListStore* store);

/**
 * Popula um GtkListStore com as imagens do Docker de forma assíncrona.
 * O store é limpo e repopulado quando os dados ficam prontos.
 * 
 * @param store O GtkListStore a ser populado
 */
void populate_docker_images_async(GtkListStore* store);

/**
 * Atualiza a tabela de imagens limpando e repopulando (síncrono).
 * 
 * @param store O GtkListStore a ser atualizado
 */
void refresh_images_table(GtkListStore* store);

/**
 * Atualiza a tabela de imagens de forma assíncrona.
 * 
 * @param store O GtkListStore a ser atualizado
 */
void refresh_images_table_async(GtkListStore* store);

#ifdef __cplusplus
}
#endif

#endif // IMAGE_H
