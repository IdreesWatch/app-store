/*
 * Public module entry object.
 *
 * project_so links main sources directly, so the default-visible descriptor
 * is a shared-object GC root. Its function references pull the optimized
 * Anemoia implementation from libnes_core.a.
 */

#include "../../../../sdk/include/idreeswatch_module.h"

#define NES_OUTPUT_WIDTH 256U
#define NES_OUTPUT_HEIGHT 224U
#define NES_AUDIO_RATE 44100U

int32_t nes_start(const idreeswatch_host_v1_t *host,
                  const idreeswatch_launch_v1_t *launch);
int32_t nes_run_frame(const idreeswatch_input_v1_t *input,
                      idreeswatch_frame_v1_t *frame);
int32_t nes_command(idreeswatch_module_command_t command, int32_t argument);
void nes_stop(void);

IDREESWATCH_EXPORT_MODULE_WITH_CONTENT(
    "org.idreeswatch.nes",
    "NES",
    IDREESWATCH_MODULE_CAP_VIDEO_RGB565 |
        IDREESWATCH_MODULE_CAP_AUDIO_PCM16 |
        IDREESWATCH_MODULE_CAP_GAMEPAD |
        IDREESWATCH_MODULE_CAP_CONTENT_REQUIRED,
    NES_OUTPUT_WIDTH,
    NES_OUTPUT_HEIGHT,
    NES_AUDIO_RATE,
    ".nes",
    "IdreesWatch/library/roms/nes",
    nes_start,
    nes_run_frame,
    nes_command,
    nes_stop
);
