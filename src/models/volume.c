#include "volume.h"
#include "../docker/docker_command.h"
#include <string.h>
#include <glib.h>

#define INITIAL_LOAD_DONE_KEY "dodo-initial-load-done"

static void parse_and_populate_volumes(GtkListStore* store, gchar* output) {
    gtk_list_store_clear(store);
    
    if (output == NULL) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, "Erro ao executar docker volume ls",
                           1, "", 2, "", 3, "", -1);
        return;
    }
    
    gchar** lines = g_strsplit(output, "\n", -1);
    
    for (gint i = 0; lines[i] != NULL; i++) {
        if (strlen(lines[i]) == 0) continue;
        
        gchar** fields = g_strsplit(lines[i], "\t", -1);
        if (g_strv_length(fields) >= 4) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               0, fields[0] ? fields[0] : "",
                               1, fields[1] ? fields[1] : "",
                               2, fields[2] ? fields[2] : "",
                               3, fields[3] ? fields[3] : "",
                               -1);
        }
        g_strfreev(fields);
    }
    
    g_strfreev(lines);
}

void populate_docker_volumes(GtkListStore* store) {
    gchar* output = execute_command("docker volume ls --format '{{.Name}}\\t{{.Driver}}\\t{{.Scope}}\\t{{.Mountpoint}}'");
    parse_and_populate_volumes(store, output);
    g_free(output);
}

static void on_volumes_data_ready(gchar* output, gpointer user_data) {
    GtkListStore* store = GTK_LIST_STORE(user_data);
    parse_and_populate_volumes(store, output);
    g_object_set_data(G_OBJECT(store), INITIAL_LOAD_DONE_KEY, GINT_TO_POINTER(TRUE));
    g_free(output);
    g_object_unref(store);
}

void populate_docker_volumes_async(GtkListStore* store) {
    g_object_set_data(G_OBJECT(store), INITIAL_LOAD_DONE_KEY, GINT_TO_POINTER(FALSE));
    g_object_ref(store);
    execute_command_async(
        "docker volume ls --format '{{.Name}}\\t{{.Driver}}\\t{{.Scope}}\\t{{.Mountpoint}}'",
        on_volumes_data_ready,
        store
    );
}

void refresh_volumes_table(GtkListStore* store) {
    gtk_list_store_clear(store);
    populate_docker_volumes(store);
}

void refresh_volumes_table_async(GtkListStore* store) {
    populate_docker_volumes_async(store);
}
