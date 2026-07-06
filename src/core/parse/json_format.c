#include "json_format.h"
#include "../runtime/command.h"
#include <stdio.h>
#include <unistd.h>

gchar* dodo_format_json_simple(const gchar* json) {
    if (!json) {
        return NULL;
    }

    gint indent = 0;
    GString* formatted = g_string_new("");
    gboolean in_string = FALSE;
    gboolean escape_next = FALSE;

    for (const gchar* p = json; *p; p++) {
        if (escape_next) {
            g_string_append_c(formatted, *p);
            escape_next = FALSE;
            continue;
        }

        if (*p == '\\') {
            escape_next = TRUE;
            g_string_append_c(formatted, *p);
            continue;
        }

        if (*p == '"') {
            in_string = !in_string;
            g_string_append_c(formatted, *p);
            continue;
        }

        if (in_string) {
            g_string_append_c(formatted, *p);
            continue;
        }

        switch (*p) {
            case '{':
            case '[':
                g_string_append_c(formatted, *p);
                g_string_append_c(formatted, '\n');
                indent++;
                for (gint i = 0; i < indent; i++) {
                    g_string_append(formatted, "  ");
                }
                break;
            case '}':
            case ']':
                g_string_append_c(formatted, '\n');
                indent--;
                for (gint i = 0; i < indent; i++) {
                    g_string_append(formatted, "  ");
                }
                g_string_append_c(formatted, *p);
                break;
            case ',':
                g_string_append_c(formatted, *p);
                g_string_append_c(formatted, '\n');
                for (gint i = 0; i < indent; i++) {
                    g_string_append(formatted, "  ");
                }
                break;
            case ':':
                g_string_append(formatted, ": ");
                break;
            case ' ':
            case '\n':
            case '\t':
                break;
            default:
                g_string_append_c(formatted, *p);
                break;
        }
    }

    return g_string_free(formatted, FALSE);
}

gchar* dodo_format_inspect_output(const gchar* raw_json, const gchar* temp_prefix) {
    if (!raw_json || strlen(raw_json) == 0) {
        return NULL;
    }

    gchar* jq_check = dodo_execute_command("which jq >/dev/null 2>&1 && echo 'ok'");
    gboolean jq_available = (jq_check && g_strcmp0(g_strstrip(jq_check), "ok") == 0);
    g_free(jq_check);

    if (jq_available) {
        const gchar* prefix = temp_prefix ? temp_prefix : "docker_inspect";
        gchar* temp_file = g_strdup_printf("/tmp/%s_%d.json", prefix, getpid());
        FILE* fp = fopen(temp_file, "w");
        if (fp) {
            fputs(raw_json, fp);
            fclose(fp);

            gchar* jq_command = g_strdup_printf("jq . %s 2>/dev/null", temp_file);
            gchar* jq_output = dodo_execute_command(jq_command);
            g_free(jq_command);
            unlink(temp_file);
            g_free(temp_file);

            if (jq_output && strlen(jq_output) > 0 && !g_strrstr(jq_output, "error")) {
                return jq_output;
            }
            if (jq_output) {
                g_free(jq_output);
            }
        } else {
            g_free(temp_file);
        }
    }

    gchar* formatted = dodo_format_json_simple(raw_json);
    return formatted ? formatted : g_strdup(raw_json);
}
