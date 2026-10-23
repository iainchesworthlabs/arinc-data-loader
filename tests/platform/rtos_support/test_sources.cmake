# SPDX-License-Identifier: GPL-3.0-or-later
# The test sources the RTOS test programs share (tests/platform/zephyr, tests/platform/freertos):
# the same files tests/CMakeLists.txt builds into adl_platform_tests and adl_engine_tests for the
# desktop platforms, listed here for the programs that cannot use that file because they are
# projects of their own. Include this after setting ADL_REPO_ROOT.
#
#   ADL_RTOS_PLATFORM_TEST_SOURCES  the platform layer: its portable types, the fake platform and
#                                   the conformance suite, which runs against the fake and
#                                   against the real host the program was built with
#   ADL_RTOS_ENGINE_TEST_SOURCES    the 615A engine, runner and whole operations, against a
#                                   simulated target on the fake platform
#   ADL_RTOS_TEST_INCLUDES          the include directories those need; rtos_support comes first
#                                   so that its temp_dir.hpp stands in for tests/support's
#
# A test added to tests/CMakeLists.txt's lists belongs here too.
set(_adl_tests "${ADL_REPO_ROOT}/tests")

set(ADL_RTOS_PLATFORM_TEST_SOURCES
    ${_adl_tests}/conformance/clock_memory_test.cpp
    ${_adl_tests}/conformance/files_test.cpp
    ${_adl_tests}/conformance/network_test.cpp
    ${_adl_tests}/conformance/threads_ipc_test.cpp
    ${_adl_tests}/unit/fake_platform_test.cpp
    ${_adl_tests}/unit/memory_test.cpp
    ${_adl_tests}/unit/platform_types_test.cpp
    ${_adl_tests}/fake/fake_host.cpp
    ${_adl_tests}/fake/fake_network.cpp
    ${_adl_tests}/fake/fake_storage.cpp
    ${_adl_tests}/fake/fake_threading.cpp
    ${_adl_tests}/fake/fake_world.cpp)

set(ADL_RTOS_ENGINE_TEST_SOURCES
    ${_adl_tests}/engine/data_loader_test.cpp
    ${_adl_tests}/engine/directory_files_test.cpp
    ${_adl_tests}/engine/find_client_test.cpp
    ${_adl_tests}/engine/operations_test.cpp
    ${_adl_tests}/engine/tftp_transfer_test.cpp
    ${_adl_tests}/engine/sim_target.cpp)

set(ADL_RTOS_TEST_INCLUDES
    "${_adl_tests}/platform/rtos_support"
    "${_adl_tests}/support"
    "${_adl_tests}/conformance"
    "${_adl_tests}/fake"
    "${_adl_tests}/engine")

unset(_adl_tests)
