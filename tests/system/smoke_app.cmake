# Starts one of the built application programs offscreen with --smoke-test and requires that it
# creates its window: the program draws its first frame, prints "smoke-test: first frame drawn"
# and exits 0. A program that cannot create its window (a QML error, a missing module, a missing
# Qt plugin) exits at once with another code, and one that never draws a frame runs into the
# time-out, so both fail. The program's settings are redirected (ADL_SETTINGS_DIR) into a
# directory under the build tree, so the run never touches the real registry key or
# configuration file; the test also requires that the program wrote its settings there, which is
# what shows the redirection took effect.
#
#   cmake -DAPP=<path to the program> -DWORKING_DIR=<its directory>
#         -DQT_PLUGIN_PATH=<the Qt kit's plugins> -DSETTINGS_DIR=<scratch directory>
#         -P smoke_app.cmake

if(NOT APP OR NOT WORKING_DIR OR NOT SETTINGS_DIR)
    message(FATAL_ERROR "Pass -DAPP=<program>, -DWORKING_DIR=<directory> and -DSETTINGS_DIR=<scratch>.")
endif()

# A fresh directory every run: what is found in it afterwards was written by this run.
file(REMOVE_RECURSE "${SETTINGS_DIR}")
file(MAKE_DIRECTORY "${SETTINGS_DIR}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "QT_QPA_PLATFORM=offscreen"
        "QT_QUICK_BACKEND=software"
        "QT_QUICK_CONTROLS_STYLE=Fusion"
        "QT_PLUGIN_PATH=${QT_PLUGIN_PATH}"
        "ADL_SETTINGS_DIR=${SETTINGS_DIR}"
        "${APP}" --smoke-test
    WORKING_DIRECTORY "${WORKING_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 25)

if(NOT result STREQUAL "0")
    message(FATAL_ERROR
        "${APP} did not draw its window and exit cleanly (result: ${result}).\n"
        "stdout:\n${output}\nstderr:\n${error}")
endif()
if(NOT output MATCHES "smoke-test: first frame drawn")
    message(FATAL_ERROR
        "${APP} exited 0 without reporting that its window drew a frame.\n"
        "stdout:\n${output}\nstderr:\n${error}")
endif()

file(GLOB_RECURSE written "${SETTINGS_DIR}/*.ini")
if(NOT written)
    message(FATAL_ERROR
        "${APP} wrote no settings under ${SETTINGS_DIR}: ADL_SETTINGS_DIR was not honoured, so "
        "the program may have written to the real settings.")
endif()

message(STATUS "${APP} drew its first frame and left; settings: ${written}")
