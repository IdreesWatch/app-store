cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(idreeswatch_tiny386)

set(ELF_LIBS
    "esp-idf/tiny386_core/libtiny386_core.a"
)
include(elf_loader)
project_so(idreeswatch_tiny386)
