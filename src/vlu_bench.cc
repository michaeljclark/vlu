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
#include <cstdlib>
#include <cassert>

#include <memory>
#include <limits>
#include <random>
#include <chrono>
#include <iomanip>
#include <sstream>

#include "vlu.h"
#include "vlu_bench.h"


/*
 * number to string
 */

static std::unique_ptr<char> make_string(uint64_t val, const char* fmt)
{
    char buf[32];
    snprintf(buf, sizeof(buf), fmt, (long long)val);
    auto p = std::unique_ptr<char>(new char [std::strlen(buf) + 1]);
    std::memcpy(p.get(), buf, std::strlen(buf) + 1);
    return p;
}


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

static void setup_random_str(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.strbuf.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = ctx.random.next_pure();
        ctx.strbuf[i] = std::unique_ptr<char>(new char [32]);
    }
}

static void setup_weighted_str(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    ctx.strbuf.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = ctx.random.next_weighted();
        ctx.strbuf[i] = std::unique_ptr<char>(new char [32]);
    }
}

static void setup_random_dec(bench_context &ctx)
{
    ctx.strbuf.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.strbuf[i] = make_string(ctx.random.next_pure(), "%lld");
    }
}

static void setup_weighted_dec(bench_context &ctx)
{
    ctx.strbuf.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.strbuf[i] = make_string(ctx.random.next_weighted(), "%lld");
    }
}

static void setup_random_hex(bench_context &ctx)
{
    ctx.strbuf.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.strbuf[i] = make_string(ctx.random.next_pure(), "%llx");
    }
}

static void setup_weighted_hex(bench_context &ctx)
{
    ctx.strbuf.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.strbuf[i] = make_string(ctx.random.next_weighted(), "%llx");
    }
}
static void setup_random_vec(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = ctx.random.next_pure();
    }
    vlu_encode_vec(ctx.vbuf, ctx.in);
}

static void setup_weighted_vec(bench_context &ctx)
{
    ctx.in.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = ctx.random.next_weighted();
    }
    vlu_encode_vec(ctx.vbuf, ctx.in);
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

static void bench_snprintf_dec_encode_56(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        snprintf(ctx.strbuf[i].get(), 32, "%lld", (long long)ctx.in[i]);
    }
}

static void bench_strtoull_dec_decode_56(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = strtoull(ctx.strbuf[i].get(), nullptr, 10);
    }
}

static void bench_snprintf_hex_encode_56(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        snprintf(ctx.strbuf[i].get(), 32, "%llx", (long long)ctx.in[i]);
    }
}

static void bench_strtoull_hex_decode_56(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = strtoull(ctx.strbuf[i].get(), nullptr, 16);
    }
}

static void bench_vlu_encode_vec(bench_context &ctx)
{
    vlu_encode_vec(ctx.vbuf, ctx.in);
}

static void bench_vlu_decode_vec(bench_context &ctx)
{
    vlu_decode_vec(ctx.out, ctx.vbuf);
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
static int bench_exec(C &&ctx, F setup, F bench)
{
    setup(ctx);

    for (size_t j = 0; j < ctx.runs; j++) {

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

    return 0;
}

/*
 * main program
 */

template<typename C>
int run_benchmark(size_t item_count, size_t benchmark, size_t runs, size_t iterations)
{
    switch (benchmark) {
    case 0:  return bench_exec(C("BARE",                          item_count, runs, iterations), setup_random,        bench_nop        );
    case 1:  return bench_exec(C("LEB_56 encode (random)",        item_count, runs, iterations), setup_random,        bench_leb_encode_56);
    case 2:  return bench_exec(C("LEB_56 encode (weighted)",      item_count, runs, iterations), setup_weighted,      bench_leb_encode_56);
    case 3:  return bench_exec(C("LEB_56 decode (random)",        item_count, runs, iterations), setup_random_uleb,   bench_leb_decode_56);
    case 4:  return bench_exec(C("LEB_56 decode (weighted)",      item_count, runs, iterations), setup_weighted_uleb, bench_leb_decode_56);
    case 5:  return bench_exec(C("VLU_56 encode (random)",        item_count, runs, iterations), setup_random,        bench_vlu_encode_56);
    case 6:  return bench_exec(C("VLU_56 encode (weighted)",      item_count, runs, iterations), setup_weighted,      bench_vlu_encode_56);
    case 7:  return bench_exec(C("VLU_56 decode (random)",        item_count, runs, iterations), setup_random_uvlu,   bench_vlu_decode_56);
    case 8:  return bench_exec(C("VLU_56 decode (weighted)",      item_count, runs, iterations), setup_weighted_uvlu, bench_vlu_decode_56);
    case 9:  return bench_exec(C("VLU_56-C encode (random)",      item_count, runs, iterations), setup_random,        bench_vlu_encode_56c);
    case 10: return bench_exec(C("VLU_56-C encode (weighted)",    item_count, runs, iterations), setup_weighted,      bench_vlu_encode_56c);
    case 11: return bench_exec(C("VLU_56-C decode (random)",      item_count, runs, iterations), setup_random_uvlu,   bench_vlu_decode_56c);
    case 12: return bench_exec(C("VLU_56-C decode (weighted)",    item_count, runs, iterations), setup_weighted_uvlu, bench_vlu_decode_56c);
    case 13: return bench_exec(C("VLU_56-vec encode (random)",    item_count, runs, iterations), setup_random,        bench_vlu_encode_vec);
    case 14: return bench_exec(C("VLU_56-vec encode (weighted)",  item_count, runs, iterations), setup_weighted,      bench_vlu_encode_vec);
    case 15: return bench_exec(C("VLU_56-vec decode (random)",    item_count, runs, iterations), setup_random_vec,    bench_vlu_decode_vec);
    case 16: return bench_exec(C("VLU_56-vec decode (weighted)",  item_count, runs, iterations), setup_weighted_vec,  bench_vlu_decode_vec);
    case 17: return bench_exec(C("snprintf/10 encode (random)",   item_count, runs, iterations), setup_random_str,    bench_snprintf_dec_encode_56);
    case 18: return bench_exec(C("snprintf/10 encode (weighted)", item_count, runs, iterations), setup_weighted_str,  bench_snprintf_dec_encode_56);
    case 19: return bench_exec(C("strtoull/10 decode (random)",   item_count, runs, iterations), setup_random_dec,    bench_strtoull_dec_decode_56);
    case 20: return bench_exec(C("strtoull/10 decode (weighted)", item_count, runs, iterations), setup_weighted_dec,  bench_strtoull_dec_decode_56);
    case 21: return bench_exec(C("snprintf/16 encode (random)",   item_count, runs, iterations), setup_random_str,    bench_snprintf_hex_encode_56);
    case 22: return bench_exec(C("snprintf/16 encode (weighted)", item_count, runs, iterations), setup_weighted_str,  bench_snprintf_hex_encode_56);
    case 23: return bench_exec(C("strtoull/16 decode (random)",   item_count, runs, iterations), setup_random_hex,    bench_strtoull_hex_decode_56);
    case 24: return bench_exec(C("strtoull/16 decode (weighted)", item_count, runs, iterations), setup_weighted_hex,  bench_strtoull_hex_decode_56);
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "print_header") == 0) {
        print_header();
    } else if (argc == 4) {
        run_benchmark<bench_context>(1<<20, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));
    } else {
        fprintf(stderr, "usage: %s <benchmark> <runs> <iterations>\n", argv[0]);
        exit(1);
    }

    return 0;
}