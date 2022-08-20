# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set platform type
set(PLATFORM "LPC17XX")

# Flash memory settings
math(EXPR FLASH_LENGTH "512 * 1024")
math(EXPR FLASH_ORIGIN "0x00000000")

if(USE_DFU)
    math(EXPR DFU_LENGTH "32 * 1024")
    math(EXPR FLASH_LENGTH "${FLASH_LENGTH} - ${DFU_LENGTH}")
    math(EXPR FLASH_ORIGIN "${FLASH_ORIGIN} + ${DFU_LENGTH}")
endif()
