# Copyright (C) 2024 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set family name
set(FAMILY "LPC")
# Set platform type
set(PLATFORM "LPC43XX")

# Memory regions for all parts
math(EXPR MEMORY_ADDRESS_SPIFI "0x14000000")
math(EXPR MEMORY_ADDRESS_SRAM0 "0x10000000")
math(EXPR MEMORY_ADDRESS_SRAM1 "0x10080000")
math(EXPR MEMORY_SIZE_SPIFI "4 * 1024 * 1024")
# Flash-based parts
math(EXPR MEMORY_ADDRESS_FLASH "0x1A000000")
math(EXPR MEMORY_SIZE_FLASH "256 * 1024")
math(EXPR MEMORY_SIZE_SRAM0 "32 * 1024")
math(EXPR MEMORY_SIZE_SRAM1 "40 * 1024")
# Flash-less parts except for LPC4310 and LPC4320
math(EXPR MEMORY_SIZE_SRAM0_FLASHLESS "96 * 1024")
math(EXPR MEMORY_SIZE_SRAM1_FLASHLESS "72 * 1024")

# Linker script settings
if(TARGET_NOR OR TARGET_SRAM)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
        # Use debug builds on flash-less parts only
        math(EXPR EXE_LENGTH "${MEMORY_SIZE_SRAM1_FLASHLESS}")
        math(EXPR ROM_LENGTH "${MEMORY_SIZE_SRAM0_FLASHLESS}")
    else()
        math(EXPR EXE_LENGTH "${MEMORY_SIZE_SRAM1}")
        math(EXPR ROM_LENGTH "${MEMORY_SIZE_SRAM0}")
    endif()
    math(EXPR EXE_ORIGIN "${MEMORY_ADDRESS_SRAM1}")
    math(EXPR ROM_ORIGIN "${MEMORY_ADDRESS_SRAM0}")

    if(TARGET_SRAM)
        # User firmware will be placed into the first sectors of NOR Flash
        set(BUNDLE_DEFS "-DCONFIG_FIRST_STAGE")
    endif()
else()
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
        math(EXPR ROM_LENGTH "64 * 1024")
    else()
        math(EXPR ROM_LENGTH "32 * 1024")
    endif()
    math(EXPR ROM_ORIGIN "${MEMORY_ADDRESS_FLASH}")

    math(EXPR EXE_LENGTH "${MEMORY_SIZE_SRAM0}")
    math(EXPR EXE_ORIGIN "${MEMORY_ADDRESS_SRAM0}")
endif()
