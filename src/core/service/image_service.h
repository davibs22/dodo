#ifndef DODO_CORE_SERVICE_IMAGE_SERVICE_H
#define DODO_CORE_SERVICE_IMAGE_SERVICE_H

#include "../runtime/command.h"

#ifdef __cplusplus
extern "C" {
#endif

void dodo_image_remove_async(const gchar* image_id,
                             DodoCommandAsyncCallback callback,
                             gpointer user_data);
void dodo_image_inspect_async(const gchar* image_id,
                              DodoCommandAsyncCallback callback,
                              gpointer user_data);
void dodo_image_export_async(const gchar* image_id,
                             const gchar* output_path,
                             DodoCommandAsyncCallback callback,
                             gpointer user_data);
void dodo_image_import_async(const gchar* input_path,
                             DodoCommandAsyncCallback callback,
                             gpointer user_data);
void dodo_container_run_async(const gchar* command,
                              DodoCommandAsyncCallback callback,
                              gpointer user_data);

#ifdef __cplusplus
}
#endif

#endif
