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

#include "Auth/BigNumber.h"
#include <openssl/bn.h>
#include <algorithm>
#include <vector>

BigNumber::BigNumber()
{
    _bn = BN_new();
    _array = NULL;
}

BigNumber::BigNumber(const BigNumber& bn)
{
    _bn = BN_dup(bn._bn);
    _array = NULL;
}

BigNumber::BigNumber(uint32 val)
{
    _bn = BN_new();
    BN_set_word(_bn, val);
    _array = NULL;
}

BigNumber::~BigNumber()
{
    BN_free(_bn);
    if (_array)
    {
        delete[] _array;
    }
}

void BigNumber::SetDword(uint32 val)
{
    BN_set_word(_bn, val);
}

void BigNumber::SetQword(uint64 val)
{
    BN_add_word(_bn, (uint32)(val >> 32));
    BN_lshift(_bn, _bn, 32);
    BN_add_word(_bn, (uint32)(val & 0xFFFFFFFF));
}

void BigNumber::SetBinary(const uint8* bytes, int len)
{
    // Input is little-endian; BN_bin2bn wants big-endian, hence the reversal.
    //
    // The buffer used to be a fixed uint8 t[1000] on the stack with no check on
    // len, so any caller passing more than a kilobyte wrote past it. Nothing in
    // the tree does today -- the largest is a 40-byte session key -- but the
    // bound was neither enforced nor documented, and the argument comes from
    // callers that read lengths off the wire.
    if (len <= 0)
    {
        BN_zero(_bn);
        return;
    }

    // Braces, not parentheses: vector<uint8> reversed(size_t(len)) is a function
    // declaration, not a vector -- the most vexing parse.
    std::vector<uint8> reversed(static_cast<size_t>(len), 0);
    for (int i = 0; i < len; ++i)
    {
        reversed[size_t(i)] = bytes[len - 1 - i];
    }

    BN_bin2bn(reversed.data(), len, _bn);
}

void BigNumber::SetHexStr(const char* str)
{
    BN_hex2bn(&_bn, str);
}

void BigNumber::SetRand(int numbits)
{
    BN_rand(_bn, numbits, 0, 1);
}

BigNumber BigNumber::operator=(const BigNumber& bn)
{
    BN_copy(_bn, bn._bn);
    return *this;
}

BigNumber BigNumber::operator+=(const BigNumber& bn)
{
    BN_add(_bn, _bn, bn._bn);
    return *this;
}

BigNumber BigNumber::operator-=(const BigNumber& bn)
{
    BN_sub(_bn, _bn, bn._bn);
    return *this;
}

BigNumber BigNumber::operator*=(const BigNumber& bn)
{
    BN_CTX* bnctx;

    bnctx = BN_CTX_new();
    BN_mul(_bn, _bn, bn._bn, bnctx);
    BN_CTX_free(bnctx);

    return *this;
}

BigNumber BigNumber::operator/=(const BigNumber& bn)
{
    BN_CTX* bnctx;

    bnctx = BN_CTX_new();
    BN_div(_bn, NULL, _bn, bn._bn, bnctx);
    BN_CTX_free(bnctx);

    return *this;
}

BigNumber BigNumber::operator%=(const BigNumber& bn)
{
    BN_CTX* bnctx;

    bnctx = BN_CTX_new();
    BN_mod(_bn, _bn, bn._bn, bnctx);
    BN_CTX_free(bnctx);

    return *this;
}

BigNumber BigNumber::Exp(const BigNumber& bn)
{
    BigNumber ret;
    BN_CTX* bnctx;

    bnctx = BN_CTX_new();
    BN_exp(ret._bn, _bn, bn._bn, bnctx);
    BN_CTX_free(bnctx);

    return ret;
}

BigNumber BigNumber::ModExp(const BigNumber& bn1, const BigNumber& bn2)
{
    BigNumber ret;
    BN_CTX* bnctx;

    bnctx = BN_CTX_new();
    BN_mod_exp(ret._bn, _bn, bn1._bn, bn2._bn, bnctx);
    BN_CTX_free(bnctx);

    return ret;
}

int BigNumber::GetNumBytes(void)
{
    return BN_num_bytes(_bn);
}

uint32 BigNumber::AsDword()
{
    return (uint32)BN_get_word(_bn);
}

bool BigNumber::isZero() const
{
    return BN_is_zero(_bn) != 0;
}

uint8* BigNumber::AsByteArray(int minSize)
{
    return AsByteArray(minSize, true);
}

uint8* BigNumber::AsByteArray(int minSize, bool reverse)
{
    const int length = (minSize >= GetNumBytes()) ? minSize : GetNumBytes();

    delete[] _array;
    _array = new uint8[length];

    // BN_bn2binpad left-pads to exactly `length` bytes. The previous code used
    // BN_bn2bin, which emits the minimal big-endian encoding at offset 0 and
    // leaves the zero padding at the *end* -- so std::reverse below moved that
    // padding to the front of the little-endian result and shifted the value by
    // however many bytes were short.
    //
    // The number has to serialise shorter than requested for this to bite, which
    // for a uniformly distributed quantity (every SRP6 value here) is a leading
    // zero byte: about 1 login in 256. That is why it survived so long -- it
    // looks exactly like a flaky network, and it is invisible 255 times out of
    // 256. Using the padding-aware call makes the mistake unwritable rather than
    // merely fixed.
    BN_bn2binpad(_bn, _array, length);

    if (reverse)
    {
        std::reverse(_array, _array + length);
    }

    return _array;
}

const char* BigNumber::AsHexStr()
{
    return BN_bn2hex(_bn);
}

const char* BigNumber::AsDecStr()
{
    return BN_bn2dec(_bn);
}
