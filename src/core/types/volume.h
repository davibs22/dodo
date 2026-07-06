#ifndef DODO_CORE_TYPES_VOLUME_H
#define DODO_CORE_TYPES_VOLUME_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gchar* name;
    gchar* driver;
    gchar* scope;
    gchar* mountpoint;
} DodoVolume;

typedef struct {
    GPtrArray* volumes;
    gboolean has_error;
} DodoVolumeList;

void dodo_volume_free(DodoVolume* volume);
void dodo_volume_list_free(DodoVolumeList* list);

#ifdef __cplusplus
}
#endif

#endif
