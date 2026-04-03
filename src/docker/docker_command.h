#ifndef DOCKER_COMMAND_H
#define DOCKER_COMMAND_H

#include <glib.h>
#include <gio/gio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Runs a system command synchronously using GSubprocess.
 * Thread-safe: can be called from any thread.
 *
 * @param command Command to execute (shell-style string)
 * @return Allocated string with command output, or NULL on error.
 *         The caller must free it with g_free().
 */
gchar* execute_command(const gchar* command);

/**
 * Callback type for asynchronous command execution.
 * The callback is always invoked on the GTK main thread (main loop).
 *
 * @param output Command output (NULL on error).
 *               The callback must free it with g_free().
 * @param user_data User data passed to execute_command_async().
 */
typedef void (*CommandAsyncCallback)(gchar* output, gpointer user_data);

/**
 * Runs a system command asynchronously.
 * The command is executed in a separate thread (via GLib thread pool)
 * and the callback is invoked on the GTK main thread with the result.
 * The UI remains responsive during execution.
 *
 * @param command Command to execute (shell-style string)
 * @param callback Function called with the result (on the main thread). Can be NULL.
 * @param user_data Data passed to the callback.
 */
void execute_command_async(const gchar* command, CommandAsyncCallback callback, gpointer user_data);

/**
 * Callback type for real-time command output streaming.
 * The callback is invoked on the GTK main thread whenever new data is received.
 *
 * @param chunk A chunk of command output (NULL when stream ends).
 *              The callback must free it with g_free().
 * @param user_data User data passed to execute_command_stream().
 */
typedef void (*CommandStreamCallback)(gchar* chunk, gpointer user_data);

/**
 * Structure used to control command streaming.
 * Use it to stop streaming when needed.
 */
typedef struct CommandStream CommandStream;

struct CommandStream {
    GSubprocess* subprocess;
    GInputStream* stdout_stream;
    GDataInputStream* data_stream;
    GSource* watch_source;
    gboolean is_running;
    CommandStreamCallback callback;
    gpointer user_data;
};

/**
 * Runs a system command with real-time output streaming.
 * The command runs asynchronously and the callback is called whenever
 * new data arrives. The UI remains responsive during execution.
 *
 * @param command Command to execute (shell-style string)
 * @param callback Function called whenever new data is received (on the main thread).
 * @param user_data Data passed to the callback.
 * @return A CommandStream structure that can be used to stop streaming.
 *         Use command_stream_stop() to stop streaming and free resources.
 */
CommandStream* execute_command_stream(const gchar* command, CommandStreamCallback callback, gpointer user_data);

/**
 * Stops command streaming and frees associated resources.
 *
 * @param stream CommandStream structure returned by execute_command_stream().
 */
void command_stream_stop(CommandStream* stream);

#ifdef __cplusplus
}
#endif

#endif // DOCKER_COMMAND_H
