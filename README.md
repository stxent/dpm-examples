# DPM Examples

Examples demonstrating the usage of drivers from the DPM library.

## Overview

`dpm-examples` is a collection of usage examples for drivers from the
DPM library. The examples target platforms with the richest set of interfaces
and memory resources, so they do not cover every available driver. Reference
linker scripts are included.

## Project Structure

The project is organized as a two-level CMake build: the top-level script
sequentially invokes lower-level build scripts for each platform.

Key directories:

* `examples/` — contains individual directories for different platforms
  and a shared `helpers/` directory used during builds for all variants.
  The per-platform directories often include board-specific initialization code
  and pin configuration examples.
* `templates/` — holds Jinja2-based templates used to generate
  platform-specific C code when the corresponding template is enabled.
* `tools/` — includes auxiliary build scripts and utilities.
* `docker/` — contains a sample Dockerfile for creating a build container.

## Components

The project provides the following general-purpose examples:

* USB DFU bootloader for loading firmware of other projects
* Displays with serial and parallel interfaces
* GNSS receivers
* Radio chips
* Touch-screen controllers
* I2C EEPROM
* SPI NOR flash and NAND flash
* NOR flash with XIP (eXecute In Place)
* Button drivers
* LED strips
* Accelerometers, gyroscopes, magnetometers, temperature, pressure,
  and humidity sensors
* Comprehensive sensor-based orientation estimation example

## USB DFU Bootloader Variants

The project offers multiple USB DFU bootloader builds tailored to different
memory topologies, with availability depending on the platform.

* **Embedded flash bootloader** (LPC17xx, LPC43xx, STM32F4xx, M48x):
  target firmware is loaded into and runs from internal flash.
* **Internal SRAM bootloader** (LPC43xx): target firmware is loaded into
  and runs from internal SRAM.
* **External NOR flash bootloader** (LPC43xx, M48x): target firmware is loaded
  into and runs from external NOR flash.
* **External SRAM bootloader** (STM32F4xx): target firmware is loaded into
  and runs from external SRAM.
* **External SDRAM bootloader** (LPC43xx): target firmware is loaded into
  and runs from external SDRAM.

For LPC43xx’s M0 core, three additional builds are available: booting from
internal flash, internal SRAM, or external NOR flash.

## Requirements

To build the project, the following tools and packages are required:

* **GCC 13 or newer** — the GNU Compiler Collection, required for
  building the x86 version.
* **ARM GCC 13 or newer** — Arm GNU Toolchain, required for
  Cortex-M embedded targets.
* **RISC-V GCC 13 or newer** — RISC-V GNU Toolchain, required for
  RISC-V embedded targets.
* **CMake 3.21 or newer** — used for configuring and generating build
  systems across platforms.
* **Python 3.6+** with the following packages:
  * `jinja2` — for rendering code templates
  * `kconfiglib` — for generating Kconfig configuration files

## Build Examples

1. Clone the repository

```sh
git clone https://github.com/stxent/dpm-examples.git
cd dpm-examples
git submodule update --init --recursive
```

2. Standard Build

```sh
mkdir build
cd build
cmake ..
make
```

## Build Options

The following build options control the build behavior:

* **CMAKE_BUILD_TYPE** — Specifies the build type. Possible values:
  empty, `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel`.
* **KCONFIG_DEFCONFIG** — Command used as a Kconfig file generator. This allows
  you to specify a custom `defconfig` command for generating initial Kconfig
  configuration files.
* **USE_BIN** — Convert executables to Binary format.
* **USE_HEX** — Convert executables to Intel HEX format.
* **USE_DFU** — Enable memory layout compatible with a bootloader.
* **USE_LTO** — Enable Link Time Optimization.
* **TARGET_NOR** — Use external NOR flash for the artifact if available.
  This option modifies the linker script to place code and data in external
  NOR memory regions.
* **TARGET_SDRAM** — Use external SDRAM for the artifact if available.
* **TARGET_SRAM** — Use internal or external SRAM for the artifact if available.
