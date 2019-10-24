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

/*
 * benchmark random numbers
 */

struct bench_random
{
    std::random_device random_device;
    std::default_random_engine random_engine;
    std::uniform_int_distribution<uint64_t> random_dist;

    bench_random() :
        random_engine(random_device()),
        random_dist(0,(1ull<<56)-1ull) {}
    bench_random(bench_random&&) : bench_random() {}

    uint64_t next_pure() {
        /* random numbers from 0 - 2^56-1 */
        return random_dist(random_engine);
    }

    uint64_t next_weighted() {
        /* random numbers from 0 - 2^56-1 */
        uint64_t val = random_dist(random_engine);
        /* (p=0.125 for each size) randomly choose 1 to 8 bytes */
        return val >> ((val & 0x7) << 3);
    }
};

/*
 * benchmark context
 */

struct bench_context
{
    const std::string name;
    const size_t item_count;
    const size_t iterations;

    std::vector<uint64_t> in;
    std::vector<uint64_t> out;
    bench_random random;

    bench_context(std::string name, const size_t item_count, const size_t iterations) :
        name(name), item_count(item_count), iterations(iterations) {}
};
