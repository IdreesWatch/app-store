//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions.
//  ESP32-S3 Stratus: Pre-loads entire WAD into PSRAM to avoid
//  SPI bus contention between SD card reads and display flushes.
//

#include <stdio.h>
#include <string.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_log.h"
static const char *TAG = "wad_psram";
#endif

typedef struct
{
    wad_file_t wad;
    FILE *fstream;       // Only used during initial load, then closed
#ifdef ESP_PLATFORM
    uint8_t *psram_data; // Entire WAD loaded into PSRAM
#endif
} stdc_wad_file_t;

extern wad_file_class_t stdc_wad_file;

static wad_file_t *W_StdC_OpenFile(char *path)
{
    stdc_wad_file_t *result;
    FILE *fstream;

    fstream = fopen(path, "rb");

    if (fstream == NULL)
    {
        return NULL;
    }


    result = Z_Malloc(sizeof(stdc_wad_file_t), PU_STATIC, 0);
    result->wad.file_class = &stdc_wad_file;
    result->wad.mapped = NULL;
    result->wad.length = M_FileLength(fstream);
    result->fstream = fstream;

#ifdef ESP_PLATFORM
    // Print PSRAM stats to see why allocation fails
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t max_block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM Stats: Free=%u bytes, Max Contiguous=%u bytes, Requested=%u bytes", 
             (unsigned)free_psram, (unsigned)max_block, result->wad.length);

    // Provide an option to allocate in chunks if contiguous is not enough
    // But since `mapped` pointer expects contiguous, we *must* allocate contiguous.
    result->psram_data = heap_caps_malloc(result->wad.length, MALLOC_CAP_SPIRAM);
    
    if (result->psram_data != NULL) {
        ESP_LOGI(TAG, "Loading WAD into PSRAM (%u bytes)...", result->wad.length);
        
        size_t total_read = 0;
        size_t chunk_size = 32768;  // 32KB chunks
        
        while (total_read < result->wad.length) {
            size_t to_read = result->wad.length - total_read;
            if (to_read > chunk_size) to_read = chunk_size;
            
            size_t bytes_read = fread(result->psram_data + total_read, 1, to_read, fstream);
            if (bytes_read == 0) break;
            total_read += bytes_read;
        }
        
        if (total_read == result->wad.length) {
            ESP_LOGI(TAG, "WAD loaded into PSRAM successfully (%u bytes)", result->wad.length);
            result->wad.mapped = result->psram_data;
            // Close the file - we don't need the SD card anymore!
            fclose(fstream);
            result->fstream = NULL;
        } else {
            ESP_LOGE(TAG, "WAD PSRAM load failed (read %u of %u)", 
                     (unsigned)total_read, result->wad.length);
            heap_caps_free(result->psram_data);
            result->psram_data = NULL;
        }
    } else {
        ESP_LOGW(TAG, "Not enough contiguous PSRAM for WAD. Free=%u, MaxBlock=%u, Req=%u", 
                 (unsigned)free_psram, (unsigned)max_block, result->wad.length);
        result->psram_data = NULL;
    }
#endif

    return &result->wad;
}

static void W_StdC_CloseFile(wad_file_t *wad)
{
    stdc_wad_file_t *stdc_wad;

    stdc_wad = (stdc_wad_file_t *) wad;

#ifdef ESP_PLATFORM
    if (stdc_wad->psram_data) {
        heap_caps_free(stdc_wad->psram_data);
        stdc_wad->psram_data = NULL;
    }
#endif

    if (stdc_wad->fstream) {
        fclose(stdc_wad->fstream);
    }
    Z_Free(stdc_wad);
}

// provided buffer.  Returns the number of bytes read.

size_t W_StdC_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    stdc_wad_file_t *stdc_wad;

    stdc_wad = (stdc_wad_file_t *) wad;

#ifdef ESP_PLATFORM
    // If WAD is loaded in PSRAM, read from memory (no SPI!)
    if (stdc_wad->psram_data) {
        if (offset + buffer_len > wad->length) {
            buffer_len = wad->length - offset;
        }
        memcpy(buffer, stdc_wad->psram_data + offset, buffer_len);
        return buffer_len;
    }
#endif

    // Fallback: read from file (SD card)
    if (stdc_wad->fstream) {
        fseek(stdc_wad->fstream, offset, SEEK_SET);
        return fread(buffer, 1, buffer_len, stdc_wad->fstream);
    }

    return 0;
}


wad_file_class_t stdc_wad_file = 
{
    W_StdC_OpenFile,
    W_StdC_CloseFile,
    W_StdC_Read,
};
