/*
 * IdreesWatchOS portable module ABI v1.
 *
 * This header is intentionally independent of ESP-IDF, FreeRTOS, and LVGL.
 * A downloadable module renders into host-owned RGB565/audio buffers and
 * receives input through a fixed-width structure. Keep fields append-only.
 */

#ifndef IDREESWATCH_MODULE_H
#define IDREESWATCH_MODULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDREESWATCH_MODULE_ABI_V1 1U
#define IDREESWATCH_MODULE_ENTRY_NAME "idreeswatch_module"
#define IDREESWATCH_MODULE_SYMBOL idreeswatch_module

#define IDREESWATCH_MODULE_CAP_VIDEO_RGB565 (1U << 0)
#define IDREESWATCH_MODULE_CAP_AUDIO_PCM16   (1U << 1)
#define IDREESWATCH_MODULE_CAP_GAMEPAD       (1U << 2)

typedef enum {
    IDREESWATCH_LOG_ERROR = 0,
    IDREESWATCH_LOG_WARN = 1,
    IDREESWATCH_LOG_INFO = 2,
    IDREESWATCH_LOG_DEBUG = 3,
} idreeswatch_log_level_t;

typedef enum {
    IDREESWATCH_BUTTON_A      = 1U << 0,
    IDREESWATCH_BUTTON_B      = 1U << 1,
    IDREESWATCH_BUTTON_SELECT = 1U << 2,
    IDREESWATCH_BUTTON_START  = 1U << 3,
    IDREESWATCH_BUTTON_UP     = 1U << 4,
    IDREESWATCH_BUTTON_DOWN   = 1U << 5,
    IDREESWATCH_BUTTON_LEFT   = 1U << 6,
    IDREESWATCH_BUTTON_RIGHT  = 1U << 7,
} idreeswatch_button_t;

typedef enum {
    IDREESWATCH_MODULE_COMMAND_RESET = 1,
    IDREESWATCH_MODULE_COMMAND_SAVE = 2,
    IDREESWATCH_MODULE_COMMAND_LOAD = 3,
    IDREESWATCH_MODULE_COMMAND_PAUSE = 4,
    IDREESWATCH_MODULE_COMMAND_RESUME = 5,
} idreeswatch_module_command_t;

typedef struct {
    uint32_t struct_size;
    void *context;
    void (*log)(void *context, idreeswatch_log_level_t level,
                const char *message);
    void *(*allocate)(void *context, size_t size, size_t alignment);
    void (*deallocate)(void *context, void *memory);
    uint64_t (*time_us)(void *context);
} idreeswatch_host_v1_t;

typedef struct {
    uint32_t struct_size;
    const char *package_id;
    const char *package_root;
    const char *data_root;
    const char *content_name;
    const void *content_data;
    size_t content_size;
} idreeswatch_launch_v1_t;

typedef struct {
    uint32_t struct_size;
    uint32_t buttons;
    uint64_t frame_number;
} idreeswatch_input_v1_t;

typedef struct {
    uint32_t struct_size;
    uint16_t *pixels;
    uint16_t width;
    uint16_t height;
    uint16_t stride_pixels;
    bool video_ready;
    int16_t *audio_samples;
    uint16_t audio_capacity_frames;
    uint16_t audio_frame_count;
} idreeswatch_frame_v1_t;

/*
 * start(), run_frame(), and command() return 0 on success. Negative values
 * are module-defined failures and are surfaced to the user as an app error.
 */
typedef struct {
    uint32_t struct_size;
    uint16_t abi_version;
    uint16_t reserved;
    const char *id;
    const char *name;
    uint32_t capabilities;
    uint16_t video_width;
    uint16_t video_height;
    uint32_t audio_sample_rate;
    int32_t (*start)(const idreeswatch_host_v1_t *host,
                     const idreeswatch_launch_v1_t *launch);
    int32_t (*run_frame)(const idreeswatch_input_v1_t *input,
                         idreeswatch_frame_v1_t *frame);
    int32_t (*command)(idreeswatch_module_command_t command, int32_t argument);
    void (*stop)(void);
} idreeswatch_module_v1_t;

#define IDREESWATCH_EXPORT_MODULE(package_id, display_name, caps, width, \
                                   height, sample_rate, start_fn, frame_fn, \
                                   command_fn, stop_fn) \
    __attribute__((used, visibility("default"))) \
    const idreeswatch_module_v1_t IDREESWATCH_MODULE_SYMBOL = { \
        .struct_size = sizeof(idreeswatch_module_v1_t), \
        .abi_version = IDREESWATCH_MODULE_ABI_V1, \
        .id = package_id, \
        .name = display_name, \
        .capabilities = caps, \
        .video_width = width, \
        .video_height = height, \
        .audio_sample_rate = sample_rate, \
        .start = start_fn, \
        .run_frame = frame_fn, \
        .command = command_fn, \
        .stop = stop_fn, \
    }

#ifdef __cplusplus
}
#endif

#endif
