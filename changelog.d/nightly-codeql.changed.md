CodeQL runs nightly, and not on every push or pull request: a full CodeQL
build costs more than the rest of the per-commit matrix, and its alerts are
triaged in Security -> Code scanning. The rest of the static analysis follows
the same schedule ([docs/static-analysis.md](docs/static-analysis.md)).
