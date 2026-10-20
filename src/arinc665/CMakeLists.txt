# The ARINC 665 library: the media set, load and integrity-check logic. It is
# GUI-free and platform-free: nothing here includes an operating system header,
# and what differs between operating systems arrives through
# src/platforms/interfaces.
#
# It is written with the embedded target on the roadmap in mind: failures are
# std::expected rather than exceptions, the CRCs are computed from compile-time
# tables, and nothing is created by a static initialiser. Not everything avoids
# the heap yet (PartNumber holds a std::string), so that is a direction, not a
# guarantee.

add_library(adl_arinc665 STATIC
    src/batch_file.cpp
    src/header_file.cpp
    src/integrity_checker.cpp
    src/load.cpp
    src/md5.cpp
    src/media_files.cpp
    src/media_label.cpp
    src/media_set.cpp
    src/naming.cpp
    src/part_number.cpp
    src/sha1.cpp
    src/wire.cpp)
add_library(adl::arinc665 ALIAS adl_arinc665)

target_include_directories(adl_arinc665 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_arinc665 PUBLIC cxx_std_23)
target_link_libraries(adl_arinc665
    PRIVATE
        adl::tracy
        adl::warnings
        adl::sanitizers
        adl::coverage)
