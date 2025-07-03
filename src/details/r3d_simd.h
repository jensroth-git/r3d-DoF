#ifndef R3D_DETAIL_SIMD_H
#define R3D_DETAIL_SIMD_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* === SIMD Capability Detection === */

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define SIMD_X86
    #if defined(__AVX512F__)
        #include <immintrin.h>
        #define HAS_AVX512F
    #elif defined(__AVX2__)
        #include <immintrin.h>
        #define HAS_AVX2
    #elif defined(__SSE4_1__)
        #include <smmintrin.h>
        #define HAS_SSE41
    #elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
        #include <emmintrin.h>
        #define HAS_SSE2
    #endif
#elif defined(__ARM_NEON__) || defined(__aarch64__) || defined(_M_ARM64)
    #define SIMD_ARM
    #include <arm_neon.h>
    #define HAS_NEON
#endif

/* === Matrix Functions === */

static inline bool r3d_simd_is_matrix_identity(const float* m)
{
    /* --- Alignment check - use fallback if unaligned --- */

    if (((uintptr_t)m & 0xF) != 0) {
        goto fallback;
    }
    
#if defined(HAS_AVX512F)
    {
        static const __m512 IDENTITY = {
            1.0f, 0.0f, 0.0f, 0.0f,  /* col0 */
            0.0f, 1.0f, 0.0f, 0.0f,  /* col1 */
            0.0f, 0.0f, 1.0f, 0.0f,  /* col2 */
            0.0f, 0.0f, 0.0f, 1.0f   /* col3 */
        };

        __m512 matrix = _mm512_load_ps(m);
        __mmask16 cmp_mask = _mm512_cmpeq_ps_mask(matrix, IDENTITY);

        return (cmp_mask == 0xFFFF);
    }
#elif defined(HAS_AVX2)
    {
        static const __m256 ID01 = {1.0f, 0.0f, 0.0f, 0.0f,     /* col0 */
                                    0.0f, 1.0f, 0.0f, 0.0f};    /* col1 */
        static const __m256 ID23 = {0.0f, 0.0f, 1.0f, 0.0f,     /* col2 */
                                    0.0f, 0.0f, 0.0f, 1.0f};    /* col3 */

        __m256 data01 = _mm256_load_ps(&m[0]);
        __m256 data23 = _mm256_load_ps(&m[8]);

        __m256 xor01 = _mm256_xor_ps(data01, ID01);
        __m256 xor23 = _mm256_xor_ps(data23, ID23);
        __m256 combined = _mm256_or_ps(xor01, xor23);

        return _mm256_testz_ps(combined, combined);
    }
#elif defined(HAS_SSE41)
    {
        static const __m128 ID0 = {1.0f, 0.0f, 0.0f, 0.0f};
        static const __m128 ID1 = {0.0f, 1.0f, 0.0f, 0.0f};
        static const __m128 ID2 = {0.0f, 0.0f, 1.0f, 0.0f};
        static const __m128 ID3 = {0.0f, 0.0f, 0.0f, 1.0f};

        __m128 col0 = _mm_load_ps(&m[0]);
        __m128 col1 = _mm_load_ps(&m[4]);
        __m128 col2 = _mm_load_ps(&m[8]);
        __m128 col3 = _mm_load_ps(&m[12]);

        __m128 xor0 = _mm_xor_ps(col0, ID0);
        __m128 xor1 = _mm_xor_ps(col1, ID1);
        __m128 xor2 = _mm_xor_ps(col2, ID2);
        __m128 xor3 = _mm_xor_ps(col3, ID3);

        __m128 combined = _mm_or_ps(_mm_or_ps(xor0, xor1), _mm_or_ps(xor2, xor3));

        return _mm_testz_ps(combined, combined);
    }
#elif defined(HAS_SSE2)
    {
        static const __m128 ID0 = {1.0f, 0.0f, 0.0f, 0.0f};
        static const __m128 ID1 = {0.0f, 1.0f, 0.0f, 0.0f};
        static const __m128 ID2 = {0.0f, 0.0f, 1.0f, 0.0f};
        static const __m128 ID3 = {0.0f, 0.0f, 0.0f, 1.0f};

        __m128 col0 = _mm_load_ps(&m[0]);
        __m128 col1 = _mm_load_ps(&m[4]);
        __m128 col2 = _mm_load_ps(&m[8]);
        __m128 col3 = _mm_load_ps(&m[12]);

        __m128 cmp0 = _mm_cmpeq_ps(col0, ID0);
        __m128 cmp1 = _mm_cmpeq_ps(col1, ID1);
        __m128 cmp2 = _mm_cmpeq_ps(col2, ID2);
        __m128 cmp3 = _mm_cmpeq_ps(col3, ID3);

        __m128 all = _mm_and_ps(_mm_and_ps(cmp0, cmp1), _mm_and_ps(cmp2, cmp3));

        return (_mm_movemask_ps(all) == 0xF);
    }
#elif defined(HAS_NEON)
    {
        static const float32x4_t ID0 = {1.0f, 0.0f, 0.0f, 0.0f};
        static const float32x4_t ID1 = {0.0f, 1.0f, 0.0f, 0.0f};
        static const float32x4_t ID2 = {0.0f, 0.0f, 1.0f, 0.0f};
        static const float32x4_t ID3 = {0.0f, 0.0f, 0.0f, 1.0f};

        float32x4_t col0 = vld1q_f32(&m[0]);
        float32x4_t col1 = vld1q_f32(&m[4]);
        float32x4_t col2 = vld1q_f32(&m[8]);
        float32x4_t col3 = vld1q_f32(&m[12]);

        uint32x4_t cmp0 = vceqq_f32(col0, ID0);
        uint32x4_t cmp1 = vceqq_f32(col1, ID1);
        uint32x4_t cmp2 = vceqq_f32(col2, ID2);
        uint32x4_t cmp3 = vceqq_f32(col3, ID3);

        uint32x4_t all = vandq_u32(vandq_u32(cmp0, cmp1), vandq_u32(cmp2, cmp3));

        /* Reduction to check if all bits are 1 */
        uint32x2_t tmp = vand_u32(vget_low_u32(all), vget_high_u32(all));
        tmp = vpmin_u32(tmp, tmp);
        
        return (vget_lane_u32(tmp, 0) == 0xFFFFFFFF);
    }
#endif

fallback:
    /* IEEE 754 constants to avoid floating point comparisons */
    static const uint32_t ONE_BITS = 0x3F800000;    /* 1.0f */
    static const uint32_t ZERO_BITS = 0x00000000;   /* 0.0f */

    const uint32_t* mi = (const uint32_t*)m;

    /* Main diagonal first (more likely to fail) */
    if (mi[0] != ONE_BITS) return false;    /* m[0][0] = 1.0 */
    if (mi[5] != ONE_BITS) return false;    /* m[1][1] = 1.0 */
    if (mi[10] != ONE_BITS) return false;   /* m[2][2] = 1.0 */
    if (mi[15] != ONE_BITS) return false;   /* m[3][3] = 1.0 */

    /* First column (off-diagonal) */
    if (mi[1] != ZERO_BITS) return false;   /* m[1][0] = 0.0 */
    if (mi[2] != ZERO_BITS) return false;   /* m[2][0] = 0.0 */
    if (mi[3] != ZERO_BITS) return false;   /* m[3][0] = 0.0 */
    
    /* Second column (off-diagonal) */
    if (mi[4] != ZERO_BITS) return false;   /* m[0][1] = 0.0 */
    if (mi[6] != ZERO_BITS) return false;   /* m[2][1] = 0.0 */
    if (mi[7] != ZERO_BITS) return false;   /* m[3][1] = 0.0 */
    
    /* Third column (off-diagonal) */
    if (mi[8] != ZERO_BITS) return false;   /* m[0][2] = 0.0 */
    if (mi[9] != ZERO_BITS) return false;   /* m[1][2] = 0.0 */
    if (mi[11] != ZERO_BITS) return false;  /* m[3][2] = 0.0 */
    
    /* Fourth column (off-diagonal) */
    if (mi[12] != ZERO_BITS) return false;  /* m[0][3] = 0.0 */
    if (mi[13] != ZERO_BITS) return false;  /* m[1][3] = 0.0 */
    if (mi[14] != ZERO_BITS) return false;  /* m[2][3] = 0.0 */
    
    return true;  /* All tests passed */
}

#endif // R3D_DETAIL_SIMD_H
