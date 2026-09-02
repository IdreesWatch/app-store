set(ANEMOIA_PPU_HEADER "${ANEMOIA_SOURCE_DIR}/src/core/ppu2C02.h")
set(ANEMOIA_PPU_SOURCE "${ANEMOIA_SOURCE_DIR}/src/core/ppu2C02.cpp")
set(ANEMOIA_CPU_SOURCE "${ANEMOIA_SOURCE_DIR}/src/core/cpu6502.cpp")
set(ANEMOIA_CPU_HEADER "${ANEMOIA_SOURCE_DIR}/src/core/cpu6502.h")
set(ANEMOIA_CONFIG_HEADER "${ANEMOIA_SOURCE_DIR}/config.h")

# The module hands native RGB565 to LVGL. Upstream's TFT configuration stores
# its palette byte-swapped for SPI pushPixels(), which forced the adapter to
# swap every output pixel back. Select the native palette once at configure
# time so completed scanlines can use memcpy.
file(READ "${ANEMOIA_CONFIG_HEADER}" ANEMOIA_CONFIG_TEXT)
string(REPLACE
    "    #define SCREEN_SWAP_BYTES        // Uncomment if colors appear wrong"
    "    // Native RGB565 is required by the IdreesWatch module host."
    ANEMOIA_CONFIG_TEXT
    "${ANEMOIA_CONFIG_TEXT}"
)
file(WRITE "${ANEMOIA_CONFIG_HEADER}" "${ANEMOIA_CONFIG_TEXT}")

# Remove fields from earlier local patch revisions so this stays idempotent.
file(READ "${ANEMOIA_PPU_HEADER}" ANEMOIA_PPU_HEADER_TEXT)
string(REPLACE
    "\n    void setRenderFrame(bool enabled) { render_frame = enabled; }"
    ""
    ANEMOIA_PPU_HEADER_TEXT
    "${ANEMOIA_PPU_HEADER_TEXT}"
)
string(REPLACE
    "\n    bool render_frame = true;"
    ""
    ANEMOIA_PPU_HEADER_TEXT
    "${ANEMOIA_PPU_HEADER_TEXT}"
)
file(WRITE "${ANEMOIA_PPU_HEADER}" "${ANEMOIA_PPU_HEADER_TEXT}")

file(READ "${ANEMOIA_PPU_SOURCE}" ANEMOIA_PPU_SOURCE_TEXT)
string(REPLACE
    "extern \"C\" bool idreeswatch_nes_render_frame;\n"
    ""
    ANEMOIA_PPU_SOURCE_TEXT
    "${ANEMOIA_PPU_SOURCE_TEXT}"
)

set(ANEMOIA_RENDER_BEGIN_IRAM
    "IRAM_ATTR void Ppu2C02::renderScanline(uint16_t current_scanline)\n{")
set(ANEMOIA_RENDER_BEGIN_TEXT
    "void Ppu2C02::renderScanline(uint16_t current_scanline)\n{")
set(ANEMOIA_RENDER_END
    "\n}\n\ninline void Ppu2C02::transferScroll()")

string(FIND "${ANEMOIA_PPU_SOURCE_TEXT}"
    "${ANEMOIA_RENDER_BEGIN_IRAM}" ANEMOIA_RENDER_BEGIN_OFFSET)
if(ANEMOIA_RENDER_BEGIN_OFFSET LESS 0)
    string(FIND "${ANEMOIA_PPU_SOURCE_TEXT}"
        "${ANEMOIA_RENDER_BEGIN_TEXT}" ANEMOIA_RENDER_BEGIN_OFFSET)
endif()
string(FIND "${ANEMOIA_PPU_SOURCE_TEXT}"
    "${ANEMOIA_RENDER_END}" ANEMOIA_RENDER_END_OFFSET)
if(ANEMOIA_RENDER_BEGIN_OFFSET LESS 0 OR ANEMOIA_RENDER_END_OFFSET LESS 0)
    message(FATAL_ERROR "Pinned Anemoia renderScanline implementation changed")
endif()

string(LENGTH "${ANEMOIA_RENDER_END}" ANEMOIA_RENDER_END_LENGTH)
math(EXPR ANEMOIA_RENDER_SUFFIX_OFFSET
    "${ANEMOIA_RENDER_END_OFFSET} + ${ANEMOIA_RENDER_END_LENGTH}")
string(SUBSTRING "${ANEMOIA_PPU_SOURCE_TEXT}" 0
    ${ANEMOIA_RENDER_BEGIN_OFFSET} ANEMOIA_PPU_PREFIX)
string(SUBSTRING "${ANEMOIA_PPU_SOURCE_TEXT}"
    ${ANEMOIA_RENDER_SUFFIX_OFFSET} -1 ANEMOIA_PPU_SUFFIX)

set(ANEMOIA_RENDER_REPLACEMENT
[=[IRAM_ATTR void Ppu2C02::renderScanline(uint16_t current_scanline)
{
    scanline = current_scanline;
    if (!draw_callback)
    {
        // Use the core's own frameskip path so mapper IRQ and sprite-zero
        // timing remain compatible while pixel generation is omitted.
        fakeSpriteHit(current_scanline);
        return;
    }
    transferScroll();
    renderBackground();
    renderSprites();
    incrementY();
    finishScanline();
}

inline void Ppu2C02::transferScroll()]=])

file(WRITE "${ANEMOIA_PPU_SOURCE}"
    "${ANEMOIA_PPU_PREFIX}${ANEMOIA_RENDER_REPLACEMENT}${ANEMOIA_PPU_SUFFIX}")

# Restore the upstream CPU entry attribute if an earlier configure removed it.
file(READ "${ANEMOIA_CPU_SOURCE}" ANEMOIA_CPU_SOURCE_TEXT)
if(NOT ANEMOIA_CPU_SOURCE_TEXT MATCHES
   "IRAM_ATTR void Cpu6502::clockFrame\\(\\)")
    string(REPLACE
        "void Cpu6502::clockFrame()"
        "IRAM_ATTR void Cpu6502::clockFrame()"
        ANEMOIA_CPU_SOURCE_TEXT
        "${ANEMOIA_CPU_SOURCE_TEXT}"
    )
endif()
file(WRITE "${ANEMOIA_CPU_SOURCE}" "${ANEMOIA_CPU_SOURCE_TEXT}")

# The upstream APU exposes a complete mixer and PCM writer, but its CPU loop
# never advances the APU clock. Clock it at half the CPU rate, matching the
# NES APU divider and the 44.1 kHz sample accumulator used by apu2A03.cpp.
# Keep the phase in the CPU object so separate clock() calls retain timing.
file(READ "${ANEMOIA_CPU_HEADER}" ANEMOIA_CPU_HEADER_TEXT)
if(NOT ANEMOIA_CPU_HEADER_TEXT MATCHES "idreeswatch_apu_phase")
    string(REPLACE
        [=[    uint16_t OAM_DMA_page = 0x00;]=]
        [=[    uint16_t OAM_DMA_page = 0x00;
    uint8_t idreeswatch_apu_phase = 0;]=]
        ANEMOIA_CPU_HEADER_TEXT
        "${ANEMOIA_CPU_HEADER_TEXT}"
    )
    file(WRITE "${ANEMOIA_CPU_HEADER}" "${ANEMOIA_CPU_HEADER_TEXT}")
endif()

if(NOT ANEMOIA_CPU_SOURCE_TEXT MATCHES "idreeswatch_apu_phase")
    set(ANEMOIA_CPU_CLOCK_PREFIX
[=[    for (int remaining_cycles = i; remaining_cycles > 0; remaining_cycles--)
    {
        if (cycles > 0)]=])
    set(ANEMOIA_CPU_CLOCK_REPLACEMENT
[=[    for (int remaining_cycles = i; remaining_cycles > 0; remaining_cycles--)
    {
        if (idreeswatch_apu_phase == 0) apu.clock();
        idreeswatch_apu_phase ^= 1;
        if (cycles > 0)]=])
    string(REPLACE
        "${ANEMOIA_CPU_CLOCK_PREFIX}"
        "${ANEMOIA_CPU_CLOCK_REPLACEMENT}"
        ANEMOIA_CPU_SOURCE_TEXT
        "${ANEMOIA_CPU_SOURCE_TEXT}"
    )
    string(REPLACE
        "    apu.reset();"
        "    apu.reset();\n    idreeswatch_apu_phase = 0;"
        ANEMOIA_CPU_SOURCE_TEXT
        "${ANEMOIA_CPU_SOURCE_TEXT}"
    )
    file(WRITE "${ANEMOIA_CPU_SOURCE}" "${ANEMOIA_CPU_SOURCE_TEXT}")
endif()
