/*
 * doomgeneric_stratus.c - DOOM platform layer for Stratus Watch
 * 
 * Implements the 5 required doomgeneric platform functions for ESP32-S3.
 * Display is handled by LVGL canvas in app_doom.c; this file bridges
 * the engine's framebuffer output to the app's canvas buffer.
 */

#include <stdio.h>
#include <string.h>

// Must define resolution BEFORE including doomgeneric.h to override defaults
// (640x400).  The Doom engine itself is 320x200; advertising a smaller
// framebuffer makes i_video.c compute fb_scaling == 0 and produces a noisy,
// uninitialised canvas on the watch.
#define DOOMGENERIC_RESX 320
#define DOOMGENERIC_RESY 200

#include "doomgeneric.h"
#include "doomkeys.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Shared state with app_doom.c. DOOM writes to the back buffer while LVGL
// displays the front buffer. The LVGL timer atomically exchanges them.
extern uint16_t *doom_canvas_front;   // Buffer LVGL is currently displaying
extern uint16_t *doom_canvas_back;    // Buffer DOOM writes to
extern uint16_t *doom_canvas_pending; // Completed frame waiting for LVGL
extern int doom_canvas_width;
extern int doom_canvas_height;
extern volatile bool doom_frame_ready;
extern volatile bool doom_running;
extern portMUX_TYPE doom_frame_mux;

// Key input queue - filled by touch controls in app_doom.c
#define DOOM_KEY_QUEUE_SIZE 32

typedef struct {
    int pressed;
    unsigned char key;
} doom_key_event_t;

static doom_key_event_t key_queue[DOOM_KEY_QUEUE_SIZE];
static volatile int key_queue_head = 0;
static volatile int key_queue_tail = 0;

// Push a key event from touch controls (called from LVGL context)
void doom_push_key(int pressed, unsigned char key)
{
    int next = (key_queue_head + 1) % DOOM_KEY_QUEUE_SIZE;
    if (next != key_queue_tail) {
        key_queue[key_queue_head].pressed = pressed;
        key_queue[key_queue_head].key = key;
        key_queue_head = next;
    }
}

// DG Platform Functions
void DG_Init(void)
{
    // Frame buffers are owned and allocated by the app in PSRAM.
}

void DG_DrawFrame(void)
{
    // Frame rendering (log removed to reduce spam).
    // DG_ScreenBuffer is the engine's 320x200 RGBA output.  Convert it once
    // into the PSRAM-backed RGB565 canvas; LVGL presents that buffer directly.
    if (!doom_canvas_back || !DG_ScreenBuffer) return;
    
    // Wait for previous frame to be consumed by display
    // This prevents DOOM from overwriting a buffer that's still being displayed
    uint16_t *dst = NULL;
    while (dst == NULL) {
        if (!doom_running) {
            return;
        }
        portENTER_CRITICAL(&doom_frame_mux);
        if (!doom_frame_ready) {
            dst = doom_canvas_back;
        }
        portEXIT_CRITICAL(&doom_frame_mux);
        if (dst != NULL) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    
    // Write to BACK buffer (DOOM renders here)
    uint32_t *src = (uint32_t *)DG_ScreenBuffer;
    int copy_w = DOOMGENERIC_RESX;
    int copy_h = DOOMGENERIC_RESY;
    
    for (int y = 0; y < copy_h; y++) {
        for (int x = 0; x < copy_w; x++) {
            uint32_t pixel = src[y * DOOMGENERIC_RESX + x];
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = pixel & 0xFF;
            
            uint16_t rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            dst[y * doom_canvas_width + x] = rgb565;
        }
    }
    
    // Hand off to LVGL: back becomes pending
    // LVGL will move pending to front when it's ready to display
    portENTER_CRITICAL(&doom_frame_mux);
    if (doom_running) {
        doom_canvas_pending = dst;
        doom_canvas_back = NULL;
        doom_frame_ready = true;
    }
    portEXIT_CRITICAL(&doom_frame_mux);
}

void DG_SleepMs(uint32_t ms)
{
    if (ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        vTaskDelay(1); // Force at least 1 tick yield to prevent starvation!
    }
}

uint32_t DG_GetTicksMs(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

int DG_GetKey(int *pressed, unsigned char *doomKey)
{
    if (key_queue_tail == key_queue_head) {
        return 0;  // No keys
    }
    
    *pressed = key_queue[key_queue_tail].pressed;
    *doomKey = key_queue[key_queue_tail].key;
    key_queue_tail = (key_queue_tail + 1) % DOOM_KEY_QUEUE_SIZE;
    
    return 1;
}

void DG_SetWindowTitle(const char *title)
{
    // No-op on embedded
    (void)title;
}
