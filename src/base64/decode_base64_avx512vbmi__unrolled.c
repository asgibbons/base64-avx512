#include "decode_base64_avx512vbmi__unrolled.h"
#include "chromiumbase64.h"

#include <assert.h>
#include <immintrin.h>

#include "decode_base64_tail_avx512vbmi.c"

// Note: constants lookup_lo, lookup_hi, joinXX were
// generated with scripts/avx512vbmi_decode_lookups.py
// loads inputs of 64 * 4 = 256 bytes at a time.
// private void decodeBlock(byte[] src, int sp, int sl, byte[] dst, int dp, boolean isURL) {

//#define BLK_SIZE 4328;

size_t decodeBlock(unsigned char *src, int start_offset, int end_offset, unsigned char *dst, int dst_offset, int isURL) {
//    unsigned char *start;
//    unsigned char *pdst;
    int size = end_offset - start_offset;
//    int num_blocks = length / BLK_SIZE;

    if(size <= 0) return 0;
//    if(num_blocks * BLK_SIZE != length) num_blocks++;

    src += start_offset;
    dst += dst_offset;
//}
//
//size_t decode_base64_avx512vbmi__unrolled(uint8_t* dst, const uint8_t* src, size_t size) {

    __m512i lookup_0 = _mm512_setr_epi32(
                                0x80808080, 0x80808080, 0x80808080, 0x80808080,
                                0x80808080, 0x80808080, 0x80808080, 0x80808080,
                                0x80808080, 0x80808080, 0x3e808080, 0x3f808080,
                                0x37363534, 0x3b3a3938, 0x80803d3c, 0x80808080);
    __m512i lookup_1 = _mm512_setr_epi32(
                                0x02010080, 0x06050403, 0x0a090807, 0x0e0d0c0b,
                                0x1211100f, 0x16151413, 0x80191817, 0x80808080,
                                0x1c1b1a80, 0x201f1e1d, 0x24232221, 0x28272625,
                                0x2c2b2a29, 0x302f2e2d, 0x80333231, 0x80808080);

    if(isURL){
        lookup_0 = _mm512_setr_epi32(
                                0x80808080, 0x80808080, 0x80808080, 0x80808080,
                                0x80808080, 0x80808080, 0x80808080, 0x80808080,
                                0x80808080, 0x80808080, 0x80808080, 0x80803e80,
                                0x37363534, 0x3b3a3938, 0x80803d3c, 0x80808080);
        lookup_1 = _mm512_setr_epi32(
                                0x02010080, 0x06050403, 0x0a090807, 0x0e0d0c0b,
                                0x1211100f, 0x16151413, 0x80191817, 0x3f808080,
                                0x1c1b1a80, 0x201f1e1d, 0x24232221, 0x28272625,
                                0x2c2b2a29, 0x302f2e2d, 0x80333231, 0x80808080);
    }

    uint8_t* start = dst;
#define OR_ALL (uint8_t)0xfe
    __m512i errorvec = _mm512_setzero_si512();
    while (size >= 64*4) {

        // 1. load input
        __m512i input0 = _mm512_loadu_si512((const __m512i*)(src + 0*64));
        __m512i input1 = _mm512_loadu_si512((const __m512i*)(src + 1*64));
        __m512i input2 = _mm512_loadu_si512((const __m512i*)(src + 2*64));
        __m512i input3 = _mm512_loadu_si512((const __m512i*)(src + 3*64));

        // 2. translate from ASCII into 6-bit values
        __m512i translated0 = _mm512_permutex2var_epi8(lookup_0, input0, lookup_1);
        __m512i translated1 = _mm512_permutex2var_epi8(lookup_0, input1, lookup_1);
        __m512i translated2 = _mm512_permutex2var_epi8(lookup_0, input2, lookup_1);
        __m512i translated3 = _mm512_permutex2var_epi8(lookup_0, input3, lookup_1);

        // 2a. check for errors --- convert MSBs to a mask
        const __m512i t0 = _mm512_ternarylogic_epi32(input0, input1, input2, OR_ALL);
        const __m512i t1 = _mm512_ternarylogic_epi32(input3, translated0, translated1, OR_ALL);
        const __m512i t2 = _mm512_ternarylogic_epi32(translated2, translated3, t0, OR_ALL);
        errorvec |= _mm512_ternarylogic_epi32(t0, t1, t2, OR_ALL);

        // 3. pack four 6-bit values into 24-bit words (all within 32-bit lanes)
        // Note: exactly the same procedure as we have in AVX2 version
        // input:  packed_dword([00dddddd|00cccccc|00bbbbbb|00aaaaaa] x 4)
        // merged: packed_dword([00000000|aaaaabbb|bbbbcccc|ccdddddd] x 4)
        const __m512i merge_ab_and_bc0 = _mm512_maddubs_epi16(translated0, _mm512_set1_epi32(0x01400140));
        const __m512i merge_ab_and_bc1 = _mm512_maddubs_epi16(translated1, _mm512_set1_epi32(0x01400140));
        const __m512i merge_ab_and_bc2 = _mm512_maddubs_epi16(translated2, _mm512_set1_epi32(0x01400140));
        const __m512i merge_ab_and_bc3 = _mm512_maddubs_epi16(translated3, _mm512_set1_epi32(0x01400140));

        __m512i merged0 = _mm512_madd_epi16(merge_ab_and_bc0, _mm512_set1_epi32(0x00011000));
        __m512i merged1 = _mm512_madd_epi16(merge_ab_and_bc1, _mm512_set1_epi32(0x00011000));
        __m512i merged2 = _mm512_madd_epi16(merge_ab_and_bc2, _mm512_set1_epi32(0x00011000));
        __m512i merged3 = _mm512_madd_epi16(merge_ab_and_bc3, _mm512_set1_epi32(0x00011000));

        // 4. pack 4 x 48 bytes into three AVX512 registers 24-bit values into continous array of 3*64 bytes
        const __m512i join01 = _mm512_setr_epi32(0x06000102, 0x090a0405, 0x0c0d0e08, 0x16101112,
                                                 0x191a1415, 0x1c1d1e18, 0x26202122, 0x292a2425,
                                                 0x2c2d2e28, 0x36303132, 0x393a3435, 0x3c3d3e38,
                                                 0x46404142, 0x494a4445, 0x4c4d4e48, 0x56505152);
        const __m512i join12 = _mm512_setr_epi32(0x191a1415, 0x1c1d1e18, 0x26202122, 0x292a2425,
                                                 0x2c2d2e28, 0x36303132, 0x393a3435, 0x3c3d3e38,
                                                 0x46404142, 0x494a4445, 0x4c4d4e48, 0x56505152,
                                                 0x595a5455, 0x5c5d5e58, 0x66606162, 0x696a6465);
        const __m512i join23 = _mm512_setr_epi32(0x2c2d2e28, 0x36303132, 0x393a3435, 0x3c3d3e38,
                                                 0x46404142, 0x494a4445, 0x4c4d4e48, 0x56505152,
                                                 0x595a5455, 0x5c5d5e58, 0x66606162, 0x696a6465,
                                                 0x6c6d6e68, 0x76707172, 0x797a7475, 0x7c7d7e78);
        const __m512i arr01 = _mm512_permutex2var_epi8(merged0, join01, merged1);
        const __m512i arr12 = _mm512_permutex2var_epi8(merged1, join12, merged2);
        const __m512i arr23 = _mm512_permutex2var_epi8(merged2, join23, merged3);

        _mm512_storeu_si512((__m512*)(dst + 0*64), arr01);
        _mm512_storeu_si512((__m512*)(dst + 1*64), arr12);
        _mm512_storeu_si512((__m512*)(dst + 2*64), arr23);

        src += 4*64;
        dst += 3*64;
        size -= 4*64;
    }

    while (size >= 64) {

        // 1. load input
        __m512i input = _mm512_loadu_si512((const __m512i*)src);

        // 2. translate from ASCII into 6-bit values
        __m512i translated = _mm512_permutex2var_epi8(lookup_0, input, lookup_1);

        // 2a. check for errors --- convert MSBs to a mask
        errorvec = _mm512_ternarylogic_epi32(errorvec, translated, input, OR_ALL);

        // 3. pack four 6-bit values into 24-bit words (all within 32-bit lanes)
        // Note: exactly the same procedure as we have in AVX2 version
        // input:  packed_dword([00dddddd|00cccccc|00bbbbbb|00aaaaaa] x 4)
        // merged: packed_dword([00000000|aaaaabbb|bbbbcccc|ccdddddd] x 4)
        const __m512i merge_ab_and_bc = _mm512_maddubs_epi16(translated,
                                                             _mm512_set1_epi32(0x01400140));

        __m512i merged = _mm512_madd_epi16(merge_ab_and_bc, _mm512_set1_epi32(0x00011000));


        // 4. pack 24-bit values into continous array of 48 bytes
        const __m512i pack = _mm512_setr_epi32(
                                0x06000102, 0x090a0405, 0x0c0d0e08, 0x16101112,
                                0x191a1415, 0x1c1d1e18, 0x26202122, 0x292a2425,
                                0x2c2d2e28, 0x36303132, 0x393a3435, 0x3c3d3e38,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000);
        const __m512i shuffled = _mm512_permutexvar_epi8(pack, merged);

        _mm512_storeu_si512((__m512*)dst, shuffled);

        src += 64;
        dst += 48;
        size -= 64;
    }

    if (_mm512_movepi8_mask(errorvec) != 0)
        return (size_t)-1;

//    size_t scalar = chromium_base64_decode((char*)dst, (const char*)src, size);
    size_t scalar = decode_base64_tail_avx512vbmi((uint8_t *)dst, (const uint8_t *)src, size, lookup_0, lookup_1);
    if (scalar == MODP_B64_ERROR)
        return (size_t)-1;

    return (dst - start) + scalar;
}
