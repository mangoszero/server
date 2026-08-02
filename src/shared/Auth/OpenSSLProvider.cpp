/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file OpenSSLProvider.cpp
 * @brief Implementation of RAII wrappers for OpenSSL providers
 */

#include <string>
#include "OpenSSLProvider.h"
#include "Log/Log.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <utility>
#include <vector>

#ifdef WIN32
#include <windows.h>
#endif

/**
 * Creates a new OpenSSL cipher context wrapper.
 */
OpenSSLCipherContext::OpenSSLCipherContext()
    : m_ctx(nullptr)
{
    m_ctx = EVP_CIPHER_CTX_new();
    if (!m_ctx)
    {
        sLog.outError("OpenSSLCipherContext: Failed to create cipher context");
    }
}

/**
 * Releases the owned OpenSSL cipher context.
 */
OpenSSLCipherContext::~OpenSSLCipherContext()
{
    if (m_ctx)
    {
        EVP_CIPHER_CTX_free(m_ctx);
        m_ctx = nullptr;
    }
}

OpenSSLCipherContext::OpenSSLCipherContext(OpenSSLCipherContext&& other) noexcept
    : m_ctx(other.m_ctx)
{
    other.m_ctx = nullptr;
}

OpenSSLCipherContext& OpenSSLCipherContext::operator=(OpenSSLCipherContext&& other) noexcept
{
    if (this != &other)
    {
        // Clean up current context
        if (m_ctx)
        {
            EVP_CIPHER_CTX_free(m_ctx);
        }

        // Move from other
        m_ctx = other.m_ctx;
        other.m_ctx = nullptr;
    }
    return *this;
}

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)

namespace
{
bool ParseProviderMajor(const std::string& version, unsigned& major)
{
    std::size_t separator = version.find('.');
    if (separator == std::string::npos || separator == 0)
        return false;

    const char* begin = version.data();
    const char* end = begin + separator;
    std::from_chars_result parsed =
        std::from_chars(begin, end, major);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

#ifdef WIN32
std::wstring ReadWindowsEnvironment(const wchar_t* name)
{
    DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0)
        return {};

    std::vector<wchar_t> buffer(required);
    while (true)
    {
        DWORD written = GetEnvironmentVariableW(
            name, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (written == 0)
            return {};
        if (written < buffer.size())
            return std::wstring(buffer.data(), written);
        buffer.resize(static_cast<std::size_t>(written) + 1);
    }
}

std::filesystem::path GetExecutableDirectory()
{
    constexpr std::size_t MAX_WINDOWS_PATH = 32768;
    std::vector<wchar_t> buffer(MAX_PATH);

    while (buffer.size() <= MAX_WINDOWS_PATH)
    {
        DWORD written = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (written == 0)
            return {};
        if (written < buffer.size())
        {
            return std::filesystem::path(
                std::wstring(buffer.data(), written)).parent_path();
        }

        if (buffer.size() == MAX_WINDOWS_PATH)
            break;
        buffer.resize((std::min)(buffer.size() * 2, MAX_WINDOWS_PATH));
    }

    return {};
}

bool ConvertWideToAnsi(const std::wstring& value, std::string& converted)
{
    converted.clear();
    if (value.empty())
        return false;

    BOOL usedDefault = FALSE;
    int required = WideCharToMultiByte(
        CP_ACP, WC_NO_BEST_FIT_CHARS, value.c_str(), -1,
        nullptr, 0, nullptr, &usedDefault);
    if (required <= 0 || usedDefault)
        return false;

    std::vector<char> buffer(static_cast<std::size_t>(required));
    usedDefault = FALSE;
    int written = WideCharToMultiByte(
        CP_ACP, WC_NO_BEST_FIT_CHARS, value.c_str(), -1,
        buffer.data(), required, nullptr, &usedDefault);
    if (written <= 0 || usedDefault)
        return false;

    converted.assign(buffer.data(), static_cast<std::size_t>(written - 1));
    return true;
}

bool IsUsableOpenSSLPath(const std::wstring& path, std::string& converted)
{
    if (!ConvertWideToAnsi(path, converted))
        return false;

    constexpr std::size_t LEGACY_SUFFIX_LENGTH = sizeof("\\legacy.dll") - 1;
    return converted.size() + LEGACY_SUFFIX_LENGTH < MAX_PATH;
}

bool ConvertForOpenSSL(
    const std::filesystem::path& directory, std::string& converted)
{
    const std::wstring& nativePath = directory.native();
    if (IsUsableOpenSSLPath(nativePath, converted))
        return true;

    DWORD required = GetShortPathNameW(nativePath.c_str(), nullptr, 0);
    if (required == 0)
        return false;

    std::vector<wchar_t> shortPath(required);
    DWORD written = GetShortPathNameW(
        nativePath.c_str(), shortPath.data(), required);
    if (written == 0 || written >= shortPath.size())
        return false;

    return IsUsableOpenSSLPath(
        std::wstring(shortPath.data(), written), converted);
}

void ConfigureBundledProviderSearchPath()
{
    if (!ReadWindowsEnvironment(L"OPENSSL_MODULES").empty())
        return;

    try
    {
        std::filesystem::path executableDirectory = GetExecutableDirectory();
        if (executableDirectory.empty())
        {
            sLog.outError(
                "OpenSSLProvider: Failed to resolve the executable directory");
            return;
        }

        std::filesystem::path providerDirectory =
            executableDirectory / L"ossl-modules";
        std::filesystem::path legacyProvider =
            providerDirectory / L"legacy.dll";
        std::error_code fileError;
        if (!std::filesystem::is_regular_file(legacyProvider, fileError))
            return;

        std::string providerPath;
        if (!ConvertForOpenSSL(providerDirectory, providerPath))
        {
            sLog.outError(
                "OpenSSLProvider: Bundled provider path cannot be represented "
                "for the OpenSSL Windows loader");
            return;
        }

        if (OSSL_PROVIDER_set_default_search_path(
                nullptr, providerPath.c_str()) != 1)
        {
            sLog.outError(
                "OpenSSLProvider: Failed to configure bundled provider path '%s'",
                providerPath.c_str());
        }
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        sLog.outError(
            "OpenSSLProvider: Failed to inspect bundled provider path: %s",
            error.what());
    }
}
#endif

OpenSSLProvider LoadLegacyProvider()
{
#ifdef WIN32
    ConfigureBundledProviderSearchPath();
#endif
    return OpenSSLProvider("legacy");
}

std::string OpenSSLModulesForDiagnostic()
{
#ifdef WIN32
    std::wstring modules = ReadWindowsEnvironment(L"OPENSSL_MODULES");
    if (modules.empty())
        return "<unset>";

    std::string converted;
    return ConvertWideToAnsi(modules, converted)
        ? converted : "<unrepresentable>";
#else
    const char* modules = std::getenv("OPENSSL_MODULES");
    return modules ? std::string(modules) : std::string("<unset>");
#endif
}
}

/**
 * Loads the named OpenSSL provider into the specified library context.
 */
OpenSSLProvider::OpenSSLProvider(const char* name, OSSL_LIB_CTX* libraryContext)
    : m_provider(nullptr), m_providerName(name ? name : "")
{
    if (!name)
    {
        sLog.outError("OpenSSLProvider: Provider name cannot be null");
        return;
    }

    m_provider = OSSL_PROVIDER_load(libraryContext, name);
    if (!m_provider)
    {
        sLog.outError("OpenSSLProvider: Failed to load provider '%s'", name);
    }
}

/**
 * Unloads the owned OpenSSL provider instance.
 */
OpenSSLProvider::~OpenSSLProvider()
{
    if (m_provider)
    {
        OSSL_PROVIDER_unload(m_provider);
        m_provider = nullptr;
    }
}

std::string OpenSSLProvider::Version() const
{
    if (!m_provider)
        return {};

    char* version = nullptr;
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_ptr(
            OSSL_PROV_PARAM_VERSION, &version, 0),
        OSSL_PARAM_construct_end()
    };
    if (OSSL_PROVIDER_get_params(m_provider, params) != 1 || !version)
        return {};
    return version;
}

OpenSSLProvider::OpenSSLProvider(OpenSSLProvider&& other) noexcept
    : m_provider(other.m_provider), m_providerName(std::move(other.m_providerName))
{
    other.m_provider = nullptr;
}

OpenSSLProvider& OpenSSLProvider::operator=(OpenSSLProvider&& other) noexcept
{
    if (this != &other)
    {
        // Clean up current provider
        if (m_provider)
        {
            OSSL_PROVIDER_unload(m_provider);
        }

        // Move from other
        m_provider = other.m_provider;
        m_providerName = std::move(other.m_providerName);
        other.m_provider = nullptr;
    }
    return *this;
}

/**
 * Initializes the OpenSSL provider manager and loads required providers.
 */
OpenSSLProviderManager::OpenSSLProviderManager()
    : m_legacyProvider(LoadLegacyProvider()),
      m_defaultProvider("default"),
      m_initialized(false)
{
    if (!m_legacyProvider.IsLoaded() || !m_defaultProvider.IsLoaded())
    {
        sLog.outError("Failed to load OpenSSL 3.x providers");

        if (!m_legacyProvider.IsLoaded())
        {
            sLog.outError("  - Legacy provider failed to load");
#ifdef WIN32
            sLog.outError("    Use a complete release with ossl-modules\\legacy.dll");
            sLog.outError("    beside the daemon, or set OPENSSL_MODULES to the");
            sLog.outError("    directory containing a matching legacy.dll.");
#endif
        }

        if (!m_defaultProvider.IsLoaded())
        {
            sLog.outError("  - Default provider failed to load");
        }
        return;
    }

    std::string legacyVersion = m_legacyProvider.Version();
    std::string defaultVersion = m_defaultProvider.Version();
    unsigned legacyMajor = 0;
    unsigned defaultMajor = 0;
    bool parsedLegacy =
        ParseProviderMajor(legacyVersion, legacyMajor);
    bool parsedDefault =
        ParseProviderMajor(defaultVersion, defaultMajor);

    unsigned runtimeMajor =
        unsigned((OpenSSL_version_num() >> 28) & 0x0f);
    if (runtimeMajor != 3
        || !parsedLegacy || legacyMajor != runtimeMajor
        || !parsedDefault || defaultMajor != runtimeMajor)
    {
        std::string modules = OpenSSLModulesForDiagnostic();
        sLog.outError(
            "OpenSSL 3.x provider/runtime validation failed: "
            "runtime='%s', legacy provider='%s', default provider='%s', "
            "OPENSSL_MODULES='%s'",
            OpenSSL_version(OPENSSL_VERSION),
            legacyVersion.empty() ? "<unavailable>" : legacyVersion.c_str(),
            defaultVersion.empty() ? "<unavailable>" : defaultVersion.c_str(),
            modules.c_str());
        return;
    }

    EVP_CIPHER* rc4 = EVP_CIPHER_fetch(nullptr, "RC4", nullptr);
    if (!rc4)
    {
        sLog.outError(
            "OpenSSL legacy provider is loaded but RC4 is unavailable");
        return;
    }
    EVP_CIPHER_free(rc4);

    m_initialized = true;
    sLog.outString(
        "OpenSSL 3.x providers loaded successfully: legacy %s, default %s",
        legacyVersion.c_str(), defaultVersion.c_str());
}

OpenSSLProviderManager& OpenSSLProviderManager::Instance()
{
    static OpenSSLProviderManager instance;
    return instance;
}

/**
 * Logs provider shutdown when the OpenSSL provider manager is destroyed.
 */
OpenSSLProviderManager::~OpenSSLProviderManager()
{
    if (m_initialized)
    {
        sLog.outString("OpenSSL 3.x providers unloaded");
    }
}

#endif // OPENSSL_VERSION_MAJOR >= 3
