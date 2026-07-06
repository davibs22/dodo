#include "network_repo.h"
#include "../runtime/command.h"
#include "../types/network.h"
#include <string.h>

void dodo_network_free(DodoNetwork* network) {
    if (network == NULL) {
        return;
    }
    g_free(network->id);
    g_free(network->name);
    g_free(network->driver);
    g_free(network->scope);
    g_free(network);
}

void dodo_network_list_free(DodoNetworkList* list) {
    if (list == NULL) {
        return;
    }
    if (list->networks) {
        g_ptr_array_unref(list->networks);
    }
    g_free(list);
}

DodoNetworkList* dodo_network_list_fetch(void) {
    DodoNetworkList* result = g_new0(DodoNetworkList, 1);
    result->networks = g_ptr_array_new_with_free_func((GDestroyNotify)dodo_network_free);

    gchar* output = dodo_execute_command(
        "docker network ls --format '{{.ID}}\t{{.Name}}\t{{.Driver}}\t{{.Scope}}'");

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
            DodoNetwork* network = g_new0(DodoNetwork, 1);
            network->id = g_strdup(fields[0] ? fields[0] : "");
            network->name = g_strdup(fields[1] ? fields[1] : "<none>");
            network->driver = g_strdup(fields[2] ? fields[2] : "");
            network->scope = g_strdup(fields[3] ? fields[3] : "");
            g_ptr_array_add(result->networks, network);
        }
        g_strfreev(fields);
    }

    g_strfreev(lines);
    return result;
}
