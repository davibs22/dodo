#ifndef DODO_CORE_PARSE_JSON_FORMAT_H
#define DODO_CORE_PARSE_JSON_FORMAT_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

gchar* dodo_format_json_simple(const gchar* json);
gchar* dodo_format_inspect_output(const gchar* raw_json, const gchar* temp_prefix);

#ifdef __cplusplus
}
#endif

#endif
