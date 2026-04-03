#ifndef STATUS_UTILS_H
#define STATUS_UTILS_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Determina o status de execução de um container e retorna a cor correspondente.
 * 
 * @param status Status completo do container (ex: "Up 2 hours")
 * @param status_text Ponteiro para receber o texto do status formatado ("● Running" ou "▪ Stopped")
 * @param color Ponteiro para receber a cor em hexadecimal (NULL para cor padrão)
 */
void get_running_status_and_color(const gchar* status, gchar** status_text, gchar** color);

/**
 * Retorna o status de carregamento para um container.
 * 
 * @param status_text Ponteiro para receber o texto do status formatado ("⟳ Loading")
 * @param color Ponteiro para receber a cor em hexadecimal
 */
void get_loading_status_and_color(gchar** status_text, gchar** color);

#ifdef __cplusplus
}
#endif

#endif // STATUS_UTILS_H
