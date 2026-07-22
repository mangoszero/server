#ifndef MANGOS_TEST_SUPPORT_HPP
#define MANGOS_TEST_SUPPORT_HPP

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>

namespace mangos::test
{
inline int failures = 0;

inline void check(bool condition, char const* expression, char const* file, int line)
{
    if (condition)
        return;

    ++failures;
    std::cerr << file << ':' << line << ": CHECK failed: " << expression << '\n';
}

inline std::string bytesToHex(uint8_t const* data, std::size_t length)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < length; ++i)
        stream << std::setw(2) << static_cast<unsigned>(data[i]);
    return stream.str();
}

inline void checkBytes(uint8_t const* actual, std::size_t actualLength,
                       std::initializer_list<uint8_t> expected,
                       char const* expression, char const* file, int line)
{
    bool const equalLength = actualLength == expected.size();
    bool equalBytes = equalLength;
    std::size_t i = 0;
    for (uint8_t byte : expected)
    {
        if (!actual || actual[i++] != byte)
            equalBytes = false;
    }

    if (equalBytes)
        return;

    ++failures;
    std::string const expectedHex = bytesToHex(expected.begin(), expected.size());
    std::string const actualHex = actual ? bytesToHex(actual, actualLength) : "<null>";
    std::cerr << file << ':' << line << ": CHECK_BYTES failed: " << expression
              << " expected=" << expectedHex << " actual=" << actualHex << '\n';
}

inline void checkHex(uint8_t const* actual, std::size_t actualLength,
                     std::string const& expectedHex,
                     char const* expression, char const* file, int line)
{
    std::string const actualHex = actual ? bytesToHex(actual, actualLength) : "<null>";
    if (actualHex == expectedHex)
        return;

    ++failures;
    std::cerr << file << ':' << line << ": CHECK_HEX failed: " << expression
              << " expected=" << expectedHex << " actual=" << actualHex << '\n';
}
}

#define CHECK(expression) \
    ::mangos::test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
#define CHECK_BYTES(actual, length, ...) \
    ::mangos::test::checkBytes((actual), (length), __VA_ARGS__, #actual, __FILE__, __LINE__)
#define CHECK_HEX(actual, length, expected) \
    ::mangos::test::checkHex((actual), (length), (expected), #actual, __FILE__, __LINE__)

#endif
