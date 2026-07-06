#include "status_utils.h"
#include "core/container_state.h"
#include <glib.h>

void get_running_status_and_color(const gchar* status, gchar** status_text, gchar** color) {
    DodoContainerState state = dodo_container_state_from_status(status);

    switch (state) {
        case DODO_CONTAINER_STATE_PAUSED:
            *status_text = g_strdup("◐ Paused");
            *color = g_strdup("#FFAA00");
            break;
        case DODO_CONTAINER_STATE_RUNNING:
            *status_text = g_strdup("● Running");
            *color = g_strdup("#00AA00");
            break;
        case DODO_CONTAINER_STATE_STOPPED:
            *status_text = g_strdup("▪ Stopped");
            *color = g_strdup("#E32929");
            break;
        default:
            *status_text = g_strdup("▪ Stopped");
            *color = NULL;
            break;
    }
}

void get_loading_status_and_color(gchar** status_text, gchar** color) {
    *status_text = g_strdup("⟳ Loading");
    *color = g_strdup("#FFAA00");
}
