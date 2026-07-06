#ifndef DODO_CORE_PARSE_DOCKER_OUTPUT_H
#define DODO_CORE_PARSE_DOCKER_OUTPUT_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

gdouble dodo_parse_docker_memory(const gchar* mem_str);
gdouble dodo_calculate_total_memory_usage(gchar* output);
gdouble dodo_get_system_total_memory(void);
void dodo_parse_docker_blockio(const gchar* blockio_str, gdouble* read_mb, gdouble* write_mb);
void dodo_parse_docker_netio(const gchar* netio_str, gdouble* received_mb, gdouble* sent_mb);
void dodo_calculate_total_network_io(gchar* output, gdouble* total_received_mb, gdouble* total_sent_mb);
void dodo_calculate_total_disk_io(gchar* output, gdouble* total_read_mb, gdouble* total_write_mb);

#ifdef __cplusplus
}
#endif

#endif
