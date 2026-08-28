if(CMAKE_BUILD_EARLY_EXPANSION)
    idf_component_register(REQUIRES driver esp_partition)
    return()
endif()

set(ANEMOIA_COMMIT d5ca83fd19041247b05d4991e9d8fc6629078332)
set(ANEMOIA_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/anemoia")
set(ANEMOIA_FILES
    LICENSE
    config.h
    src/ControllerTypes.h
    src/debug.h
    src/flash_mmap.h
    src/profiler.h
    src/core/apu2A03.cpp src/core/apu2A03.h
    src/core/bus.cpp src/core/bus.h
    src/core/cartridge.cpp src/core/cartridge.h
    src/core/cpu6502.cpp src/core/cpu6502.h
    src/core/mapper.cpp src/core/mapper.h
    src/core/mirror_mode.h
    src/core/ppu2C02.cpp src/core/ppu2C02.h src/core/ppu_palettes.h
    src/core/rom_backends.h src/core/rom_types.h
    src/core/mappers/mapper000.cpp src/core/mappers/mapper000.h
    src/core/mappers/mapper001.cpp src/core/mappers/mapper001.h
    src/core/mappers/mapper002.cpp src/core/mappers/mapper002.h
    src/core/mappers/mapper003.cpp src/core/mappers/mapper003.h
    src/core/mappers/mapper004.cpp src/core/mappers/mapper004.h
    src/core/mappers/mapper069.cpp src/core/mappers/mapper069.h
)

foreach(ANEMOIA_FILE IN LISTS ANEMOIA_FILES)
    set(ANEMOIA_LOCAL "${ANEMOIA_SOURCE_DIR}/${ANEMOIA_FILE}")
    if(NOT EXISTS "${ANEMOIA_LOCAL}")
        get_filename_component(ANEMOIA_LOCAL_DIR "${ANEMOIA_LOCAL}" DIRECTORY)
        file(MAKE_DIRECTORY "${ANEMOIA_LOCAL_DIR}")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/Shim06/Anemoia-ESP32/${ANEMOIA_COMMIT}/${ANEMOIA_FILE}"
            "${ANEMOIA_LOCAL}"
            TLS_VERIFY ON
            STATUS ANEMOIA_DOWNLOAD_STATUS
        )
        list(GET ANEMOIA_DOWNLOAD_STATUS 0 ANEMOIA_DOWNLOAD_CODE)
        if(NOT ANEMOIA_DOWNLOAD_CODE EQUAL 0)
            list(GET ANEMOIA_DOWNLOAD_STATUS 1 ANEMOIA_DOWNLOAD_MESSAGE)
            message(FATAL_ERROR "Could not fetch ${ANEMOIA_FILE}: ${ANEMOIA_DOWNLOAD_MESSAGE}")
        endif()
    endif()
endforeach()

set(ANEMOIA_CORE_DIR "${ANEMOIA_SOURCE_DIR}/src/core")
set(ANEMOIA_SOURCES)
foreach(ANEMOIA_FILE IN LISTS ANEMOIA_FILES)
    if(ANEMOIA_FILE MATCHES "\\.cpp$")
        list(APPEND ANEMOIA_SOURCES "${ANEMOIA_SOURCE_DIR}/${ANEMOIA_FILE}")
    endif()
endforeach()

idf_component_register(
    SRCS "nes_module.cpp" ${ANEMOIA_SOURCES}
    INCLUDE_DIRS
        "."
        "compat"
        "${ANEMOIA_SOURCE_DIR}"
        "${ANEMOIA_SOURCE_DIR}/src"
        "${ANEMOIA_CORE_DIR}"
    REQUIRES driver esp_partition
)

# The watch panel cannot present a full 256x224 frame at the source core's
# 60 Hz SPI cadence. Anemoia's FRAMESKIP keeps CPU/APU timing at 60 Hz but
# renders every other PPU frame, giving the host a stable 30 Hz stream.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    OPTIMIZATION_FLAGS=1
    FRAMESKIP=1
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Ofast
    -fPIC
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -Wno-array-bounds
    -Wno-error=sequence-point
    -Wno-unused-function
    -Wno-unused-parameter
)
