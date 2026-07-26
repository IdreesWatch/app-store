cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(idreeswatch_nes)

execute_process(
    COMMAND ${CMAKE_C_COMPILER} -print-libgcc-file-name
    OUTPUT_VARIABLE IDREESWATCH_LIBGCC
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
set(ELF_LIBS
    "esp-idf/nes_core/libnes_core.a"
    "${IDREESWATCH_LIBGCC}"
)
include(elf_loader)
project_so(idreeswatch_nes)
