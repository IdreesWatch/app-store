/* IdreesWatch downloadable-module platform layer for DOOM Generic. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "doomgeneric.h"
#include "doomkeys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../../../../../sdk/include/idreeswatch_module.h"

#define DOOM_WIDTH 320U
#define DOOM_HEIGHT 200U
#define DOOM_FRAME_DURATION_US 28571U
#define KEY_QUEUE_CAPACITY 32U

typedef union allocation_header allocation_header_t;
union allocation_header {
    struct {
        allocation_header_t *previous;
        allocation_header_t *next;
        size_t size;
    } metadata;
    max_align_t alignment;
};

typedef struct {
    int pressed;
    unsigned char key;
} key_event_t;

static const idreeswatch_host_v1_t *module_host;
static allocation_header_t *allocations;
static idreeswatch_frame_v1_t *current_frame;
static key_event_t key_queue[KEY_QUEUE_CAPACITY];
static unsigned int key_head;
static unsigned int key_tail;
static uint32_t previous_buttons;
static bool started;
static char wad_path[256];
static char config_path[256];

void doom_module_stop(void);

void *doom_module_malloc(size_t size)
{
    if (!module_host || !module_host->allocate || size == 0 ||
        size > SIZE_MAX - sizeof(allocation_header_t)) {
        return NULL;
    }
    allocation_header_t *header = module_host->allocate(
        module_host->context, sizeof(*header) + size, 16);
    if (!header) return NULL;
    header->metadata.previous = NULL;
    header->metadata.next = allocations;
    header->metadata.size = size;
    if (allocations) allocations->metadata.previous = header;
    allocations = header;
    return header + 1;
}

void doom_module_free(void *memory)
{
    if (!memory || !module_host || !module_host->deallocate) return;
    allocation_header_t *header = (allocation_header_t *)memory - 1;
    if (header->metadata.previous) {
        header->metadata.previous->metadata.next = header->metadata.next;
    } else {
        allocations = header->metadata.next;
    }
    if (header->metadata.next) {
        header->metadata.next->metadata.previous = header->metadata.previous;
    }
    module_host->deallocate(module_host->context, header);
}

void *doom_module_calloc(size_t count, size_t size)
{
    if (count && size > SIZE_MAX / count) return NULL;
    size_t total = count * size;
    void *memory = doom_module_malloc(total);
    if (memory) memset(memory, 0, total);
    return memory;
}

void *doom_module_realloc(void *memory, size_t size)
{
    if (!memory) return doom_module_malloc(size);
    if (!size) {
        doom_module_free(memory);
        return NULL;
    }
    allocation_header_t *header = (allocation_header_t *)memory - 1;
    void *replacement = doom_module_malloc(size);
    if (!replacement) return NULL;
    memcpy(replacement, memory,
           header->metadata.size < size ? header->metadata.size : size);
    doom_module_free(memory);
    return replacement;
}

char *doom_module_strdup(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = doom_module_malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}

void *doom_module_heap_malloc(size_t size, uint32_t capabilities)
{
    (void)capabilities;
    return doom_module_malloc(size);
}

static void module_log(idreeswatch_log_level_t level, const char *message)
{
    if (module_host && module_host->log) {
        module_host->log(module_host->context, level, message);
    }
}

static void queue_key(int pressed, unsigned char key)
{
    unsigned int next = (key_head + 1U) % KEY_QUEUE_CAPACITY;
    if (next == key_tail) return;
    key_queue[key_head].pressed = pressed;
    key_queue[key_head].key = key;
    key_head = next;
}

static void update_key(uint32_t current, uint32_t changed,
                       uint32_t button, unsigned char key)
{
    if (changed & button) queue_key((current & button) != 0, key);
}

void DG_Init(void) {}

void DG_DrawFrame(void)
{
    if (!current_frame || !current_frame->video_requested ||
        !current_frame->pixels || !DG_ScreenBuffer) {
        return;
    }
    const uint32_t *source = (const uint32_t *)DG_ScreenBuffer;
    uint16_t *destination = current_frame->pixels;
    for (unsigned int y = 0; y < DOOM_HEIGHT; ++y) {
        uint16_t *row = destination + (size_t)y * current_frame->stride_pixels;
        for (unsigned int x = 0; x < DOOM_WIDTH; ++x) {
            uint32_t pixel = source[(size_t)y * DOOM_WIDTH + x];
            row[x] = (uint16_t)(((pixel & 0x00f80000U) >> 8) |
                                ((pixel & 0x0000fc00U) >> 5) |
                                ((pixel & 0x000000f8U) >> 3));
        }
    }
    current_frame->video_ready = true;
}

void DG_SleepMs(uint32_t milliseconds)
{
    vTaskDelay(pdMS_TO_TICKS(milliseconds ? milliseconds : 1U));
}

uint32_t DG_GetTicksMs(void)
{
    if (!module_host || !module_host->time_us) return 0;
    return (uint32_t)(module_host->time_us(module_host->context) / 1000U);
}

int DG_GetKey(int *pressed, unsigned char *doom_key)
{
    if (key_tail == key_head) return 0;
    *pressed = key_queue[key_tail].pressed;
    *doom_key = key_queue[key_tail].key;
    key_tail = (key_tail + 1U) % KEY_QUEUE_CAPACITY;
    return 1;
}

void DG_SetWindowTitle(const char *title) { (void)title; }

int32_t doom_module_start(const idreeswatch_host_v1_t *host,
                          const idreeswatch_launch_v1_t *launch)
{
    if (!host || host->struct_size < sizeof(*host) || !host->allocate ||
        !host->deallocate || !host->time_us || !launch ||
        launch->struct_size < sizeof(*launch) || !launch->content_name ||
        !launch->data_root || strstr(launch->content_name, "..")) {
        return -1;
    }
    module_host = host;
    int wad_length = snprintf(wad_path, sizeof(wad_path), "/sdcard/%s",
                              launch->content_name);
    int config_length = snprintf(config_path, sizeof(config_path),
                                 "%s/doom.cfg", launch->data_root);
    if (wad_length <= 0 || (size_t)wad_length >= sizeof(wad_path) ||
        config_length <= 0 || (size_t)config_length >= sizeof(config_path)) {
        module_host = NULL;
        return -2;
    }

    char *arguments[] = {
        "doom", "-iwad", wad_path, "-config", config_path, "-nosound", NULL
    };
    doomgeneric_Create(6, arguments);
    if (!DG_ScreenBuffer) {
        doom_module_stop();
        return -3;
    }
    started = true;
    module_log(IDREESWATCH_LOG_INFO, "Downloadable DOOM engine ready");
    return 0;
}

int32_t doom_module_run_frame(const idreeswatch_input_v1_t *input,
                              idreeswatch_frame_v1_t *frame)
{
    if (!started || !input || !frame || !frame->pixels ||
        frame->width != DOOM_WIDTH || frame->height != DOOM_HEIGHT ||
        frame->stride_pixels < DOOM_WIDTH) {
        return -1;
    }
    uint32_t changed = input->buttons ^ previous_buttons;
    update_key(input->buttons, changed, IDREESWATCH_BUTTON_RIGHT, KEY_RIGHTARROW);
    update_key(input->buttons, changed, IDREESWATCH_BUTTON_LEFT, KEY_LEFTARROW);
    update_key(input->buttons, changed, IDREESWATCH_BUTTON_UP, KEY_UPARROW);
    update_key(input->buttons, changed, IDREESWATCH_BUTTON_DOWN, KEY_DOWNARROW);
    update_key(input->buttons, changed, IDREESWATCH_BUTTON_A, KEY_FIRE);
    update_key(input->buttons, changed, IDREESWATCH_BUTTON_B, KEY_USE);
    update_key(input->buttons, changed, IDREESWATCH_BUTTON_START, KEY_ENTER);
    update_key(input->buttons, changed, IDREESWATCH_BUTTON_SELECT, KEY_ESCAPE);
    previous_buttons = input->buttons;

    frame->video_ready = false;
    frame->audio_frame_count = 0;
    frame->frame_duration_us = DOOM_FRAME_DURATION_US;
    current_frame = frame;
    doomgeneric_Tick();
    current_frame = NULL;
    return 0;
}

int32_t doom_module_command(idreeswatch_module_command_t command,
                            int32_t argument)
{
    (void)argument;
    if (!started) return -1;
    return command == IDREESWATCH_MODULE_COMMAND_PAUSE ||
           command == IDREESWATCH_MODULE_COMMAND_RESUME ? 0 : -2;
}

void doom_module_stop(void)
{
    started = false;
    current_frame = NULL;
    previous_buttons = 0;
    key_head = key_tail = 0;
    doomgeneric_FreeScreenBuffer();
    while (allocations && module_host && module_host->deallocate) {
        allocation_header_t *next = allocations->metadata.next;
        module_host->deallocate(module_host->context, allocations);
        allocations = next;
    }
    module_log(IDREESWATCH_LOG_INFO, "Downloadable DOOM engine stopped");
    module_host = NULL;
}
