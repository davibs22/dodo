#include "docker_output.h"
#include <string.h>

static gdouble parse_size_to_bytes(const gchar* size_str) {
    if (size_str == NULL || strlen(size_str) == 0) {
        return 0.0;
    }

    gchar* str = g_strdup(size_str);
    g_strstrip(str);
    gchar* lower = g_ascii_strdown(str, -1);
    g_free(str);

    gchar* endptr;
    gdouble value = g_strtod(lower, &endptr);

    if (endptr == lower) {
        g_free(lower);
        return 0.0;
    }

    gdouble multiplier = 1.0;
    if (g_str_has_suffix(endptr, "kib") || g_str_has_suffix(endptr, "kb")) {
        multiplier = 1024.0;
    } else if (g_str_has_suffix(endptr, "mib") || g_str_has_suffix(endptr, "mb")) {
        multiplier = 1024.0 * 1024.0;
    } else if (g_str_has_suffix(endptr, "gib") || g_str_has_suffix(endptr, "gb")) {
        multiplier = 1024.0 * 1024.0 * 1024.0;
    } else if (g_str_has_suffix(endptr, "tib") || g_str_has_suffix(endptr, "tb")) {
        multiplier = 1024.0 * 1024.0 * 1024.0 * 1024.0;
    }

    g_free(lower);
    return value * multiplier;
}

static void parse_io_pair(const gchar* io_str, gdouble* first_mb, gdouble* second_mb) {
    *first_mb = 0.0;
    *second_mb = 0.0;

    if (io_str == NULL || strlen(io_str) == 0) {
        return;
    }

    gchar* str = g_strdup(io_str);
    g_strstrip(str);
    gchar** parts = g_strsplit(str, "/", 2);
    g_free(str);

    for (gint part_idx = 0; part_idx < 2; part_idx++) {
        if (parts[part_idx] == NULL) {
            continue;
        }

        gchar* part_str = g_strstrip(parts[part_idx]);
        gchar* lower = g_ascii_strdown(part_str, -1);
        gchar* endptr;
        gdouble value = g_strtod(lower, &endptr);

        if (endptr != lower) {
            gdouble multiplier = 1.0;
            gchar* trimmed_endptr = g_strstrip(endptr);
            if (g_str_has_suffix(trimmed_endptr, "tib") || g_str_has_suffix(trimmed_endptr, "tb")) {
                multiplier = 1024.0 * 1024.0;
            } else if (g_str_has_suffix(trimmed_endptr, "gib") || g_str_has_suffix(trimmed_endptr, "gb")) {
                multiplier = 1024.0;
            } else if (g_str_has_suffix(trimmed_endptr, "mib") || g_str_has_suffix(trimmed_endptr, "mb")) {
                multiplier = 1.0;
            } else if (g_str_has_suffix(trimmed_endptr, "kib") || g_str_has_suffix(trimmed_endptr, "kb")) {
                multiplier = 1.0 / 1024.0;
            } else if (g_str_has_suffix(trimmed_endptr, "b")) {
                multiplier = 1.0 / (1024.0 * 1024.0);
            }

            if (part_idx == 0) {
                *first_mb = value * multiplier;
            } else {
                *second_mb = value * multiplier;
            }
        }
        g_free(lower);
    }

    g_strfreev(parts);
}

gdouble dodo_parse_docker_memory(const gchar* mem_str) {
    return parse_size_to_bytes(mem_str);
}

gdouble dodo_calculate_total_memory_usage(gchar* output) {
    if (output == NULL || strlen(output) == 0) {
        return 0.0;
    }

    gdouble total_memory = 0.0;
    gchar** lines = g_strsplit(output, "\n", -1);

    for (gint i = 0; lines[i] != NULL; i++) {
        gchar* line = g_strstrip(lines[i]);
        if (strlen(line) == 0) {
            continue;
        }
        gchar** parts = g_strsplit(line, "/", 2);
        if (parts[0] != NULL) {
            total_memory += dodo_parse_docker_memory(parts[0]);
        }
        g_strfreev(parts);
    }

    g_strfreev(lines);
    return total_memory;
}

gdouble dodo_get_system_total_memory(void) {
    GError* error = NULL;
    gchar* contents = NULL;
    gsize length = 0;

    if (!g_file_get_contents("/proc/meminfo", &contents, &length, &error)) {
        if (error) {
            g_warning("Error reading /proc/meminfo: %s", error->message);
            g_error_free(error);
        }
        return 0.0;
    }

    gchar** lines = g_strsplit(contents, "\n", -1);
    g_free(contents);

    gdouble total_mem_kb = 0.0;
    for (gint i = 0; lines[i] != NULL; i++) {
        if (g_str_has_prefix(lines[i], "MemTotal:")) {
            gchar** parts = g_strsplit(lines[i], ":", 2);
            if (parts[1] != NULL) {
                gchar* value_str = g_strstrip(parts[1]);
                gchar* endptr;
                total_mem_kb = g_strtod(value_str, &endptr);
                if (endptr == value_str || total_mem_kb <= 0.0) {
                    total_mem_kb = 0.0;
                }
            }
            g_strfreev(parts);
            break;
        }
    }

    g_strfreev(lines);
    return total_mem_kb * 1024.0;
}

void dodo_parse_docker_blockio(const gchar* blockio_str, gdouble* read_mb, gdouble* write_mb) {
    parse_io_pair(blockio_str, read_mb, write_mb);
}

void dodo_parse_docker_netio(const gchar* netio_str, gdouble* received_mb, gdouble* sent_mb) {
    parse_io_pair(netio_str, received_mb, sent_mb);
}

void dodo_calculate_total_network_io(gchar* output, gdouble* total_received_mb, gdouble* total_sent_mb) {
    *total_received_mb = 0.0;
    *total_sent_mb = 0.0;

    if (output == NULL || strlen(output) == 0) {
        return;
    }

    gchar** lines = g_strsplit(output, "\n", -1);

    for (gint i = 0; lines[i] != NULL; i++) {
        gchar* line = g_strstrip(lines[i]);
        if (strlen(line) == 0) {
            continue;
        }

        gdouble received_mb = 0.0;
        gdouble sent_mb = 0.0;
        dodo_parse_docker_netio(line, &received_mb, &sent_mb);

        *total_received_mb += received_mb;
        *total_sent_mb += sent_mb;
    }

    g_strfreev(lines);
}

void dodo_calculate_total_disk_io(gchar* output, gdouble* total_read_mb, gdouble* total_write_mb) {
    *total_read_mb = 0.0;
    *total_write_mb = 0.0;

    if (output == NULL || strlen(output) == 0) {
        return;
    }

    gchar** lines = g_strsplit(output, "\n", -1);

    for (gint i = 0; lines[i] != NULL; i++) {
        gchar* line = g_strstrip(lines[i]);
        if (strlen(line) == 0) {
            continue;
        }

        gdouble read_mb = 0.0;
        gdouble write_mb = 0.0;
        dodo_parse_docker_blockio(line, &read_mb, &write_mb);

        *total_read_mb += read_mb;
        *total_write_mb += write_mb;
    }

    g_strfreev(lines);
}
