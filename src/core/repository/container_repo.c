#include "container_repo.h"
#include "../runtime/command.h"
#include "../types/container.h"
#include <string.h>

static void destroy_container_array(gpointer data) {
    g_ptr_array_unref((GPtrArray*)data);
}

void dodo_container_free(DodoContainer* container) {
    if (container == NULL) {
        return;
    }
    g_free(container->id);
    g_free(container->image);
    g_free(container->command);
    g_free(container->created);
    g_free(container->status);
    g_free(container->ports);
    g_free(container->names);
    g_free(container->compose_project);
    g_free(container);
}

void dodo_container_list_free(DodoContainerList* list) {
    if (list == NULL) {
        return;
    }
    if (list->project_groups) {
        g_hash_table_destroy(list->project_groups);
    }
    if (list->standalone_containers) {
        g_ptr_array_unref(list->standalone_containers);
    }
    g_free(list);
}

static gchar* get_compose_project(const gchar* container_id) {
    gchar* command = g_strdup_printf(
        "docker inspect --format '{{index .Config.Labels \"com.docker.compose.project\"}}' %s",
        container_id);
    gchar* output = dodo_execute_command(command);
    g_free(command);

    if (output && strlen(output) > 0 && strlen(output) < 200) {
        g_strstrip(output);
        if (strlen(output) > 0 && strcmp(output, "<no value>") != 0) {
            return output;
        }
    }

    if (output) {
        g_free(output);
    }
    return NULL;
}

DodoContainerList* dodo_container_list_fetch(void) {
    DodoContainerList* result = g_new0(DodoContainerList, 1);

    gchar* output = dodo_execute_command(
        "docker container ls -a --format "
        "'{{.ID}}\t{{.Image}}\t{{.Command}}\t{{.CreatedAt}}\t{{.Status}}\t{{.Ports}}\t{{.Names}}'");

    if (output == NULL) {
        result->has_error = TRUE;
        return result;
    }

    gchar** lines = g_strsplit(output, "\n", -1);
    g_free(output);

    result->project_groups = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, destroy_container_array);
    result->standalone_containers = g_ptr_array_new_with_free_func((GDestroyNotify)dodo_container_free);

    for (gint i = 0; lines[i] != NULL; i++) {
        if (strlen(lines[i]) == 0) {
            continue;
        }

        gchar** fields = g_strsplit(lines[i], "\t", -1);
        if (g_strv_length(fields) >= 7) {
            DodoContainer* container = g_new0(DodoContainer, 1);
            container->id = g_strdup(fields[0] ? fields[0] : "");
            container->image = g_strdup(fields[1] ? fields[1] : "");
            container->command = g_strdup(fields[2] ? fields[2] : "");
            container->created = g_strdup(fields[3] ? fields[3] : "");
            container->status = g_strdup(fields[4] ? fields[4] : "");
            container->ports = g_strdup(fields[5] ? fields[5] : "");
            container->names = g_strdup(fields[6] ? fields[6] : "");
            container->compose_project = get_compose_project(container->id);

            if (container->compose_project && strlen(container->compose_project) > 0) {
                GPtrArray* group = g_hash_table_lookup(result->project_groups, container->compose_project);
                if (group == NULL) {
                    group = g_ptr_array_new_with_free_func((GDestroyNotify)dodo_container_free);
                    g_hash_table_insert(result->project_groups,
                                        g_strdup(container->compose_project), group);
                }
                g_ptr_array_add(group, container);
            } else {
                g_free(container->compose_project);
                container->compose_project = NULL;
                g_ptr_array_add(result->standalone_containers, container);
            }
        }
        g_strfreev(fields);
    }

    g_strfreev(lines);
    return result;
}
