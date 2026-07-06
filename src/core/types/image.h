#ifndef DODO_CORE_TYPES_IMAGE_H
#define DODO_CORE_TYPES_IMAGE_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gchar* repository;
    gchar* tag;
    gchar* id;
    gchar* created;
    gchar* size;
} DodoImage;

typedef struct {
    GPtrArray* images;
    gboolean has_error;
} DodoImageList;

void dodo_image_free(DodoImage* image);
void dodo_image_list_free(DodoImageList* list);

#ifdef __cplusplus
}
#endif

#endif
