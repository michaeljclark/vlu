/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019, Michael Clark <michaeljclark@mac.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <vector>

#include "bits.h"

#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64) || defined(_M_AMD64)
#ifndef USE_MISALIGNED_LOADS
#define USE_MISALIGNED_LOADS 1
#endif
#endif

/*
 * Bit field macros
 */

template <typename U>
static U extract_field(U value, size_t offset, size_t width) {
    return (value >> offset) & ((U(1) << width)-1);
}

template <typename U>
static U insert_field(U value, size_t offset, size_t width) {
    return (value & ((U(1) << width)-1)) << offset;
}

template <typename U>
static U replace_field(U orig, U replacement, size_t offset, size_t width) {
    return (orig & ~( ((U(1) << width)-1) << offset) ) |
        (replacement & ((U(1) << width)-1)) << offset;
}

/*
 * Variable Length Unary
 *
 * VLU is a little-endian variable length integer coding
 * that prefixes payload bits with an unary coded length.
 *
 * The length is recovered by counting least significant one
 * bits, which encodes a count of n-bit quantums. The data bits
 * are stored in the remaining bits of the first byte followed
 * by the number of bytes indicated in the unary value. e.g.
 *
 *   bits_per_quantum = 8
 *   unary_value = count_trailing_zeros(not(number))
 *   encoded_bits = (unary_value + 1) * (bits_per_quantum - 1)
 *
 * With 8 bit quantum, the encoded size is similar to LEB128, 
 * 7-bits can be stored in 1 byte, and 56-bits in 8 bytes.
 *
 * Decoding, however, is significantly faster, as it is not
 * necessary to check for continuation bits every byte.
 *
 * The scheme can be modified to support big integers by adding
 * continuation bits at a machine word interval (64 bits),
 * reducing the SHIFT-MASK-BRANCH frequency by a factor of 8.
 *
 * To encode:
 *
 *   shamt    = 8 - ((clz(num) - 1) / 7) + 1
 *   encoded  = integer << shamt
 *   if num ≠ 0 then:
 *       encoded = encoded | ((1 << (shamt - 1)) - 1)
 *
 * To decode:
 *
 *   shamt    = ctz(~encoded) + 1
 *   integer  = encoded >> shamt
 *
 * Example:
 *
 *   |  byte-8  |  byte-7  |          |  byte-2  |  byte-1  |
 *   |----------|----------|----------|----------|----------|
 *   |          |          |          |          | nnnnnnn0 |
 *   |          |          |          | nnnnnnnn | nnnnnn01 |
 *   |          | nnnnnnnn | ........ | nnnnnnnn | n0111111 |
 *   | nnnnnnnn | nnnnnnnn | ........ | nnnnnnnn | 01111111 |
 *
 */

struct vlu_result
{
    uint64_t val;
    int64_t shamt;
};

/*
 * vlu_encoded_size_56c - VLU8 packet size in bytes
 */
static int vlu_encoded_size_56c(uint64_t num, uint64_t limit = 8)
{
    if (!num) return 1;
    int lz = clz(num);
    int t1 = 8 - ((lz - 1) / 7);
    bool cont = t1 >= limit;
    return cont ? limit : t1 + 1;
}

/*
 * vlu_decoded_size_56c - VLU8 packet size in bytes
 */
static int vlu_decoded_size_56c(uint64_t uvlu, uint64_t limit = 8)
{
    int t1 = ctz(~uvlu);
    bool cont = t1 >= limit;
    int shamt = cont ? limit : t1 + 1;
    return shamt;
}

/*
 * vlu_encode_56c - VLU8 encoding with continuation support
 *
 * returns {
 *   val:   encoded value
 *   shamt: shift value from 1 to 8, or -1 for continuation
 * }
 */
static struct vlu_result vlu_encode_56c(uint64_t num, uint64_t limit = 8)
{
    if (!num) return vlu_result{ 0, 1 };
    int lz = clz(num);
    int t1 = 8 - ((lz - 1) / 7);
    bool cont = t1 >= limit;
    int shamt = cont ? limit : t1 + 1;
    uint64_t uvlu = (num << shamt)
        | ((1ull << (shamt-1))-1)
        | ((uint64_t)cont << (limit-1));
    return vlu_result{ uvlu, shamt | -(int64_t)cont };
}

/*
 * vlu_decode_56c - VLU8 decoding with continuation support
 *
 * @param vlu value to decode
 * @param limit for continuation
 * @returns (struct vlu_result) {
 *   val:   decoded value
 *   shamt: shift value from 1 to 8, or -1 for continuation
 * }
 */
#if defined (__GNUC__) && defined(__x86_64__)
static vlu_result vlu_decode_56c(uint64_t vlu, uint64_t limit = 8)
{
    struct vlu_result r;
    uint64_t tmp1, tmp2;
    asm volatile (
        "mov     %[vlu], %[tmp1]            \n\t"
        "not     %[tmp1]                    \n\t" /* ~vlu */
        "tzcnt   %[tmp1], %[tmp1]           \n\t" /* tz = ctz(~vlu) */
        "xor     %[tmp2], %[tmp2]           \n\t"
        "cmp     %[limit], %[tmp1]          \n\t"
        "setge   %b[tmp2]                   \n\t" /* shamt >= limit */
        "lea     1(%[tmp1]), %[shamt]       \n\t" /* shamt = tz + 1*/
        "mov     %[limit], %[tmp1]          \n\t"
        "cmovg   %[tmp1], %[shamt]          \n\t" /* shamt >= limit */
        "imul    $7, %[shamt], %[tmp1]      \n\t" /* st7 = shamt * 7 */
        "shrx    %[shamt], %[vlu], %[val]   \n\t" /* r = vlu >> shamt */
        "neg     %[tmp2]                    \n\t" /* mk8 = -(shamt >= limit) */
        "or      %[tmp2], %[shamt]          \n\t" /* shamt |= -(shamt >= limit) */
        "not     %[tmp2]                    \n\t"
        "shlx    %[tmp1], %[tmp2], %[tmp2]  \n\t" /* mask << st7 */
        "andn    %[val], %[tmp2], %[val]    " /* r & ~(mask << st7) */
        : [val] "=&r" (r.val), [shamt] "=&r" (r.shamt),
          [tmp1] "=&r" (tmp1), [tmp2] "=&r" (tmp2)
        : [vlu] "r" (vlu), [limit] "r" (limit)
        : "cc"
    );
    return r;
}
#else
static vlu_result vlu_decode_56c(uint64_t vlu, uint64_t limit = 8)
{
    int t1 = ctz(~vlu);
    bool cont = t1 >= limit;
    int shamt = cont ? limit : t1 + 1;
    uint64_t mask = ~(-(int64_t)!cont << (shamt * 7));
    uint64_t num = (vlu >> shamt) & mask;
    return vlu_result{ num, shamt | -(int64_t)cont };
}
#endif

/*
 * vlu_size_vec - calculate packed size in bytes
 */
static size_t vlu_size_vec(std::vector<uint64_t> &vec)
{
    size_t len = 0;
    for (uint64_t val : vec) {
        size_t shamt = vlu_encoded_size_56c(val);
        assert(shamt > 0 && shamt < 9);
        len += shamt;
    }
    return len;
}

/*
 * vlu_size_vec - get size of array
 */
#if USE_MISALIGNED_LOADS
static size_t vlu_items_vec(std::vector<uint8_t> &vec)
{
    size_t items = 0;
    size_t l = vec.size();
    for (size_t i = 0 ; i < l;) {
        uint64_t d = 0;
        size_t s = std::min((size_t)8,l-i);
        switch (s) {
        case 1: d = *reinterpret_cast<uint8_t*>(&vec[i]); break;
        case 2: d = *reinterpret_cast<uint16_t*>(&vec[i]); break;
        case 3: std::memcpy(&d, &vec[i], s); break;
        case 4: d = *reinterpret_cast<uint32_t*>(&vec[i]); break;
        case 5: case 6: case 7: std::memcpy(&d, &vec[i], s); break;
        case 8: d = *reinterpret_cast<uint64_t*>(&vec[i]); break;
        default: std::memcpy(&d, &vec[i], s); break;
        }
        size_t shamt = vlu_decoded_size_56c(d);
        assert(shamt > 0 && shamt < 9);
        i += shamt;
        items++;
    }
    return items;
}
#else
static size_t vlu_items_vec(std::vector<uint8_t> &vec)
{
    size_t items = 0;
    ptrdiff_t l = vec.size();

    ptrdiff_t shamt = 8;
    uint64_t lo = 0, hi;

    size_t s = std::min((size_t)8, (size_t)l);
    switch (s) {
    case 0: hi = 0; break;
    case 8: hi = *reinterpret_cast<uint64_t*>(&vec[0]); break;
    default: hi = 0; std::memcpy(&hi, &vec[0], s); break;
    }

    for (ptrdiff_t i = 0, j = i - shamt; i < l;) {

        if ((i>>3) > (j>>3)) {
            ptrdiff_t x = (i&~7)+8;
            lo = hi;
            size_t s = std::max((ptrdiff_t)0,std::min((ptrdiff_t)8, l-x));
            switch (s) {
            case 0: hi = 0; break;
            case 8: hi = *reinterpret_cast<uint64_t*>(&vec[x]); break;
            default: hi = 0; std::memcpy(&hi, &vec[x], s); break;
            }
        }
        size_t y = (i&7)<<3;
        uint64_t data = y == 0 ? lo : (lo >> y) | (hi << -y);

        shamt = vlu_decoded_size_56c(data);
        assert(shamt > 0 && shamt < 9);

        items++;
        j = i;
        i += shamt;
    }

    return items;
}
#endif

/*
 * vlu_encode_vec - encode array
 */
#if USE_MISALIGNED_LOADS
static void vlu_encode_vec(std::vector<uint8_t> &dst, std::vector<uint64_t> &src)
{
    size_t l = src.size();
    size_t o = 0;

    size_t items = vlu_size_vec(src);
    dst.resize(items);

    ptrdiff_t i = 0;
    for (uint64_t v : src)
    {
        vlu_result r = vlu_encode_56c(v);
        switch (r.shamt) {
        case 1: *reinterpret_cast<uint8_t*>(&dst[o]) = (uint8_t)r.val; break;
        case 2: *reinterpret_cast<uint16_t*>(&dst[o]) = (uint16_t)r.val; break;
        case 3: std::memcpy(&dst[o], &r.val, r.shamt); break;
        case 4: *reinterpret_cast<uint32_t*>(&dst[o]) = (uint32_t)r.val; break;
        case 5: case 6: case 7: std::memcpy(&dst[o], &r.val, r.shamt); break;
        case 8: *reinterpret_cast<uint64_t*>(&dst[o]) = r.val; break;
        default: std::memcpy(&dst[o], &r.val, r.shamt); break;
        }
        assert(r.shamt > 0 && r.shamt < 9);
        o += r.shamt;
    }
}
#else
static void vlu_encode_vec(std::vector<uint8_t> &dst, std::vector<uint64_t> &src)
{
    size_t o = 0;

    uint64_t lo = 0, hi = 0;

    size_t items = vlu_size_vec(src);
    dst.resize(items);
    ptrdiff_t l = dst.size();

    ptrdiff_t i = 0, j = 0;
    for (uint64_t v : src)
    {
        vlu_result r = vlu_encode_56c(v);

        size_t y = (i&7)<<3;
        if (y == 0) {
            lo = r.val;
        } else {
            lo |= (r.val << y);
            hi |= (r.val >> -y);
        }

        j = i;
        i += r.shamt;
        o++;

        if ((i>>3) > (j>>3)) {
            ptrdiff_t x = (i&~7)-8;
            size_t s = std::max((ptrdiff_t)0,std::min((ptrdiff_t)8, l-x));
            switch (s) {
            case 0: lo = 0; break;
            case 8: *reinterpret_cast<uint64_t*>(&dst[x]) = lo; lo = 0; break;
            default: std::memcpy(&dst[x], &lo, s); lo = 0; break;
            }
            lo = hi;
            hi = 0;
        }
    }
}
#endif

/*
 * vlu_decode_vec - decode array
 */
#if USE_MISALIGNED_LOADS
static void vlu_decode_vec(std::vector<uint64_t> &dst, std::vector<uint8_t> &src)
{
    size_t l = src.size();
    size_t i = 0, o = 0;

    size_t items = vlu_items_vec(src);
    dst.resize(items);

    for (; i < l - 8; )  {
        uint64_t d = *reinterpret_cast<uint64_t*>(&src[i]);
        vlu_result r = vlu_decode_56c(d);
        assert(r.shamt > 0);
        assert(o < items);
        dst[o] = r.val;
        i += r.shamt;
        o++;
    }

    for (; i < l; ) {
        uint64_t d = 0;
        size_t s = std::min((size_t)8,l-i);
        std::memcpy(&d, &src[i], s);
        vlu_result r = vlu_decode_56c(d);
        assert(r.shamt > 0);
        assert(o < items);
        dst[o] = r.val;
        i += r.shamt;
        o++;
    }
}
#else
static void vlu_decode_vec(std::vector<uint64_t> &dst, std::vector<uint8_t> &src)
{
    ptrdiff_t l = src.size();

    uint64_t lo = 0, hi;

    size_t items = vlu_items_vec(src);
    dst.resize(items);

    size_t s = std::min((size_t)8, (size_t)l);
    switch (s) {
    case 0: hi = 0; break;
    case 8: hi = *reinterpret_cast<uint64_t*>(&src[0]); break;
    default: hi = 0; std::memcpy(&hi, &src[0], s); break;
    }

    size_t o = 0;
    for (ptrdiff_t i = 0, j = i - 8; i < l;) {

        if ((i>>3) > (j>>3)) {
            ptrdiff_t x = (i&~7)+8;
            lo = hi;
            size_t s = std::max((ptrdiff_t)0,std::min((ptrdiff_t)8, l-x));
            switch (s) {
            case 0: hi = 0; break;
            case 8: hi = *reinterpret_cast<uint64_t*>(&src[x]); break;
            default: hi = 0; std::memcpy(&hi, &src[x], s); break;
            }
        }
        size_t y = (i&7)<<3;
        uint64_t data = y == 0 ? lo : (lo >> y) | (hi << -y);

        vlu_result r = vlu_decode_56c(data);
        assert(r.shamt > 0);
        assert(o < items);
        dst[o] = r.val;

        j = i;
        i += r.shamt;
        o++;
    }
}
#endif


/*
 * leb_encode_56 - LEB128 encoding up to 56-bits
 */
static vlu_result leb_encode_56(uint64_t num)
{
    uint64_t leb = 0;
    size_t i;
    for (i = 0; i < 8; i++) {
        int8_t b = (int8_t)extract_field<uint64_t>(num, 0, 7);
        num >>= 7;
        b |= (num == 0 ? 0 : 0x80);
        leb |= insert_field<uint64_t>(b, i*8, 8);
        if (num == 0) break;
    }
    return vlu_result{ leb, (int)i + 1 };
}

/*
 * leb_decode_56 - LEB128 decoding up to 56-bits
 */
static vlu_result leb_decode_56(uint64_t leb)
{
    uint64_t num = 0;
    size_t i;
    for (i = 0; i < 8; i++) {
        int8_t b = (int8_t)extract_field<uint64_t>(leb, i*8, 8);
        num |= insert_field<uint64_t>(b, i*7, 7);
        if (b >= 0) break;
    }
    return vlu_result{ num, (int)i + 1 };
}

/*
 * leb_encoded_size_56 - LEB128 packet size in bytes
 */
static int leb_encoded_size_56(uint64_t num)
{
    if (!num) return 1;
    int lz = clz(num);
    int t1 = 8 - ((lz - 1) / 7);
    bool cont = t1 > 7;
    return cont ? 8 : t1 + 1;
}

/*
 * leb_decoded_size_56 - LEB128 packet size in bytes
 */
static int leb_decoded_size_56(uint64_t leb)
{
    size_t i;
    for (i = 0; i < 8; i++) {
        int8_t b = (int8_t)extract_field<uint64_t>(leb, i*8, 8);
        if (b >= 0) break;
    }
    return (int)i + 1;
}

/*
 * leb_size_vec - calculate packed size in bytes
 */
static size_t leb_size_vec(std::vector<uint64_t> &vec)
{
    size_t len = 0;
    for (uint64_t val : vec) {
        size_t shamt = leb_encoded_size_56(val);
        assert(shamt > 0 && shamt < 9);
        len += shamt;
    }
    return len;
}

/*
 * leb_size_vec - get size of array
 */
static size_t leb_items_vec(std::vector<uint8_t> &vec)
{
    size_t items = 0;
    size_t l = vec.size();
    for (size_t i = 0 ; i < l;) {
        uint64_t d = 0;
        size_t s = std::min((size_t)8,l-i);
        switch (s) {
        case 1: d = *reinterpret_cast<uint8_t*>(&vec[i]); break;
        case 2: d = *reinterpret_cast<uint16_t*>(&vec[i]); break;
        case 3: std::memcpy(&d, &vec[i], s); break;
        case 4: d = *reinterpret_cast<uint32_t*>(&vec[i]); break;
        case 5: case 6: case 7: std::memcpy(&d, &vec[i], s); break;
        case 8: d = *reinterpret_cast<uint64_t*>(&vec[i]); break;
        default: std::memcpy(&d, &vec[i], s); break;
        }
        size_t shamt = leb_decoded_size_56(d);
        assert(shamt > 0 && shamt < 9);
        i += shamt;
        items++;
    }
    return items;
}

/*
 * leb_encode_vec - encode array
 */
static void leb_encode_vec(std::vector<uint8_t> &dst, std::vector<uint64_t> &src)
{
    size_t l = src.size();
    size_t o = 0;

    size_t items = leb_size_vec(src);
    dst.resize(items);

    for (uint64_t v : src)
    {
        vlu_result r = leb_encode_56(v);
        switch (r.shamt) {
        case 1: *reinterpret_cast<uint8_t*>(&dst[o]) = (uint8_t)r.val; break;
        case 2: *reinterpret_cast<uint16_t*>(&dst[o]) = (uint16_t)r.val; break;
        case 3: std::memcpy(&dst[o], &r.val, r.shamt); break;
        case 4: *reinterpret_cast<uint32_t*>(&dst[o]) = (uint32_t)r.val; break;
        case 5: case 6: case 7: std::memcpy(&dst[o], &r.val, r.shamt); break;
        case 8: *reinterpret_cast<uint64_t*>(&dst[o]) = r.val; break;
        default: std::memcpy(&dst[o], &r.val, r.shamt); break;
        }
        assert(r.shamt > 0 && r.shamt < 9);
        o += r.shamt;
    }
}

/*
 * leb_decode_vec - decode array
 */
static void leb_decode_vec(std::vector<uint64_t> &dst, std::vector<uint8_t> &src)
{
    size_t l = src.size();
    size_t i = 0, o = 0;

    size_t items = leb_items_vec(src);
    dst.resize(items);

    for (; i < l - 8; )  {
        uint64_t d = *reinterpret_cast<uint64_t*>(&src[i]);
        vlu_result r = leb_decode_56(d);
        assert(r.shamt > 0);
        assert(o < items);
        dst[o] = r.val;
        i += r.shamt;
        o++;
    }

    for (; i < l; ) {
        uint64_t d = 0;
        size_t s = std::min((size_t)8,l-i);
        std::memcpy(&d, &src[i], s);
        vlu_result r = leb_decode_56(d);
        assert(r.shamt > 0);
        assert(o < items);
        dst[o] = r.val;
        i += r.shamt;
        o++;
    }
}
