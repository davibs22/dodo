#ifndef DODO_CORE_SERVICE_STATS_SERVICE_H
#define DODO_CORE_SERVICE_STATS_SERVICE_H

#include "../runtime/command.h"

#ifdef __cplusplus
extern "C" {
#endif

void dodo_stats_fetch_cpu_async(DodoCommandAsyncCallback callback, gpointer user_data);
void dodo_stats_fetch_memory_async(DodoCommandAsyncCallback callback, gpointer user_data);
void dodo_stats_fetch_blockio_async(DodoCommandAsyncCallback callback, gpointer user_data);
void dodo_stats_fetch_netio_async(DodoCommandAsyncCallback callback, gpointer user_data);

#ifdef __cplusplus
}
#endif

#endif
