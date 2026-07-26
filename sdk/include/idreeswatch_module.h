/*
 * Stable lifecycle descriptor for portable IdreesWatchOS modules.
 *
 * This is the public shape the ELF runtime will resolve. Keep it small and
 * versioned; modules must not depend on app_manager internals.
 */

#ifndef IDREESWATCH_MODULE_H
#define IDREESWATCH_MODULE_H

#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IDREESWATCH_MODULE_ABI_V1 1
#define IDREESWATCH_MODULE_SYMBOL idreeswatch_module

typedef struct {
    uint16_t abi_version;
    const char *id;
    esp_err_t (*create)(lv_obj_t *parent);
    void (*destroy)(void);
    void (*suspend)(void);
    void (*resume)(void);
} idreeswatch_module_v1_t;

#define IDREESWATCH_EXPORT_MODULE(package_id, create_fn, destroy_fn, \
                                   suspend_fn, resume_fn) \
    __attribute__((used, visibility("default"))) \
    const idreeswatch_module_v1_t IDREESWATCH_MODULE_SYMBOL = { \
        .abi_version = IDREESWATCH_MODULE_ABI_V1, \
        .id = package_id, \
        .create = create_fn, \
        .destroy = destroy_fn, \
        .suspend = suspend_fn, \
        .resume = resume_fn, \
    }

#ifdef __cplusplus
}
#endif

#endif
