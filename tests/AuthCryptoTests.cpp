#include "TestSupport.hpp"

#include "Auth/ARC4.h"
#include "Auth/BigNumber.h"
#include "Auth/HMACSHA1.h"
#include "Auth/OpenSSLProvider.h"
#include "Auth/Sha1.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <string>

#include <openssl/crypto.h>

int main()
{
    BigNumber number;
    number.SetHexStr("1234");
    CHECK_BYTES(number.AsByteArray(4, false), 4, {0x00, 0x00, 0x12, 0x34});
    CHECK_BYTES(number.AsByteArray(4, true), 4, {0x34, 0x12, 0x00, 0x00});
    CHECK_BYTES(number.AsByteArray(2, false), 2, {0x12, 0x34});
    CHECK_BYTES(number.AsByteArray(2, true), 2, {0x34, 0x12});

    BigNumber zero;
    CHECK_BYTES(zero.AsByteArray(4, false), 4, {0x00, 0x00, 0x00, 0x00});
    CHECK_BYTES(zero.AsByteArray(4, true), 4, {0x00, 0x00, 0x00, 0x00});

    Sha1Hash sha1;
    sha1.UpdateData(std::string("abc"));
    sha1.Finalize();
    CHECK_HEX(sha1.GetDigest(), sha1.GetLength(),
              "a9993e364706816aba3e25717850c26c9cd0d89d");

    std::array<uint8, 20> hmacKey{};
    hmacKey.fill(0x0b);
    HMACSHA1 hmac(static_cast<uint32>(hmacKey.size()), hmacKey.data());
    hmac.UpdateData(std::string("Hi There"));
    hmac.Finalize();
    CHECK_HEX(hmac.GetDigest(), hmac.GetLength(),
              "b617318655057264e28bc0b6fb378c8ef146be00");

    OpenSSLProviderManager providerManager;
    CHECK(providerManager.IsInitialized());

    unsigned const runtimeMajor = static_cast<unsigned>((OpenSSL_version_num() >> 28) & 0x0f);
    CHECK(runtimeMajor == 3);
    auto checkProviderMajor = [runtimeMajor](OpenSSLProvider const& provider)
    {
        std::string const providerVersion = provider.Version();
        unsigned providerMajor = 0;
        CHECK(!providerVersion.empty());
        std::size_t const delimiter = providerVersion.find('.');
        CHECK(delimiter != std::string::npos);
        if (delimiter == std::string::npos)
            return;

        char const* const providerBegin = providerVersion.data();
        char const* const providerEnd = providerBegin + delimiter;
        std::from_chars_result const parsed =
            std::from_chars(providerBegin, providerEnd, providerMajor);
        CHECK(parsed.ec == std::errc{});
        CHECK(parsed.ptr == providerEnd);
        CHECK(providerMajor == runtimeMajor);
    };
    checkProviderMajor(providerManager.GetLegacyProvider());
    checkProviderMajor(providerManager.GetDefaultProvider());

    uint8 rc4Key[] = {'K', 'e', 'y'};
    uint8 rc4Data[] = {'P', 'l', 'a', 'i', 'n', 't', 'e', 'x', 't'};
    ARC4 rc4(rc4Key, static_cast<uint8>(sizeof(rc4Key)));
    rc4.UpdateData(sizeof(rc4Data), rc4Data);
    CHECK_HEX(rc4Data, sizeof(rc4Data), "bbf316e8d940af0ad3");

    return mangos::test::failures == 0 ? 0 : 1;
}
