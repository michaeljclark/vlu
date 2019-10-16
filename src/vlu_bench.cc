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

#include "vlu.h"

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

    uint64_t next_pure() {
        /* random numbers from 0 - 2^56-1 */
        return random_dist(random_engine);
    }

    uint64_t next_weighted() {
        /* random numbers from 0 - 2^56-1 */
        uint64_t val = random_dist(random_engine);
        /* (p=0.125 for each size) randomly choose 1 to 8 bytes */
        switch (val & 0x7) {
        case 0: return val >> 56;
        case 1: return val >> 48;
        case 2: return val >> 40;
        case 3: return val >> 32;
        case 4: return val >> 24;
        case 5: return val >> 16;
        case 6: return val >> 8;
        case 7: return val;
        default: break;
        }
        __builtin_unreachable();
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

/*
 * benchmark setup
 */

static void setup_random(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = ctx.random.next_pure();
    }
}

static void setup_weighted(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = ctx.random.next_weighted();
    }
}

static void setup_random_uvlu(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = encode_uvlu(ctx.random.next_pure());
    }
}

static void setup_weighted_uvlu(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = encode_uvlu(ctx.random.next_weighted());
    }
}

static void setup_random_uleb(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = encode_uleb(ctx.random.next_pure());
    }
}

static void setup_weighted_uleb(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = encode_uleb(ctx.random.next_weighted());
    }
}

/*
 * benchmarks
 */

static void bench_nop(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = ctx.in[i];
    }
}

static void bench_encode_uvlu(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = encode_uvlu(ctx.in[i]);
    }
}

static void bench_decode_uvlu(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = decode_uvlu(ctx.in[i]);
    }
}

static void bench_encode_uleb(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = encode_uleb(ctx.in[i]);
    }
}

static void bench_decode_uleb(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = decode_uleb(ctx.in[i]);
    }
}

/*
 * benchmark result formatting
 */

struct bench_mem_field {
    const char *name;
    const char *fmt;
    size_t width;
};

static const bench_mem_field bench_mem_fields[] = {
    /* name */            /* fmt    width */
    { "Benchmark",           "%-20s",   30 },
    { "Item count",          "%-10u",   10 },
    { "Iterations",          "%-10u",   10 },
    { "Size KiB",            "%-10u",   10 },
    { "Time µs   ", /*dumb*/ "%-10u",   10 },
    { "GiB/sec",             "%-9.3lf", 10 },
};

static void print_header()
{
    std::stringstream h1, h2;
    for (const auto &f : bench_mem_fields) {
        h1 << std::left << std::setfill(' ') << std::setw(f.width) << f.name << "|";
        h2 << std::right << std::setfill('-') << std::setw(f.width+1) << "|";
    }
    printf("|%s\n|%s\n", h1.str().c_str(), h2.str().c_str());
}

static void print_data(bench_context &ctx, size_t total_data_size,
                       size_t runtime_us, double throughput_gbsec)
{
   printf("|%-30s|%-10lu|%-10lu|%-10lu|%-10lu|%9.3lf |\n",
            ctx.name.c_str(),
            ctx.item_count,
            ctx.iterations,
            total_data_size,
            runtime_us,
            throughput_gbsec);
}

/*
 * benchmark timing and execution
 */

template <typename C, typename F>
static void bench_exec(C ctx, F setup, F bench)
{
    setup(ctx);

    const auto t1 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < ctx.iterations; i++) bench(ctx);
    const auto t2 = std::chrono::high_resolution_clock::now();

    size_t total_data_size = ctx.item_count * ctx.iterations * sizeof(uint64_t);
    auto runtime_us = std::chrono::duration_cast
        <std::chrono::microseconds>(t2 - t1).count();
    double throughput_gbsec = (double)(total_data_size /
        (double)(1<<30)) / ((double)runtime_us / 1e6);

    print_data(ctx, total_data_size/(1<<20), runtime_us, throughput_gbsec);
}

/*
 * simple tests
 */

void test_encode_uvlu()
{
    bench_random random;

    for (size_t i = 0; i < 100; i++) {
        uint64_t val = random.next_pure();
        assert(decode_uvlu(encode_uvlu(val)) == val);
    }
}

void test_encode_uleb()
{
    bench_random random;

    assert(decode_uleb(0x268EE5) == 624485);
    assert(encode_uleb(624485) == 0x268EE5);
    assert(decode_uleb(encode_uleb(4521192081866880ull)) == 4521192081866880ull);

    for (size_t i = 0; i < 100; i++) {
        uint64_t val = random.next_pure();
        assert(decode_uleb(encode_uleb(val)) == val);
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

template<typename C>
void run_benchmarks(size_t item_count, size_t iterations)
{
    bench_exec(C("BARE",                  item_count, iterations), setup_random,        bench_nop        );
    bench_exec(C("LEB encode (random)",   item_count, iterations), setup_random,        bench_encode_uleb);
    bench_exec(C("LEB decode (random)",   item_count, iterations), setup_random_uleb,   bench_decode_uleb);
    bench_exec(C("LEB encode (weighted)", item_count, iterations), setup_weighted,      bench_encode_uleb);
    bench_exec(C("LEB decode (weighted)", item_count, iterations), setup_weighted_uleb, bench_decode_uleb);
    bench_exec(C("VLU encode (random)",   item_count, iterations), setup_random,        bench_encode_uvlu);
    bench_exec(C("VLU decode (random)",   item_count, iterations), setup_random_uvlu,   bench_decode_uvlu);
    bench_exec(C("VLU encode (weighted)", item_count, iterations), setup_weighted,      bench_encode_uvlu);
    bench_exec(C("VLU decode (weighted)", item_count, iterations), setup_weighted_uvlu, bench_decode_uvlu);
}

int main(int argc, char **argv)
{
    size_t iterations = 0;

    if (argc == 2) {
        iterations = atoi(argv[1]);
    } else {
        fprintf(stderr, "usage: %s <iterations>\n", argv[0]);
        exit(1);
    }

    run_tests();
    print_header();
    run_benchmarks<bench_context>(1<<20, iterations);

    return 0;
}