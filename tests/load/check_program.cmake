# SPDX-License-Identifier: GPL-3.0-or-later
# The built program adl_load, started as a process, for what needs no target: what it prints for --version
# and --help, the exit code and the stream of wrong usage, and the refusal of a media set that is not there,
# which comes after `started` and before any socket is used. The rest of the program is tested through its run
# function (tests/load); this is the one test that goes through main and the real standard output.
#
#   cmake -DPROGRAM=<adl_load> -DWORK_DIR=<scratch> -P tests/load/check_program.cmake
cmake_minimum_required(VERSION 3.28)

foreach(required PROGRAM WORK_DIR)
    if(NOT ${required})
        message(FATAL_ERROR "Pass -D${required}=...")
    endif()
endforeach()

# Runs the program with the arguments; sets <prefix>_code, <prefix>_out and <prefix>_err.
function(run_program prefix)
    execute_process(
        COMMAND "${PROGRAM}" ${ARGN}
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    string(REPLACE "\r\n" "\n" out "${out}")
    string(REPLACE "\r\n" "\n" err "${err}")
    set(${prefix}_code "${code}" PARENT_SCOPE)
    set(${prefix}_out "${out}" PARENT_SCOPE)
    set(${prefix}_err "${err}" PARENT_SCOPE)
endfunction()

# Fails with `what` unless the condition that follows it (the arguments of an if) is true.
function(expect what)
    if(${ARGN})
        return()
    endif()
    message(FATAL_ERROR "${what}")
endfunction()

run_program(version --version)
expect("--version exited with ${version_code}: ${version_err}" version_code EQUAL 0)
expect("--version printed \"${version_out}\"" version_out MATCHES "^adl_load [0-9][^ ]* events 1\n$")
expect("--version names the program with .exe" NOT version_out MATCHES "[.]exe")

run_program(help --help)
expect("--help exited with ${help_code}" help_code EQUAL 0)
expect("--help printed \"${help_out}\"" help_out MATCHES "^usage:\n" AND help_out MATCHES "adl_load download")
expect("--help wrote to standard error: ${help_err}" help_err MATCHES "^$")

run_program(nothing)
expect("no arguments exited with ${nothing_code}" nothing_code EQUAL 2)
expect("wrong usage wrote to standard output: ${nothing_out}" nothing_out MATCHES "^$")
expect("wrong usage printed \"${nothing_err}\"" nothing_err MATCHES "^adl_load: No command given")

run_program(no_events information --json --target 10.0.0.2)
expect("an Information operation without --position exited with ${no_events_code}" no_events_code EQUAL 2)
expect("wrong usage wrote events: ${no_events_out}" no_events_out MATCHES "^$")
expect("wrong usage printed \"${no_events_err}\"" no_events_err MATCHES "--position is missing")

# A media set that is not there: started, the reason, finished, exit code 3, and nothing sent.
run_program(missing upload --target 10.0.0.2 --position HNPFMS_L --media-set "${WORK_DIR}/no-such-media-set"
    --all-loads --json)
expect("an upload of a media set that is not there exited with ${missing_code}" missing_code EQUAL 3)
expect("the first event was not started: ${missing_out}"
    missing_out MATCHES "^{\"event\":\"started\",\"t\":[0-9]+[.][0-9][0-9][0-9],\"program\":\"adl_load\",")
expect("the last event was not finished with the reason: ${missing_out}"
    missing_out MATCHES "{\"event\":\"finished\",[^\n]*\"reason\":\"media_set_unreadable\",[^\n]*\"exit_code\":3}\n$")

message(STATUS "adl_load as a process: --version, --help, wrong usage and an unreadable media set are as they should be")
