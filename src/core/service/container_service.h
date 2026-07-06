#ifndef DODO_CORE_SERVICE_CONTAINER_SERVICE_H
#define DODO_CORE_SERVICE_CONTAINER_SERVICE_H

#include "../runtime/command.h"

#ifdef __cplusplus
extern "C" {
#endif

void dodo_container_stop_async(const gchar* container_id,
                               DodoCommandAsyncCallback callback,
                               gpointer user_data);
void dodo_container_start_async(const gchar* container_id,
                                DodoCommandAsyncCallback callback,
                                gpointer user_data);
void dodo_container_restart_async(const gchar* container_id,
                                  DodoCommandAsyncCallback callback,
                                  gpointer user_data);
void dodo_container_pause_async(const gchar* container_id,
                                DodoCommandAsyncCallback callback,
                                gpointer user_data);
void dodo_container_unpause_async(const gchar* container_id,
                                  DodoCommandAsyncCallback callback,
                                  gpointer user_data);
void dodo_container_remove_async(const gchar* container_id,
                                 gboolean force,
                                 DodoCommandAsyncCallback callback,
                                 gpointer user_data);
void dodo_container_inspect_async(const gchar* container_id,
                                    DodoCommandAsyncCallback callback,
                                    gpointer user_data);
gchar* dodo_container_logs_tail(const gchar* container_id, guint tail_lines);
DodoCommandStream* dodo_container_logs_stream(const gchar* container_id,
                                                gboolean follow,
                                                DodoCommandStreamCallback callback,
                                                gpointer user_data);

#ifdef __cplusplus
}
#endif

#endif
