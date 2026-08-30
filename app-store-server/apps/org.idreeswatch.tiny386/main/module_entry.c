#include "../../../../sdk/include/idreeswatch_module.h"

int32_t tiny386_module_start(const idreeswatch_host_v1_t *host,
                             const idreeswatch_launch_v1_t *launch);
int32_t tiny386_module_run_frame(const idreeswatch_input_v1_t *input,
                                 idreeswatch_frame_v1_t *frame);
int32_t tiny386_module_command(idreeswatch_module_command_t command,
                               int32_t argument);
void tiny386_module_stop(void);

IDREESWATCH_EXPORT_MODULE_WITH_CONTENT(
    "org.idreeswatch.tiny386",
    "tiny386",
    IDREESWATCH_MODULE_CAP_VIDEO_RGB565 |
        IDREESWATCH_MODULE_CAP_GAMEPAD |
        IDREESWATCH_MODULE_CAP_KEYBOARD |
        IDREESWATCH_MODULE_CAP_POINTER |
        IDREESWATCH_MODULE_CAP_CONTENT_FILE |
        IDREESWATCH_MODULE_CAP_CONTENT_REQUIRED,
    320,
    240,
    0,
    ".ini",
    "IdreesWatch/Tiny386",
    tiny386_module_start,
    tiny386_module_run_frame,
    tiny386_module_command,
    tiny386_module_stop
);
