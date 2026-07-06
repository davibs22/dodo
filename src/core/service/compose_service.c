#include "compose_service.h"
#include "../runtime/command.h"

static void run_compose_command_async(const gchar* action,
                                      const gchar* project_name,
                                      DodoCommandAsyncCallback callback,
                                      gpointer user_data) {
    gchar* command = g_strdup_printf("docker compose -p %s %s", project_name, action);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}

void dodo_compose_stop_async(const gchar* project_name,
                             DodoCommandAsyncCallback callback,
                             gpointer user_data) {
    run_compose_command_async("stop", project_name, callback, user_data);
}

void dodo_compose_start_async(const gchar* project_name,
                              DodoCommandAsyncCallback callback,
                              gpointer user_data) {
    run_compose_command_async("start", project_name, callback, user_data);
}

void dodo_compose_restart_async(const gchar* project_name,
                                DodoCommandAsyncCallback callback,
                                gpointer user_data) {
    run_compose_command_async("restart", project_name, callback, user_data);
}

void dodo_compose_up_async(const gchar* project_name,
                           DodoCommandAsyncCallback callback,
                           gpointer user_data) {
    run_compose_command_async("up -d", project_name, callback, user_data);
}

void dodo_compose_down_async(const gchar* project_name,
                             DodoCommandAsyncCallback callback,
                             gpointer user_data) {
    run_compose_command_async("down", project_name, callback, user_data);
}
