#include "status_utils.h"
#include <glib.h>

void get_running_status_and_color(const gchar* status, gchar** status_text, gchar** color) {
    if (status == NULL) {
        *status_text = g_strdup("▪ Stopped");
        *color = NULL; // Cor padrão
        return;
    }
    if (g_strstr_len(status, -1, "(Paused)") != NULL) {
        *status_text = g_strdup("◐ Paused");
        *color = g_strdup("#FFAA00"); // Laranja/Amarelo para pausado
    }
    else if (g_str_has_prefix(status, "Up")) {
        *status_text = g_strdup("● Running");
        *color = g_strdup("#00AA00"); // Verde em hexadecimal
    } else {
        *status_text = g_strdup("▪ Stopped");
        *color = g_strdup("#E32929"); // Cor padrão
    }
}

void get_loading_status_and_color(gchar** status_text, gchar** color) {
    *status_text = g_strdup("⟳ Loading");
    *color = g_strdup("#FFAA00"); // Laranja/Amarelo para carregamento
}
