# The Qt-Core part of the shared interface code, as a library the three applications' cores link:
# Message, a phrase translated when it is read, so what a model holds in the operator's language
# follows a language switch. The rest of libs/ui (the QML components, fonts, window state, language
# manager) is not a library: cmake/AdlUi.cmake adds it to each application target.

find_package(Qt6 6.12 REQUIRED COMPONENTS Core)

add_library(adl_ui_message STATIC
    include/adl/ui/message.hpp
    message.cpp)
add_library(adl::ui_message ALIAS adl_ui_message)

target_include_directories(adl_ui_message PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_ui_message PUBLIC cxx_std_23)
# The same as the applications' cores: Qt's keyword macros would clash with the platform layer's.
target_compile_definitions(adl_ui_message PUBLIC QT_NO_KEYWORDS)
target_link_libraries(adl_ui_message
    PUBLIC
        Qt6::Core
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)
