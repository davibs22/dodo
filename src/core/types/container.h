#ifndef DODO_CORE_TYPES_CONTAINER_H
#define DODO_CORE_TYPES_CONTAINER_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DODO_CONTAINER_STATE_STOPPED,
    DODO_CONTAINER_STATE_RUNNING,
    DODO_CONTAINER_STATE_PAUSED,
    DODO_CONTAINER_STATE_UNKNOWN
} DodoContainerState;

typedef struct {
    gchar* id;
    gchar* image;
    gchar* command;
    gchar* created;
    gchar* status;
    gchar* ports;
    gchar* names;
    gchar* compose_project;
} DodoContainer;

typedef struct {
    GHashTable* project_groups;
    GPtrArray* standalone_containers;
    gboolean has_error;
} DodoContainerList;

#define DODO_STANDALONE_GROUP_NAME "individual containers"

void dodo_container_free(DodoContainer* container);
void dodo_container_list_free(DodoContainerList* list);

#ifdef __cplusplus
}
#endif

#endif
