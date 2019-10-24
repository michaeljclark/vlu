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

/*
 * vlu_size_56 - VLU8 packet size in bytes
 */
static int vlu_size_56(uint64_t num)
{
    int lz = clz(num);
    return 9 - ((lz - 1) / 7);
}

/*
 * vlu_encode_56 - VLU8 encoding up to 56-bits
 */
static uint64_t vlu_encode_56(uint64_t num)
{
    int lz = clz(num);
    int t1 = 8 - ((lz - 1) / 7);
    int shamt = t1 + 1;
    uint64_t uvlu = (num << shamt)
        | (((num!=0) << (shamt-1))-(num!=0));
    return uvlu;
}

/*
 * vlu_decode_56 - VLU8 decoding up to 56-bits
 */
static uint64_t vlu_decode_56(uint64_t uvlu)
{
    int t1 = ctz(~uvlu);
    int shamt = t1 + 1;
    uint64_t num = uvlu >> shamt;
    return num;
}

/*
 * vlu_encode_56c - VLU8 encoding with continuation support
 */
static uint64_t vlu_encode_56c(uint64_t num)
{
    int lz = clz(num);
    int t1 = 8 - ((lz - 1) / 7);
    bool cont = t1 > 7;
    int shamt = cont ? 8 : t1 + 1;
    uint64_t uvlu = (num << shamt)
        | (((num!=0) << (shamt-1))-(num!=0))
        | (-(int)cont & 0x80);
    return uvlu;
}

/*
 * vlu_decode_56c - VLU8 decoding with continuation support
 */

struct vlu_result
{
    uint64_t val;
    int64_t shamt;
};

#if defined (__GNUC__) && defined(__x86_64__)
static vlu_result vlu_decode_56c(uint64_t vlu)
{
    /*
     * Optimized Intel assembly language
     *
     *    mov     rax, rdi
     *    not     rax
     *    tzcnt   rax, rax
     *    cmp     rax, 0x7
     *    lea     rdx, [rax + 1]
     *    mov     rax, 0x8
     *    cmovg   rdx, rax
     *    shrx    rax, rdi, rdx
     *    ret     ; struct { rax; rdx; }
     */
    struct vlu_result r;
    uint64_t tmp1;
    asm volatile (
        "tzcnt   %[tmp1], %[tmp1]           \n\t"
        "cmp     $0x7, %[tmp1]              \n\t"
        "lea     1(%[tmp1]),%[shamt]        \n\t"
        "mov     $0x8, %[tmp1]              \n\t"
        "cmovg   %[tmp1], %[shamt]          \n\t"
        "shrx    %[shamt], %[vlu], %[val]   "
        : [val] "=r" (r.val), [shamt] "=r" (r.shamt)
        : [vlu] "r" (vlu), [tmp1] "r" (~vlu)
        : "cc"
    );
    return r;
}
#else
static vlu_result vlu_decode_56c(uint64_t vlu)
{
    int t1 = ctz(~vlu);
    int shamt = t1 > 7 ? 8 : t1 + 1;
    return vlu_result{ vlu >> shamt, shamt };
}
#endif

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
