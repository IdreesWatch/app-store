/*
 * ESP-IDF Stub Header for Emulator
 * This provides minimal type definitions for compiling ESP32 code to WASM
 */

#ifndef ESP_ERR_H
#define ESP_ERR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ESP-IDF error codes */
#define ESP_OK          0
#define ESP_FAIL        -1
#define ESP_ERR_NO_MEM  0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_INVALID_VERSION 0x10A
#define ESP_ERR_INVALID_MAC 0x10B
#define ESP_ERR_NOT_FINISHED 0x10C
#define ESP_ERR_NOT_ALLOWED 0x10D

typedef int32_t esp_err_t;

#define ESP_ERROR_CHECK(x) do { esp_err_t __err_rc = (x); } while(0)

#ifdef __cplusplus
}
#endif

#endif /* ESP_ERR_H */
