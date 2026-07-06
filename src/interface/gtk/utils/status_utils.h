#ifndef STATUS_UTILS_H
#define STATUS_UTILS_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Determines a container's running status and returns the corresponding color.
 * 
 * @param status Full container status (e.g., "Up 2 hours")
 * @param status_text Pointer to receive the formatted status text ("● Running" or "▪ Stopped")
 * @param color Pointer to receive the hex color (NULL for default color)
 */
void get_running_status_and_color(const gchar* status, gchar** status_text, gchar** color);

/**
 * Returns the loading status for a container.
 * 
 * @param status_text Pointer to receive the formatted status text ("⟳ Loading")
 * @param color Pointer to receive the hex color
 */
void get_loading_status_and_color(gchar** status_text, gchar** color);

#ifdef __cplusplus
}
#endif

#endif // STATUS_UTILS_H
