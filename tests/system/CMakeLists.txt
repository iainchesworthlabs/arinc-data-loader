# System tests: the programs a user runs, started as processes rather than linked into a test.
# The Qt applications are started offscreen and must still be running when the watchdog stops
# them; an application that cannot load its QML exits at once, which is what this catches. The
# deeper end-to-end behaviour is in tests/ui (the window driven through Qt Quick Test), the
# loopback integration test (a whole 615A operation over real sockets) and tests/app's QML test.

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
            -P "${CMAKE_CURRENT_SOURCE_DIR}/smoke_app.cmake")
    set_tests_properties(system.${program}.starts PROPERTIES
        TIMEOUT 30
        LABELS "system"
        ENVIRONMENT "TEMP=${CMAKE_BINARY_DIR};TMP=${CMAKE_BINARY_DIR};TMPDIR=${CMAKE_BINARY_DIR}")
endforeach()
