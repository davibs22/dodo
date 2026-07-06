#include "volume_service.h"
#include "../runtime/command.h"

void dodo_volume_remove_async(const gchar* volume_name,
                              DodoCommandAsyncCallback callback,
                              gpointer user_data) {
    gchar* command = g_strdup_printf("docker volume rm %s", volume_name);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}

void dodo_volume_inspect_async(const gchar* volume_name,
                               DodoCommandAsyncCallback callback,
                               gpointer user_data) {
    gchar* command = g_strdup_printf("docker volume inspect %s", volume_name);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}
