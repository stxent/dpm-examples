# Copyright (C) 2023 xent
# Project is distributed under the terms of the GNU General Public License v3.0

# Set family name
set(FAMILY "NUMICRO")
# Set platform type
set(PLATFORM "M48X")

# Available memory regions
math(EXPR ADDRESS_FLASH "0x00000000")
math(EXPR ADDRESS_SDRAM "0x60000000")
math(EXPR ADDRESS_SPIM  "0x08000000")

# Linker script settings
if(TARGET_NOR)
    if(USE_DFU)
        set(DFU_LENGTH 131072)
    else()
        set(DFU_LENGTH 0)
    endif()

    math(EXPR ROM_LENGTH "4 * 1024 * 1024 - ${DFU_LENGTH}")
    math(EXPR ROM_ORIGIN "${ADDRESS_SPIM} + ${DFU_LENGTH}")
    set(DISABLE_LITERAL_POOL ON)
elseif(TARGET_SDRAM)
    math(EXPR ROM_LENGTH "4 * 1024 * 1024")
    math(EXPR ROM_ORIGIN "${ADDRESS_SDRAM}")
    set(DISABLE_LITERAL_POOL ON)
else()
    if(USE_DFU)
        set(DFU_LENGTH 32768)
    else()
        set(DFU_LENGTH 0)
    endif()

    math(EXPR ROM_LENGTH "256 * 1024 - ${DFU_LENGTH}")
    math(EXPR ROM_ORIGIN "${ADDRESS_FLASH} + ${DFU_LENGTH}")
endif()

set(BUNDLE_LIBS "m")

# Define template list
set(TEMPLATES_LIST
        spim_w25:USE_DTR=true
        systick
)
