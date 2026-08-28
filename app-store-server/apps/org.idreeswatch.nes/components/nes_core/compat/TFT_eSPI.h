#ifndef IDREESWATCH_ANEMOIA_TFT_COMPAT_H
#define IDREESWATCH_ANEMOIA_TFT_COMPAT_H

#include <stdint.h>

class TFT_eSPI
{
public:
    void pushPixelsDMA(uint16_t *, uint32_t) {}
    void pushPixels(uint16_t *, uint32_t) {}
};

#endif
