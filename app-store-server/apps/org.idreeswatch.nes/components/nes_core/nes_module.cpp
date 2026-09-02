/* IdreesWatch adapter for the Anemoia ESP32 NES core. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../../../../sdk/include/idreeswatch_module.h"
#include "SD.h"
#include "core/cartridge.h"
#include "core/cpu6502.h"

#define NES_OUTPUT_WIDTH 256U
#define NES_OUTPUT_HEIGHT 224U
#define NES_FRAME_DURATION_US 16639U

static const idreeswatch_host_v1_t *module_host;
static Cpu6502 *cpu;
static Cartridge *cartridge;
static uint16_t *frame_pixels;
static uint16_t frame_stride;
static uint16_t draw_scanline;
static bool draw_requested;
static bool frame_rendered;
static bool started;

void nes_platform_set_host(const idreeswatch_host_v1_t *host);
void nes_platform_release_allocations(void);
void nes_audio_begin_frame(void);
size_t nes_audio_copy(int16_t *destination, size_t capacity,
                      uint32_t *dropped);

extern "C" void nes_stop(void);

static void module_log(idreeswatch_log_level_t level, const char *message)
{
    if (module_host && module_host->log) {
        module_host->log(module_host->context, level, message);
    }
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
        memcpy(destination, source_row,
               NES_OUTPUT_WIDTH * sizeof(uint16_t));
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
    nes_platform_set_host(host);
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
    module_log(IDREESWATCH_LOG_INFO,
               "Anemoia NES core ready (60Hz timing, adaptive PPU render)");
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
    frame_pixels = frame->pixels;
    frame_stride = frame->stride_pixels;
    draw_scanline = 0;
    draw_requested = frame->video_requested;
    frame_rendered = false;
    nes_audio_begin_frame();
    frame->video_ready = false;
    cpu->bus.ppu.setDrawCallback(draw_requested ? draw_scanlines : nullptr);
    cpu->clockFrame();

    /* Do not publish a skipped PPU frame. The host retains the previous
     * canvas and requests another render without disturbing NES timing. */
    frame->video_ready = draw_requested && frame_rendered;
    frame->frame_duration_us = NES_FRAME_DURATION_US;
    size_t count = nes_audio_copy(frame->audio_samples,
                                  frame->audio_capacity_frames,
                                  nullptr);
    frame->audio_frame_count = static_cast<uint16_t>(count);
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
    SD.setMemory(nullptr, 0);
    nes_platform_release_allocations();
    module_log(IDREESWATCH_LOG_INFO, "Anemoia NES core stopped");
    module_host = nullptr;
}
