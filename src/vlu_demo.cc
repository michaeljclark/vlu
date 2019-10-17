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
 * binary string formatting
 */

std::string to_binary(uint64_t val)
{
    std::string s(71, ' ');
    for (size_t i = 0; i < 64; i++) {
        s[70-i-i/8] = ((val >> i) & 1) ? '1' : '-'; /* ░▒▓█ */
    }
    return s;
}

static void print_binary(const char* name, size_t aux, uint64_t val)
{
    printf("[%zu] %s= % 20" PRId64 " 0x%016" PRIx64 " (%s)\n",
        aux, name, val, val, to_binary(val).c_str());
}

static void print_one_uvlu(uint64_t val)
{
    uint64_t vlu = vlu_encode_56c(val);
    uint64_t num = vlu_decode_56c(vlu);
    size_t sz = vlu_size_56(val);

    print_binary("IN       ", sz, val);
    print_binary(" \\VLU    ", sz, vlu);
    print_binary("   \\OUT  ", sz, num);
    printf("%s\n", num == val ? "PASS" : "FAIL");
}

/*
 * output
 */

void test_output_uvlu()
{
    print_one_uvlu(0);
    print_one_uvlu(1);
    print_one_uvlu(2);
    print_one_uvlu(3);
    print_one_uvlu(4);
    print_one_uvlu(5);
    print_one_uvlu(6);
    print_one_uvlu(7);
    print_one_uvlu(8);
    print_one_uvlu(127);
    print_one_uvlu(128);
    print_one_uvlu(129);
    print_one_uvlu(255);
    print_one_uvlu(256);
    print_one_uvlu(257);
    print_one_uvlu(0x0088888888888888);
    print_one_uvlu(0x00ffffffffffffff);
    print_one_uvlu(0x0188888888888888);
    print_one_uvlu(0x01ffffffffffffff);
    print_one_uvlu(0x8888888888888888);
    print_one_uvlu(0xffffffffffffffff);
}

/*
 * main program
 */

int main(int argc, char **argv)
{
    test_output_uvlu();

    return 0;
}