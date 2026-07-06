#include "volume_repo.h"
#include "../runtime/command.h"
#include "../types/volume.h"
#include <string.h>

void dodo_volume_free(DodoVolume* volume) {
    if (volume == NULL) {
        return;
    }
    g_free(volume->name);
    g_free(volume->driver);
    g_free(volume->scope);
    g_free(volume->mountpoint);
    g_free(volume);
}

void dodo_volume_list_free(DodoVolumeList* list) {
    if (list == NULL) {
        return;
    }
    if (list->volumes) {
        g_ptr_array_unref(list->volumes);
    }
    g_free(list);
}

DodoVolumeList* dodo_volume_list_fetch(void) {
    DodoVolumeList* result = g_new0(DodoVolumeList, 1);
    result->volumes = g_ptr_array_new_with_free_func((GDestroyNotify)dodo_volume_free);

    gchar* output = dodo_execute_command(
        "docker volume ls --format '{{.Name}}\\t{{.Driver}}\\t{{.Scope}}\\t{{.Mountpoint}}'");

    if (output == NULL) {
        result->has_error = TRUE;
        return result;
    }

    gchar** lines = g_strsplit(output, "\n", -1);
    g_free(output);

    for (gint i = 0; lines[i] != NULL; i++) {
        if (strlen(lines[i]) == 0) {
            continue;
        }

        gchar** fields = g_strsplit(lines[i], "\t", -1);
        if (g_strv_length(fields) >= 4) {
            DodoVolume* volume = g_new0(DodoVolume, 1);
            volume->name = g_strdup(fields[0] ? fields[0] : "");
            volume->driver = g_strdup(fields[1] ? fields[1] : "");
            volume->scope = g_strdup(fields[2] ? fields[2] : "");
            volume->mountpoint = g_strdup(fields[3] ? fields[3] : "");
            g_ptr_array_add(result->volumes, volume);
        }
        g_strfreev(fields);
    }

    g_strfreev(lines);
    return result;
}
