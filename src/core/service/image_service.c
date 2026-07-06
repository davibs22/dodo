#include "image_service.h"
#include "../runtime/command.h"

void dodo_image_remove_async(const gchar* image_id,
                             DodoCommandAsyncCallback callback,
                             gpointer user_data) {
    gchar* command = g_strdup_printf("docker rmi -f %s", image_id);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}

void dodo_image_inspect_async(const gchar* image_id,
                              DodoCommandAsyncCallback callback,
                              gpointer user_data) {
    gchar* command = g_strdup_printf("docker image inspect %s", image_id);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}

void dodo_image_export_async(const gchar* image_id,
                             const gchar* output_path,
                             DodoCommandAsyncCallback callback,
                             gpointer user_data) {
    gchar* command = g_strdup_printf("docker save %s -o %s", image_id, output_path);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}

void dodo_image_import_async(const gchar* input_path,
                             DodoCommandAsyncCallback callback,
                             gpointer user_data) {
    gchar* command = g_strdup_printf("docker load -i %s", input_path);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}

void dodo_container_run_async(const gchar* command,
                              DodoCommandAsyncCallback callback,
                              gpointer user_data) {
    dodo_execute_command_async(command, callback, user_data);
}
