# Copyright (C) 2026 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set family name
set(FAMILY "STM32")
# Set platform type
set(PLATFORM "STM32F4XX")

# Available memory regions
math(EXPR ADDRESS_FLASH "0x08000000")

# Linker script settings
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" OR "${CMAKE_BUILD_TYPE}" STREQUAL "")
    math(EXPR ROM_LENGTH "64 * 1024")
else()
    math(EXPR ROM_LENGTH "32 * 1024")
endif()
math(EXPR ROM_ORIGIN "${ADDRESS_FLASH}")
