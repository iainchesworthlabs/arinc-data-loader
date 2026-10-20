# The 615A runner: the one piece of the 615A library that calls the platform. It opens the
# sockets the engine asks for, waits on them from one thread, and hands the datagrams and the
# time to adl::arinc615a's state machines (the DataLoader and the FindClient) and carries out what
# they answer. It also has the FileSource and FileSink that put a load's files on, and the
# target's files in, the platform's file system. Nothing else in the 615A library includes a
# platform header.

add_library(adl_arinc615a_runner STATIC
    src/directory_files.cpp
    src/runner.cpp)
add_library(adl::arinc615a_runner ALIAS adl_arinc615a_runner)

target_include_directories(adl_arinc615a_runner PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_arinc615a_runner PUBLIC cxx_std_23)
target_link_libraries(adl_arinc615a_runner
    PUBLIC
        adl::arinc615a
        adl::platform
    PRIVATE
        adl::tracy
        adl::warnings
        adl::sanitizers
        adl::coverage)
