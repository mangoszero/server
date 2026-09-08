#ifndef MANGOS_MANGOSD_TEST_H
#define MANGOS_MANGOSD_TEST_H

#include <string>

/// Destructive harness (`mangosd --allow-destructive-tests -t <name>`).
/// Use only a disposable database configuration. Runs AFTER config +
/// DB init but BEFORE world load. Returns 0 on pass, non-zero on fail.
int RunMangosdTest(std::string const& name);

#endif
