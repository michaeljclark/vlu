# Variable Length Unary

## Overview

VLU is a little-endian variable length integer coding
that prefixes payload bits with an unary coded length.

The length is recovered by counting least significant one
bits, which encodes a count of n-bit quantums. The data bits
are stored in the remaining bits of the first n-bit quantum
followed by the number of bits indicated by the unary value.

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

It is expected to use an 8-bit quantum for the unary code
which means the packet size is one machine word (64 bits).
This reduces the SHIFT-MASK-BRANCH frequency by a factor of 8
compared to per-byte continuation codes like LEB128. A VLU8
implementation can fetch 64-bits and process 56-bits at once.
VLU with an 8-bit quantum shall be referred to as VLU8.

### Encoder pseudo-code

```
  shamt    = 8 - ((clz(num) - 1) / 7) + 1
  encoded  = integer << shamt
  if num ≠ 0 then:
      encoded = encoded | ((1 << (shamt - 1)) - 1)
```

_**Note:** the expression is for one 56-bit packet without continuation bit._

### Decoder pseudo-code

```
  shamt    = ctz(~encoded) + 1
  integer  = encoded >> shamt
```

_**Note:** the expression is for one 56-bit packet without continuation bit._

### Example encoding

The following tables shows variable unary length and payload bit layout:

```
 |  byte-1  |          |  byte-8  |  byte-7  |                   |  byte-2  |  byte-1  |
-|----------|----------|----------|----------|-------------------|----------|----------|
 |          |          |          |          |                   |          | nnnnnnn0 |
 |          |  8-byte  |          |          |                   | nnnnnnnn | nnnnnn01 |
 |          |  packet  |          | nnnnnnnn | ........ ........ | nnnnnnnn | n0111111 |
 |          |  bound.  | nnnnnnnn | nnnnnnnn | ........ ........ | nnnnnnnn | 01111111 |
 | nnnnnnn0 |          | nnnnnnnn | nnnnnnnn | ........ ........ | nnnnnnnn | 11111111 |
```

(note: 0b)

## Benchmarks

Benchmarks run on a single-core of an Intel Core i9-7980XE CPU at \~4.0GHz:

|Benchmark                     |Item count|Iterations|Size KiB  |Time µs   |GiB/sec   |
|------------------------------|----------|----------|----------|----------|----------|
|BARE                          |1048576   |1000      |8000      |625283    |   12.494 |
|LEB_56 encode (random)        |1048576   |1000      |8000      |5466135   |    1.429 |
|LEB_56 decode (random)        |1048576   |1000      |8000      |4413817   |    1.770 |
|LEB_56 encode (weighted)      |1048576   |1000      |8000      |8049317   |    0.971 |
|LEB_56 decode (weighted)      |1048576   |1000      |8000      |7409354   |    1.054 |
|VLU_56 encode (random)        |1048576   |1000      |8000      |2087182   |    3.743 |
|VLU_56 decode (random)        |1048576   |1000      |8000      |788366    |    9.910 |
|VLU_56 encode (weighted)      |1048576   |1000      |8000      |2082917   |    3.751 |
|VLU_56 decode (weighted)      |1048576   |1000      |8000      |796852    |    9.804 |
|VLU_56C encode (random)       |1048576   |1000      |8000      |2902893   |    2.691 |
|VLU_56C decode (random)       |1048576   |1000      |8000      |895172    |    8.727 |
|VLU_56C encode (weighted)     |1048576   |1000      |8000      |2870418   |    2.722 |
|VLU_56C decode (weighted)     |1048576   |1000      |8000      |902739    |    8.654 |

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

### Encoder (C)

Example 64-bit VLU encoder:

```C
uint64_t vlu_encode_56(uint64_t num)
{
    int leading_zeros = __builtin_clzll(num);
    int trailing_ones = 8 - ((leading_zeros - 1) / 7);
    int shamt = trailing_ones + 1;
    uint64_t uvlu = (num << shamt) | (((num!=0) << (shamt-1))-(num!=0));
    return uvlu;
}
```

### Decoder (C)

Example 64-bit VLU decoder:

```C
uint64_t vlu_decode_56(uint64_t uvlu)
{
    int trailing_ones = __builtin_ctzll(~uvlu);
    int shamt = (trailing_ones + 1);
    uint64_t num = uvlu >> shamt;
    return num;
}
```

### Decoder (asm)

5 instructions to decode 64-bit VLU packet on x86_64 Haswell with BMI2 _(runs at ~ 80% of uncompressed speed)_:

```asm
vlu_decode_56:
        mov     rax, rdi
        not     rax
        tzcnt   rax, rax
        inc     rax
        shrx    rax, rdi, rax
```

Compare to 64-bit LEB packet on x86_64 _(loops up to 8 times per word, runs at ~ 10 to 20% of uncompressed speed)_:

```asm
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
```