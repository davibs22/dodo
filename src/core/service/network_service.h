#ifndef DODO_CORE_SERVICE_NETWORK_SERVICE_H
#define DODO_CORE_SERVICE_NETWORK_SERVICE_H

#include "../runtime/command.h"

#ifdef __cplusplus
extern "C" {
#endif

void dodo_network_remove_async(const gchar* network_name,
                               DodoCommandAsyncCallback callback,
                               gpointer user_data);
void dodo_network_inspect_async(const gchar* network_name,
                                DodoCommandAsyncCallback callback,
                                gpointer user_data);

#ifdef __cplusplus
}
#endif

#endif
