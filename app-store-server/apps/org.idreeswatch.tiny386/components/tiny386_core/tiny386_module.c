/* IdreesWatch portable adapter for hchunhui/tiny386. */

#undef malloc
#undef calloc
#undef realloc
#undef free
#undef strdup
#undef fopen
#undef fclose

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../../../../sdk/include/idreeswatch_module.h"
#include "i8042.h"
#include "ini.h"
#include "pc.h"

#define TINY386_WIDTH 320U
#define TINY386_HEIGHT 240U
#define TINY386_FRAME_US 16667U
#define TINY386_CPU_BUDGET_US 13500U
#define TINY386_ROOT "/sdcard/IdreesWatch/Tiny386"
#define TINY386_RELATIVE_ROOT "IdreesWatch/Tiny386/"
#define TINY386_MAX_FILES 16U
#define TINY386_MIN_RAM (2L * 1024L * 1024L)
#define TINY386_DEFAULT_RAM (4L * 1024L * 1024L)
#define TINY386_MAX_RAM (6L * 1024L * 1024L)

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
static FILE *open_files[TINY386_MAX_FILES];
static PCConfig pc_config;
static PC *pc;
static uint16_t *video_buffer;
static uint32_t previous_buttons;
static uint8_t previous_pointer_buttons;
static bool display_dirty;
static bool started;
static char config_path[256];

static void module_log(idreeswatch_log_level_t level, const char *message)
{
    if (module_host && module_host->log) {
        module_host->log(module_host->context, level, message);
    }
}

void *tiny386_malloc(size_t size)
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

void tiny386_free(void *memory)
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

void *tiny386_calloc(size_t count, size_t size)
{
    if (count && size > SIZE_MAX / count) return NULL;
    size_t total = count * size;
    void *memory = tiny386_malloc(total);
    if (memory) memset(memory, 0, total);
    return memory;
}

void *tiny386_realloc(void *memory, size_t size)
{
    if (!memory) return tiny386_malloc(size);
    if (!size) {
        tiny386_free(memory);
        return NULL;
    }
    allocation_header_t *header = (allocation_header_t *)memory - 1;
    void *replacement = tiny386_malloc(size);
    if (!replacement) return NULL;
    memcpy(replacement, memory,
           header->metadata.size < size ? header->metadata.size : size);
    tiny386_free(memory);
    return replacement;
}

char *tiny386_strdup(const char *text)
{
    if (!text) return NULL;
    size_t length = strlen(text) + 1;
    char *copy = tiny386_malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}

FILE *tiny386_fopen(const char *path, const char *mode)
{
    FILE *file = fopen(path, mode);
    if (!file) return NULL;
    for (size_t i = 0; i < TINY386_MAX_FILES; ++i) {
        if (!open_files[i]) {
            open_files[i] = file;
            return file;
        }
    }
    fclose(file);
    errno = EMFILE;
    return NULL;
}

int tiny386_fclose(FILE *file)
{
    if (!file) return EOF;
    for (size_t i = 0; i < TINY386_MAX_FILES; ++i) {
        if (open_files[i] == file) {
            open_files[i] = NULL;
            break;
        }
    }
    return fclose(file);
}

void *bigmalloc(size_t size)
{
    return tiny386_malloc(size);
}

uint32_t get_uticks(void)
{
    return module_host && module_host->time_us
        ? (uint32_t)module_host->time_us(module_host->context) : 0;
}

/* Tiny386 only needs monotonic guest timing. Keep it inside the module so the
 * emulator does not need additional OS imports. */
int clock_gettime(clockid_t clock_id, struct timespec *value)
{
    (void)clock_id;
    if (!value) {
        errno = EINVAL;
        return -1;
    }
    uint64_t now = module_host && module_host->time_us
        ? module_host->time_us(module_host->context) : 0;
    value->tv_sec = (time_t)(now / 1000000ULL);
    value->tv_nsec = (long)((now % 1000000ULL) * 1000ULL);
    return 0;
}

int usleep(useconds_t usec)
{
    (void)usec;
    return 0;
}

int load_rom(void *physical_memory, const char *path, uword address,
             int backward)
{
    FILE *file = tiny386_fopen(path, "rb");
    if (!file || fseek(file, 0, SEEK_END) != 0) {
        if (file) tiny386_fclose(file);
        return -1;
    }
    long length = ftell(file);
    if (length <= 0 || length > 1024L * 1024L ||
        (backward && (uword)length > address) ||
        fseek(file, 0, SEEK_SET) != 0) {
        tiny386_fclose(file);
        return -1;
    }
    uint8_t *destination = (uint8_t *)physical_memory + address;
    if (backward) destination -= length;
    size_t read_bytes = fread(destination, 1, (size_t)length, file);
    tiny386_fclose(file);
    return read_bytes == (size_t)length ? (int)length : -1;
}

static bool file_exists(const char *path)
{
    FILE *file = tiny386_fopen(path, "rb");
    if (!file) return false;
    tiny386_fclose(file);
    return true;
}

static bool resolve_path(const char **field)
{
    if (!field || !*field || !(*field)[0] || strstr(*field, "..")) {
        return false;
    }
    const char *value = *field;
    char resolved[256];
    if (strncmp(value, TINY386_ROOT "/", sizeof(TINY386_ROOT)) == 0) {
        if (strlen(value) >= sizeof(resolved)) return false;
        strcpy(resolved, value);
    } else {
        if (strncmp(value, "/sdcard/", 8) == 0) {
            const char *leaf = strrchr(value, '/');
            value = leaf ? leaf + 1 : value;
        }
        int written = snprintf(resolved, sizeof(resolved), "%s/%s",
                               TINY386_ROOT, value);
        if (written <= 0 || (size_t)written >= sizeof(resolved)) return false;
    }
    if (!file_exists(resolved)) return false;
    char *replacement = tiny386_strdup(resolved);
    if (!replacement) return false;
    tiny386_free((void *)*field);
    *field = replacement;
    return true;
}

static bool resolve_optional_path(const char **field)
{
    return !field || !*field || !(*field)[0] || resolve_path(field);
}

static bool prepare_configuration(const char *relative_ini)
{
    if (!relative_ini || strstr(relative_ini, "..") ||
        strncmp(relative_ini, TINY386_RELATIVE_ROOT,
                sizeof(TINY386_RELATIVE_ROOT) - 1U) != 0) {
        return false;
    }
    int written = snprintf(config_path, sizeof(config_path), "/sdcard/%s",
                           relative_ini);
    if (written <= 0 || (size_t)written >= sizeof(config_path)) return false;

    memset(&pc_config, 0, sizeof(pc_config));
    pc_config.mem_size = TINY386_DEFAULT_RAM;
    pc_config.vga_mem_size = 256L * 1024L;
    pc_config.width = TINY386_WIDTH;
    pc_config.height = TINY386_HEIGHT;
    pc_config.cpu_gen = 3;
    pc_config.fpu = 0;
    if (ini_parse(config_path, parse_conf_ini, &pc_config) != 0) return false;

    pc_config.width = TINY386_WIDTH;
    pc_config.height = TINY386_HEIGHT;
    pc_config.fpu = 0;
    if (pc_config.cpu_gen < 3 || pc_config.cpu_gen > 6) pc_config.cpu_gen = 3;
    if (pc_config.mem_size < TINY386_MIN_RAM) {
        pc_config.mem_size = TINY386_MIN_RAM;
    } else if (pc_config.mem_size > TINY386_MAX_RAM) {
        pc_config.mem_size = TINY386_MAX_RAM;
    }
    if (pc_config.vga_mem_size < 256L * 1024L) {
        pc_config.vga_mem_size = 256L * 1024L;
    } else if (pc_config.vga_mem_size > 512L * 1024L) {
        pc_config.vga_mem_size = 512L * 1024L;
    }

    if (!resolve_path(&pc_config.bios) ||
        !resolve_path(&pc_config.vga_bios)) {
        return false;
    }
    bool has_boot_media = false;
    for (size_t i = 0; i < 4; ++i) {
        if (pc_config.disks[i] && pc_config.disks[i][0]) {
            if (!resolve_path(&pc_config.disks[i])) return false;
            has_boot_media = true;
        }
    }
    for (size_t i = 0; i < 2; ++i) {
        if (pc_config.fdd[i] && pc_config.fdd[i][0]) {
            if (!resolve_path(&pc_config.fdd[i])) return false;
            has_boot_media = true;
        }
    }
    if (!resolve_optional_path(&pc_config.kernel) ||
        !resolve_optional_path(&pc_config.initrd) ||
        !resolve_optional_path(&pc_config.linuxstart)) {
        return false;
    }
    if (pc_config.kernel && pc_config.kernel[0]) has_boot_media = true;
    if (!has_boot_media) return false;

    /* Fail before pc_new() if the requested guest memory cannot be provided as
     * one PSRAM-only allocation alongside VGA/device overhead. */
    size_t probe_size = (size_t)pc_config.mem_size +
                        (size_t)pc_config.vga_mem_size + 512U * 1024U;
    void *probe = tiny386_malloc(probe_size);
    if (!probe) return false;
    tiny386_free(probe);
    return true;
}

static void redraw(void *opaque, int x, int y, int width, int height)
{
    (void)opaque;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    display_dirty = true;
}

static void update_key(uint32_t buttons, uint32_t changed,
                       uint32_t button, int linux_keycode)
{
    if ((changed & button) && pc && pc->kbd) {
        ps2_put_keycode(pc->kbd, (buttons & button) != 0, linux_keycode);
    }
}

static void update_input(uint32_t buttons)
{
    uint32_t changed = buttons ^ previous_buttons;
    update_key(buttons, changed, IDREESWATCH_BUTTON_UP, 103);
    update_key(buttons, changed, IDREESWATCH_BUTTON_DOWN, 108);
    update_key(buttons, changed, IDREESWATCH_BUTTON_LEFT, 105);
    update_key(buttons, changed, IDREESWATCH_BUTTON_RIGHT, 106);
    update_key(buttons, changed, IDREESWATCH_BUTTON_A, 28);      /* Enter */
    update_key(buttons, changed, IDREESWATCH_BUTTON_B, 1);       /* Escape */
    update_key(buttons, changed, IDREESWATCH_BUTTON_SELECT, 15); /* Tab */
    update_key(buttons, changed, IDREESWATCH_BUTTON_START, 29);  /* Ctrl */
    previous_buttons = buttons;
}

static void close_tracked_files(void)
{
    for (size_t i = 0; i < TINY386_MAX_FILES; ++i) {
        if (open_files[i]) {
            FILE *file = open_files[i];
            open_files[i] = NULL;
            fclose(file);
        }
    }
}

static void release_allocations(void)
{
    while (allocations && module_host && module_host->deallocate) {
        allocation_header_t *next = allocations->metadata.next;
        module_host->deallocate(module_host->context, allocations);
        allocations = next;
    }
}

int32_t tiny386_module_start(const idreeswatch_host_v1_t *host,
                             const idreeswatch_launch_v1_t *launch)
{
    if (!host || host->struct_size < sizeof(*host) || !host->allocate ||
        !host->deallocate || !host->time_us || !launch ||
        launch->struct_size < sizeof(*launch) || !launch->content_name) {
        return -1;
    }
    module_host = host;
    if (!prepare_configuration(launch->content_name)) {
        module_log(IDREESWATCH_LOG_ERROR,
                   "Tiny386 setup missing INI, BIOS, VGA BIOS, or boot disk");
        close_tracked_files();
        release_allocations();
        module_host = NULL;
        return -2;
    }
    started = true;
    module_log(IDREESWATCH_LOG_INFO,
               "Tiny386 configured; guest RAM and heap are PSRAM-only");
    return 0;
}

int32_t tiny386_module_run_frame(const idreeswatch_input_v1_t *input,
                                 idreeswatch_frame_v1_t *frame)
{
    if (!started || !input || !frame || !frame->pixels ||
        frame->width != TINY386_WIDTH || frame->height != TINY386_HEIGHT ||
        frame->stride_pixels < TINY386_WIDTH) {
        return -1;
    }
    if (!pc) {
        video_buffer = frame->pixels;
        memset(video_buffer, 0, TINY386_WIDTH * TINY386_HEIGHT *
                                sizeof(uint16_t));
        pc = pc_new(redraw, NULL, (uint8_t *)video_buffer, &pc_config);
        if (!pc) return -2;
        load_bios_and_reset(pc);
        pc->boot_start_time = get_uticks();
        display_dirty = true;
        module_log(IDREESWATCH_LOG_INFO, "Tiny386 PC powered on");
    } else if (video_buffer != frame->pixels) {
        return -3;
    }

    update_input(input->buttons);
    const size_t pointer_input_size =
        offsetof(idreeswatch_input_v1_t, pointer_wheel) +
        sizeof(input->pointer_wheel);
    if (input->struct_size >= pointer_input_size) {
        if (input->key_sequence && input->key_code && pc->kbd) {
            ps2_put_keycode(pc->kbd, input->key_pressed, input->key_code);
        }
        if (pc->mouse && (input->pointer_dx || input->pointer_dy ||
                          input->pointer_wheel ||
                          input->pointer_buttons != previous_pointer_buttons)) {
            ps2_mouse_event(pc->mouse, input->pointer_dx, input->pointer_dy,
                            input->pointer_wheel, input->pointer_buttons);
            previous_pointer_buttons = input->pointer_buttons;
        }
    }
    uint64_t started_us = module_host->time_us(module_host->context);
    do {
        if (pc->shutdown_state == 8) break;
        pc_step(pc);
    } while (module_host->time_us(module_host->context) - started_us <
             TINY386_CPU_BUDGET_US);
    pc_vga_step(pc);

    frame->video_ready = frame->video_requested && display_dirty;
    if (frame->video_ready) display_dirty = false;
    frame->audio_frame_count = 0;
    frame->frame_duration_us = TINY386_FRAME_US;
    return 0;
}

int32_t tiny386_module_command(idreeswatch_module_command_t command,
                               int32_t argument)
{
    (void)argument;
    if (!started || !pc) return -1;
    if (command == IDREESWATCH_MODULE_COMMAND_RESET) {
        load_bios_and_reset(pc);
        display_dirty = true;
        return 0;
    }
    return -2;
}

void tiny386_module_stop(void)
{
    started = false;
    pc = NULL;
    video_buffer = NULL;
    previous_buttons = 0;
    previous_pointer_buttons = 0;
    close_tracked_files();
    release_allocations();
    module_log(IDREESWATCH_LOG_INFO, "Tiny386 PC powered off");
    module_host = NULL;
}
