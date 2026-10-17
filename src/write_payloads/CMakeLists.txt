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
