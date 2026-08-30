if(CMAKE_BUILD_EARLY_EXPANSION)
    idf_component_register()
    return()
endif()

set(TINY386_COMMIT 239bf87b842e75ffc342af7bdafc814b87f63284)
set(TINY386_SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/tiny386")
set(TINY386_FILES
    LICENSE
    adlib.c adlib.h
    i386.c i386.h i386ins.def simd.inc
    i8042.c i8042.h
    i8254.c i8254.h
    i8257.c i8257.h
    i8259.c i8259.h
    ide.c ide.h
    ini.c ini.h
    misc.c misc.h
    ne2000.c ne2000.h
    pc.c pc.h
    pci.c pci.h
    pcspk.c pcspk.h
    sb16.c sb16.h
    vga.c vga.h
)

foreach(TINY386_FILE IN LISTS TINY386_FILES)
    set(TINY386_LOCAL "${TINY386_SOURCE_DIR}/${TINY386_FILE}")
    if(NOT EXISTS "${TINY386_LOCAL}")
        get_filename_component(TINY386_LOCAL_DIR "${TINY386_LOCAL}" DIRECTORY)
        file(MAKE_DIRECTORY "${TINY386_LOCAL_DIR}")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/hchunhui/tiny386/${TINY386_COMMIT}/${TINY386_FILE}"
            "${TINY386_LOCAL}"
            TLS_VERIFY ON
            STATUS TINY386_DOWNLOAD_STATUS
        )
        list(GET TINY386_DOWNLOAD_STATUS 0 TINY386_DOWNLOAD_CODE)
        if(NOT TINY386_DOWNLOAD_CODE EQUAL 0)
            list(GET TINY386_DOWNLOAD_STATUS 1 TINY386_DOWNLOAD_MESSAGE)
            message(FATAL_ERROR
                "Could not fetch ${TINY386_FILE}: ${TINY386_DOWNLOAD_MESSAGE}")
        endif()
    endif()
endforeach()

set(TINY386_SOURCES)
foreach(TINY386_FILE IN LISTS TINY386_FILES)
    if(TINY386_FILE MATCHES "\\.c$")
        list(APPEND TINY386_SOURCES "${TINY386_SOURCE_DIR}/${TINY386_FILE}")
    endif()
endforeach()

idf_component_register(
    SRCS "tiny386_module.c" ${TINY386_SOURCES}
    INCLUDE_DIRS "." "${TINY386_SOURCE_DIR}"
)

# The watch owns storage, display, timing, and memory. The core receives a
# portable C shim and never links board drivers into the downloadable module.
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    BPP=16
    FULL_UPDATE=1
    malloc=tiny386_malloc
    calloc=tiny386_calloc
    realloc=tiny386_realloc
    free=tiny386_free
    strdup=tiny386_strdup
    fopen=tiny386_fopen
    fclose=tiny386_fclose
)

# Avoid host-terminal dependencies while retaining the normal Xtensa CPU core.
set_source_files_properties(
    "${TINY386_SOURCE_DIR}/misc.c"
    "${TINY386_SOURCE_DIR}/pc.c"
    PROPERTIES COMPILE_DEFINITIONS "__wasm__=1"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -fPIC
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    -Wno-all
    -Wno-error
    -Wno-unused-function
    -Wno-unused-parameter
)

# Keep the two largest translation units compact enough for Xtensa's shared
# object relocation pass while retaining optimization on the CPU hot path.
set_source_files_properties(
    "${TINY386_SOURCE_DIR}/i386.c"
    "${TINY386_SOURCE_DIR}/vga.c"
    PROPERTIES COMPILE_OPTIONS "-O2;-fno-inline-functions"
)
