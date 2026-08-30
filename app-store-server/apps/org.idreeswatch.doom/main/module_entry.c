#include "../../../../sdk/include/idreeswatch_module.h"

int32_t doom_module_start(const idreeswatch_host_v1_t *host,
                          const idreeswatch_launch_v1_t *launch);
int32_t doom_module_run_frame(const idreeswatch_input_v1_t *input,
                              idreeswatch_frame_v1_t *frame);
int32_t doom_module_command(idreeswatch_module_command_t command,
                            int32_t argument);
void doom_module_stop(void);

IDREESWATCH_EXPORT_MODULE_WITH_CONTENT(
    "org.idreeswatch.doom",
    "DOOM",
    IDREESWATCH_MODULE_CAP_VIDEO_RGB565 |
        IDREESWATCH_MODULE_CAP_GAMEPAD |
        IDREESWATCH_MODULE_CAP_CONTENT_FILE |
        IDREESWATCH_MODULE_CAP_CONTENT_REQUIRED,
    320,
    200,
    0,
    ".wad",
    "IdreesWatch/library/doom/wads",
    doom_module_start,
    doom_module_run_frame,
    doom_module_command,
    doom_module_stop
);
