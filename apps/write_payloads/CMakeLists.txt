add_executable(adl_write_payloads main.cpp)
target_link_libraries(adl_write_payloads
    PRIVATE
        adl::arinc665
        adl::media_tree
        adl::samples
        adl::warnings
        adl::sanitizers
        adl::coverage)

adl_deploy_sanitizer_runtime(adl_write_payloads)

# A command-line tool for people who work with media sets: it ships with the programs.
if(ADL_PLATFORM_DESKTOP)
    install(TARGETS adl_write_payloads
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT runtime)
endif()
