# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set family name
set(FAMILY "LPC")
# Set platform type
set(PLATFORM "LPC17XX")

# Available memory regions
math(EXPR ADDRESS_FLASH "0x00000000")

# Linker script settings
if(USE_DFU)
    set(DFU_LENGTH 16384)
else()
    set(DFU_LENGTH 0)
endif()

math(EXPR ROM_LENGTH "512 * 1024 - ${DFU_LENGTH}")
math(EXPR ROM_ORIGIN "${ADDRESS_FLASH} + ${DFU_LENGTH}")

# Define template list
set(TEMPLATES_LIST
        display_tft
        display_tft_spi
        i2c_m24
        sensor_ds18b20
        sensor_mpu6050
        sensor_ms5607
        sensor_sht20
        sensor_xpt2046
        systick
)
