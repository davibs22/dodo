#ifndef DODO_CORE_CONTAINER_STATE_H
#define DODO_CORE_CONTAINER_STATE_H

#include "types/container.h"
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

DodoContainerState dodo_container_state_from_status(const gchar* status);
gboolean dodo_container_is_running_status(const gchar* status);
void dodo_compose_group_summary(GPtrArray* containers, gchar** status_text,
                                guint* running_count, guint* total_count);

#ifdef __cplusplus
}
#endif

#endif
