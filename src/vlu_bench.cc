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
#include "vlu_bench.h"

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
        ctx.in[i] = vlu_encode_56(ctx.random.next_pure()).val;
    }
}

static void setup_weighted_uvlu(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = vlu_encode_56(ctx.random.next_weighted()).val;
    }
}

static void setup_random_uleb(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = leb_encode_56(ctx.random.next_pure());
    }
}

static void setup_weighted_uleb(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = leb_encode_56(ctx.random.next_weighted());
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

static void bench_vlu_encode_56c(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = vlu_encode_56c(ctx.in[i]).val;
    }
}

static void bench_vlu_decode_56c(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = vlu_decode_56c(ctx.in[i]).val;
    }
}

static void bench_vlu_encode_56(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = vlu_encode_56(ctx.in[i]).val;
    }
}

static void bench_vlu_decode_56(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = vlu_decode_56(ctx.in[i]).val;
    }
}

static void bench_leb_encode_56(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = leb_encode_56(ctx.in[i]);
    }
}

static void bench_leb_decode_56(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = leb_decode_56(ctx.in[i]);
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
#define _NUM_ "%-10" PRIu64
    printf("|%-30s|" _NUM_ "|" _NUM_ "|" _NUM_ "|" _NUM_ "|%9.3lf |\n",
        ctx.name.c_str(), ctx.item_count, ctx.iterations,
        total_data_size, runtime_us, throughput_gbsec);
#undef _NUM_
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
 * main program
 */

template<typename C>
void run_benchmarks(size_t item_count, size_t iterations)
{
    bench_exec(C("BARE",                  item_count, iterations), setup_random,        bench_nop        );
    bench_exec(C("LEB_56 encode (random)",   item_count, iterations), setup_random,        bench_leb_encode_56);
    bench_exec(C("LEB_56 decode (random)",   item_count, iterations), setup_random_uleb,   bench_leb_decode_56);
    bench_exec(C("LEB_56 encode (weighted)", item_count, iterations), setup_weighted,      bench_leb_encode_56);
    bench_exec(C("LEB_56 decode (weighted)", item_count, iterations), setup_weighted_uleb, bench_leb_decode_56);
    bench_exec(C("VLU_56 encode (random)",   item_count, iterations), setup_random,        bench_vlu_encode_56);
    bench_exec(C("VLU_56 decode (random)",   item_count, iterations), setup_random_uvlu,   bench_vlu_decode_56);
    bench_exec(C("VLU_56 encode (weighted)", item_count, iterations), setup_weighted,      bench_vlu_encode_56);
    bench_exec(C("VLU_56 decode (weighted)", item_count, iterations), setup_weighted_uvlu, bench_vlu_decode_56);
    bench_exec(C("VLU_56C encode (random)",   item_count, iterations), setup_random,        bench_vlu_encode_56c);
    bench_exec(C("VLU_56C decode (random)",   item_count, iterations), setup_random_uvlu,   bench_vlu_decode_56c);
    bench_exec(C("VLU_56C encode (weighted)", item_count, iterations), setup_weighted,      bench_vlu_encode_56c);
    bench_exec(C("VLU_56C decode (weighted)", item_count, iterations), setup_weighted_uvlu, bench_vlu_decode_56c);
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

    print_header();
    run_benchmarks<bench_context>(1<<20, iterations);

    return 0;
}