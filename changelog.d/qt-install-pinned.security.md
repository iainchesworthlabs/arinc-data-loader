The CI builds no longer trust Qt's download server for the integrity of the Qt kit: every archive is
checked against a SHA-256 pinned in the repository before it is unpacked, and the Python package that
unpacks it is installed from a hash-locked requirements file instead of an unpinned PyPI install.
