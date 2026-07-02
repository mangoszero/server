#ifndef MANGOS_MANGOSD_TEST_H
#define MANGOS_MANGOSD_TEST_H

#include <string>

/// In-process mangosd test harness (`mangosd -t <name>`). Runs AFTER config +
/// DB init but BEFORE world load. Returns 0 on pass, non-zero on fail.
int RunMangosdTest(std::string const& name);

#endif
