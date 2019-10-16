# Variable Length Unary

## Overview

VLU is a little-endian variable length integer coding
that prefixes payload bits with an unary coded length.

The length is recovered by counting least significant one
bits, which encodes a count of n-bit quantums. The data bits
are stored in the remaining bits of the first byte followed
by the number of bytes indicated in the unary value. e.g.

```
  bits_per_quantum = 8
  unary_value = count_trailing_zeros(not(number))
  encoded_bits = (unary_value + 1)(bits_per_quantum - 1)
```

With 8 bit quantum, the encoded size is similar to LEB128, 
7-bits can be stored in 1 byte, and 56-bits in 8 bytes.

Decoding, however, is significantly faster, as it is not
necessary to check for continuation bits every byte.

The scheme can be modified to support big integers by adding
continuation bits at a machine word interval (64 bits),
reducing the SHIFT-MASK-BRANCH frequency by a factor of 8.

### Encoding

```
  shamt    = 8 - (clz(num) >> 3)
  encoded  = (integer << shamt) | ((1 << (shamt-1))-1);
```

### Decoding

```
  shamt    = ctz(~encoded) + 1
  integer  = encoded >> shamt
```

### Example

The following tables shows variable unary length and payload bit layout:

```
  |  byte-8  |  byte-7  |                   |  byte-2  |  byte-1  |
  |----------|----------|-------------------|----------|----------|
  |          |          |                   |          | nnnnnnn0 |
  |          |          |                   | nnnnnnnn | nnnnnn01 |
  |          | nnnnnnnn | ........ ........ | nnnnnnnn | n0111111 |
  | nnnnnnnn | nnnnnnnn | ........ ........ | nnnnnnnn | 01111111 |
```

### Benchmarks

Benchmarks run on a single-core of an Intel Core i9-7980XE CPU at \~4.0GHz:

|Benchmark                     |Item count|Iterations|Size KiB  |Time µs   |GiB/sec   |
|------------------------------|----------|----------|----------|----------|----------|
|BARE                          |1048576   |1000      |8000      |651623    |   11.989 |
|LEB encode (random)           |1048576   |1000      |8000      |4597423   |    1.699 |
|LEB decode (random)           |1048576   |1000      |8000      |3338013   |    2.340 |
|LEB encode (weighted)         |1048576   |1000      |8000      |7136777   |    1.095 |
|LEB decode (weighted)         |1048576   |1000      |8000      |6826465   |    1.144 |
|VLU encode (random)           |1048576   |1000      |8000      |1122738   |    6.958 |
|VLU decode (random)           |1048576   |1000      |8000      |798394    |    9.785 |
|VLU encode (weighted)         |1048576   |1000      |8000      |1122009   |    6.963 |
|VLU decode (weighted)         |1048576   |1000      |8000      |804048    |    9.716 |

## Example code

### Encoder (C)

Example 64-bit encoder:

```C
uint64_t encode_uvlu(uint64_t num)
{
    size_t leading_zeros = __builtin_clzll(num);
    size_t shamt = 8 - (leading_zeros >> 3);
    uint64_t uvlu = (num << shamt) | ((1ull << (shamt-1))-1);
    return uvlu;
}
```

### Decoder (C)

Example 64-bit decoder:

```C
uint64_t decode_uvlu(uint64_t uvlu)
{
    size_t trailing_ones = __builtin_ctzll(~uvlu);
    size_t shamt = (trailing_ones + 1);
    uint64_t num = uvlu >> shamt;
    return num;
}
```

### Decoder (asm)

5 instructions to decode 64-bit VLU on x86_64 Haswell with BMI2 _(runs at ~ 80% of uncompressed speed)_:

```asm
decode_uvlu128:
        mov     rax, rdi
        not     rax
        tzcnt   rax, rax
        inc     rax
        shrx    rax, rdi, rax
```

Compare to LEB 64-bit on x86_64 _(loops up to 8 times per word, runs at ~ 10 to 20% of uncompressed speed)_:

```asm
decode_uleb128:
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