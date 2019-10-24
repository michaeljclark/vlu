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

    assert(vlu_encode_56(0).val == 0b0);
    assert(vlu_encode_56(1).val == 0b10);
    assert(vlu_encode_56(2).val == 0b100);
    assert(vlu_encode_56(0x00ffffffffffffff).val == 0xffffffffffffff7f);

    assert(vlu_encode_56c(0).val == 0b0);
    assert(vlu_encode_56c(1).val == 0b10);
    assert(vlu_encode_56c(2).val == 0b100);
    assert(vlu_encode_56c(0x00ffffffffffffff).val == 0xffffffffffffff7f);

    for (size_t i = 0; i < 100; i++) {
        uint64_t val = random.next_pure();
        assert(vlu_decode_56(vlu_encode_56(val)) == val);
    }
}

void test_encode_uleb()
{
    bench_random random;

    assert(leb_decode_56(0x268EE5).val == 624485);
    assert(leb_encode_56(624485).val == 0x268EE5);
    assert(leb_decode_56(leb_encode_56(4521192081866880ull).val).val == 4521192081866880ull);

    for (size_t i = 0; i < 100; i++) {
        uint64_t val = random.next_pure();
        assert(leb_decode_56(leb_encode_56(val).val).val == val);
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