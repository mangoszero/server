/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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

#ifndef MANGOS_H_AUTH_BIGNUMBER
#define MANGOS_H_AUTH_BIGNUMBER

#include "Common/Common.h"

struct bignum_st;

/**
 * @brief Arbitrary precision integer arithmetic using OpenSSL BIGNUM
 *
 * BigNumber provides a C++ wrapper around OpenSSL's BIGNUM structure,
 * enabling arbitrary precision integer arithmetic for cryptographic operations.
 * Used primarily for RSA key generation and cryptographic computations.
 */
class BigNumber
{
    public:
        /**
         * @brief Default constructor - creates a zero BigNumber
         */
        BigNumber();
        /**
         * @brief Copy constructor
         * @param bn BigNumber to copy from
         */
        BigNumber(const BigNumber& bn);
        /**
         * @brief Constructor from 32-bit unsigned integer
         * @param uint32 Initial value
         */
        BigNumber(uint32);
        /**
         * @brief Destructor - frees OpenSSL BIGNUM resources
         */
        ~BigNumber();

        /**
         * @brief Set value from 32-bit unsigned integer
         * @param uint32 Value to set
         */
        void SetDword(uint32);
        /**
         * @brief Set value from 64-bit unsigned integer
         * @param uint64 Value to set
         */
        void SetQword(uint64);
        /**
         * @brief Set value from binary data
         * @param bytes Pointer to binary data
         * @param len Length of binary data in bytes
         */
        void SetBinary(const uint8* bytes, int len);
        /**
         * @brief Set value from hexadecimal string
         * @param str Hexadecimal string representation
         */
        void SetHexStr(const char* str);

        /**
         * @brief Set to a random value
         * @param numbits Number of bits for the random value
         */
        void SetRand(int numbits);

        /**
         * @brief Assignment operator
         * @param bn BigNumber to assign from
         * @return Reference to this BigNumber
         */
        BigNumber operator=(const BigNumber& bn);

        /**
         * @brief Addition assignment operator
         * @param bn BigNumber to add
         * @return Reference to this BigNumber
         */
        BigNumber operator+=(const BigNumber& bn);
        /**
         * @brief Addition operator
         * @param bn BigNumber to add
         * @return New BigNumber with sum
         */
        BigNumber operator+(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t += bn;
        }
        /**
         * @brief Subtraction assignment operator
         * @param bn BigNumber to subtract
         * @return Reference to this BigNumber
         */
        BigNumber operator-=(const BigNumber& bn);
        /**
         * @brief Subtraction operator
         * @param bn BigNumber to subtract
         * @return New BigNumber with difference
         */
        BigNumber operator-(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t -= bn;
        }
        /**
         * @brief Multiplication assignment operator
         * @param bn BigNumber to multiply by
         * @return Reference to this BigNumber
         */
        BigNumber operator*=(const BigNumber& bn);
        /**
         * @brief Multiplication operator
         * @param bn BigNumber to multiply by
         * @return New BigNumber with product
         */
        BigNumber operator*(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t *= bn;
        }
        /**
         * @brief Division assignment operator
         * @param bn BigNumber to divide by
         * @return Reference to this BigNumber
         */
        BigNumber operator/=(const BigNumber& bn);
        /**
         * @brief Division operator
         * @param bn BigNumber to divide by
         * @return New BigNumber with quotient
         */
        BigNumber operator/(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t /= bn;
        }
        /**
         * @brief Modulo assignment operator
         * @param bn BigNumber for modulo operation
         * @return Reference to this BigNumber
         */
        BigNumber operator%=(const BigNumber& bn);
        /**
         * @brief Modulo operator
         * @param bn BigNumber for modulo operation
         * @return New BigNumber with remainder
         */
        BigNumber operator%(const BigNumber& bn)
        {
            BigNumber t(*this);
            return t %= bn;
        }

        /**
         * @brief Check if the BigNumber is zero
         * @return True if value is zero, false otherwise
         */
        bool isZero() const;

        /**
         * @brief Modular exponentiation: (this ^ bn1) mod bn2
         * @param bn1 Exponent
         * @param bn2 Modulus
         * @return New BigNumber with result
         */
        BigNumber ModExp(const BigNumber& bn1, const BigNumber& bn2);
        /**
         * @brief Exponentiation: this ^ bn
         * @param Exponent value
         * @return New BigNumber with result
         */
        BigNumber Exp(const BigNumber&);

        /**
         * @brief Get the number of bytes needed to represent this value
         * @return Number of bytes
         */
        int GetNumBytes(void);

        /**
         * @brief Get the underlying OpenSSL BIGNUM structure
         * @return Pointer to OpenSSL BIGNUM structure
         */
        struct bignum_st* BN() { return _bn; }

        /**
         * @brief Convert to 32-bit unsigned integer
         * @return 32-bit unsigned integer value
         */
        uint32 AsDword();
        /**
         * @brief Convert to byte array
         * @param minSize Minimum size of the array (pads with zeros if needed)
         * @return Pointer to byte array
         */
        uint8* AsByteArray(int minSize = 0);
        /**
         * @brief Convert to byte array with optional byte reversal
         * @param minSize Minimum size of the array
         * @param reverse If true, reverse byte order
         * @return Pointer to byte array
         */
        uint8* AsByteArray(int minSize, bool reverse);
        /**
         * @brief Convert to hexadecimal string
         * @return Hexadecimal string representation
         */
        const char* AsHexStr();
        /**
         * @brief Convert to decimal string
         * @return Decimal string representation
         */
        const char* AsDecStr();

    private:
        struct bignum_st* _bn; /**< OpenSSL BIGNUM structure */
        uint8* _array; /**< Cached byte array representation */
};
#endif
