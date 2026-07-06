#include "image_repo.h"
#include "../runtime/command.h"
#include "../types/image.h"
#include <string.h>

void dodo_image_free(DodoImage* image) {
    if (image == NULL) {
        return;
    }
    g_free(image->repository);
    g_free(image->tag);
    g_free(image->id);
    g_free(image->created);
    g_free(image->size);
    g_free(image);
}

void dodo_image_list_free(DodoImageList* list) {
    if (list == NULL) {
        return;
    }
    if (list->images) {
        g_ptr_array_unref(list->images);
    }
    g_free(list);
}

DodoImageList* dodo_image_list_fetch(void) {
    DodoImageList* result = g_new0(DodoImageList, 1);
    result->images = g_ptr_array_new_with_free_func((GDestroyNotify)dodo_image_free);

    gchar* output = dodo_execute_command(
        "docker images --format '{{.Repository}}\t{{.Tag}}\t{{.ID}}\t{{.CreatedSince}}\t{{.Size}}'");

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
        if (g_strv_length(fields) >= 5) {
            DodoImage* image = g_new0(DodoImage, 1);
            image->repository = g_strdup(fields[0] ? fields[0] : "<none>");
            image->tag = g_strdup(fields[1] ? fields[1] : "<none>");
            image->id = g_strdup(fields[2] ? fields[2] : "");
            image->created = g_strdup(fields[3] ? fields[3] : "");
            image->size = g_strdup(fields[4] ? fields[4] : "");
            g_ptr_array_add(result->images, image);
        }
        g_strfreev(fields);
    }

    g_strfreev(lines);
    return result;
}
