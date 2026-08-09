/*
 * IdreesWatch NES reference module.
 *
 * Emulator core: Nofrendo as maintained by the retro-go project. See
 * nofrendo/COPYING and THIRD_PARTY.md. ROM content is supplied by the user.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "idreeswatch_module.h"
#include "nofrendo.h"
#include "nes/input.h"
#include "nes/nes.h"
#include "nes/rom.h"

#define NES_OUTPUT_WIDTH 256U
#define NES_OUTPUT_HEIGHT 224U
#define NES_AUDIO_RATE 24000U

typedef union {
    struct {
        size_t size;
    } metadata;
    max_align_t alignment;
} allocation_header_t;

static const idreeswatch_host_v1_t *module_host;
static nes_t *console;
static uint8_t *indexed_frame;
static uint16_t *palette;
static bool started;

/*
 * Keep emulator allocations in PSRAM through the host allocator. Defining
 * these locally also removes malloc/free from the shared object's imports.
 */
void *malloc(size_t size)
{
    if (!module_host || !module_host->allocate || size == 0) return NULL;
    if (size > SIZE_MAX - sizeof(allocation_header_t)) return NULL;
    allocation_header_t *header = module_host->allocate(
        module_host->context, sizeof(*header) + size, 16);
    if (!header) return NULL;
    header->metadata.size = size;
    return header + 1;
}

void free(void *memory)
{
    if (!memory || !module_host || !module_host->deallocate) return;
    module_host->deallocate(module_host->context,
                            ((allocation_header_t *)memory) - 1);
}

void *calloc(size_t count, size_t size)
{
    if (count && size > SIZE_MAX / count) return NULL;
    size_t total = count * size;
    void *memory = malloc(total);
    if (memory) memset(memory, 0, total);
    return memory;
}

void *realloc(void *memory, size_t size)
{
    if (!memory) return malloc(size);
    if (size == 0) {
        free(memory);
        return NULL;
    }
    allocation_header_t *old_header =
        ((allocation_header_t *)memory) - 1;
    void *replacement = malloc(size);
    if (!replacement) return NULL;
    size_t copy_size = old_header->metadata.size < size
        ? old_header->metadata.size : size;
    memcpy(replacement, memory, copy_size);
    free(memory);
    return replacement;
}

char *strdup(const char *text)
{
    if (!text) return NULL;
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}

static void module_log(idreeswatch_log_level_t level, const char *message)
{
    if (module_host && module_host->log) {
        module_host->log(module_host->context, level, message);
    }
}

int printf(const char *format, ...)
{
    char buffer[192];
    va_list arguments;
    va_start(arguments, format);
    int result = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    buffer[sizeof(buffer) - 1] = '\0';
    module_log(IDREESWATCH_LOG_DEBUG, buffer);
    return result;
}

/*
 * The reference ABI deliberately exposes no arbitrary filesystem access.
 * Nofrendo's optional FDS/save-state paths therefore fail closed; cartridge
 * content itself arrives through the host-owned launch buffer.
 */
FILE *fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return NULL;
}

size_t fread(void *buffer, size_t size, size_t count, FILE *stream)
{
    (void)buffer;
    (void)size;
    (void)count;
    (void)stream;
    return 0;
}

int fclose(FILE *stream)
{
    (void)stream;
    return EOF;
}

int32_t nes_start(const idreeswatch_host_v1_t *host,
                  const idreeswatch_launch_v1_t *launch)
{
    if (!host || host->struct_size < sizeof(*host) ||
        !host->allocate || !host->deallocate ||
        !launch || launch->struct_size < sizeof(*launch) ||
        !launch->content_data || launch->content_size < 16) {
        return -1;
    }
    module_host = host;
    console = nes_init(SYS_DETECT, NES_AUDIO_RATE, false, NULL);
    if (!console) {
        module_host = NULL;
        return -2;
    }

    rom_t *rom = rom_loadmem((uint8_t *)launch->content_data,
                             launch->content_size);
    if (!rom) {
        nes_shutdown();
        console = NULL;
        module_host = NULL;
        return -3;
    }
    if (nes_insertcart(rom) != 0) {
        /* nes_insertcart() owns failure cleanup through nes_shutdown(). */
        console = NULL;
        module_host = NULL;
        return -3;
    }

    indexed_frame = calloc(NES_SCREEN_PITCH * NES_SCREEN_HEIGHT,
                           sizeof(*indexed_frame));
    palette = nofrendo_buildpalette(NES_PALETTE_NESCLASSIC, 16);
    if (!indexed_frame || !palette) {
        nes_shutdown();
        console = NULL;
        free(indexed_frame);
        free(palette);
        indexed_frame = NULL;
        palette = NULL;
        module_host = NULL;
        return -4;
    }
    started = true;
    module_log(IDREESWATCH_LOG_INFO, "Nofrendo NES module ready");
    return 0;
}

int32_t nes_run_frame(const idreeswatch_input_v1_t *input,
                      idreeswatch_frame_v1_t *frame)
{
    if (!started || !console || !input || !frame || !frame->pixels ||
        frame->width != NES_OUTPUT_WIDTH ||
        frame->height != NES_OUTPUT_HEIGHT ||
        frame->stride_pixels < NES_OUTPUT_WIDTH) {
        return -1;
    }

    input_update(0, (int)(input->buttons & 0xffU));
    nes_setvidbuf(indexed_frame);
    const bool draw_frame = frame->video_requested;
    frame->video_ready = false;
    nes_emulate(draw_frame);

    if (draw_frame) {
        for (uint16_t y = 0; y < NES_OUTPUT_HEIGHT; ++y) {
            const uint8_t *source =
                NES_SCREEN_GETPTR(indexed_frame, 0, y + 8);
            uint16_t *destination =
                frame->pixels + (size_t)y * frame->stride_pixels;
            for (uint16_t x = 0; x < NES_OUTPUT_WIDTH; ++x) {
                destination[x] = palette[source[x]];
            }
        }
        frame->video_ready = true;
    }
    frame->frame_duration_us = 1000000U / (uint32_t)console->refresh_rate;

    size_t audio_frames = (size_t)console->apu->samples_per_frame;
    if (frame->audio_samples && audio_frames <= frame->audio_capacity_frames) {
        memcpy(frame->audio_samples, console->apu->buffer,
               audio_frames * sizeof(int16_t));
        frame->audio_frame_count = (uint16_t)audio_frames;
    } else {
        frame->audio_frame_count = 0;
    }
    return 0;
}

int32_t nes_command(idreeswatch_module_command_t command, int32_t argument)
{
    (void)argument;
    if (!started || !console) return -1;
    if (command == IDREESWATCH_MODULE_COMMAND_RESET) {
        nes_reset(true);
        return 0;
    }
    return -2;
}

void nes_stop(void)
{
    if (started) nes_shutdown();
    started = false;
    console = NULL;
    free(indexed_frame);
    free(palette);
    indexed_frame = NULL;
    palette = NULL;
    module_log(IDREESWATCH_LOG_INFO, "Nofrendo NES module stopped");
    module_host = NULL;
}
