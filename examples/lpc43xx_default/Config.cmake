# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set family name
set(FAMILY "LPC")
# Set platform type
set(PLATFORM "LPC43XX")

# Memory regions for all parts
math(EXPR MEMORY_ADDRESS_SDRAM "0x28000000")
math(EXPR MEMORY_ADDRESS_SPIFI "0x14000000")
math(EXPR MEMORY_ADDRESS_SRAM0 "0x10000000")
math(EXPR MEMORY_SIZE_SDRAM "4 * 1024 * 1024")
math(EXPR MEMORY_SIZE_SPIFI "4 * 1024 * 1024")
# Flash-based parts
math(EXPR MEMORY_ADDRESS_FLASH "0x1A000000")
math(EXPR MEMORY_SIZE_FLASH "256 * 1024")
math(EXPR MEMORY_SIZE_SRAM0 "32 * 1024")
# Flash-less parts
math(EXPR MEMORY_SIZE_SRAM0_FLASHLESS "96 * 1024")

# Linker script settings
if(TARGET_NOR)
    if(USE_DFU)
        set(DFU_LENGTH 131072)
    else()
        set(DFU_LENGTH 0)
    endif()

    math(EXPR ROM_LENGTH "${MEMORY_SIZE_SPIFI} - ${DFU_LENGTH}")
    math(EXPR ROM_ORIGIN "${MEMORY_ADDRESS_SPIFI} + ${DFU_LENGTH}")
    set(DISABLE_LITERAL_POOL ON)
elseif(TARGET_SDRAM)
    math(EXPR ROM_LENGTH "${MEMORY_SIZE_SDRAM}")
    math(EXPR ROM_ORIGIN "${MEMORY_ADDRESS_SDRAM}")
    set(DISABLE_LITERAL_POOL ON)
elseif(TARGET_SRAM)
    math(EXPR ROM_LENGTH "${MEMORY_SIZE_SRAM0_FLASHLESS}")
    math(EXPR ROM_ORIGIN "${MEMORY_ADDRESS_SRAM0}")
else()
    if(USE_DFU)
        set(DFU_LENGTH 32768)
    else()
        set(DFU_LENGTH 0)
    endif()

    math(EXPR ROM_LENGTH "${MEMORY_SIZE_FLASH} - ${DFU_LENGTH}")
    math(EXPR ROM_ORIGIN "${MEMORY_ADDRESS_FLASH} + ${DFU_LENGTH}")
endif()

set(BUNDLE_LIBS "m")

# Define template list
set(TEMPLATES_LIST
        attitude_dcm
        display_tft
        display_tft_spi
        i2c_m24
        sensor_complex
        sensor_hmc5883
        sensor_mpu6000
        sensor_mpu6000_spi=sensor_mpu6000:USE_SPI=true
        sensor_ms5607
        sensor_ms5607_spi=sensor_ms5607:USE_SPI=true
        sensor_sht20
        sensor_xpt2046
        spi_w25
        spim_w25
        systick
)
