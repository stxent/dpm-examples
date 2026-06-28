# Copyright (C) 2026 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set family name
set(FAMILY "STM32")
# Set platform type
set(PLATFORM "STM32F4XX")

# Memory regions for all parts
math(EXPR MEMORY_ADDRESS_FLASH "0x08000000")
math(EXPR MEMORY_SIZE_FLASH "512 * 1024")

if(USE_DFU)
    set(DFU_LENGTH 32768)
else()
    set(DFU_LENGTH 0)
endif()

math(EXPR ROM_LENGTH "${MEMORY_SIZE_FLASH} - ${DFU_LENGTH}")
math(EXPR ROM_ORIGIN "${MEMORY_ADDRESS_FLASH} + ${DFU_LENGTH}")

set(BUNDLE_LIBS "m")

# Define template list
set(TEMPLATES_LIST
        spi_w25q
        spi_mx35
        systick
)
