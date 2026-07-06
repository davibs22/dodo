#ifndef DODO_CORE_SERVICE_COMPOSE_SERVICE_H
#define DODO_CORE_SERVICE_COMPOSE_SERVICE_H

#include "../runtime/command.h"

#ifdef __cplusplus
extern "C" {
#endif

void dodo_compose_stop_async(const gchar* project_name,
                             DodoCommandAsyncCallback callback,
                             gpointer user_data);
void dodo_compose_start_async(const gchar* project_name,
                              DodoCommandAsyncCallback callback,
                              gpointer user_data);
void dodo_compose_restart_async(const gchar* project_name,
                                DodoCommandAsyncCallback callback,
                                gpointer user_data);
void dodo_compose_up_async(const gchar* project_name,
                           DodoCommandAsyncCallback callback,
                           gpointer user_data);
void dodo_compose_down_async(const gchar* project_name,
                             DodoCommandAsyncCallback callback,
                             gpointer user_data);

#ifdef __cplusplus
}
#endif

#endif
