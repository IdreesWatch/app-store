/* IdreesWatch adapter for the Anemoia ESP32 NES core. */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../../../sdk/include/idreeswatch_module.h"
#include "SD.h"
#include "core/cartridge.h"
#include "core/cpu6502.h"
#include "driver/i2s.h"

#define NES_OUTPUT_WIDTH 256U
#define NES_OUTPUT_HEIGHT 224U
#define NES_FRAME_DURATION_US 16639U

typedef union allocation_header allocation_header_t;
union allocation_header {
    struct {
        allocation_header_t *previous;
        allocation_header_t *next;
        size_t size;
    } metadata;
    max_align_t alignment;
};

static const idreeswatch_host_v1_t *module_host;
static allocation_header_t *allocations;
static Cpu6502 *cpu;
static Cartridge *cartridge;
static uint16_t *frame_pixels;
static uint16_t frame_stride;
static uint16_t draw_scanline;
static bool draw_requested;
static bool frame_rendered;
static bool started;
static int16_t audio_staging[1024];
static size_t audio_count;
static uint64_t metrics_started_us;
static uint32_t metrics_frames;
static uint32_t metrics_rendered;
static uint32_t metrics_audio_frames;
static uint32_t metrics_audio_dropped;
static uint64_t metrics_frame_time_us;
static uint32_t metrics_max_frame_us;

SDClass SD;

extern "C" void nes_stop(void);

static void module_log(idreeswatch_log_level_t level, const char *message)
{
    if (module_host && module_host->log) {
        module_host->log(module_host->context, level, message);
    }
}

static void reset_metrics(uint64_t now_us)
{
    metrics_started_us = now_us;
    metrics_frames = 0;
    metrics_rendered = 0;
    metrics_audio_frames = 0;
    metrics_audio_dropped = 0;
    metrics_frame_time_us = 0;
    metrics_max_frame_us = 0;
}

static void report_metrics(uint64_t now_us)
{
    if (!module_host || !module_host->time_us || !metrics_started_us ||
        now_us <= metrics_started_us) {
        return;
    }
    const uint64_t window_us = now_us - metrics_started_us;
    if (window_us < 5000000ULL || metrics_frames == 0) return;

    char message[224];
    const uint32_t emu_fps = static_cast<uint32_t>(
        (static_cast<uint64_t>(metrics_frames) * 1000000ULL +
         window_us / 2ULL) / window_us);
    const uint32_t render_fps = static_cast<uint32_t>(
        (static_cast<uint64_t>(metrics_rendered) * 1000000ULL +
         window_us / 2ULL) / window_us);
    const uint32_t audio_hz = static_cast<uint32_t>(
        (static_cast<uint64_t>(metrics_audio_frames) * 1000000ULL +
         window_us / 2ULL) / window_us);
    const uint32_t average_us = static_cast<uint32_t>(
        metrics_frame_time_us / metrics_frames);
    snprintf(message, sizeof(message),
             "NES PERF emu=%lu fps=%lu render=%lu fps=%lu "
             "frame_avg=%luus frame_max=%luus audio=%luHz "
             "audio_drop=%lu",
             static_cast<unsigned long>(metrics_frames),
             static_cast<unsigned long>(emu_fps),
             static_cast<unsigned long>(metrics_rendered),
             static_cast<unsigned long>(render_fps),
             static_cast<unsigned long>(average_us),
             static_cast<unsigned long>(metrics_max_frame_us),
             static_cast<unsigned long>(audio_hz),
             static_cast<unsigned long>(metrics_audio_dropped));
    module_log(IDREESWATCH_LOG_INFO, message);
    reset_metrics(now_us);
}

extern "C" __attribute__((weak)) void *malloc(size_t size) noexcept
{
    if (!module_host || !module_host->allocate || size == 0 ||
        size > SIZE_MAX - sizeof(allocation_header_t)) {
        return nullptr;
    }
    allocation_header_t *header = static_cast<allocation_header_t *>(
        module_host->allocate(module_host->context,
                              sizeof(*header) + size, 16));
    if (!header) return nullptr;
    header->metadata.previous = nullptr;
    header->metadata.next = allocations;
    header->metadata.size = size;
    if (allocations) allocations->metadata.previous = header;
    allocations = header;
    return header + 1;
}

extern "C" __attribute__((weak)) void free(void *memory) noexcept
{
    if (!memory || !module_host || !module_host->deallocate) return;
    allocation_header_t *header = static_cast<allocation_header_t *>(memory) - 1;
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

extern "C" __attribute__((weak)) void *calloc(size_t count, size_t size) noexcept
{
    if (count && size > SIZE_MAX / count) return nullptr;
    size_t total = count * size;
    void *memory = malloc(total);
    if (memory) memset(memory, 0, total);
    return memory;
}

extern "C" __attribute__((weak)) void *realloc(void *memory, size_t size) noexcept
{
    if (!memory) return malloc(size);
    if (!size) {
        free(memory);
        return nullptr;
    }
    allocation_header_t *header = static_cast<allocation_header_t *>(memory) - 1;
    void *replacement = malloc(size);
    if (!replacement) return nullptr;
    memcpy(replacement, memory,
           header->metadata.size < size ? header->metadata.size : size);
    free(memory);
    return replacement;
}

extern "C" __attribute__((weak)) char *strdup(const char *text) noexcept
{
    size_t length = strlen(text) + 1;
    char *copy = static_cast<char *>(malloc(length));
    if (copy) memcpy(copy, text, length);
    return copy;
}

void *operator new(size_t size) { return malloc(size); }
void *operator new[](size_t size) { return malloc(size); }
void operator delete(void *memory) noexcept { free(memory); }
void operator delete[](void *memory) noexcept { free(memory); }
void operator delete(void *memory, size_t) noexcept { free(memory); }
void operator delete[](void *memory, size_t) noexcept { free(memory); }

extern "C" void __cxa_pure_virtual(void) {}

extern "C" int printf(const char *format, ...)
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

/* The standalone core writes interleaved unsigned stereo. Capture one channel
 * and center it into the host's signed mono PCM frame. */
extern "C" esp_err_t i2s_write(i2s_port_t, const void *source, size_t bytes,
                               size_t *bytes_written, TickType_t)
{
    const uint16_t *samples = static_cast<const uint16_t *>(source);
    size_t stereo_frames = bytes / (2U * sizeof(uint16_t));
    size_t capacity = (sizeof(audio_staging) / sizeof(audio_staging[0])) - audio_count;
    if (stereo_frames > capacity) stereo_frames = capacity;
    for (size_t i = 0; i < stereo_frames; ++i) {
        audio_staging[audio_count++] = static_cast<int16_t>(
            static_cast<int32_t>(samples[i * 2U]) - 32768);
    }
    if (bytes_written) *bytes_written = bytes;
    return ESP_OK;
}

bool mappedROM_init(MappedROM *, Cartridge *, uint32_t, uint8_t, uint8_t)
{
    return false;
}

static void draw_scanlines(uint8_t *buffer, uint32_t size)
{
    const uint16_t *source = reinterpret_cast<const uint16_t *>(buffer);
    uint16_t rows = static_cast<uint16_t>(
        size / (NES_OUTPUT_WIDTH * sizeof(uint16_t)));
    for (uint16_t row = 0; row < rows; ++row, ++draw_scanline) {
        if (!draw_requested || draw_scanline < 8 || draw_scanline >= 232) continue;
        uint16_t *destination = frame_pixels +
            static_cast<size_t>(draw_scanline - 8) * frame_stride;
        const uint16_t *source_row = source +
            static_cast<size_t>(row) * NES_OUTPUT_WIDTH;
        for (uint16_t x = 0; x < NES_OUTPUT_WIDTH; ++x) {
            destination[x] = __builtin_bswap16(source_row[x]);
        }
        frame_rendered = true;
    }
}

static bool supported_rom(const uint8_t *rom, size_t size)
{
    if (!rom || size < 16 || memcmp(rom, "NES\x1a", 4) != 0) return false;
    uint8_t mapper = static_cast<uint8_t>((rom[7] & 0xf0U) | (rom[6] >> 4));
    return mapper == 0 || mapper == 1 || mapper == 2 || mapper == 3 ||
           mapper == 4 || mapper == 69;
}

extern "C" int32_t nes_start(const idreeswatch_host_v1_t *host,
                             const idreeswatch_launch_v1_t *launch)
{
    if (!host || host->struct_size < sizeof(*host) || !host->allocate ||
        !host->deallocate || !launch || launch->struct_size < sizeof(*launch) ||
        !supported_rom(static_cast<const uint8_t *>(launch->content_data),
                       launch->content_size)) {
        return -1;
    }

    module_host = host;
    SD.setMemory(launch->content_data, launch->content_size);
    cpu = new Cpu6502();
    cartridge = new Cartridge("module.nes", ROMBackend::LRU);
    if (!cpu || !cartridge || !cartridge->isValid()) {
        module_log(IDREESWATCH_LOG_ERROR, "Unsupported or invalid NES cartridge");
        nes_stop();
        return -2;
    }
    cpu->bus.ppu.setDrawCallback(draw_scanlines);
    cpu->bus.insertCartridge(cartridge);
    cpu->reset();
    started = true;
    reset_metrics(module_host->time_us
                      ? module_host->time_us(module_host->context) : 0);
    module_log(IDREESWATCH_LOG_INFO,
               "Anemoia NES core ready (60Hz emulation, 30Hz render)");
    return 0;
}

extern "C" int32_t nes_run_frame(const idreeswatch_input_v1_t *input,
                                 idreeswatch_frame_v1_t *frame)
{
    if (!started || !cpu || !input || !frame || !frame->pixels ||
        frame->width != NES_OUTPUT_WIDTH || frame->height != NES_OUTPUT_HEIGHT ||
        frame->stride_pixels < NES_OUTPUT_WIDTH) {
        return -1;
    }

    cpu->bus.setController(static_cast<uint8_t>(input->buttons));
    const uint64_t frame_started_us = module_host && module_host->time_us
        ? module_host->time_us(module_host->context) : 0;
    frame_pixels = frame->pixels;
    frame_stride = frame->stride_pixels;
    draw_scanline = 0;
    draw_requested = frame->video_requested;
    frame_rendered = false;
    audio_count = 0;
    frame->video_ready = false;
    cpu->clockFrame();

    /* FRAMESKIP deliberately renders alternate PPU frames.  Do not publish
     * an unrendered module buffer as a new frame; the host will retain the
     * previous canvas and request the next render without disturbing timing. */
    frame->video_ready = draw_requested && frame_rendered;
    frame->frame_duration_us = NES_FRAME_DURATION_US;
    size_t count = audio_count;
    if (count > frame->audio_capacity_frames) count = frame->audio_capacity_frames;
    if (audio_count > count) {
        metrics_audio_dropped += static_cast<uint32_t>(audio_count - count);
    }
    if (frame->audio_samples && count) {
        memcpy(frame->audio_samples, audio_staging, count * sizeof(int16_t));
        frame->audio_frame_count = static_cast<uint16_t>(count);
    } else {
        frame->audio_frame_count = 0;
    }
    const uint64_t frame_finished_us = module_host && module_host->time_us
        ? module_host->time_us(module_host->context) : frame_started_us;
    ++metrics_frames;
    if (frame_rendered) ++metrics_rendered;
    metrics_audio_frames += static_cast<uint32_t>(count);
    if (frame_finished_us >= frame_started_us) {
        const uint64_t elapsed_us = frame_finished_us - frame_started_us;
        metrics_frame_time_us += elapsed_us;
        if (elapsed_us > metrics_max_frame_us) {
            metrics_max_frame_us = elapsed_us > UINT32_MAX
                ? UINT32_MAX : static_cast<uint32_t>(elapsed_us);
        }
    }
    report_metrics(frame_finished_us);
    return 0;
}

extern "C" int32_t nes_command(idreeswatch_module_command_t command,
                               int32_t argument)
{
    (void)argument;
    if (!started || !cpu) return -1;
    if (command == IDREESWATCH_MODULE_COMMAND_RESET) {
        cpu->reset();
        return 0;
    }
    return -2;
}

extern "C" void nes_stop(void)
{
    started = false;
    cpu = nullptr;
    cartridge = nullptr;
    frame_pixels = nullptr;
    while (allocations && module_host && module_host->deallocate) {
        allocation_header_t *next = allocations->metadata.next;
        module_host->deallocate(module_host->context, allocations);
        allocations = next;
    }
    SD.setMemory(nullptr, 0);
    reset_metrics(0);
    module_log(IDREESWATCH_LOG_INFO, "Anemoia NES core stopped");
    module_host = nullptr;
}
