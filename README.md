# Variable Length Unary Integer Coding

## Overview

VLU is a little-endian variable length integer coding
that prefixes payload bits with an unary coded length.

The length is recovered by counting least significant one
bits, which encodes a count of n-bit quantums. The data bits
are stored in the remaining bits of the first n-bit quantum
followed by the number of bits indicated by the unary value.

![vlu](/vlu.png)
_Figure 1: VLU - Variable Length Unary Integer Coding_

The algorithm is parameterizable for different word and packet
sizes, however it is expected that an 8 bit quantum is used.

```
  bits_per_quantum = 8
  unary_value = count_trailing_zeros(not(number))
  encoded_bits = (unary_value + 1) * (bits_per_quantum - 1)
```

With 8 bit quantum, the encoded size is similar to LEB128, 
7-bits can be stored in 1 byte, and 56-bits in 8 bytes.
Decoding, however, is significantly faster, as it is not
necessary to check for continuation bits every byte.

The code can support integers larger than 56-bits by
interpreting the most significant bit of the unary indicator
(bit 8 where the n-bit quantum is 8), as a continuation code,
causing the decoder to append the maximum number of bits from
the packet and continue decoding by appending the next packet.

```
  packet_total_bits = bits_per_quantum ^ 2
  packet_payload_bits = bits_per_quantum * (bits_per_quantum - 1)
```

An 8-bit quantum for the unary code means the packet size is
one machine word (64 bits).
This reduces the SHIFT-MASK-BRANCH frequency by a factor of 8
compared to per-byte continuation codes like LEB128. A VLU8
implementation can fetch 64-bits and process 56-bits at once.
VLU with an 8-bit quantum shall be referred to as VLU8.

### Continuation support

It is possible to implement a simple continuation scheme limiting
unary code decoding to 8-bits and signaling a continuation if all
8 bits of the first byte are set.

### Encoder pseudo-code

simple implementation:

```
  t1       = 8 - ((clz(num) - 1) / 7)
  shamt    = t1 + 1
  if num ≠ 0 then:
      encoded = (integer << shamt) | ((1 << (shamt - 1)) - 1)
```

with bit-8 continuations:

```
  t1       = 8 - ((clz(num) - 1) / 7)
  cont     = t1 > 7
  shamt    = cont ? 8 : t1 + 1
  if num ≠ 0 then:
      encoded = (integer << shamt) | ((1 << (shamt - 1)) - 1)
              | (cont << 8)
```

### Decoder pseudo-code

simple implementation:

```
  t1       = ctz(~encoded)
  shamt    = t1 + 1
  integer  = (encoded >> shamt) & ((1 << (shamt << 3)) - 1)
```

with bit-8 continuations:

```
  t1       = ctz(encoded)
  shamt    = t1 > 7 ? 8 : t1 + 1;
  integer  = (encoded >> shamt) & ((1 << (shamt << 3)) - 1)
```

## Benchmarks

Benchmarks run on a single-core of an Intel Core i9-7980XE CPU at \~4.0GHz:

|Benchmark                     |Item count|Iterations|Size KiB  |Time µs   |GiB/sec   |
|------------------------------|----------|----------|----------|----------|----------|
|BARE                          |1048576   |1000      |8000      |655456    |   11.919 |
|LEB_56 encode (random)        |1048576   |1000      |8000      |5509095   |    1.418 |
|LEB_56 decode (random)        |1048576   |1000      |8000      |4570407   |    1.709 |
|LEB_56 encode (weighted)      |1048576   |1000      |8000      |8013996   |    0.975 |
|LEB_56 decode (weighted)      |1048576   |1000      |8000      |7731451   |    1.010 |
|VLU_56 encode (random)        |1048576   |1000      |8000      |2212099   |    3.532 |
|VLU_56 decode (random)        |1048576   |1000      |8000      |1295747   |    6.029 |
|VLU_56 encode (weighted)      |1048576   |1000      |8000      |2196455   |    3.557 |
|VLU_56 decode (weighted)      |1048576   |1000      |8000      |1285929   |    6.075 |
|VLU_56C encode (random)       |1048576   |1000      |8000      |2963704   |    2.636 |
|VLU_56C decode (random)       |1048576   |1000      |8000      |1407615   |    5.550 |
|VLU_56C encode (weighted)     |1048576   |1000      |8000      |2924660   |    2.671 |
|VLU_56C decode (weighted)     |1048576   |1000      |8000      |1417983   |    5.510 |

_**Note:** 'VLU_56C' denotes the VLU decoder variant that checks for continuations._

## Statistics

This table shows bytes, encoded bits and total bits for VLU8:

| Bytes | Payload Bits |   Total Bits |
|-------|--------------|--------------|
|     1 |            7 |            8 |
|     2 |           14 |           16 |
|     3 |           21 |           24 |
|     4 |           28 |           32 |
|     5 |           35 |           40 |
|     6 |           42 |           48 |
|     7 |           49 |           56 |
|     8 |           56 |           64 |
|     9 |           63 |           72 |
|    10 |           70 |           80 |
|    ,, |           ,, |           ,, |
|     n |          n*7 |          n*8 |

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
    int lz = __builtin_clzll(num);
    int t1 = 8 - ((lz - 1) / 7);
    bool cont = t1 > 7;
    int shamt = cont ? 8 : t1 + 1;
    uint64_t uvlu = (num << shamt)
        | (((num!=0) << (shamt-1))-(num!=0))
        | (-cont & 0x80);
    return (vlu_result) { uvlu, shamt };
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
    uint64_t mask = ~(-!cont << (shamt << 3));
    uint64_t num = (uvlu >> shamt) & mask;
    return (vlu_result) { num, shamt };
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