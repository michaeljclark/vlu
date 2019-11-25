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
 * benchmark context
 */

struct bench_context
{
    const std::string name;
    const size_t item_count;
    const size_t runs;
    const size_t iterations;

    std::vector<uint64_t> in;
    std::vector<uint64_t> out;
    std::vector<std::unique_ptr<char>> strbuf;
    std::vector<uint8_t> vbuf;
    bench_random random;

    bench_context(std::string name, size_t item_count, size_t runs, size_t iterations) :
        name(name), item_count(item_count), runs(runs), iterations(iterations) {}
};

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
 * random number functions
 */

static uint64_t random_8(bench_context &ctx) { return ctx.random.pure_8(); }
static uint64_t random_56(bench_context &ctx) { return ctx.random.pure_56(); }
static uint64_t random_mix(bench_context &ctx) { return ctx.random.mix_56(); }


/*
 * benchmark setup
 */

static void setup_dfl(bench_context &ctx, uint64_t(*rnd)(bench_context&))
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = rnd(ctx);
    }
}

static void setup_uvlu(bench_context &ctx, uint64_t(*rnd)(bench_context&))
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = vlu_encode_56(rnd(ctx)).val;
    }
}

static void setup_uleb(bench_context &ctx, uint64_t(*rnd)(bench_context&))
{
    ctx.in.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = leb_encode_56(rnd(ctx)).val;
    }
}

static void setup_str(bench_context &ctx, uint64_t(*rnd)(bench_context&))
{
    ctx.in.resize(ctx.item_count);
    ctx.strbuf.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = rnd(ctx);
        ctx.strbuf[i] = std::unique_ptr<char>(new char [32]);
    }
}

static void setup_dec(bench_context &ctx, uint64_t(*rnd)(bench_context&))
{
    ctx.strbuf.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.strbuf[i] = make_string(rnd(ctx), "%lld");
    }
}

static void setup_hex(bench_context &ctx, uint64_t(*rnd)(bench_context&))
{
    ctx.strbuf.resize(ctx.item_count);
    ctx.out.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.strbuf[i] = make_string(rnd(ctx), "%llx");
    }
}

static void setup_vec(bench_context &ctx, uint64_t(*rnd)(bench_context&))
{
    ctx.in.resize(ctx.item_count);
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.in[i] = rnd(ctx);
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
        ctx.out[i] = leb_encode_56(ctx.in[i]).val;
    }
}

static void bench_leb_decode_56(bench_context &ctx)
{
    for (size_t i = 0; i < ctx.item_count; i++) {
        ctx.out[i] = leb_decode_56(ctx.in[i]).val;
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

static void bench_leb_encode_vec(bench_context &ctx)
{
    leb_encode_vec(ctx.vbuf, ctx.in);
}

static void bench_leb_decode_vec(bench_context &ctx)
{
    leb_decode_vec(ctx.out, ctx.vbuf);
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
    { "Benchmark",           "%-32s",   32 },
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
    printf("|%-32s|" _NUM_ "|" _NUM_ "|" _NUM_ "|" _NUM_ "|%9.3lf |\n",
        ctx.name.c_str(), ctx.item_count, ctx.iterations,
        total_data_size, runtime_us, throughput_gbsec);
#undef _NUM_
}

/*
 * benchmark timing and execution
 */

template <typename C, typename S, typename R, typename B>
static int bench_exec(C &&ctx, S setup, R random, B bench)
{
    setup(ctx, random);

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
    case 0:  return bench_exec(C("BARE",                            item_count, runs, iterations), setup_dfl,  random_56,  bench_nop        );
    case 1:  return bench_exec(C("LEB_56-raw encode (random-8)",    item_count, runs, iterations), setup_dfl,  random_8,   bench_leb_encode_56);
    case 2:  return bench_exec(C("LEB_56-raw encode (random-56)",   item_count, runs, iterations), setup_dfl,  random_56,  bench_leb_encode_56);
    case 3:  return bench_exec(C("LEB_56-raw encode (random-mix)",  item_count, runs, iterations), setup_dfl,  random_mix, bench_leb_encode_56);
    case 4:  return bench_exec(C("LEB_56-raw decode (random-8)",    item_count, runs, iterations), setup_uleb, random_8,   bench_leb_decode_56);
    case 5:  return bench_exec(C("LEB_56-raw decode (random-56)",   item_count, runs, iterations), setup_uleb, random_56,  bench_leb_decode_56);
    case 6:  return bench_exec(C("LEB_56-raw decode (random-mix)",  item_count, runs, iterations), setup_uleb, random_mix, bench_leb_decode_56);
    case 7:  return bench_exec(C("LEB_56-pack encode (random-8)",   item_count, runs, iterations), setup_dfl,  random_8,   bench_leb_encode_vec);
    case 8:  return bench_exec(C("LEB_56-pack encode (random-56)",  item_count, runs, iterations), setup_dfl,  random_56,  bench_leb_encode_vec);
    case 9:  return bench_exec(C("LEB_56-pack encode (random-mix)", item_count, runs, iterations), setup_dfl,  random_mix, bench_leb_encode_vec);
    case 10: return bench_exec(C("LEB_56-pack decode (random-8)",   item_count, runs, iterations), setup_vec,  random_8,   bench_leb_decode_vec);
    case 11: return bench_exec(C("LEB_56-pack decode (random-56)",  item_count, runs, iterations), setup_vec,  random_56,  bench_leb_decode_vec);
    case 12: return bench_exec(C("LEB_56-pack decode (random-mix)", item_count, runs, iterations), setup_vec,  random_mix, bench_leb_decode_vec);
    case 13: return bench_exec(C("VLU_56-raw encode (random-8)",    item_count, runs, iterations), setup_dfl,  random_8,   bench_vlu_encode_56);
    case 14: return bench_exec(C("VLU_56-raw encode (random-56)",   item_count, runs, iterations), setup_dfl,  random_56,  bench_vlu_encode_56);
    case 15: return bench_exec(C("VLU_56-raw encode (random-mix)",  item_count, runs, iterations), setup_dfl,  random_mix, bench_vlu_encode_56);
    case 16: return bench_exec(C("VLU_56-raw decode (random-8)",    item_count, runs, iterations), setup_uvlu, random_8,   bench_vlu_decode_56);
    case 17: return bench_exec(C("VLU_56-raw decode (random-56)",   item_count, runs, iterations), setup_uvlu, random_56,  bench_vlu_decode_56);
    case 18: return bench_exec(C("VLU_56-raw decode (random-mix)",  item_count, runs, iterations), setup_uvlu, random_mix, bench_vlu_decode_56);
    case 19: return bench_exec(C("VLU_56-pack encode (random-8)",   item_count, runs, iterations), setup_dfl,  random_8,   bench_vlu_encode_vec);
    case 20: return bench_exec(C("VLU_56-pack encode (random-56)",  item_count, runs, iterations), setup_dfl,  random_56,  bench_vlu_encode_vec);
    case 21: return bench_exec(C("VLU_56-pack encode (random-mix)", item_count, runs, iterations), setup_dfl,  random_mix, bench_vlu_encode_vec);
    case 22: return bench_exec(C("VLU_56-pack decode (random-8)",   item_count, runs, iterations), setup_vec,  random_8,   bench_vlu_decode_vec);
    case 23: return bench_exec(C("VLU_56-pack decode (random-56)",  item_count, runs, iterations), setup_vec,  random_56,  bench_vlu_decode_vec);
    case 24: return bench_exec(C("VLU_56-pack decode (random-mix)", item_count, runs, iterations), setup_vec,  random_mix, bench_vlu_decode_vec);
    case 25: return bench_exec(C("snprintf/10 encode (random-8)" ,  item_count, runs, iterations), setup_str,  random_8,   bench_snprintf_dec_encode_56);
    case 26: return bench_exec(C("snprintf/10 encode (random-56)",  item_count, runs, iterations), setup_str,  random_56,  bench_snprintf_dec_encode_56);
    case 27: return bench_exec(C("snprintf/10 encode (random-mix)", item_count, runs, iterations), setup_str,  random_mix, bench_snprintf_dec_encode_56);
    case 28: return bench_exec(C("strtoull/10 decode (random-8)",   item_count, runs, iterations), setup_dec,  random_8,   bench_strtoull_dec_decode_56);
    case 29: return bench_exec(C("strtoull/10 decode (random-56)",  item_count, runs, iterations), setup_dec,  random_56,  bench_strtoull_dec_decode_56);
    case 30: return bench_exec(C("strtoull/10 decode (random-mix)", item_count, runs, iterations), setup_dec,  random_mix, bench_strtoull_dec_decode_56);
    case 31: return bench_exec(C("snprintf/16 encode (random-8)",   item_count, runs, iterations), setup_str,  random_8,   bench_snprintf_hex_encode_56);
    case 32: return bench_exec(C("snprintf/16 encode (random-56)",  item_count, runs, iterations), setup_str,  random_56,  bench_snprintf_hex_encode_56);
    case 33: return bench_exec(C("snprintf/16 encode (random-mix)", item_count, runs, iterations), setup_str,  random_mix, bench_snprintf_hex_encode_56);
    case 34: return bench_exec(C("strtoull/16 decode (random-8)",   item_count, runs, iterations), setup_hex,  random_8,   bench_strtoull_hex_decode_56);
    case 35: return bench_exec(C("strtoull/16 decode (random-56)",  item_count, runs, iterations), setup_hex,  random_56,  bench_strtoull_hex_decode_56);
    case 36: return bench_exec(C("strtoull/16 decode (random-mix)", item_count, runs, iterations), setup_hex,  random_mix, bench_strtoull_hex_decode_56);
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