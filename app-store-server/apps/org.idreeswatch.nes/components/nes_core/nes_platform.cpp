/* Allocation, logging, and audio shims for the Anemoia module. */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../../../../sdk/include/idreeswatch_module.h"
#include "SD.h"
#include "core/cartridge.h"
#include "driver/i2s.h"

typedef union allocation_header allocation_header_t;
union allocation_header {
    struct {
        allocation_header_t *previous;
        allocation_header_t *next;
        size_t size;
    } metadata;
    max_align_t alignment;
};

static const idreeswatch_host_v1_t *platform_host;
static allocation_header_t *allocations;
static int16_t audio_staging[1024];
static size_t audio_count;

SDClass SD;

void nes_platform_set_host(const idreeswatch_host_v1_t *host)
{
    platform_host = host;
}

void nes_platform_release_allocations(void)
{
    while (allocations && platform_host && platform_host->deallocate) {
        allocation_header_t *next = allocations->metadata.next;
        platform_host->deallocate(platform_host->context, allocations);
        allocations = next;
    }
    platform_host = nullptr;
}

void nes_audio_begin_frame(void)
{
    audio_count = 0;
}

size_t nes_audio_copy(int16_t *destination, size_t capacity,
                      uint32_t *dropped)
{
    size_t count = audio_count < capacity ? audio_count : capacity;
    if (dropped && audio_count > count) {
        *dropped += static_cast<uint32_t>(audio_count - count);
    }
    if (!destination || count == 0) return 0;
    memcpy(destination, audio_staging, count * sizeof(int16_t));
    return count;
}

extern "C" void *malloc(size_t size) noexcept
{
    if (!platform_host || !platform_host->allocate || size == 0 ||
        size > SIZE_MAX - sizeof(allocation_header_t)) {
        return nullptr;
    }
    allocation_header_t *header = static_cast<allocation_header_t *>(
        platform_host->allocate(platform_host->context,
                                sizeof(*header) + size, 16));
    if (!header) return nullptr;
    header->metadata.previous = nullptr;
    header->metadata.next = allocations;
    header->metadata.size = size;
    if (allocations) allocations->metadata.previous = header;
    allocations = header;
    return header + 1;
}

extern "C" void free(void *memory) noexcept
{
    if (!memory || !platform_host || !platform_host->deallocate) return;
    allocation_header_t *header = static_cast<allocation_header_t *>(memory) - 1;
    if (header->metadata.previous) {
        header->metadata.previous->metadata.next = header->metadata.next;
    } else {
        allocations = header->metadata.next;
    }
    if (header->metadata.next) {
        header->metadata.next->metadata.previous = header->metadata.previous;
    }
    platform_host->deallocate(platform_host->context, header);
}

extern "C" void *calloc(size_t count, size_t size) noexcept
{
    if (count && size > SIZE_MAX / count) return nullptr;
    size_t total = count * size;
    void *memory = malloc(total);
    if (memory) memset(memory, 0, total);
    return memory;
}

extern "C" void *realloc(void *memory, size_t size) noexcept
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

extern "C" char *strdup(const char *text) noexcept
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
    if (platform_host && platform_host->log) {
        platform_host->log(platform_host->context,
                           IDREESWATCH_LOG_DEBUG, buffer);
    }
    return result;
}

/* The standalone core writes interleaved unsigned stereo. Capture one channel
 * and center it into the host's signed mono PCM frame. */
extern "C" esp_err_t i2s_write(i2s_port_t, const void *source, size_t bytes,
                               size_t *bytes_written, TickType_t)
{
    const uint16_t *samples = static_cast<const uint16_t *>(source);
    size_t stereo_frames = bytes / (2U * sizeof(uint16_t));
    size_t capacity = (sizeof(audio_staging) / sizeof(audio_staging[0])) -
                      audio_count;
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
