#ifndef DODO_CORE_RUNTIME_COMMAND_H
#define DODO_CORE_RUNTIME_COMMAND_H

#include <glib.h>
#include <gio/gio.h>

#ifdef __cplusplus
extern "C" {
#endif

gchar* dodo_execute_command(const gchar* command);

typedef void (*DodoCommandAsyncCallback)(gchar* output, gpointer user_data);

void dodo_execute_command_async(const gchar* command,
                                DodoCommandAsyncCallback callback,
                                gpointer user_data);

typedef void (*DodoCommandStreamCallback)(gchar* chunk, gpointer user_data);

typedef struct DodoCommandStream DodoCommandStream;

struct DodoCommandStream {
    GSubprocess* subprocess;
    GInputStream* stdout_stream;
    GDataInputStream* data_stream;
    GSource* watch_source;
    GThread* thread;
    guint end_idle_id;
    gboolean is_running;
    DodoCommandStreamCallback callback;
    gpointer user_data;
};

DodoCommandStream* dodo_execute_command_stream(const gchar* command,
                                               DodoCommandStreamCallback callback,
                                               gpointer user_data);

void dodo_command_stream_stop(DodoCommandStream* stream);

#ifdef __cplusplus
}
#endif

#endif
