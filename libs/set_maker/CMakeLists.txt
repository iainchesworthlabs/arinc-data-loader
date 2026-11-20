# SPDX-License-Identifier: GPL-3.0-or-later
# The generator's logic without a window and without Qt: what a person decides about a media set (a
# plan), the rules that a plan has to follow, the making of the set (read the files, build the load and
# the set, write the tree beside its place, read it back with the loader's own check, move it into place),
# and the reading of a set on disk into the facts a manifest needs. The generator's window
# (apps/media_gen) and the command adl_make_set (apps/make_set) both build on it.
#
# Not part of the adl package and not installed: it is the implementation of the generator, not an
# interface for others to build on. Platform-free: it uses std::filesystem and the libraries below.

add_library(adl_set_maker STATIC
    include/adl/set_maker/inspect.hpp
    include/adl/set_maker/make.hpp
    include/adl/set_maker/plan.hpp
    include/adl/set_maker/validate.hpp
    src/inspect.cpp
    src/make.cpp
    src/plan.cpp
    src/validate.cpp)
add_library(adl::set_maker ALIAS adl_set_maker)

target_include_directories(adl_set_maker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(adl_set_maker PUBLIC cxx_std_23)
target_link_libraries(adl_set_maker
    PUBLIC
        adl::arinc665
        adl::loader_core
        adl::media_tree
    PRIVATE
        adl::warnings
        adl::sanitizers
        adl::coverage)

# The libFuzzer harnesses for what this library reads (a set of members) and for the description file of
# adl_make_set are kept in fuzz/, since the fuzzing workflows look for harnesses and seeds in libs/*/fuzz.
# They are added from apps/make_set, where the reader and the writer of the document they use are.
