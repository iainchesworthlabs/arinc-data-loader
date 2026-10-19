# Starts one of the built application programs offscreen and holds it open for three seconds.
# A program that cannot create its window (a QML error, a missing module, a missing Qt plugin)
# exits at once, so the watchdog firing is the pass and any early exit is the failure.
#
#   cmake -DAPP=<path to the program> -DWORKING_DIR=<its directory>
#         -DQT_PLUGIN_PATH=<the Qt kit's plugins> -P smoke_app.cmake

if(NOT APP OR NOT WORKING_DIR)
    message(FATAL_ERROR "Pass -DAPP=<program> and -DWORKING_DIR=<directory>.")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "QT_QUICK_CONTROLS_STYLE=Fusion"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
        "${APP}"
    WORKING_DIRECTORY "${WORKING_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 3)

if(NOT result MATCHES "timeout")
    message(FATAL_ERROR
        "${APP} exited instead of staying open (${result}).\n"
        "stdout:\n${output}\nstderr:\n${error}")
endif()

message(STATUS "${APP} started and stayed up for three seconds")
