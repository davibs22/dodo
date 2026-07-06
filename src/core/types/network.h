#ifndef DODO_CORE_TYPES_NETWORK_H
#define DODO_CORE_TYPES_NETWORK_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gchar* id;
    gchar* name;
    gchar* driver;
    gchar* scope;
} DodoNetwork;

typedef struct {
    GPtrArray* networks;
    gboolean has_error;
} DodoNetworkList;

void dodo_network_free(DodoNetwork* network);
void dodo_network_list_free(DodoNetworkList* list);

#ifdef __cplusplus
}
#endif

#endif
