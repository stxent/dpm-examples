Installation
------------

Examples require GNU toolchain for ARM Cortex-M processors and CMake version 3.13 or newer.

Quickstart
----------

Clone git repository:

```sh
git clone https://github.com/stxent/dpm-examples.git
cd dpm-examples
git submodule update --init --recursive
```

Build project with CMake:

```sh
mkdir build
cd build
cmake ..
make
```

Useful settings
---------------

* CMAKE_BUILD_TYPE — specifies the build type.
* USE_LTO — enables Link Time Optimization.
