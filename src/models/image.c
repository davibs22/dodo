#include "image.h"
#include "../docker/docker_command.h"
#include <string.h>
#include <glib.h>
static void parse_and_populate_images(GtkListStore* store, gchar* output) {
    gtk_list_store_clear(store);
    
    if (output == NULL) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                           0, "Erro ao executar docker images",
                           1, "", 2, "", 3, "", 4, "", -1);
        return;
    }
    
    gchar** lines = g_strsplit(output, "\n", -1);
    
    for (gint i = 0; lines[i] != NULL; i++) {
        if (strlen(lines[i]) == 0) continue;
        
        gchar** fields = g_strsplit(lines[i], "\t", -1);
        if (g_strv_length(fields) >= 5) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               0, fields[0] ? fields[0] : "<none>",  // REPOSITORY
                               1, fields[1] ? fields[1] : "<none>",  // TAG
                               2, fields[2] ? fields[2] : "",         // IMAGE ID
                               3, fields[3] ? fields[3] : "",         // CREATED
                               4, fields[4] ? fields[4] : "",         // SIZE
                               -1);
        }
        g_strfreev(fields);
    }
    
    g_strfreev(lines);
}

void populate_docker_images(GtkListStore* store) {
    gchar* output = execute_command("docker images --format '{{.Repository}}\t{{.Tag}}\t{{.ID}}\t{{.CreatedSince}}\t{{.Size}}'");
    parse_and_populate_images(store, output);
    g_free(output);
}
static void on_images_data_ready(gchar* output, gpointer user_data) {
    GtkListStore* store = GTK_LIST_STORE(user_data);
    parse_and_populate_images(store, output);
    g_free(output);
    g_object_unref(store);
}

void populate_docker_images_async(GtkListStore* store) {
    g_object_ref(store);
    execute_command_async(
        "docker images --format '{{.Repository}}\t{{.Tag}}\t{{.ID}}\t{{.CreatedSince}}\t{{.Size}}'",
        on_images_data_ready,
        store
    );
}

void refresh_images_table(GtkListStore* store) {
    gtk_list_store_clear(store);
    populate_docker_images(store);
}

void refresh_images_table_async(GtkListStore* store) {
    populate_docker_images_async(store);
}
