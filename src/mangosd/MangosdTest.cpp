#include "MangosdTest.h"
#include "Log.h"
#include <cstdio>

int RunMangosdTest(std::string const& name)
{
    if (name == "noop")
    {
        printf("noop OK\n");
        return 0;
    }

    printf("%s FAIL: unknown test\n", name.c_str());
    return 2;
}
