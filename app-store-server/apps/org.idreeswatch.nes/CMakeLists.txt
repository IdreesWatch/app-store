cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(idreeswatch_nes)

set(ELF_LIBS "esp-idf/nes_core/libnes_core.a")
include(elf_loader)
project_so(idreeswatch_nes)
