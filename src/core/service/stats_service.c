#include "stats_service.h"
#include "../runtime/command.h"

void dodo_stats_fetch_cpu_async(DodoCommandAsyncCallback callback, gpointer user_data) {
    dodo_execute_command_async(
        "docker stats --no-stream --format '{{.Name}}:::{{.CPUPerc}}'",
        callback, user_data);
}

void dodo_stats_fetch_memory_async(DodoCommandAsyncCallback callback, gpointer user_data) {
    dodo_execute_command_async(
        "docker stats --no-stream --format '{{.MemUsage}}'",
        callback, user_data);
}

void dodo_stats_fetch_blockio_async(DodoCommandAsyncCallback callback, gpointer user_data) {
    dodo_execute_command_async(
        "docker stats --no-stream --format '{{.BlockIO}}'",
        callback, user_data);
}

void dodo_stats_fetch_netio_async(DodoCommandAsyncCallback callback, gpointer user_data) {
    dodo_execute_command_async(
        "docker stats --no-stream --format '{{.NetIO}}'",
        callback, user_data);
}
