# VcpkgRoot.cmake - decides which vcpkg checkout this configure uses.
#
# The repository pins vcpkg as a git submodule (deps/vcpkg) so a fresh clone
# and CI build against exactly the same scripts. A developer with several
# checkouts and worktrees does not want a copy of vcpkg in each of them, and
# must not point them all at one directory that something else might delete or
# move to another commit. This module picks the root, first match wins:
#
#   1. CMAKE_TOOLCHAIN_FILE given on the command line, in a preset or by an IDE:
#      used as it is. (A dead path gets a message that says what is dead.)
#   2. ADL_VCPKG_ROOT (CMake variable or environment): that directory.
#      A different commit from the pin is a warning, not an error.
#   3. ADL_VCPKG_STORE, else ICL_VCPKG_STORE (each a CMake variable or an
#      environment variable): a shared store. ICL_VCPKG_STORE is the one to set
#      per machine; every Iain Chesworth Labs repository that carries this
#      module reads it, and ADL_VCPKG_STORE overrides it for this one.
#      The commit the submodule pins gets its own checkout in
#      <store>/<first 12 of the commit>, made once from a shared clone of the
#      submodule's repository (<store>/mirror, or ADL_VCPKG_MIRROR /
#      ICL_VCPKG_MIRROR) with `git worktree add`. Every checkout, worktree and
#      repository that pins that commit reuses it; a new pin gets a new
#      directory, and nothing that already exists is ever moved or modified.
#      (There is deliberately no org-wide root: one directory cannot be at the
#      commit every repository pins.)
#   4. The deps/vcpkg submodule, initialised here if it is not checked out.
#
# The chosen root is left in ADL_VCPKG_ROOT_DIR (cache, internal).

include_guard(GLOBAL)

# The commit the superproject pins deps/vcpkg to ("" without git history).
function(_adl_vcpkg_pin source_dir out)
    set(${out} "" PARENT_SCOPE)
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        return()
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" ls-files --stage -- deps/vcpkg
        OUTPUT_VARIABLE entry
        RESULT_VARIABLE rc
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(rc EQUAL 0 AND entry MATCHES "^160000 ([0-9a-f]+) ")
        set(${out} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    endif()
endfunction()

function(_adl_vcpkg_head root out)
    set(${out} "" PARENT_SCOPE)
    find_package(Git QUIET)
    if(NOT GIT_FOUND)
        return()
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${root}" rev-parse HEAD
        OUTPUT_VARIABLE head
        RESULT_VARIABLE rc
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(rc EQUAL 0)
        set(${out} "${head}" PARENT_SCOPE)
    endif()
endfunction()

# Says what is wrong with a path that should hold vcpkg, as text for a message.
function(_adl_vcpkg_describe path out)
    set(text "${path} does not hold vcpkg.")
    if(IS_SYMLINK "${path}")
        file(READ_SYMLINK "${path}" target)
        set(text "${path} is a link to ${target}, which ")
        if(EXISTS "${target}")
            string(APPEND text "has no scripts/buildsystems/vcpkg.cmake in it.")
        else()
            string(APPEND text "does not exist. Whatever it pointed at has been deleted or moved.")
        endif()
    elseif(EXISTS "${path}")
        string(APPEND text " The directory exists but has no scripts/buildsystems/vcpkg.cmake.")
    else()
        string(APPEND text " It does not exist.")
    endif()
    set(${out} "${text}" PARENT_SCOPE)
endfunction()

function(_adl_vcpkg_fail)
    list(JOIN ARGV "" what)
    message(FATAL_ERROR
        "${what}\n"
        "\n"
        "Options, from the least to the most setup:\n"
        "  * git submodule update --init deps/vcpkg      (one copy per checkout, with its full history:\n"
        "    not --depth 1, since vcpkg reads the manifest baseline and the older port versions from it;\n"
        "    if deps/vcpkg is shallow already, git -C deps/vcpkg fetch --unshallow)\n"
        "  * set ICL_VCPKG_STORE to a directory shared by your checkouts and repositories (a\n"
        "    copy per pinned vcpkg commit, made on first use; see docs/building.md)\n"
        "  * set ADL_VCPKG_ROOT to a vcpkg checkout you manage yourself\n")
endfunction()

# Runs git in `dir`; leaves the exit code and output in _adl_git_rc and _adl_git_log.
function(_adl_vcpkg_git dir)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -c core.longpaths=true -C "${dir}" ${ARGN}
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE log
        ERROR_VARIABLE log)
    set(_adl_git_rc "${rc}" PARENT_SCOPE)
    set(_adl_git_log "${log}" PARENT_SCOPE)
endfunction()

# The first non-empty of the listed settings, each a CMake variable or an
# environment variable of the same name; "" when none is set.
function(_adl_vcpkg_setting out)
    set(${out} "" PARENT_SCOPE)
    foreach(name IN LISTS ARGN)
        if(DEFINED ${name} AND NOT "${${name}}" STREQUAL "")
            set(${out} "${${name}}" PARENT_SCOPE)
            return()
        elseif(DEFINED ENV{${name}} AND NOT "$ENV{${name}}" STREQUAL "")
            set(${out} "$ENV{${name}}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

# The checkout of `pin` in the shared store, created on first use.
function(_adl_vcpkg_store_checkout store source_dir pin out)
    if(NOT pin)
        _adl_vcpkg_fail(
            "A vcpkg store is set but the commit deps/vcpkg is pinned to cannot be read "
            "(is ${source_dir} a git checkout, and is git on the PATH?).")
    endif()
    string(SUBSTRING "${pin}" 0 12 short)
    set(dest "${store}/${short}")
    set(ready "${dest}/.adl-ready")
    if(EXISTS "${ready}")
        set(${out} "${dest}" PARENT_SCOPE)
        return()
    endif()

    _adl_vcpkg_setting(mirror ADL_VCPKG_MIRROR ICL_VCPKG_MIRROR)
    if(mirror STREQUAL "")
        set(mirror "${store}/mirror")
    else()
        file(TO_CMAKE_PATH "${mirror}" mirror)
    endif()

    find_package(Git REQUIRED)
    file(MAKE_DIRECTORY "${store}")
    message(STATUS "vcpkg ${short} is not in ${store} yet; taking the store lock")
    file(LOCK "${store}/.lock" GUARD FUNCTION TIMEOUT 3600 RESULT_VARIABLE lock_rc)
    if(NOT lock_rc STREQUAL "0")
        _adl_vcpkg_fail("Could not take ${store}/.lock (${lock_rc}); another configure may be creating the checkout.")
    endif()
    # Another configure may have finished while this one waited.
    if(EXISTS "${ready}")
        set(${out} "${dest}" PARENT_SCOPE)
        return()
    endif()

    # The shared clone of the fork the submodule points at.
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${source_dir}" config -f .gitmodules --get submodule.deps/vcpkg.url
        OUTPUT_VARIABLE url OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE url_rc)
    if(NOT url_rc EQUAL 0)
        set(url "")
    endif()
    if(NOT EXISTS "${mirror}/HEAD" AND NOT EXISTS "${mirror}/.git")
        if(url STREQUAL "")
            _adl_vcpkg_fail("No clone of vcpkg at ${mirror} and no URL for deps/vcpkg in .gitmodules to make one from.")
        endif()
        message(STATUS "Cloning ${url} to ${mirror} (once, for every checkout that shares ${store})")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -c core.longpaths=true clone --bare "${url}" "${mirror}"
            RESULT_VARIABLE rc ERROR_VARIABLE log OUTPUT_VARIABLE log)
        if(NOT rc EQUAL 0)
            _adl_vcpkg_fail("Cloning ${url} failed:\n${log}")
        endif()
        _adl_vcpkg_git("${mirror}" config remote.origin.fetch "+refs/heads/*:refs/heads/*")
    endif()

    _adl_vcpkg_git("${mirror}" cat-file -e "${pin}^{commit}")
    if(NOT _adl_git_rc EQUAL 0)
        message(STATUS "Fetching ${pin} into ${mirror}")
        _adl_vcpkg_git("${mirror}" fetch origin)
        _adl_vcpkg_git("${mirror}" cat-file -e "${pin}^{commit}")
        if(NOT _adl_git_rc EQUAL 0 AND NOT url STREQUAL "")
            # The mirror's origin is a different repository from the one this
            # checkout pins (another repository's fork, say): take the
            # branches of the pinned one too. They land under refs/fetched/ so
            # they cannot collide with the mirror's own branches.
            message(STATUS "Fetching ${url} into ${mirror}")
            _adl_vcpkg_git("${mirror}" fetch "${url}" "+refs/heads/*:refs/fetched/*")
            _adl_vcpkg_git("${mirror}" cat-file -e "${pin}^{commit}")
        endif()
        if(NOT _adl_git_rc EQUAL 0)
            _adl_vcpkg_fail("Commit ${pin} (pinned by deps/vcpkg) is not in ${mirror}, even after fetching its origin and ${url}.")
        endif()
    endif()

    # A half-made checkout from an interrupted configure: git knows it as a
    # worktree, so it can remove it; anything else is left for a human.
    _adl_vcpkg_git("${mirror}" worktree prune)
    if(EXISTS "${dest}")
        _adl_vcpkg_git("${mirror}" worktree remove --force "${dest}")
        if(EXISTS "${dest}")
            _adl_vcpkg_fail("${dest} exists but is not a finished checkout, and git will not remove it. Delete that directory and configure again.")
        endif()
    endif()

    message(STATUS "Checking out vcpkg ${short} into ${dest} (a few minutes, once)")
    _adl_vcpkg_git("${mirror}" worktree add --detach "${dest}" "${pin}")
    if(NOT _adl_git_rc EQUAL 0)
        _adl_vcpkg_fail("git worktree add ${dest} ${pin} failed:\n${_adl_git_log}")
    endif()
    file(WRITE "${ready}" "${pin}\n")
    set(${out} "${dest}" PARENT_SCOPE)
endfunction()

# Sets CMAKE_TOOLCHAIN_FILE (unless the caller supplied one) and
# ADL_VCPKG_ROOT_DIR. Must run before project().
function(adl_resolve_vcpkg source_dir)
    set(rel "scripts/buildsystems/vcpkg.cmake")

    # A toolchain given from outside wins. One this function chose on an
    # earlier configure comes back from the cache looking the same, so it is
    # remembered and resolved again: the pin may have moved since.
    if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_TOOLCHAIN_FILE STREQUAL "")
        if(NOT DEFINED ADL_VCPKG_AUTO_TOOLCHAIN
           OR NOT CMAKE_TOOLCHAIN_FILE STREQUAL ADL_VCPKG_AUTO_TOOLCHAIN)
            if(NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
                get_filename_component(root "${CMAKE_TOOLCHAIN_FILE}/../../.." ABSOLUTE)
                _adl_vcpkg_describe("${root}" why)
                _adl_vcpkg_fail("CMAKE_TOOLCHAIN_FILE is ${CMAKE_TOOLCHAIN_FILE}, which does not exist.\n${why}")
            endif()
            get_filename_component(root "${CMAKE_TOOLCHAIN_FILE}/../../.." ABSOLUTE)
            set(ADL_VCPKG_ROOT_DIR "${root}" CACHE INTERNAL "")
            return()
        endif()
    endif()

    _adl_vcpkg_pin("${source_dir}" pin)
    set(root "")

    set(want_root "")
    if(DEFINED ADL_VCPKG_ROOT AND NOT ADL_VCPKG_ROOT STREQUAL "")
        set(want_root "${ADL_VCPKG_ROOT}")
    elseif(DEFINED ENV{ADL_VCPKG_ROOT} AND NOT "$ENV{ADL_VCPKG_ROOT}" STREQUAL "")
        set(want_root "$ENV{ADL_VCPKG_ROOT}")
    endif()

    _adl_vcpkg_setting(want_store ADL_VCPKG_STORE ICL_VCPKG_STORE)

    if(want_root)
        file(TO_CMAKE_PATH "${want_root}" root)
        if(NOT EXISTS "${root}/${rel}")
            _adl_vcpkg_describe("${root}" why)
            _adl_vcpkg_fail("ADL_VCPKG_ROOT is set to ${root}.\n${why}")
        endif()
        if(pin)
            _adl_vcpkg_head("${root}" head)
            if(head AND NOT head STREQUAL pin)
                message(WARNING
                    "ADL_VCPKG_ROOT is at ${head}, but deps/vcpkg pins ${pin}. "
                    "Ports and scripts that do not match the manifest baseline can fail to build. "
                    "A vcpkg store (ICL_VCPKG_STORE) keeps a checkout per pinned commit instead.")
            endif()
        endif()
    elseif(want_store)
        file(TO_CMAKE_PATH "${want_store}" store)
        _adl_vcpkg_store_checkout("${store}" "${source_dir}" "${pin}" root)
        if(NOT EXISTS "${root}/${rel}")
            _adl_vcpkg_describe("${root}" why)
            _adl_vcpkg_fail("The shared checkout ${root} is unusable.\n${why}")
        endif()
    else()
        set(root "${source_dir}/deps/vcpkg")
        if(NOT EXISTS "${root}/${rel}")
            find_package(Git QUIET)
            if(GIT_FOUND AND EXISTS "${source_dir}/.git" AND NOT IS_SYMLINK "${root}")
                # The whole history, not --depth 1: the manifest's builtin-baseline is a commit
                # other than the pin, and vcpkg reads it and the older versions of each port out
                # of this repository's history (docs/building.md, "Clone").
                message(STATUS "Initialising vendored vcpkg submodule (deps/vcpkg)")
                execute_process(
                    COMMAND "${GIT_EXECUTABLE}" submodule update --init -- deps/vcpkg
                    WORKING_DIRECTORY "${source_dir}"
                    RESULT_VARIABLE rc)
                if(NOT rc EQUAL 0)
                    _adl_vcpkg_fail("Failed to initialise the vcpkg submodule at deps/vcpkg.")
                endif()
            endif()
        endif()
        if(NOT EXISTS "${root}/${rel}")
            # A link is never initialised over: what it points at is shared.
            _adl_vcpkg_describe("${root}" why)
            _adl_vcpkg_fail("Vendored vcpkg not found at deps/vcpkg.\n${why}")
        endif()
    endif()

    set(toolchain "${root}/${rel}")
    # CMake keeps the toolchain file this build directory was first configured
    # with and ignores a different one on a later configure, so a pin that
    # moved (or a store that was switched on) would leave the old scripts in
    # use while everything else follows the new ones.
    if(DEFINED ADL_VCPKG_AUTO_TOOLCHAIN AND NOT ADL_VCPKG_AUTO_TOOLCHAIN STREQUAL toolchain)
        message(FATAL_ERROR
            "vcpkg now resolves to\n"
            "  ${toolchain}\n"
            "but this build directory was configured with\n"
            "  ${ADL_VCPKG_AUTO_TOOLCHAIN}\n"
            "(the pin in deps/vcpkg moved, or ADL_VCPKG_* changed). CMake keeps the "
            "toolchain file of a build directory's first configure, so configure again with "
            "--fresh (cmake --fresh --preset <name>), which clears the cache but keeps the "
            "directory.")
    endif()
    set(CMAKE_TOOLCHAIN_FILE "${toolchain}" CACHE FILEPATH "vcpkg toolchain file" FORCE)
    set(ADL_VCPKG_AUTO_TOOLCHAIN "${toolchain}" CACHE INTERNAL "")
    set(ADL_VCPKG_ROOT_DIR "${root}" CACHE INTERNAL "")
    message(STATUS "vcpkg: ${root}")
endfunction()
