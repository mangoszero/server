#include "MangosdTest.h"

#include <cstdio>
#include <map>

namespace
{
    std::map<std::string, MangosdTest>& Tests()
    {
        static std::map<std::string, MangosdTest> tests;
        return tests;
    }
}

void RegisterMangosdTest(char const* name, MangosdTest test)
{
    Tests()[name] = test;
}

int RunMangosdTest(std::string const& name)
{
    if (name == "noop")
    {
        printf("noop OK\n");
        return 0;
    }

    std::map<std::string, MangosdTest>::const_iterator itr = Tests().find(name);
    if (itr == Tests().end())
    {
        printf("%s FAIL: unknown test\n", name.c_str());
        return 2;
    }

    return itr->second();
}
