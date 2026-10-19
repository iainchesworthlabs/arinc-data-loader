# A smoke test of the Zephyr platform layer on a Cortex-M3 (mps2/an385) under QEMU, built with the
# Zephyr SDK's GCC and picolibc, without exceptions. See src/main.cpp for what it checks and
# tools/ci/zephyr_qemu_smoke.sh for how it is built and run. The repository is a Zephyr module
# (zephyr/module.yml); CONFIG_ADL_LIBRARIES also links the 665 and 615A libraries, so that they are
# built for the board too.
cmake_minimum_required(VERSION 3.20.0)

get_filename_component(ADL_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
list(APPEND ZEPHYR_EXTRA_MODULES "${ADL_REPO_ROOT}")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(adl_zephyr_smoke LANGUAGES C CXX)

target_sources(app PRIVATE src/main.cpp)
target_link_libraries(app PRIVATE
    adl::platform
    adl::arinc615a_runner
    adl::arinc615a
    adl::arinc665)
