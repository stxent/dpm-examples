# Copyright (C) 2024 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set family name
set(FAMILY "LPC")
# Set platform type
set(PLATFORM "LPC43XX")

# Available memory regions
math(EXPR ADDRESS_FLASH "0x1A000000")
math(EXPR ADDRESS_NOR   "0x14000000")
math(EXPR ADDRESS_SRAM0 "0x10000000")
math(EXPR ADDRESS_SRAM1 "0x10080000")

# Linker script settings
if(TARGET_NOR OR TARGET_SRAM)
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        # 96 KiB or 128 KiB RAM available on flashless parts only
        math(EXPR ROM_LENGTH "96 * 1024")
    else()
        # 32 KiB RAM available on all parts
        math(EXPR ROM_LENGTH "32 * 1024")
    endif()
    math(EXPR ROM_ORIGIN "${ADDRESS_SRAM0}")

    if(TARGET_SRAM)
        # User firmware will be placed into the first sectors of NOR Flash
        set(BUNDLE_DEFS "-DCONFIG_FIRST_STAGE")
    endif()

    math(EXPR EXE_ORIGIN "${ADDRESS_SRAM1}")
    math(EXPR EXE_LENGTH "72 * 1024")
else()
    if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        math(EXPR ROM_LENGTH "64 * 1024")
    else()
        math(EXPR ROM_LENGTH "32 * 1024")
    endif()
    math(EXPR ROM_ORIGIN "${ADDRESS_FLASH}")

    math(EXPR EXE_ORIGIN "${ADDRESS_SRAM0}")
    math(EXPR EXE_LENGTH "96 * 1024")
endif()
