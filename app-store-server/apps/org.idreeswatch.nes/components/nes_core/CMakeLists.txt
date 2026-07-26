file(GLOB_RECURSE NOFRENDO_SOURCES
    "nofrendo/*.c"
)

idf_component_register(
    SRCS
        "nes_module.c"
        ${NOFRENDO_SOURCES}
    INCLUDE_DIRS
        "."
        "nofrendo"
        "nofrendo/nes"
        "nofrendo/mappers"
        "../../../../../sdk/include"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -fPIC
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    -fno-builtin
    -Wno-array-bounds
    -Wno-format
    -Wno-unused-function
)
