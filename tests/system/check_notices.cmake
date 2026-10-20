# The notices the build places beside the executables are the ones About > Licences shows (they
# are embedded from the same file): check that the file is there, names what the build ships, and
# carries the text of the terms - the LGPL for Qt, the OFL for the typeface - and no placeholder.
#
#   cmake -DBIN_DIR=<bin> -DQT_VERSION=<x.y.z> [-DEXPECTED="<text>|<text>"] -P check_notices.cmake
#
# EXPECTED is what the platform adds (ADL_PLATFORM_NOTICE_EXPECTED), separated by "|".

foreach(_required BIN_DIR QT_VERSION)
    if(NOT ${_required})
        message(FATAL_ERROR "Pass -D${_required}=<value>.")
    endif()
endforeach()

foreach(_file NOTICES.txt LICENSE.txt)
    if(NOT EXISTS "${BIN_DIR}/${_file}")
        message(FATAL_ERROR "${_file} is not beside the executables in ${BIN_DIR}")
    endif()
endforeach()

file(READ "${BIN_DIR}/NOTICES.txt" _notices)
file(READ "${BIN_DIR}/LICENSE.txt" _licence)

set(_expected
    "Qt ${QT_VERSION}"
    "GNU LESSER GENERAL PUBLIC LICENSE"
    "SIL OPEN FONT LICENSE"
    "Archivo"
    "version 3 or later"
    "Aeronautical Radio, Inc.")
if(EXPECTED)
    string(REPLACE "|" ";" _platform_expected "${EXPECTED}")
    list(APPEND _expected ${_platform_expected})
endif()
foreach(_text IN LISTS _expected)
    string(FIND "${_notices}" "${_text}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR "NOTICES.txt does not contain '${_text}'")
    endif()
endforeach()

string(FIND "${_notices}" "{{" _token)
if(NOT _token EQUAL -1)
    message(FATAL_ERROR "NOTICES.txt still contains a {{placeholder}}")
endif()
string(FIND "${_licence}" "GNU GENERAL PUBLIC LICENSE" _gpl)
if(_gpl EQUAL -1)
    message(FATAL_ERROR "LICENSE.txt is not the GPL")
endif()
message(STATUS "NOTICES.txt and LICENSE.txt are in place")
