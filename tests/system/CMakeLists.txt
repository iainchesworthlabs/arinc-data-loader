# System tests: the programs a user runs, started as processes rather than linked into a test.
# The Qt applications are started offscreen with --smoke-test and must draw their window's first
# frame and exit 0; an application that cannot load its QML exits at once with an error, and one
# that never shows its window runs into the time-out. Their settings go to a directory under the
# build tree (ADL_SETTINGS_DIR), never the real registry key or configuration file. The deeper
# end-to-end behaviour is in tests/ui (the window driven through Qt Quick Test), the loopback
# integration test (a whole 615A operation over real sockets) and apps/app/tests's QML test.

if(NOT ADL_BUILD_APP)
    return()
endif()

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

foreach(program adl_app adl_media_gen adl_mock_target)
    add_test(NAME system.${program}.starts
        COMMAND "${CMAKE_COMMAND}"
            "-DAPP=$<TARGET_FILE:${program}>"
            "-DWORKING_DIR=$<TARGET_FILE_DIR:${program}>"
            "-DQT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}"
            "-DSETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/${program}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/smoke_app.cmake")
    set_tests_properties(system.${program}.starts PROPERTIES
        TIMEOUT 40
        LABELS "system"
        ENVIRONMENT "TEMP=${CMAKE_BINARY_DIR};TMP=${CMAKE_BINARY_DIR};TMPDIR=${CMAKE_BINARY_DIR}")
endforeach()

# The licences travel with the build: NOTICES.txt (what it ships from others, with their terms)
# and LICENSE.txt beside the executables. About > Licences shows the same file, embedded.
add_test(NAME system.notices
    COMMAND "${CMAKE_COMMAND}"
        "-DBIN_DIR=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        "-DQT_VERSION=${Qt6_VERSION}"
        "-DEXPECTED=${ADL_PLATFORM_NOTICE_EXPECTED}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_notices.cmake")
set_tests_properties(system.notices PROPERTIES TIMEOUT 30 LABELS "system")
