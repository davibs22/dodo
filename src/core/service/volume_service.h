#ifndef DODO_CORE_SERVICE_VOLUME_SERVICE_H
#define DODO_CORE_SERVICE_VOLUME_SERVICE_H

#include "../runtime/command.h"

#ifdef __cplusplus
extern "C" {
#endif

void dodo_volume_remove_async(const gchar* volume_name,
                              DodoCommandAsyncCallback callback,
                              gpointer user_data);
void dodo_volume_inspect_async(const gchar* volume_name,
                               DodoCommandAsyncCallback callback,
                               gpointer user_data);

#ifdef __cplusplus
}
#endif

#endif
