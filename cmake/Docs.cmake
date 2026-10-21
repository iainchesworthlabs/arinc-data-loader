# The documentation site: docs/ built by MkDocs into a static site (mkdocs.yml, docs/ci.md,
# "Documentation site").
#
#   python -m pip install --require-hashes -r .github/requirements/docs.txt
#   cmake --build <build dir> --target docs        # the site is <build dir>/site
#
# The build is strict (a link, a heading anchor or a page that is wrong fails it), as it is in CI. The
# target exists whether or not MkDocs is installed, so that a missing tool is reported by the target and
# not by a "no such target" error.

find_program(ADL_MKDOCS mkdocs DOC "MkDocs, installed from .github/requirements/docs.txt")

set(ADL_DOCS_SITE_DIR "${PROJECT_BINARY_DIR}/site" CACHE PATH "Where the docs target writes the site")

if(ADL_MKDOCS)
    add_custom_target(docs
        COMMAND "${CMAKE_COMMAND}" -E env NO_MKDOCS_2_WARNING=true
                "${ADL_MKDOCS}" build --strict
                --config-file "${PROJECT_SOURCE_DIR}/mkdocs.yml"
                --site-dir "${ADL_DOCS_SITE_DIR}"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Building the documentation site (strict) into ${ADL_DOCS_SITE_DIR}"
        VERBATIM USES_TERMINAL)
else()
    add_custom_target(docs
        COMMAND "${CMAKE_COMMAND}" -E echo
                "docs: MkDocs was not found. Install it with: python -m pip install --require-hashes -r .github/requirements/docs.txt"
        COMMAND "${CMAKE_COMMAND}" -E false
        COMMENT "The documentation site needs MkDocs"
        VERBATIM)
endif()
