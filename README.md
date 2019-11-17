# Variable Length Unary Integer Coding

## Overview

VLU is a little-endian variable length integer coding that prefixes
data bits with unary code length bits.

The length is recovered by counting least significant set bits,
which encode a count of _n-bit_ basic units. The data bits compactly
trail the unary code prefix.

![vlu](/images/vlu.png)
_Figure 1: VLU - Variable Length Unary Integer Coding_

The algorithm is parameterizable for different basic unit sizes,
however it is expected that the basic unit will be 8 and thus the
unary code length will encode a count of octets or bytes. e.g.

```
  bits_per_unit = 8
  unary_value   = count_trailing_zeros(~number)
  encoded_bits  = (unary_value + 1) * (bits_per_unit - 1)
```

With an 8 bit basic unit, the encoded size is similar to LEB128;
7-bits can be stored in 1 byte, 56-bits in 8 bytes and 112-bits
in 16 bytes. Decoding, however, is significantly faster than LEB128,
as it is not necessary to check for continuation bits every byte,
instead the length can be decoded in a single count bits operation.

The unary code size can be limited to support splitting the length
prefix and the integer value across multiple words, by interpreting
the most significant bit of a prefix limit, as a continuation code,
effectively dividing the bits into two streams; the prefix value
containing length, and the remaining bits containing binary data.

With a basic unit size of 8 and a limit of 16, an 112-bit value
can be encoded using a 16-bit length prefix and 112 bits of data.

```
  bits_per_unit = 8
  prefix_limit  = 16
  interval_size = 128 (8 * 16)
```

This encoding reduces the SHIFT-MASK-BRANCH frequency in scalar code
by a factor of 8 compared to per-byte continuation codes like LEB128.
VLU with an 8-bit quantum and 16-bit prefix limit shall be referred
to as VLU8.

### Continuation support

Values larger than 112-bits can be encoded by setting bit 16 and
continuing the prefix and data in another 128-bit interval. This
requires a minimum read of one byte, and optimally 16-bits, to
read the length of the first interval.

For VLU8, the bits per unit is 8, the prefix limit can be 8 or 16,
the number of bits encoded per interval is 56 or 112, and the
interval sizes 64-bits or 128-bits. The choice of interval size
depends on the intended application. It is possible to have
an alternate encoding for the length in subsequent data, size as
encoding the length as a fixed size binary field.

### Encoder pseudo-code

simple implementation without continuation:

```
  t1            = units_per_word - ((clz(num) - 1) / (bits_per_unit - 1))
  shamt         = t1 + 1
  if num ≠ 0 then:
        encoded = (integer << shamt) | ((1 << (shamt-1))-1)
```

with bit-8 continuations:

```
  t1            = units_per_word - ((clz(num) - 1) / (bits_per_unit - 1))
  cont          = t1 >= prefix_limit
  shamt         = cont ? prefix_limit : t1 + 1
  if num ≠ 0 then:
        encoded = (integer << shamt) | ((1 << (shamt - 1)) - 1)
                | (cont << (prefix_limit - 1))
```

### Decoder pseudo-code

simple implementation without continuation:

```
  t1            = ctz(~encoded)
  shamt         = t1 + 1
  integer       = (encoded >> shamt) & ((1 << (shamt * 7)) - 1)
```

with bit-8 continuations:

```
  prefix_limit  = 8
  t1            = ctz(~encoded)
  cont          = t1 >= prefix_limit
  shamt         = cont ? prefix_limit : t1 + 1
  integer       = (encoded >> shamt) & ((1 << (shamt * 7)) - 1)
```

### Notes

- `clz` is an abbreviation of `count_leading_zeros`
- `ctz` is an abbreviation of `count_trailing_zeros`

## Benchmarks

Benchmarks run on a single-core of an Intel Core i9-7980XE CPU at \~4.0GHz:

![vlu-benchmarks](/images/benchmarks.png)
_Figure 2: VLU8 Benchmarks (GiB/sec)_

### Performance Data

|Benchmark |random-8-encode (GiB/sec) |random-56-encode (GiB/sec) |random-mix-encode (GiB/sec) |random-8-decode (GiB/sec) |random-56-decode (GiB/sec) |random-mix-decode (GiB/sec) |
|:---------------------------|-------------------:|-------------------:|-------------------:|-------------------:|-------------------:|-------------------:|
|VLU&#8209;56&#8209;raw      |3.149               |3.172               |2.704               |5.258               |5.226               |5.153               |
|LEB&#8209;56&#8209;raw      |2.101               |1.475               |0.979               |2.05                |1.817               |1.071               |
|VLU&#8209;56&#8209;pack     |0.864               |1.742               |0.838               |1.236               |1.237               |1.222               |
|LEB&#8209;56&#8209;pack     |1.316               |1.055               |0.682               |0.905               |0.151               |0.283               |
|snprintf/10                 |0.157               |0.125               |0.126               |0.644               |0.315               |0.357               |
|snprintf/16                 |0.173               |0.14                |0.143               |0.585               |0.127               |0.212               |

#### Benchmark Notes

- `VLU-56-pack`, `LEB-56-pack` test encoding and packing to byte vectors or decoding and unpacking from byte vectors.
- `VLU-56-raw`, `LEB-56-raw` test the raw encode and decode functions.
- `snprintf` denotes `strtoull` and `snprintf` for ASCII decimal or hexidecimal.

## Example code

Definitions:

```C
struct vlu_result
{
    uint64_t val;
    int64_t shamt;
};
```

### Encoder (C)

Example 64-bit VLU encoder:

```C
struct vlu_result vlu_encode_56c(uint64_t num)
{
    if (!num) return (vlu_result) { 0, 1 };
    int lz = __builtin_clzll(num);
    int t1 = 8 - ((lz - 1) / 7);
    bool cont = t1 > 7;
    int shamt = cont ? 8 : t1 + 1;
    uint64_t uvlu = (num << shamt)
        | ((1 << (shamt - 1)) - 1)
        | (-cont & 0x80);
    return (vlu_result) { uvlu, shamt | -(int64_t)cont };
}
```

### Decoder (C)

Example 64-bit VLU decoder:

```C
struct vlu_result vlu_decode_56c(uint64_t uvlu)
{
    int t1 = __builtin_ctzll(~uvlu);
    bool cont = t1 > 7;
    int shamt = cont ? 8 : t1 + 1;
    uint64_t mask = ~(-(int64_t)!cont << (shamt * 7));
    uint64_t num = (uvlu >> shamt) & mask;
    return (vlu_result) { num, shamt | -(int64_t)cont };
}
```

### Decoder (asm)

#### Variant 1

5 instructions to decode 64-bit VLU packet on x86_64 Haswell with BMI2
**without continuation support** _(limited to 56-bit)_:

```
vlu_decode_56:
        mov     rdx, rdi
        not     rdx
        tzcnt   rdx, rdx
        inc     rdx
        shrx    rax, rdi, rdx
        ret
```

**Note:** _The unary count is returned in `rdx`, to map to the _x86_64_
structure return ABI, however, the shift does not use the documented
continuation scheme that limits the unary code to 8-bits, rather it
simply shifts the 64-bit word by the number of bits in the unary code._

#### Variant 2

8 instructions to decode 64-bit VLU packet on x86_64 Haswell with BMI2
**continuation support** _(unlimited)_:

```
vlu_decode_56c:
        mov     rax, rdi
        not     rax
        tzcnt   rax, rax
        cmp     rax, 0x7
        lea     rdx, [rax + 1]
        mov     rax, 0x8
        cmovg   rdx, rax
        shrx    rax, rdi, rdx
        ret
```

**Note:** _The unary count is returned in `rdx`, to map to the _x86_64_
structure return ABI. The schedule of `CMP` and `CMOVG` has been
carefully selected, along with the choice of `LEA` to compose the byte
count, so that the condition code register is undisturbed._

#### Variant 3

14 instructions to decode 64-bit VLU packet on x86_64 Haswell with BMI2
**continuations and subword masking**:

```
vlu_decode_56c:
        mov     rcx, rdi
        not     rcx
        tzcnt   rcx, rcx
        xor     rsi, rsi
        cmp     rcx, 7
        setne   sil
        lea     rdx, [rcx + 1]
        mov     rcx, 8
        cmovg   rdx, rcx
        shrx    rax, rdi, rdx
        lea     rcx, [rdx * 8]
        neg     rsi
        shlx    rsi, rsi, rcx
        andn    rax, rsi, rax
        ret
```

**Note:** _The unary count is returned in `rdx`, to map to the _x86_64_
structure return ABI._

#### Comparison with LEB

Compare to 64-bit LEB packet on x86_64 _(loops up to 8 times per word)_:

```
leb_decode_56:
        xor     eax, eax
        xor     esi, esi
        xor     r9d, r9d
.L15:
        mov     ecx, esi
        mov     r8, rdi
        shr     r8, cl
        mov     ecx, eax
        mov     rdx, r8
        and     edx, 127
        sal     rdx, cl
        or      r9, rdx
        test    r8b, r8b
        jns     .L13
        add     eax, 7
        add     esi, 8
        cmp     eax, 56
        jne     .L15
.L13:
        mov     rax, r9
        ret
```