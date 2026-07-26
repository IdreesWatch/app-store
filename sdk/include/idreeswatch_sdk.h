/**
 * @file idreeswatch_sdk.h
 * @brief Unified SDK Header for IdreesWatchOS
 * 
 * This file includes all necessary headers for developing apps and watchfaces
 * for the Stratus Watch. It provides auto-registration macros that allow
 * developers to drop in their code without modifying the core OS.
 */

#ifndef IDREESWATCH_SDK_H
#define IDREESWATCH_SDK_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Core Libraries
// ============================================================================
#include "esp_log.h"
#include "esp_err.h"
#include "lvgl.h"
#include "idreeswatch_module.h"

// ============================================================================
// OS Framework APIs
// ============================================================================
#include "apps/app_manager.h"
#include "watchfaces/watchface_manager.h"
#include "ui_framework/ui_manager.h"
#include "ui_framework/ui_theme.h"
#include "ui_framework/ui_widgets.h"
#include "ui_framework/display_config.h"

// ============================================================================
// Hardware & Services
// ============================================================================
#include "hal_display.h"
#include "hal_touch.h"
#include "hal_power.h"
#include "hal_imu.h"
#include "hal_audio.h"
#include "hal_rtc.h"

#include "services/time_service.h"
#include "services/power_service.h"
#include "services/health_service.h"
#include "services/wifi_service.h"
#include "services/settings_service.h"
#include "services/theme_service.h"
#include "services/sideload_service.h"

// ============================================================================
// Auto-Registration Macros
// ============================================================================

/**
 * @brief Automatically register an app on boot.
 * 
 * Usage:
 * @code
 * void my_app_create(lv_obj_t *parent) { ... }
 * void my_app_destroy(void) { ... }
 * 
 * IDREESWATCH_REGISTER_APP(
 *     my_app,              // Unique identifier (used for var naming, no spaces)
 *     "My App",            // Display name
 *     LV_SYMBOL_STAR,      // Icon symbol
 *     my_app_create,       // Create callback
 *     my_app_destroy       // Destroy callback
 * );
 * @endcode
 */
#define IDREESWATCH_REGISTER_APP(id, display_name, icon_symbol, create_fn, destroy_fn) \
    static const app_info_t __app_info_##id = { \
        .name = display_name, \
        .icon = icon_symbol, \
        .icon_img = NULL, \
        .create = create_fn, \
        .destroy = destroy_fn \
    }; \
    __attribute__((constructor)) static void __register_app_##id(void) { \
        app_register(&__app_info_##id); \
    }

/**
 * @brief Automatically register a watchface on boot.
 * 
 * Usage:
 * @code
 * void my_wf_create(lv_obj_t *parent) { ... }
 * void my_wf_update(void) { ... }
 * void my_wf_destroy(void) { ... }
 * 
 * IDREESWATCH_REGISTER_WATCHFACE(
 *     my_wf,               // Unique identifier
 *     "My Watchface",      // Display name
 *     "John Doe",          // Author
 *     my_wf_create,        // Create callback
 *     my_wf_update,        // Update callback (1Hz)
 *     my_wf_destroy        // Destroy callback
 * );
 * @endcode
 */
#define IDREESWATCH_REGISTER_WATCHFACE(id, disp_name, author_name, create_fn, update_fn, destroy_fn) \
    static watchface_t __wf_info_##id = { \
        .id = 0, /* ID assigned by manager */ \
        .name = disp_name, \
        .author = author_name, \
        .create = create_fn, \
        .update = update_fn, \
        .destroy = destroy_fn \
    }; \
    __attribute__((constructor)) static void __register_wf_##id(void) { \
        watchface_register(&__wf_info_##id); \
    }

// ============================================================================
// Compatibility with Web Designer
// ============================================================================
/* 
 * The web designer outputs code like this:
 * 
 * const watchface_info_t wf_my_face_info = {
 *     .name = "My Face",
 *     .create = wf_my_face_create,
 *     .update = wf_my_face_update,
 *     .destroy = wf_my_face_destroy,
 * };
 * 
 * We provide a macro to intercept this and auto-register it.
 */

// We define a stub for the web designer type
typedef struct {
    const char *name;
    const char *description;
    void (*create)(lv_obj_t *parent);
    void (*update)(void);
    void (*destroy)(void);
} watchface_info_t;

// A wrapper to extract data from the web designer's struct and register it
#define IDREESWATCH_REGISTER_WEB_WATCHFACE(struct_instance) \
    __attribute__((constructor)) static void __register_web_wf_##struct_instance(void) { \
        static watchface_t wf; \
        wf.id = 0; \
        wf.name = (struct_instance).name; \
        wf.author = "Web Designer"; \
        wf.create = (struct_instance).create; \
        wf.update = (struct_instance).update; \
        wf.destroy = (struct_instance).destroy; \
        watchface_register(&wf); \
    }

#ifdef __cplusplus
}
#endif

#endif // IDREESWATCH_SDK_H
