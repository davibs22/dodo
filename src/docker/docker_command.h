#ifndef DOCKER_COMMAND_H
#define DOCKER_COMMAND_H

#include <glib.h>
#include <gio/gio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Executa um comando do sistema de forma síncrona usando GSubprocess.
 * Thread-safe: pode ser chamado de qualquer thread.
 * 
 * @param command O comando a ser executado (string estilo shell)
 * @return Uma string alocada com a saída do comando, ou NULL em caso de erro.
 *         O chamador é responsável por liberar a memória com g_free().
 */
gchar* execute_command(const gchar* command);

/**
 * Tipo de callback para execução assíncrona de comandos.
 * O callback é sempre invocado na thread principal do GTK (main loop).
 * 
 * @param output A saída do comando (NULL em caso de erro).
 *               O callback é responsável por liberar com g_free().
 * @param user_data Dados do usuário passados para execute_command_async().
 */
typedef void (*CommandAsyncCallback)(gchar* output, gpointer user_data);

/**
 * Executa um comando do sistema de forma assíncrona.
 * O comando é executado em uma thread separada (via GLib thread pool)
 * e o callback é invocado na thread principal do GTK com o resultado.
 * A UI permanece responsiva durante a execução.
 * 
 * @param command O comando a ser executado (string estilo shell)
 * @param callback Função chamada com o resultado (na thread principal). Pode ser NULL.
 * @param user_data Dados passados para o callback.
 */
void execute_command_async(const gchar* command, CommandAsyncCallback callback, gpointer user_data);

/**
 * Tipo de callback para streaming de saída de comandos em tempo real.
 * O callback é invocado na thread principal do GTK sempre que novos dados são recebidos.
 * 
 * @param chunk Um pedaço da saída do comando (NULL quando o stream termina).
 *              O callback é responsável por liberar com g_free().
 * @param user_data Dados do usuário passados para execute_command_stream().
 */
typedef void (*CommandStreamCallback)(gchar* chunk, gpointer user_data);

/**
 * Estrutura para controlar o streaming de comandos.
 * Use esta estrutura para parar o streaming quando necessário.
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
 * Executa um comando do sistema com streaming de saída em tempo real.
 * O comando é executado de forma assíncrona e o callback é chamado sempre que
 * novos dados são recebidos. A UI permanece responsiva durante a execução.
 * 
 * @param command O comando a ser executado (string estilo shell)
 * @param callback Função chamada sempre que novos dados são recebidos (na thread principal).
 * @param user_data Dados passados para o callback.
 * @return Uma estrutura CommandStream que pode ser usada para parar o streaming.
 *         Use command_stream_stop() para parar o streaming e liberar recursos.
 */
CommandStream* execute_command_stream(const gchar* command, CommandStreamCallback callback, gpointer user_data);

/**
 * Para o streaming de um comando e libera os recursos associados.
 * 
 * @param stream A estrutura CommandStream retornada por execute_command_stream().
 */
void command_stream_stop(CommandStream* stream);

#ifdef __cplusplus
}
#endif

#endif // DOCKER_COMMAND_H
