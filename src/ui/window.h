#ifndef WINDOW_H
#define WINDOW_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Cria e configura a janela principal da aplicação.
 * 
 * @param argc Número de argumentos da linha de comando
 * @param argv Array de argumentos da linha de comando
 * @return GtkWidget* Ponteiro para a janela principal criada
 */
GtkWidget* create_main_window(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif // WINDOW_H
