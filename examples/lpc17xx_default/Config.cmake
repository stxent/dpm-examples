# Copyright (C) 2022 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set family name
set(FAMILY "LPC")
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
