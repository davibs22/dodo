#include "network_service.h"
#include "../runtime/command.h"

void dodo_network_remove_async(const gchar* network_name,
                               DodoCommandAsyncCallback callback,
                               gpointer user_data) {
    gchar* command = g_strdup_printf("docker network rm %s", network_name);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}

void dodo_network_inspect_async(const gchar* network_name,
                                DodoCommandAsyncCallback callback,
                                gpointer user_data) {
    gchar* command = g_strdup_printf("docker network inspect %s", network_name);
    dodo_execute_command_async(command, callback, user_data);
    g_free(command);
}
