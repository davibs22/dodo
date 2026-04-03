#ifndef WINDOW_H
#define WINDOW_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates and configures the application's main window.
 * 
 * @param argc Number of command-line arguments
 * @param argv Command-line arguments array
 * @return GtkWidget* Pointer to the created main window
 */
GtkWidget* create_main_window(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif // WINDOW_H
