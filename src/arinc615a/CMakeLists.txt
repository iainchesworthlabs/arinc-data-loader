# The ARINC 615A library: the TFTP packets and the protocol files of ARINC 615A-3, written
# into and read out of bytes. It is GUI-free and platform-free: nothing here includes an
# operating system header, and it never sends or receives a packet itself; the engine
# that does is a later stage, over src/platforms.
#
# It is written to run on an embedded target: every encoder writes into a span the
# caller supplies and every decoder returns views over the bytes it was given, so nothing
# is allocated, no iostream is used, failures are std::expected rather than exceptions,
# and nothing needs RTTI. It uses adl::arinc665 for the check value numbers only; it does
# not build an ARINC 665 model.

add_library(adl_arinc615a STATIC
    src/checksum.cpp
    src/data_loader.cpp
    src/data_loader_operations.cpp
    src/find.cpp
    src/find_client.cpp
    src/protocol_files.cpp
    src/tftp.cpp
    src/tftp_transfer.cpp
    src/wire.cpp)
add_library(adl::arinc615a ALIAS adl_arinc615a)

target_include_directories(adl_arinc615a PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_arinc615a PUBLIC cxx_std_23)
target_link_libraries(adl_arinc615a
    PUBLIC
        adl::arinc665
    PRIVATE
        adl::tracy
        adl::warnings
        adl::sanitizers
        adl::coverage)
