if(CMAKE_BUILD_EARLY_EXPANSION)
    idf_component_register(REQUIRES freertos esp_timer)
    return()
endif()

set(DOOM_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../../components/doomgeneric")
file(GLOB DOOM_SOURCES "${DOOM_SOURCE_DIR}/*.c")
list(REMOVE_ITEM DOOM_SOURCES
    "${DOOM_SOURCE_DIR}/doomgeneric_allegro.c"
    "${DOOM_SOURCE_DIR}/doomgeneric_emscripten.c"
    "${DOOM_SOURCE_DIR}/doomgeneric_stratus.c"
)

idf_component_register(
    SRCS "doom_module.c" ${DOOM_SOURCES}
    INCLUDE_DIRS
        "."
        "${DOOM_SOURCE_DIR}"
    REQUIRES freertos esp_timer
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    DOOMGENERIC_RESX=320
    DOOMGENERIC_RESY=200
    ESP_PLATFORM=1
    malloc=doom_module_malloc
    calloc=doom_module_calloc
    realloc=doom_module_realloc
    free=doom_module_free
    strdup=doom_module_strdup
    heap_caps_malloc=doom_module_heap_malloc
    heap_caps_free=doom_module_free
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -fPIC
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    -Wno-all
    -Wno-extra
    -Wno-incompatible-pointer-types
    -Wno-int-conversion
    -Wno-pointer-sign
    -Wno-implicit-function-declaration
    -Wno-format
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-unused-function
    -Wno-missing-braces
    -Wno-sign-compare
    -Wno-char-subscripts
    -Wno-discarded-qualifiers
    -Wno-write-strings
)
