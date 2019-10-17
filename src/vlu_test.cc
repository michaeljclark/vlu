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
#include "vlu_bench.h"

/*
 * simple tests
 */

void test_encode_uvlu()
{
    bench_random random;

    assert(encode_uvlu_56(0) == 0b0);
    assert(encode_uvlu_56(1) == 0b10);
    assert(encode_uvlu_56(2) == 0b100);
    assert(encode_uvlu_56(0x00ffffffffffffff) == 0xffffffffffffff7f);

    assert(encode_uvlu_c(0) == 0b0);
    assert(encode_uvlu_c(1) == 0b10);
    assert(encode_uvlu_c(2) == 0b100);
    assert(encode_uvlu_c(0x00ffffffffffffff) == 0xffffffffffffff7f);

    for (size_t i = 0; i < 100; i++) {
        uint64_t val = random.next_pure();
        assert(decode_uvlu_56(encode_uvlu_56(val)) == val);
    }
}

void test_encode_uleb()
{
    bench_random random;

    assert(decode_uleb_56(0x268EE5) == 624485);
    assert(encode_uleb_56(624485) == 0x268EE5);
    assert(decode_uleb_56(encode_uleb_56(4521192081866880ull)) == 4521192081866880ull);

    for (size_t i = 0; i < 100; i++) {
        uint64_t val = random.next_pure();
        assert(decode_uleb_56(encode_uleb_56(val)) == val);
    }
}

/*
 * main program
 */


void run_tests()
{
    test_encode_uvlu();
    test_encode_uleb();
}

int main(int argc, char **argv)
{
    run_tests();

    return 0;
}