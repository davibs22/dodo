#include "docker_command.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

gchar* execute_command(const gchar* command) {
    GError* error = NULL;
    gchar** argv = NULL;
    gchar* stdout_buf = NULL;
    if (!g_shell_parse_argv(command, NULL, &argv, &error)) {
        g_warning("execute_command: failed to parse '%s': %s", command, error->message);
        g_error_free(error);
        return NULL;
    }
    GSubprocess* subprocess = g_subprocess_newv(
        (const gchar* const*)argv,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &error
    );
    g_strfreev(argv);
    
    if (subprocess == NULL) {
        g_warning("execute_command: failed to create subprocess for '%s': %s", command, error->message);
        g_error_free(error);
        return NULL;
    }
    if (!g_subprocess_communicate_utf8(subprocess, NULL, NULL, &stdout_buf, NULL, &error)) {
        g_warning("execute_command: failed to communicate with subprocess '%s': %s", command, error->message);
        g_error_free(error);
        g_object_unref(subprocess);
        return NULL;
    }
    
    g_object_unref(subprocess);
    return stdout_buf;
}
typedef struct {
    gchar* command;
    gchar* output;
    CommandAsyncCallback callback;
    gpointer user_data;
} AsyncCommandData;
static gboolean async_deliver_result(gpointer data) {
    AsyncCommandData* async_data = (AsyncCommandData*)data;
    
    if (async_data->callback) {
        async_data->callback(async_data->output, async_data->user_data);
    } else {
        g_free(async_data->output);
    }
    
    g_free(async_data->command);
    g_free(async_data);
    return G_SOURCE_REMOVE;
}
static gpointer async_command_worker(gpointer data) {
    AsyncCommandData* async_data = (AsyncCommandData*)data;
    async_data->output = execute_command(async_data->command);
    g_idle_add(async_deliver_result, async_data);
    
    return NULL;
}

void execute_command_async(const gchar* command, CommandAsyncCallback callback, gpointer user_data) {
    AsyncCommandData* data = g_new0(AsyncCommandData, 1);
    data->command = g_strdup(command);
    data->callback = callback;
    data->user_data = user_data;
    g_thread_new("docker-async-cmd", async_command_worker, data);
}
typedef struct {
    gchar* chunk;
    CommandStreamCallback callback;
    gpointer user_data;
} StreamChunkData;
static gboolean deliver_stream_chunk(gpointer data) {
    StreamChunkData* chunk_data = (StreamChunkData*)data;
    
    if (chunk_data->callback) {
        chunk_data->callback(chunk_data->chunk, chunk_data->user_data);
    } else {
        if (chunk_data->chunk) g_free(chunk_data->chunk);
    }
    
    g_free(chunk_data);
    return G_SOURCE_REMOVE;
}
static gpointer stream_reader_worker(gpointer data) {
    CommandStream* cmd_stream = (CommandStream*)data;
    GError* error = NULL;
    gchar buffer[4096];
    gssize bytes_read;
    
    while (cmd_stream->is_running) {
        bytes_read = g_input_stream_read(cmd_stream->stdout_stream, buffer, sizeof(buffer) - 1, NULL, &error);
        
        if (error) {
            if (error->domain != G_IO_ERROR || 
                (error->code != G_IO_ERROR_CLOSED && error->code != G_IO_ERROR_BROKEN_PIPE)) {
                g_warning("Error reading stream: %s", error->message);
            }
            g_error_free(error);
            break;
        }
        
        if (bytes_read <= 0) {
            break;
        }
        buffer[bytes_read] = '\0';
        StreamChunkData* chunk_data = g_new(StreamChunkData, 1);
        chunk_data->chunk = g_strdup(buffer);
        chunk_data->callback = cmd_stream->callback;
        chunk_data->user_data = cmd_stream->user_data;
        if (cmd_stream->user_data) {
            g_idle_add(deliver_stream_chunk, chunk_data);
        } else {
            g_free(chunk_data->chunk);
            g_free(chunk_data);
        }
    }
    cmd_stream->is_running = FALSE;
    if (cmd_stream->callback && cmd_stream->user_data) {
        StreamChunkData* chunk_data = g_new(StreamChunkData, 1);
        chunk_data->chunk = NULL; // NULL indicates end of stream
        chunk_data->callback = cmd_stream->callback;
        chunk_data->user_data = cmd_stream->user_data;
        g_idle_add(deliver_stream_chunk, chunk_data);
    }
    if (cmd_stream->stdout_stream) {
        g_input_stream_close(cmd_stream->stdout_stream, NULL, NULL);
        g_object_unref(cmd_stream->stdout_stream);
        cmd_stream->stdout_stream = NULL;
    }
    g_free(cmd_stream);
    
    return NULL;
}

CommandStream* execute_command_stream(const gchar* command, CommandStreamCallback callback, gpointer user_data) {
    GError* error = NULL;
    gchar** argv = NULL;
    if (!g_shell_parse_argv(command, NULL, &argv, &error)) {
        g_warning("execute_command_stream: failed to parse '%s': %s", command, error->message);
        g_error_free(error);
        return NULL;
    }
    GSubprocess* subprocess = g_subprocess_newv(
        (const gchar* const*)argv,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error
    );
    g_strfreev(argv);
    
    if (subprocess == NULL) {
        g_warning("execute_command_stream: failed to create subprocess for '%s': %s", command, error->message);
        g_error_free(error);
        return NULL;
    }
    GInputStream* stdout_stream = g_subprocess_get_stdout_pipe(subprocess);
    if (!stdout_stream) {
        g_warning("execute_command_stream: failed to get stdout pipe");
        g_object_unref(subprocess);
        return NULL;
    }
    CommandStream* cmd_stream = g_new0(CommandStream, 1);
    cmd_stream->subprocess = subprocess;
    cmd_stream->stdout_stream = g_object_ref(stdout_stream); // Keep reference
    cmd_stream->data_stream = NULL; // Not used in this implementation
    cmd_stream->watch_source = NULL; // Not used in this implementation
    cmd_stream->is_running = TRUE;
    cmd_stream->callback = callback;
    cmd_stream->user_data = user_data;
    g_thread_new("docker-stream-reader", stream_reader_worker, cmd_stream);
    
    return cmd_stream;
}

void command_stream_stop(CommandStream* stream) {
    if (!stream) return;
    stream->callback = NULL;
    stream->user_data = NULL;
    stream->is_running = FALSE;
    if (stream->subprocess) {
        g_subprocess_force_exit(stream->subprocess);
        g_object_unref(stream->subprocess);
        stream->subprocess = NULL;
    }
}
