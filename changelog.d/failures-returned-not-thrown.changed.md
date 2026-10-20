Failures are returned (`std::expected`), not thrown; the singleton and the
factory that registered itself during static initialisation are gone; a
checker is chosen by `IntegrityChecker::make`.
