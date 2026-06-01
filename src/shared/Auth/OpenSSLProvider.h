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
 * @file OpenSSLProvider.h
 * @brief RAII wrapper for OpenSSL 3.x providers
 *
 * This file provides RAII wrappers for OpenSSL provider management,
 * ensuring proper cleanup and exception safety for OpenSSL 3.x
 * provider loading and unloading.
 */

#ifndef _AUTH_OPENSSL_PROVIDER_H
#define _AUTH_OPENSSL_PROVIDER_H

#include "Common/Common.h"
#include <openssl/evp.h>

/**
 * @brief RAII wrapper for EVP_CIPHER_CTX
 *
 * This class manages EVP_CIPHER_CTX lifecycle with proper
 * initialization and cleanup, preventing memory leaks.
 */
class OpenSSLCipherContext
{
public:
    /**
     * @brief Constructor - creates new cipher context
     */
    OpenSSLCipherContext();

    /**
     * @brief Destructor - automatically frees context
     */
    ~OpenSSLCipherContext();

    /**
     * @brief Get the underlying cipher context
     * @return EVP_CIPHER_CTX pointer
     */
    EVP_CIPHER_CTX* Get() const { return m_ctx; }

    /**
     * @brief Check if context is valid
     * @return true if context is valid, false otherwise
     */
    bool IsValid() const { return m_ctx != nullptr; }

    /**
     * @brief Move constructor
     */
    OpenSSLCipherContext(OpenSSLCipherContext&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    OpenSSLCipherContext& operator=(OpenSSLCipherContext&& other) noexcept;

    // Delete copy operations
    OpenSSLCipherContext(const OpenSSLCipherContext&) = delete;
    OpenSSLCipherContext& operator=(const OpenSSLCipherContext&) = delete;

private:
    EVP_CIPHER_CTX* m_ctx;    /**< OpenSSL cipher context */
};

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
#include <openssl/provider.h>

/**
 * @brief RAII wrapper for OpenSSL 3.x OSSL_PROVIDER
 *
 * This class automatically loads and unloads OpenSSL providers,
 * ensuring proper cleanup when the wrapper goes out of scope.
 * It provides exception safety and prevents resource leaks.
 */
class OpenSSLProvider
{
public:
    /**
     * @brief Constructor - loads specified provider
     * @param name Provider name (e.g., "legacy", "default")
     * @param libraryContext Library context (NULL for default)
     */
    OpenSSLProvider(const char* name, OSSL_LIB_CTX* libraryContext = nullptr);

    /**
     * @brief Destructor - automatically unloads provider
     */
    ~OpenSSLProvider();

    /**
     * @brief Check if provider is successfully loaded
     * @return true if provider is loaded, false otherwise
     */
    bool IsLoaded() const { return m_provider != nullptr; }

    /**
     * @brief Get the underlying provider handle
     * @return OSSL_PROVIDER pointer or nullptr if not loaded
     */
    OSSL_PROVIDER* Get() const { return m_provider; }

    /**
     * @brief Move constructor
     */
    OpenSSLProvider(OpenSSLProvider&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    OpenSSLProvider& operator=(OpenSSLProvider&& other) noexcept;

    // Delete copy operations to prevent double-free
    OpenSSLProvider(const OpenSSLProvider&) = delete;
    OpenSSLProvider& operator=(const OpenSSLProvider&) = delete;

private:
    OSSL_PROVIDER* m_provider;    /**< OpenSSL provider handle */
    std::string m_providerName;   /**< Provider name for logging */
};

/**
 * @brief RAII wrapper for OpenSSL 3.x provider management
 *
 * This class manages both legacy and default providers for OpenSSL 3.x,
 * ensuring they are loaded and unloaded properly. It's designed to be
 * used at application startup to handle provider initialization.
 */
class OpenSSLProviderManager
{
public:
    /**
     * @brief Constructor - loads legacy and default providers
     */
    OpenSSLProviderManager();

    /**
     * @brief Destructor - automatically unloads providers
     */
    ~OpenSSLProviderManager();

    /**
     * @brief Check if providers are successfully loaded
     * @return true if both providers loaded successfully
     */
    bool IsInitialized() const { return m_initialized; }

    /**
     * @brief Get legacy provider
     * @return Reference to legacy provider
     */
    const OpenSSLProvider& GetLegacyProvider() const { return m_legacyProvider; }

    /**
     * @brief Get default provider
     * @return Reference to default provider
     */
    const OpenSSLProvider& GetDefaultProvider() const { return m_defaultProvider; }

private:
    OpenSSLProvider m_legacyProvider;   /**< Legacy provider for compatibility */
    OpenSSLProvider m_defaultProvider;    /**< Default provider */
    bool m_initialized;                    /**< Initialization status */
};

#endif // OPENSSL_VERSION_MAJOR >= 3

#endif // _AUTH_OPENSSL_PROVIDER_H
