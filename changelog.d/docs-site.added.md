**The documentation is published as a website**
(<https://iainchesworthlabs.github.io/arinc-data-loader/>, [docs/ci.md](docs/ci.md#documentation-site)): the
pages in `docs/`, including the user's guide, are built with MkDocs and Material and published to GitHub
Pages when `main` changes. The build is strict: a link to a page or heading that is not there, or a page that
is missing from the navigation, fails the `Docs build (strict)` check on every pull request and fails a
release before it is built. `cmake --build <dir> --target docs` builds the same site locally.
