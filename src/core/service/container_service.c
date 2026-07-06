#include "container_service.h"
#include "../runtime/command.h"

static void run_container_command_async(const gchar* action,
                                        const gchar* container_id,
                                        DodoCommandAsyncCallback callback,
                                        gpointer user_data) {
    gchar* command = g_strdup_printf("docker %s %s", action, container_id);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}

void dodo_container_stop_async(const gchar* container_id,
                               DodoCommandAsyncCallback callback,
                               gpointer user_data) {
    run_container_command_async("stop", container_id, callback, user_data);
}

void dodo_container_start_async(const gchar* container_id,
                                DodoCommandAsyncCallback callback,
                                gpointer user_data) {
    run_container_command_async("start", container_id, callback, user_data);
}

void dodo_container_restart_async(const gchar* container_id,
                                  DodoCommandAsyncCallback callback,
                                  gpointer user_data) {
    run_container_command_async("restart", container_id, callback, user_data);
}

void dodo_container_pause_async(const gchar* container_id,
                                DodoCommandAsyncCallback callback,
                                gpointer user_data) {
    run_container_command_async("pause", container_id, callback, user_data);
}

void dodo_container_unpause_async(const gchar* container_id,
                                  DodoCommandAsyncCallback callback,
                                  gpointer user_data) {
    run_container_command_async("unpause", container_id, callback, user_data);
}

void dodo_container_remove_async(const gchar* container_id,
                                 gboolean force,
                                 DodoCommandAsyncCallback callback,
                                 gpointer user_data) {
    gchar* command = force
        ? g_strdup_printf("docker rm -f %s", container_id)
        : g_strdup_printf("docker rm %s", container_id);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}

void dodo_container_inspect_async(const gchar* container_id,
                                    DodoCommandAsyncCallback callback,
                                    gpointer user_data) {
    gchar* command = g_strdup_printf("docker inspect %s", container_id);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}

gchar* dodo_container_logs_tail(const gchar* container_id, guint tail_lines) {
    gchar* command = g_strdup_printf("docker logs --tail %u %s", tail_lines, container_id);
    gchar* output = dodo_execute_command(command);
    g_free(command);
    return output;
}

DodoCommandStream* dodo_container_logs_stream(const gchar* container_id,
                                                gboolean follow,
                                                DodoCommandStreamCallback callback,
                                                gpointer user_data) {
    gchar* command = follow
        ? g_strdup_printf("docker logs -f --tail 0 %s", container_id)
        : g_strdup_printf("docker logs -f %s", container_id);
    DodoCommandStream* stream = dodo_execute_command_stream(command, callback, user_data);
    g_free(command);
    return stream;
}
