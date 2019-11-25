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

#include <cstdio>
#include <cstdint>
#include <cinttypes>
#include <cassert>

#include <limits>
#include <random>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

#include "vlu.h"

/*
 * random numbers
 */

struct bench_random
{
    std::random_device random_device;
    std::default_random_engine random_engine;
    std::uniform_int_distribution<uint64_t> random_dist_8;
    std::uniform_int_distribution<uint64_t> random_dist_56;

    bench_random() :
        random_engine(random_device()),
        random_dist_8(0,(1ull<<8)-1ull),
        random_dist_56(0,(1ull<<56)-1ull) {}
    bench_random(bench_random&&) : bench_random() {}

    uint64_t pure_8() {
        /* random numbers from 0 - 2^8-1 */
        return random_dist_8(random_engine);
    }

    uint64_t pure_56() {
        /* random numbers from 0 - 2^56-1 */
        return random_dist_56(random_engine);
    }

    uint64_t mix_56() {
        /* random numbers from 0 - 2^56-1 */
        uint64_t val = random_dist_56(random_engine);
        /* (p=0.125 for each size) randomly choose 1 to 8 bytes */
        return val >> ((val & 0x7) << 3);
    }
};

/*
 * simple tests
 */

void test_encode_uvlu()
{
    bench_random random;

    assert(vlu_encoded_size_56(0) == 1);
    assert(vlu_encoded_size_56(1) == 1);
    assert(vlu_encoded_size_56(2) == 1);
    assert(vlu_encoded_size_56(0x123456789abcde) == 8);
    assert(vlu_encoded_size_56(0xffffffffffffff) == 8);

    assert(vlu_decoded_size_56(0b0000) == 1);
    assert(vlu_decoded_size_56(0b0001) == 2);
    assert(vlu_decoded_size_56(0b0101) == 2);
    assert(vlu_decoded_size_56(0b0011) == 3);
    assert(vlu_decoded_size_56(0b1011) == 3);
    assert(vlu_decoded_size_56(0x123456789abcde7f) == 8);
    assert(vlu_decoded_size_56(0xffffffffffffff7f) == 8);

    assert(vlu_encode_56(0).val == 0b0);
    assert(vlu_encode_56(0).shamt == 1);
    assert(vlu_encode_56(1).val == 0b10);
    assert(vlu_encode_56(1).shamt == 1);
    assert(vlu_encode_56(2).val == 0b100);
    assert(vlu_encode_56(2).shamt == 1);
    assert(vlu_encode_56(0x00123456789abcde).val == 0x123456789abcde7f);
    assert(vlu_encode_56(0x00123456789abcde).shamt == 8);
    assert(vlu_encode_56(0x00ffffffffffffff).val == 0xffffffffffffff7f);
    assert(vlu_encode_56(0x00ffffffffffffff).shamt == 8);
    assert(vlu_encode_56(0xffffffffffffffff).val == 0xffffffffffffffff);
    assert(vlu_encode_56(0xffffffffffffffff).shamt == -1); /* continuation */

    assert(vlu_decode_56(0b0000).val == 0b0);
    assert(vlu_decode_56(0b0000).shamt == 1);
    assert(vlu_decode_56(0b0001).val == 0);
    assert(vlu_decode_56(0b0001).shamt == 2);
    assert(vlu_decode_56(0b0101).val == 1);
    assert(vlu_decode_56(0b0101).shamt == 2);
    assert(vlu_decode_56(0b0011).val == 0);
    assert(vlu_decode_56(0b0011).shamt == 3);
    assert(vlu_decode_56(0b1011).val == 1);
    assert(vlu_decode_56(0b1011).shamt == 3);
    assert(vlu_decode_56(0x123456789abcde7f).val == 0x123456789abcde);
    assert(vlu_decode_56(0x123456789abcde7f).shamt == 8);
    assert(vlu_decode_56(0xffffffffffffff7f).val == 0x00ffffffffffffff);
    assert(vlu_decode_56(0xffffffffffffff7f).shamt == 8);
    assert(vlu_decode_56(0xffffffffffffffff).val == 0x00ffffffffffffff);
    assert(vlu_decode_56(0xffffffffffffffff).shamt == -1); /* continuation */

    /* mask test */
    assert(vlu_decode_56(0xff80 | vlu_encode_56(0x7d).val).val == 0x7d);

    for (size_t i = 0; i < 100; i++) {
        uint64_t val = random.mix_56() & ((1ull<<56)-1ull);
        uint64_t enc = vlu_encode_56(val).val;
        uint64_t dec = vlu_decode_56(enc).val;
        /*
        printf(
            "val=0x%016" PRIx64
            " -> enc=0x%016" PRIx64
            " <- dec=0x%016" PRIx64 "\n",
            val, enc, dec
        );
        */
        assert(dec == val);
    }
}

void test_roundtrip_uvlu_u7()
{
    std::vector<uint64_t> d1 = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
    };
    std::vector<uint8_t> d2;
    std::vector<uint64_t> d3;
    vlu_encode_vec(d2, d1);
    vlu_decode_vec(d3, d2);
    assert(d2.size() == 16);
    assert(d1.size() == d3.size());
    for (size_t i = 0; i < d1.size(); i++) {
        assert(d1[i] == d3[i]);
    }
}

void test_roundtrip_uvlu_u14()
{
    std::vector<uint64_t> d1 = {
          250,   256,   257,   258,
         1000,  1001,  1002,  1003,
         5000,  5001,  5002,  5003,
        10000, 10001, 10000, 10001,
    };
    std::vector<uint8_t> d2;
    std::vector<uint64_t> d3;
    vlu_encode_vec(d2, d1);
    vlu_decode_vec(d3, d2);
    assert(d2.size() == 32);
    assert(d1.size() == d3.size());
    for (size_t i = 0; i < d1.size(); i++) {
        assert(d1[i] == d3[i]);
    }
}

void test_roundtrip_uvlu_u21()
{
    std::vector<uint64_t> d1 = {
        32768,  32769,  32770,  32771,
        65536,  65537,  65538,  65539,
        90000,  90001,  90000,  90001,
       100000, 100001, 100000, 100001,
    };
    std::vector<uint8_t> d2;
    std::vector<uint64_t> d3;
    vlu_encode_vec(d2, d1);
    vlu_decode_vec(d3, d2);
    assert(d2.size() == 48);
    assert(d1.size() == d3.size());
    for (size_t i = 0; i < d1.size(); i++) {
        assert(d1[i] == d3[i]);
    }
}

void test_encode_uleb()
{
    bench_random random;

    assert(leb_decode_56(0x268EE5).val == 624485);
    assert(leb_encode_56(624485).val == 0x268EE5);
    assert(leb_decode_56(leb_encode_56(4521192081866880ull).val).val == 4521192081866880ull);

    for (size_t i = 0; i < 100; i++) {
        uint64_t val = random.mix_56();
        assert(leb_decode_56(leb_encode_56(val).val).val == val);
    }
}

void test_roundtrip_uleb_u7()
{
    std::vector<uint64_t> d1 = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
    };
    std::vector<uint8_t> d2;
    std::vector<uint64_t> d3;
    leb_encode_vec(d2, d1);
    leb_decode_vec(d3, d2);
    assert(d2.size() == 16);
    assert(d1.size() == d3.size());
    for (size_t i = 0; i < d1.size(); i++) {
        assert(d1[i] == d3[i]);
    }
}

void test_roundtrip_uleb_u14()
{
    std::vector<uint64_t> d1 = {
          250,   256,   257,   258,
         1000,  1001,  1002,  1003,
         5000,  5001,  5002,  5003,
        10000, 10001, 10000, 10001,
    };
    std::vector<uint8_t> d2;
    std::vector<uint64_t> d3;
    leb_encode_vec(d2, d1);
    leb_decode_vec(d3, d2);
    assert(d2.size() == 32);
    assert(d1.size() == d3.size());
    for (size_t i = 0; i < d1.size(); i++) {
        assert(d1[i] == d3[i]);
    }
}

void test_roundtrip_uleb_u21()
{
    std::vector<uint64_t> d1 = {
        32768,  32769,  32770,  32771,
        65536,  65537,  65538,  65539,
        90000,  90001,  90000,  90001,
       100000, 100001, 100000, 100001,
    };
    std::vector<uint8_t> d2;
    std::vector<uint64_t> d3;
    leb_encode_vec(d2, d1);
    leb_decode_vec(d3, d2);
    assert(d2.size() == 48);
    assert(d1.size() == d3.size());
    for (size_t i = 0; i < d1.size(); i++) {
        assert(d1[i] == d3[i]);
    }
}

/*
 * main program
 */


void run_tests()
{
    test_encode_uvlu();
    test_roundtrip_uvlu_u7();
    test_roundtrip_uvlu_u14();
    test_roundtrip_uvlu_u21();
    test_encode_uleb();
    test_roundtrip_uleb_u7();
    test_roundtrip_uleb_u14();
    test_roundtrip_uleb_u21();
}

int main(int argc, char **argv)
{
    run_tests();

    return 0;
}