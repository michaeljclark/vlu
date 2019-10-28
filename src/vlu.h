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
 * vlu_size_56 - VLU8 packet size in bytes
 */
static int vlu_size_56(uint64_t num)
{
    int lz = clz(num);
    return 9 - ((lz - 1) / 7);
}

/*
 * vlu_size_56c - VLU8 packet size in bytes
 */
static int vlu_size_56c(uint64_t num)
{
    int lz = clz(num);
    int t1 = 8 - ((lz - 1) / 7);
    bool cont = t1 > 7;
    return cont ? 8 : t1 + 1;
}

/*
 * vlu_encode_56 - VLU8 encoding up to 56-bits
 */
static struct vlu_result vlu_encode_56(uint64_t num)
{
    int lz = clz(num);
    int t1 = 8 - ((lz - 1) / 7);
    int shamt = t1 + 1;
    uint64_t uvlu = (num << shamt)
        | (((num!=0) << (shamt-1))-(num!=0));
    return (vlu_result) { uvlu, shamt };
}

/*
 * vlu_decode_56 - VLU8 decoding up to 56-bits
 */
static struct vlu_result vlu_decode_56(uint64_t uvlu)
{
    int t1 = ctz(~uvlu);
    bool cond = t1 > 7;
    int shamt = cond ? 8 : t1 + 1;
    uint64_t mask = ~(-!cond << (shamt << 3));
    uint64_t num = (uvlu >> shamt) & mask;
    return (vlu_result) { num, shamt };
}

/*
 * vlu_encode_56c - VLU8 encoding with continuation support
 */
static struct vlu_result vlu_encode_56c(uint64_t num)
{
    int lz = clz(num);
    int t1 = 8 - ((lz - 1) / 7);
    bool cont = t1 > 7;
    int shamt = cont ? 8 : t1 + 1;
    uint64_t uvlu = (num << shamt)
        | (((num!=0) << (shamt-1))-(num!=0))
        | (cont << 8);
    return (vlu_result) { uvlu, shamt };
}

/*
 * vlu_decode_56c - VLU8 decoding with continuation support
 */

#if defined (__GNUC__) && defined(__x86_64__)
static vlu_result vlu_decode_56c(uint64_t vlu)
{
    /*
     * Optimized Intel assembly language
     *
     *    mov     rcx, rdi
     *    not     rcx
     *    tzcnt   rcx, rcx
     *    xor     rsi, rsi
     *    cmp     rcx, 7
     *    setne   sil
     *    lea     rdx, [rcx + 1]
     *    mov     rcx, 8
     *    cmovg   rdx, rcx
     *    shrx    rax, rdi, rdx
     *    lea     rcx, [rdx * 8]
     *    neg     rsi
     *    shlx    rsi, rsi, rcx
     *    andn    rax, rsi, rax
     *    ret     ; struct { rax; rdx; }
     */
    struct vlu_result r;
    uint64_t tmp1, tmp2;
    asm volatile (
        ".intel_syntax                      \n\t"
        "mov     %[tmp1], %[vlu]            \n\t"
        "not     %[tmp1]                    \n\t" /* ~vlu */
        "tzcnt   %[tmp1], %[tmp1]           \n\t" /* tz = ctz(~vlu) */
        "xor     %[tmp2], %[tmp2]           \n\t"
        "cmp     %[tmp1], 7                 \n\t"
        "setne   %b[tmp2]                   \n\t" /* shamt > 7 */
        "lea     %[shamt], [%[tmp1] + 1]    \n\t" /* shamt = tz + 1*/
        "mov     %[tmp1], 8                 \n\t"
        "cmovg   %[shamt], %[tmp1]          \n\t" /* shamt > 7 ? 8 : shamt */
        "shrx    %[val], %[vlu], %[shamt]   \n\t" /* r = vlu >> shamt */
        "lea     %[tmp1], [%[shamt] * 8]    \n\t" /* sh8 = shamt << 3 */
        "neg     %[tmp2]                    \n\t" /* mk8 = -(shamt > 7) */
        "shlx    %[tmp2], %[tmp2], %[tmp1]  \n\t" /* mk8 << sh8 */
        "andn    %[val], %[tmp2], %[val]    \n\t" /* r & ~(mk8 << sh8) */
        ".att_syntax"
        : [val] "=&r" (r.val), [shamt] "=&r" (r.shamt),
          [tmp1] "=&r" (tmp1), [tmp2] "=&r" (tmp2)
        : [vlu] "r" (vlu)
        : "cc"
    );
    return r;
}
#else
static vlu_result vlu_decode_56c(uint64_t vlu)
{
    int t1 = ctz(~vlu);
    bool cont = t1 > 7;
    int shamt = cont ? 8 : t1 + 1;
    uint64_t mask = ~(-!cont << (shamt << 3));
    uint64_t num = (vlu >> shamt) & mask;
    return vlu_result{ num, shamt };
}
#endif

/*
 * vlu_encode_vec - encode array
 */
static void vlu_encode_vec(std::vector<uint8_t> &dst, std::vector<uint64_t> &src)
{
    size_t l = src.size();
    for (size_t i = 0 ; i < l; i++) {
        vlu_result r = vlu_encode_56c(src[i]);
        uint8_t *p = reinterpret_cast<uint8_t*>(&r.val);
        size_t o = dst.size();
        dst.resize(o + r.shamt);
        std::memcpy(&dst[o], p, r.shamt);
    }
}

/*
 * vlu_decode_vec - decode array
 */
static void vlu_decode_vec(std::vector<uint64_t> &dst, std::vector<uint8_t> &src)
{
    size_t l = src.size();
    for (size_t i = 0 ; i < l;) {
        uint64_t d = 0;
        std::memcpy(&d, &src[i], std::min((size_t)8,l-i));
        vlu_result r = vlu_decode_56c(d);
        dst.push_back(r.val);
        i += r.shamt;
    }
}

/*
 * leb_encode_56 - LEB128 encoding up to 56-bits
 */
static uint64_t leb_encode_56(uint64_t num)
{
    uint64_t orig = num;
    uint64_t leb = 0;
    for (size_t i = 0; i < 8; i++) {
        int8_t b = (int8_t)extract_field<uint64_t>(num, 0, 7);
        num >>= 7;
        b |= (num == 0 ? 0 : 0x80);
        leb |= insert_field<uint64_t>(b, i*8, 8);
        if (num == 0) break;
    }
    return leb;
}

/*
 * leb_decode_56 - LEB128 decoding up to 56-bits
 */
static uint64_t leb_decode_56(uint64_t leb)
{
    uint64_t num = 0;
    for (size_t i = 0; i < 8; i++) {
        int8_t b = (int8_t)extract_field<uint64_t>(leb, i*8, 8);
        num |= insert_field<uint64_t>(b, i*7, 7);
        if (b >= 0) break;
    }
    return num;
}
