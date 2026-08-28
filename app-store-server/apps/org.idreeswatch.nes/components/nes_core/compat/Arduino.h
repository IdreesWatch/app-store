#ifndef IDREESWATCH_ANEMOIA_ARDUINO_COMPAT_H
#define IDREESWATCH_ANEMOIA_ARDUINO_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"

#undef IRAM_ATTR
#undef DMA_ATTR
#undef DRAM_ATTR
#define IRAM_ATTR
#define DMA_ATTR
#define DRAM_ATTR

#endif
