#include "container_state.h"
#include "types/container.h"
#include <string.h>

DodoContainerState dodo_container_state_from_status(const gchar* status) {
    if (status == NULL) {
        return DODO_CONTAINER_STATE_STOPPED;
    }
    if (g_strstr_len(status, -1, "(Paused)") != NULL) {
        return DODO_CONTAINER_STATE_PAUSED;
    }
    if (g_str_has_prefix(status, "Up")) {
        return DODO_CONTAINER_STATE_RUNNING;
    }
    if (status[0] == '\0') {
        return DODO_CONTAINER_STATE_UNKNOWN;
    }
    return DODO_CONTAINER_STATE_STOPPED;
}

gboolean dodo_container_is_running_status(const gchar* status) {
    return dodo_container_state_from_status(status) == DODO_CONTAINER_STATE_RUNNING;
}

void dodo_compose_group_summary(GPtrArray* containers, gchar** status_text,
                                guint* running_count, guint* total_count) {
    if (running_count) *running_count = 0;
    if (total_count) *total_count = 0;

    if (containers == NULL || containers->len == 0) {
        if (status_text) *status_text = g_strdup("");
        return;
    }

    guint running = 0;
    guint total = containers->len;

    for (guint i = 0; i < containers->len; i++) {
        DodoContainer* container = g_ptr_array_index(containers, i);
        if (dodo_container_is_running_status(container->status)) {
            running++;
        }
    }

    if (running_count) *running_count = running;
    if (total_count) *total_count = total;

    if (status_text) {
        if (running == 0) {
            *status_text = g_strdup_printf("All Stopped (%d)", total);
        } else if (running == total) {
            *status_text = g_strdup_printf("All Running (%d)", total);
        } else {
            *status_text = g_strdup_printf("%d/%d Running", running, total);
        }
    }
}
