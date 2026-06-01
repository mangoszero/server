/**
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file OpenSSLProvider.cpp
 * @brief Implementation of RAII wrappers for OpenSSL providers
 */

#include "OpenSSLProvider.h"
#include "Log/Log.h"

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
    : m_legacyProvider("legacy"), m_defaultProvider("default"), m_initialized(false)
{
    // Check if both providers loaded successfully
    if (m_legacyProvider.IsLoaded() && m_defaultProvider.IsLoaded())
    {
        m_initialized = true;
        sLog.outString("OpenSSL 3.x providers loaded successfully: legacy, default");
    }
    else
    {
        sLog.outError("Failed to load OpenSSL 3.x providers");

        if (!m_legacyProvider.IsLoaded())
        {
            sLog.outError("  - Legacy provider failed to load");
#ifdef WIN32
            sLog.outError("    Please check you have set the following Environment Variable:");
            sLog.outError("    OPENSSL_MODULES=C:\\OpenSSL-Win64\\bin");
            sLog.outError("    (where C:\\OpenSSL-Win64\\bin is the location you installed OpenSSL");
#endif
        }

        if (!m_defaultProvider.IsLoaded())
        {
            sLog.outError("  - Default provider failed to load");
        }
    }
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
