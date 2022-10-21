#ifndef BMSSE_UTIL__H__INCLUDED__
#define BMSSE_UTIL__H__INCLUDED__
/*
Copyright(c) 2002-2017 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

For more information please visit:  http://bitmagic.io
*/

/*! \file bmsse_util.h
    \brief Compute functions for SSE SIMD instruction set (internal)
*/

namespace bm
{

/** @defgroup SSE2 SSE2 functions
    Processor specific optimizations for SSE2 instructions (internals)
    @internal
    @ingroup bvector
 */

#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif


/*! 
  @brief SSE2 reinitialization guard class

  SSE2 requires to call _mm_empty() if we are intermixing
  MMX integer commands with floating point arithmetics.
  This class guards critical code fragments where SSE2 integer
  is used.

  As of 2015 _mm_empty() is considered deprecated, and not even recognised
  by some compilers (like MSVC) in 64-bit mode.
  As MMX instructions gets old, we here deprecate and comment out 
  use of _mm_empty()

  @ingroup SSE2
*/
class sse_empty_guard
{
public:
    BMFORCEINLINE sse_empty_guard() BMNOEXCEPT
    {
        //_mm_empty();
    }

    BMFORCEINLINE ~sse_empty_guard() BMNOEXCEPT
    {
        //_mm_empty();
    }
};



/*! 
    @brief XOR array elements to specified mask
    *dst = *src ^ mask

    @ingroup SSE2
*/
inline 
void sse2_xor_arr_2_mask(__m128i* BMRESTRICT dst, 
                         const __m128i* BMRESTRICT src, 
                         const __m128i* BMRESTRICT src_end,
                         bm::word_t mask) BMNOEXCEPT
{
     __m128i xM = _mm_set1_epi32((int)mask);
     do
     {
        _mm_store_si128(dst+0, _mm_xor_si128(_mm_load_si128(src+0), xM));
        _mm_store_si128(dst+1, _mm_xor_si128(_mm_load_si128(src+1), xM));
        _mm_store_si128(dst+2, _mm_xor_si128(_mm_load_si128(src+2), xM));
        _mm_store_si128(dst+3, _mm_xor_si128(_mm_load_si128(src+3), xM));
        dst += 4; src += 4;
     } while (src < src_end);
}


/*! 
    @brief Inverts array elements and NOT them to specified mask
    *dst = ~*src & mask

    @ingroup SSE2
*/
inline
void sse2_andnot_arr_2_mask(__m128i* BMRESTRICT dst, 
                            const __m128i* BMRESTRICT src, 
                            const __m128i* BMRESTRICT src_end,
                            bm::word_t mask) BMNOEXCEPT
{
     __m128i xM = _mm_set1_epi32((int)mask);
     do
     {
        _mm_store_si128(dst+0, _mm_andnot_si128(_mm_load_si128(src+0), xM)); // xmm1 = (~xmm1) & xM
        _mm_store_si128(dst+1, _mm_andnot_si128(_mm_load_si128(src+1), xM)); 
        _mm_store_si128(dst+2, _mm_andnot_si128(_mm_load_si128(src+2), xM)); 
        _mm_store_si128(dst+3, _mm_andnot_si128(_mm_load_si128(src+3), xM)); 
        dst += 4; src += 4;
     } while (src < src_end);
}

/*! 
    @brief AND blocks2
    *dst &= *src
    @return 0 if no bits were set
    @ingroup SSE2
*/
inline
unsigned sse2_and_block(__m128i* BMRESTRICT dst,
                       const __m128i* BMRESTRICT src) BMNOEXCEPT
{
    __m128i m1A, m1B, m1C, m1D;
    __m128i accA, accB, accC, accD;
    
    const __m128i* BMRESTRICT src_end =
        (const __m128i*)((bm::word_t*)(src) + bm::set_block_size);

    accA = accB = accC = accD = _mm_setzero_si128();

    do
    {
        m1A = _mm_and_si128(_mm_load_si128(src+0), _mm_load_si128(dst+0));
        m1B = _mm_and_si128(_mm_load_si128(src+1), _mm_load_si128(dst+1));
        m1C = _mm_and_si128(_mm_load_si128(src+2), _mm_load_si128(dst+2));
        m1D = _mm_and_si128(_mm_load_si128(src+3), _mm_load_si128(dst+3));

        _mm_store_si128(dst+0, m1A);
        _mm_store_si128(dst+1, m1B);
        _mm_store_si128(dst+2, m1C);
        _mm_store_si128(dst+3, m1D);

        accA = _mm_or_si128(accA, m1A);
        accB = _mm_or_si128(accB, m1B);
        accC = _mm_or_si128(accC, m1C);
        accD = _mm_or_si128(accD, m1D);
        
        src += 4; dst += 4;
    } while (src < src_end);
    
    accA = _mm_or_si128(accA, accB); // A = A | B
    accC = _mm_or_si128(accC, accD); // C = C | D
    accA = _mm_or_si128(accA, accC); // A = A | C


    bm::id_t BM_ALIGN16 macc[4] BM_ALIGN16ATTR;
    _mm_store_si128((__m128i*)macc, accA);
    return macc[0] | macc[1] | macc[2] | macc[3];
}

/*!
    @brief AND array elements against another array (unaligned)
    *dst &= *src
 
    @return 0 if no bits were set

    @ingroup SSE2
*/
inline
unsigned sse2_and_arr_unal(__m128i* BMRESTRICT dst,
                       const __m128i* BMRESTRICT src,
                       const __m128i* BMRESTRICT src_end) BMNOEXCEPT
{
    __m128i m1A, m2A, m1B, m2B, m1C, m2C, m1D, m2D;
    __m128i accA, accB, accC, accD;

    accA = _mm_setzero_si128();
    accB = _mm_setzero_si128();
    accC = _mm_setzero_si128();
    accD = _mm_setzero_si128();

    do
    {
        m1A = _mm_loadu_si128(src+0);
        m2A = _mm_load_si128(dst+0);
        m1A = _mm_and_si128(m1A, m2A);
        _mm_store_si128(dst+0, m1A);
        accA = _mm_or_si128(accA, m1A);
        
        m1B = _mm_loadu_si128(src+1);
        m2B = _mm_load_si128(dst+1);
        m1B = _mm_and_si128(m1B, m2B);
        _mm_store_si128(dst+1, m1B);
        accB = _mm_or_si128(accB, m1B);

        m1C = _mm_loadu_si128(src+2);
        m2C = _mm_load_si128(dst+2);
        m1C = _mm_and_si128(m1C, m2C);
        _mm_store_si128(dst+2, m1C);
        accC = _mm_or_si128(accC, m1C);

        m1D = _mm_loadu_si128(src+3);
        m2D = _mm_load_si128(dst+3);
        m1D = _mm_and_si128(m1D, m2D);
        _mm_store_si128(dst+3, m1D);
        accD = _mm_or_si128(accD, m1D);
        
        src += 4; dst += 4;
    } while (src < src_end);
    
    accA = _mm_or_si128(accA, accB); // A = A | B
    accC = _mm_or_si128(accC, accD); // C = C | D
    accA = _mm_or_si128(accA, accC); // A = A | C


    bm::id_t BM_ALIGN16 macc[4] BM_ALIGN16ATTR;
    _mm_store_si128((__m128i*)macc, accA);
    return macc[0] | macc[1] | macc[2] | macc[3];
}


inline
unsigned sse2_and_block(__m128i* BMRESTRICT dst,
                        const __m128i* BMRESTRICT src,
                        const __m128i* BMRESTRICT src_end) BMNOEXCEPT
{
    __m128i m1A, m2A, m1B, m2B, m1C, m2C, m1D, m2D;
    __m128i accA, accB, accC, accD;

    accA = _mm_setzero_si128();
    accB = _mm_setzero_si128();
    accC = _mm_setzero_si128();
    accD = _mm_setzero_si128();

    do
    {
        m1A = _mm_load_si128(src + 0);
        m2A = _mm_load_si128(dst + 0);
        m1A = _mm_and_si128(m1A, m2A);
        _mm_store_si128(dst + 0, m1A);
        accA = _mm_or_si128(accA, m1A);

        m1B = _mm_load_si128(src + 1);
        m2B = _mm_load_si128(dst + 1);
        m1B = _mm_and_si128(m1B, m2B);
        _mm_store_si128(dst + 1, m1B);
        accB = _mm_or_si128(accB, m1B);

        m1C = _mm_load_si128(src + 2);
        m2C = _mm_load_si128(dst + 2);
        m1C = _mm_and_si128(m1C, m2C);
        _mm_store_si128(dst + 2, m1C);
        accC = _mm_or_si128(accC, m1C);

        m1D = _mm_load_si128(src + 3);
        m2D = _mm_load_si128(dst + 3);
        m1D = _mm_and_si128(m1D, m2D);
        _mm_store_si128(dst + 3, m1D);
        accD = _mm_or_si128(accD, m1D);

        src += 4; dst += 4;
    } while (src < src_end);

    accA = _mm_or_si128(accA, accB); // A = A | B
    accC = _mm_or_si128(accC, accD); // C = C | D
    accA = _mm_or_si128(accA, accC); // A = A | C


    bm::id_t BM_ALIGN16 macc[4] BM_ALIGN16ATTR;
    _mm_store_si128((__m128i*)macc, accA);
    return macc[0] | macc[1] | macc[2] | macc[3];
}



/*! 
    @brief OR array elements against another array
    *dst |= *src
    @return true if all bits are 1
    @ingroup SSE2
*/
inline
bool sse2_or_block(__m128i* BMRESTRICT dst,
                   const __m128i* BMRESTRICT src) BMNOEXCEPT
{
    __m128i m1A, m2A, m1B, m2B, m1C, m2C, m1D, m2D;
    __m128i mAccF0 = _mm_set1_epi32(~0u); // broadcast 0xFF
    __m128i mAccF1 = _mm_set1_epi32(~0u); // broadcast 0xFF
    const __m128i* BMRESTRICT src_end =
        (const __m128i*)((bm::word_t*)(src) + bm::set_block_size);

    do
    {
        m1A = _mm_load_si128(src + 0);
        m2A = _mm_load_si128(dst + 0);
        m1A = _mm_or_si128(m1A, m2A);
        _mm_store_si128(dst + 0, m1A);

        m1B = _mm_load_si128(src + 1);
        m2B = _mm_load_si128(dst + 1);
        m1B = _mm_or_si128(m1B, m2B);
        _mm_store_si128(dst + 1, m1B);

        m1C = _mm_load_si128(src + 2);
        m2C = _mm_load_si128(dst + 2);
        m1C = _mm_or_si128(m1C, m2C);
        _mm_store_si128(dst + 2, m1C);

        m1D = _mm_load_si128(src + 3);
        m2D = _mm_load_si128(dst + 3);
        m1D = _mm_or_si128(m1D, m2D);
        _mm_store_si128(dst + 3, m1D);

        mAccF1 = _mm_and_si128(mAccF1, m1C);
        mAccF1 = _mm_and_si128(mAccF1, m1D);
        mAccF0 = _mm_and_si128(mAccF0, m1A);
        mAccF0 = _mm_and_si128(mAccF0, m1B);

        src += 4; dst += 4;
    } while (src < src_end);

    __m128i maskF = _mm_set1_epi32(~0u);
    mAccF0 = _mm_and_si128(mAccF0, mAccF1);
    __m128i wcmpA = _mm_cmpeq_epi8(mAccF0, maskF);
    unsigned maskA = unsigned(_mm_movemask_epi8(wcmpA));

    return (maskA == 0xFFFFu);
}

/*!
    @brief OR array elements against another array (unaligned)
    *dst |= *src
    @return true if all bits are 1
    @ingroup SSE2
*/
inline
bool sse2_or_arr_unal(__m128i* BMRESTRICT dst,
                      const __m128i* BMRESTRICT src,
                      const __m128i* BMRESTRICT src_end) BMNOEXCEPT
{
    __m128i m1A, m2A, m1B, m2B, m1C, m2C, m1D, m2D;
    __m128i mAccF0 = _mm_set1_epi32(~0u); // broadcast 0xFF
    __m128i mAccF1 = _mm_set1_epi32(~0u); // broadcast 0xFF
    do
    {
        m1A = _mm_loadu_si128(src + 0);
        m2A = _mm_load_si128(dst + 0);
        m1A = _mm_or_si128(m1A, m2A);
        _mm_store_si128(dst + 0, m1A);

        m1B = _mm_loadu_si128(src + 1);
        m2B = _mm_load_si128(dst + 1);
        m1B = _mm_or_si128(m1B, m2B);
        _mm_store_si128(dst + 1, m1B);

        m1C = _mm_loadu_si128(src + 2);
        m2C = _mm_load_si128(dst + 2);
        m1C = _mm_or_si128(m1C, m2C);
        _mm_store_si128(dst + 2, m1C);

        m1D = _mm_loadu_si128(src + 3);
        m2D = _mm_load_si128(dst + 3);
        m1D = _mm_or_si128(m1D, m2D);
        _mm_store_si128(dst + 3, m1D);

        mAccF1 = _mm_and_si128(mAccF1, m1C);
        mAccF1 = _mm_and_si128(mAccF1, m1D);
        mAccF0 = _mm_and_si128(mAccF0, m1A);
        mAccF0 = _mm_and_si128(mAccF0, m1B);

        src += 4; dst += 4;
    } while (src < src_end);

    __m128i maskF = _mm_set1_epi32(~0u);
    mAccF0 = _mm_and_si128(mAccF0, mAccF1);
    __m128i wcmpA = _mm_cmpeq_epi8(mAccF0, maskF);
    unsigned maskA = unsigned(_mm_movemask_epi8(wcmpA));
    return (maskA == 0xFFFFu);
}

/*!
    @brief OR 2 blocks anc copy result to the destination
    *dst = *src1 | src2
    @return true if all bits are 1

    @ingroup SSE2
*/
inline
bool sse2_or_block_2way(__m128i* BMRESTRICT dst,
    const __m128i* BMRESTRICT src1,
    const __m128i* BMRESTRICT src2) BMNOEXCEPT
{
    __m128i m1A, m1B, m1C, m1D;
    __m128i mAccF0 = _mm_set1_epi32(~0u); // broadcast 0xFF
    __m128i mAccF1 = _mm_set1_epi32(~0u); // broadcast 0xFF
    const __m128i* BMRESTRICT src_end1 =
        (const __m128i*)((bm::word_t*)(src1) + bm::set_block_size);

    do
    {
        m1A = _mm_or_si128(_mm_load_si128(src1 + 0), _mm_load_si128(src2 + 0));
        m1B = _mm_or_si128(_mm_load_si128(src1 + 1), _mm_load_si128(src2 + 1));
        m1C = _mm_or_si128(_mm_load_si128(src1 + 2), _mm_load_si128(src2 + 2));
        m1D = _mm_or_si128(_mm_load_si128(src1 + 3), _mm_load_si128(src2 + 3));

        _mm_store_si128(dst + 0, m1A);
        _mm_store_si128(dst + 1, m1B);
        _mm_store_si128(dst + 2, m1C);
        _mm_store_si128(dst + 3, m1D);

        mAccF1 = _mm_and_si128(mAccF1, m1C);
        mAccF1 = _mm_and_si128(mAccF1, m1D);
        mAccF0 = _mm_and_si128(mAccF0, m1A);
        mAccF0 = _mm_and_si128(mAccF0, m1B);

        src1 += 4; src2 += 4; dst += 4;

    } while (src1 < src_end1);

    __m128i maskF = _mm_set1_epi32(~0u);
    mAccF0 = _mm_and_si128(mAccF0, mAccF1);
    __m128i wcmpA = _mm_cmpeq_epi8(mAccF0, maskF);
    unsigned maskA = unsigned(_mm_movemask_epi8(wcmpA));
    return (maskA == 0xFFFFu);
}

/*!
    @brief OR array elements against another 2 arrays
    *dst |= *src1 | src2
    @return true if all bits are 1

    @ingroup SSE2
*/
inline
bool sse2_or_block_3way(__m128i* BMRESTRICT dst,
    const __m128i* BMRESTRICT src1,
    const __m128i* BMRESTRICT src2) BMNOEXCEPT
{
    __m128i m1A, m1B, m1C, m1D;
    __m128i mAccF0 = _mm_set1_epi32(~0u); // broadcast 0xFF
    __m128i mAccF1 = _mm_set1_epi32(~0u); // broadcast 0xFF
    const __m128i* BMRESTRICT src_end1 =
        (const __m128i*)((bm::word_t*)(src1) + bm::set_block_size);

    do
    {
        m1A = _mm_or_si128(_mm_load_si128(src1 + 0), _mm_load_si128(dst + 0));
        m1B = _mm_or_si128(_mm_load_si128(src1 + 1), _mm_load_si128(dst + 1));
        m1C = _mm_or_si128(_mm_load_si128(src1 + 2), _mm_load_si128(dst + 2));
        m1D = _mm_or_si128(_mm_load_si128(src1 + 3), _mm_load_si128(dst + 3));

        m1A = _mm_or_si128(m1A, _mm_load_si128(src2 + 0));
        m1B = _mm_or_si128(m1B, _mm_load_si128(src2 + 1));
        m1C = _mm_or_si128(m1C, _mm_load_si128(src2 + 2));
        m1D = _mm_or_si128(m1D, _mm_load_si128(src2 + 3));

        _mm_store_si128(dst + 0, m1A);
        _mm_store_si128(dst + 1, m1B);
        _mm_store_si128(dst + 2, m1C);
        _mm_store_si128(dst + 3, m1D);

        mAccF1 = _mm_and_si128(mAccF1, m1C);
        mAccF1 = _mm_and_si128(mAccF1, m1D);
        mAccF0 = _mm_and_si128(mAccF0, m1A);
        mAccF0 = _mm_and_si128(mAccF0, m1B);

        src1 += 4; src2 += 4; dst += 4;

    } while (src1 < src_end1);

    __m128i maskF = _mm_set1_epi32(~0u);
    mAccF0 = _mm_and_si128(mAccF0, mAccF1);
    __m128i wcmpA = _mm_cmpeq_epi8(mAccF0, maskF);
    unsigned maskA = unsigned(_mm_movemask_epi8(wcmpA));
    return (maskA == 0xFFFFu);
}

/*!
    @brief OR array elements against another 2 arrays
    *dst |= *src1 | src2 | src3 | src4
    @return true if all bits are 1

    @ingroup SSE2
*/
inline
bool sse2_or_block_5way(__m128i* BMRESTRICT dst,
    const __m128i* BMRESTRICT src1,
    const __m128i* BMRESTRICT src2,
    const __m128i* BMRESTRICT src3,
    const __m128i* BMRESTRICT src4) BMNOEXCEPT
{
    __m128i m1A, m1B, m1C, m1D;
    __m128i mAccF0 = _mm_set1_epi32(~0u); // broadcast 0xFF
    __m128i mAccF1 = _mm_set1_epi32(~0u); // broadcast 0xFF

    const __m128i* BMRESTRICT src_end1 =
        (const __m128i*)((bm::word_t*)(src1) + bm::set_block_size);

    do
    {
        m1A = _mm_or_si128(_mm_load_si128(src1 + 0), _mm_load_si128(dst + 0));
        m1B = _mm_or_si128(_mm_load_si128(src1 + 1), _mm_load_si128(dst + 1));
        m1C = _mm_or_si128(_mm_load_si128(src1 + 2), _mm_load_si128(dst + 2));
        m1D = _mm_or_si128(_mm_load_si128(src1 + 3), _mm_load_si128(dst + 3));

        m1A = _mm_or_si128(m1A, _mm_load_si128(src2 + 0));
        m1B = _mm_or_si128(m1B, _mm_load_si128(src2 + 1));
        m1C = _mm_or_si128(m1C, _mm_load_si128(src2 + 2));
        m1D = _mm_or_si128(m1D, _mm_load_si128(src2 + 3));

        m1A = _mm_or_si128(m1A, _mm_load_si128(src3 + 0));
        m1B = _mm_or_si128(m1B, _mm_load_si128(src3 + 1));
        m1C = _mm_or_si128(m1C, _mm_load_si128(src3 + 2));
        m1D = _mm_or_si128(m1D, _mm_load_si128(src3 + 3));

        m1A = _mm_or_si128(m1A, _mm_load_si128(src4 + 0));
        m1B = _mm_or_si128(m1B, _mm_load_si128(src4 + 1));
        m1C = _mm_or_si128(m1C, _mm_load_si128(src4 + 2));
        m1D = _mm_or_si128(m1D, _mm_load_si128(src4 + 3));

        _mm_stream_si128(dst + 0, m1A);
        _mm_stream_si128(dst + 1, m1B);
        _mm_stream_si128(dst + 2, m1C);
        _mm_stream_si128(dst + 3, m1D);

        mAccF1 = _mm_and_si128(mAccF1, m1C);
        mAccF1 = _mm_and_si128(mAccF1, m1D);
        mAccF0 = _mm_and_si128(mAccF0, m1A);
        mAccF0 = _mm_and_si128(mAccF0, m1B);

        src1 += 4; src2 += 4;
        src3 += 4; src4 += 4;
        
        _mm_prefetch ((const char*)src3, _MM_HINT_T0);
        _mm_prefetch ((const char*)src4, _MM_HINT_T0);

        dst += 4;

    } while (src1 < src_end1);

    __m128i maskF = _mm_set1_epi32(~0u);
    mAccF0 = _mm_and_si128(mAccF0, mAccF1);
    __m128i wcmpA = _mm_cmpeq_epi8(mAccF0, maskF);
    unsigned maskA = unsigned(_mm_movemask_epi8(wcmpA));
    return (maskA == 0xFFFFu);
}



/*! 
    @brief XOR block against another
    *dst ^= *src
    @return 0 if no bits were set
    @ingroup SSE2
*/
inline
unsigned sse2_xor_block(__m128i* BMRESTRICT dst,
                       const __m128i* BMRESTRICT src) BMNOEXCEPT
{
    __m128i m1A, m1B, m1C, m1D;
    __m128i accA, accB, accC, accD;
    
    const __m128i* BMRESTRICT src_end =
        (const __m128i*)((bm::word_t*)(src) + bm::set_block_size);

    accA = accB = accC = accD = _mm_setzero_si128();

    do
    {
        m1A = _mm_xor_si128(_mm_load_si128(src+0), _mm_load_si128(dst+0));
        m1B = _mm_xor_si128(_mm_load_si128(src+1), _mm_load_si128(dst+1));
        m1C = _mm_xor_si128(_mm_load_si128(src+2), _mm_load_si128(dst+2));
        m1D = _mm_xor_si128(_mm_load_si128(src+3), _mm_load_si128(dst+3));

        _mm_store_si128(dst+0, m1A);
        _mm_store_si128(dst+1, m1B);
        _mm_store_si128(dst+2, m1C);
        _mm_store_si128(dst+3, m1D);

        accA = _mm_or_si128(accA, m1A);
        accB = _mm_or_si128(accB, m1B);
        accC = _mm_or_si128(accC, m1C);
        accD = _mm_or_si128(accD, m1D);
        
        src += 4; dst += 4;
    } while (src < src_end);
    
    accA = _mm_or_si128(accA, accB); // A = A | B
    accC = _mm_or_si128(accC, accD); // C = C | D
    accA = _mm_or_si128(accA, accC); // A = A | C

    bm::id_t BM_ALIGN16 macc[4] BM_ALIGN16ATTR;
    _mm_store_si128((__m128i*)macc, accA);
    return macc[0] | macc[1] | macc[2] | macc[3];
}

/*!
    @brief 3 operand XOR
    *dst = *src1 ^ src2
    @return 0 if no bits were set
    @ingroup SSE2
*/
inline
unsigned sse2_xor_block_2way(__m128i* BMRESTRICT dst,
                             const __m128i* BMRESTRICT src1, 
                             const __m128i* BMRESTRICT src2) BMNOEXCEPT
{
    __m128i m1A, m1B, m1C, m1D;
    __m128i accA, accB, accC, accD;

    const __m128i* BMRESTRICT src1_end =
        (const __m128i*)((bm::word_t*)(src1) + bm::set_block_size);

    accA = accB = accC = accD = _mm_setzero_si128();

    do
    {
        m1A = _mm_xor_si128(_mm_load_si128(src1 + 0), _mm_load_si128(src2 + 0));
        m1B = _mm_xor_si128(_mm_load_si128(src1 + 1), _mm_load_si128(src2 + 1));
        m1C = _mm_xor_si128(_mm_load_si128(src1 + 2), _mm_load_si128(src2 + 2));
        m1D = _mm_xor_si128(_mm_load_si128(src1 + 3), _mm_load_si128(src2 + 3));

        _mm_store_si128(dst + 0, m1A);
        _mm_store_si128(dst + 1, m1B);
        _mm_store_si128(dst + 2, m1C);
        _mm_store_si128(dst + 3, m1D);

        accA = _mm_or_si128(accA, m1A);
        accB = _mm_or_si128(accB, m1B);
        accC = _mm_or_si128(accC, m1C);
        accD = _mm_or_si128(accD, m1D);

        src1 += 4; src2 += 4; dst += 4;
    } while (src1 < src1_end);

    accA = _mm_or_si128(accA, accB); // A = A | B
    accC = _mm_or_si128(accC, accD); // C = C | D
    accA = _mm_or_si128(accA, accC); // A = A | C

    bm::id_t BM_ALIGN16 macc[4] BM_ALIGN16ATTR;
    _mm_store_si128((__m128i*)macc, accA);
    return macc[0] | macc[1] | macc[2] | macc[3];
}


/*! 
    @brief AND-NOT (SUB) array elements against another array
    *dst &= ~*src

    @return 0 if no bits were set

    @ingroup SSE2
*/
inline
unsigned sse2_sub_block(__m128i* BMRESTRICT dst,
                        const __m128i* BMRESTRICT src) BMNOEXCEPT
{
    __m128i m1A, m1B, m1C, m1D;
    __m128i accA, accB, accC, accD;

    accA = accB = accC = accD = _mm_setzero_si128();

    const __m128i* BMRESTRICT src_end =
        (const __m128i*)((bm::word_t*)(src) + bm::set_block_size);

    do
    {
        m1A = _mm_andnot_si128(_mm_load_si128(src+0), _mm_load_si128(dst+0));
        m1B = _mm_andnot_si128(_mm_load_si128(src+1), _mm_load_si128(dst+1));
        m1C = _mm_andnot_si128(_mm_load_si128(src+2), _mm_load_si128(dst+2));
        m1D = _mm_andnot_si128(_mm_load_si128(src+3), _mm_load_si128(dst+3));

        _mm_store_si128(dst+0, m1A);
        _mm_store_si128(dst+1, m1B);
        _mm_store_si128(dst+2, m1C);
        _mm_store_si128(dst+3, m1D);

        accA = _mm_or_si128(accA, m1A);
        accB = _mm_or_si128(accB, m1B);
        accC = _mm_or_si128(accC, m1C);
        accD = _mm_or_si128(accD, m1D);
        
        src += 4; dst += 4;
    } while (src < src_end);
    
    accA = _mm_or_si128(accA, accB); // A = A | B
    accC = _mm_or_si128(accC, accD); // C = C | D
    accA = _mm_or_si128(accA, accC); // A = A | C


    bm::id_t BM_ALIGN16 macc[4] BM_ALIGN16ATTR;
    _mm_store_si128((__m128i*)macc, accA);
    return macc[0] | macc[1] | macc[2] | macc[3];
}


/*! 
    @brief SSE2 block memset
    *dst = value

    @ingroup SSE2
*/

inline
void sse2_set_block(__m128i* BMRESTRICT dst, bm::word_t value) BMNOEXCEPT
{
    __m128i* BMRESTRICT dst_end =
        (__m128i*)((bm::word_t*)(dst) + bm::set_block_size);

    __m128i xmm0 = _mm_set1_epi32((int)value);
    do
    {            
        _mm_store_si128(dst, xmm0);        
        _mm_store_si128(dst+1, xmm0);
        _mm_store_si128(dst+2, xmm0);
        _mm_store_si128(dst+3, xmm0);

        _mm_store_si128(dst+4, xmm0);
        _mm_store_si128(dst+5, xmm0);
        _mm_store_si128(dst+6, xmm0);
        _mm_store_si128(dst+7, xmm0);

        dst += 8;        
    } while (dst < dst_end);
}

/*! 
    @brief SSE2 block copy
    *dst = *src

    @ingroup SSE2
*/
inline
void sse2_copy_block(__m128i* BMRESTRICT dst, 
                     const __m128i* BMRESTRICT src) BMNOEXCEPT
{
    __m128i xmm0, xmm1, xmm2, xmm3;
    const __m128i* BMRESTRICT src_end =
        (const __m128i*)((bm::word_t*)(src) + bm::set_block_size);

    do
    {
        xmm0 = _mm_load_si128(src+0);
        xmm1 = _mm_load_si128(src+1);
        xmm2 = _mm_load_si128(src+2);
        xmm3 = _mm_load_si128(src+3);
        
        _mm_store_si128(dst+0, xmm0);
        _mm_store_si128(dst+1, xmm1);
        _mm_store_si128(dst+2, xmm2);
        _mm_store_si128(dst+3, xmm3);
        
        xmm0 = _mm_load_si128(src+4);
        xmm1 = _mm_load_si128(src+5);
        xmm2 = _mm_load_si128(src+6);
        xmm3 = _mm_load_si128(src+7);
        
        _mm_store_si128(dst+4, xmm0);
        _mm_store_si128(dst+5, xmm1);
        _mm_store_si128(dst+6, xmm2);
        _mm_store_si128(dst+7, xmm3);
        
        src += 8; dst += 8;
        
    } while (src < src_end);    
}

/*!
    @brief SSE2 block copy (unaligned SRC)
    *dst = *src

    @ingroup SSE2
*/
inline
void sse2_copy_block_unalign(__m128i* BMRESTRICT dst,
                            const __m128i* BMRESTRICT src) BMNOEXCEPT
{
    __m128i xmm0, xmm1, xmm2, xmm3;
    const __m128i* BMRESTRICT src_end =
        (const __m128i*)((bm::word_t*)(src) + bm::set_block_size);

    do
    {
        xmm0 = _mm_loadu_si128(src+0);
        xmm1 = _mm_loadu_si128(src+1);
        xmm2 = _mm_loadu_si128(src+2);
        xmm3 = _mm_loadu_si128(src+3);

        _mm_store_si128(dst+0, xmm0);
        _mm_store_si128(dst+1, xmm1);
        _mm_store_si128(dst+2, xmm2);
        _mm_store_si128(dst+3, xmm3);

        xmm0 = _mm_loadu_si128(src+4);
        xmm1 = _mm_loadu_si128(src+5);
        xmm2 = _mm_loadu_si128(src+6);
        xmm3 = _mm_loadu_si128(src+7);

        _mm_store_si128(dst+4, xmm0);
        _mm_store_si128(dst+5, xmm1);
        _mm_store_si128(dst+6, xmm2);
        _mm_store_si128(dst+7, xmm3);

        src += 8; dst += 8;

    } while (src < src_end);
}


/*!
    @brief SSE2 block copy
    *dst = *src

    @ingroup SSE2
*/
inline
void sse2_stream_block(__m128i* BMRESTRICT dst,
                     const __m128i* BMRESTRICT src) BMNOEXCEPT
{
    __m128i xmm0, xmm1, xmm2, xmm3;
    const __m128i* BMRESTRICT src_end =
        (const __m128i*)((bm::word_t*)(src) + bm::set_block_size);

    do
    {
        xmm0 = _mm_load_si128(src+0);
        xmm1 = _mm_load_si128(src+1);
        xmm2 = _mm_load_si128(src+2);
        xmm3 = _mm_load_si128(src+3);
        
        _mm_stream_si128(dst+0, xmm0);
        _mm_stream_si128(dst+1, xmm1);
        _mm_stream_si128(dst+2, xmm2);
        _mm_stream_si128(dst+3, xmm3);
        
        xmm0 = _mm_load_si128(src+4);
        xmm1 = _mm_load_si128(src+5);
        xmm2 = _mm_load_si128(src+6);
        xmm3 = _mm_load_si128(src+7);
        
        _mm_stream_si128(dst+4, xmm0);
        _mm_stream_si128(dst+5, xmm1);
        _mm_stream_si128(dst+6, xmm2);
        _mm_stream_si128(dst+7, xmm3);
        
        src += 8; dst += 8;
        
    } while (src < src_end);
}

/*!
    @brief SSE2 block copy (unaligned src)
    *dst = *src

    @ingroup SSE2
*/
inline
void sse2_stream_block_unalign(__m128i* BMRESTRICT dst,
                     const __m128i* BMRESTRICT src) BMNOEXCEPT
{
    __m128i xmm0, xmm1, xmm2, xmm3;
    const __m128i* BMRESTRICT src_end =
        (const __m128i*)((bm::word_t*)(src) + bm::set_block_size);

    do
    {
        xmm0 = _mm_loadu_si128(src+0);
        xmm1 = _mm_loadu_si128(src+1);
        xmm2 = _mm_loadu_si128(src+2);
        xmm3 = _mm_loadu_si128(src+3);

        _mm_stream_si128(dst+0, xmm0);
        _mm_stream_si128(dst+1, xmm1);
        _mm_stream_si128(dst+2, xmm2);
        _mm_stream_si128(dst+3, xmm3);

        xmm0 = _mm_loadu_si128(src+4);
        xmm1 = _mm_loadu_si128(src+5);
        xmm2 = _mm_loadu_si128(src+6);
        xmm3 = _mm_loadu_si128(src+7);

        _mm_stream_si128(dst+4, xmm0);
        _mm_stream_si128(dst+5, xmm1);
        _mm_stream_si128(dst+6, xmm2);
        _mm_stream_si128(dst+7, xmm3);

        src += 8; dst += 8;

    } while (src < src_end);
}


/*! 
    @brief Invert bit block
    *dst = ~*dst
    or
    *dst ^= *dst 

    @ingroup SSE2
*/
inline 
void sse2_invert_block(__m128i* BMRESTRICT dst) BMNOEXCEPT
{
    __m128i maskF = _mm_set1_epi32(~0u);
    __m128i* BMRESTRICT dst_end =
        (__m128i*)((bm::word_t*)(dst) + bm::set_block_size);

    __m128i mA, mB, mC, mD;
    do 
    {
        mA = _mm_load_si128(dst + 0);
        mB = _mm_load_si128(dst + 1);
        mA = _mm_xor_si128(mA, maskF);
        mB = _mm_xor_si128(mB, maskF);
        _mm_store_si128(dst, mA);
        _mm_store_si128(dst + 1, mB);

        mC = _mm_load_si128(dst + 2);
        mD = _mm_load_si128(dst + 3);
        mC = _mm_xor_si128(mC, maskF);
        mD = _mm_xor_si128(mD, maskF);
        _mm_store_si128(dst + 2, mC);
        _mm_store_si128(dst + 3, mD);

        dst += 4;

    } while (dst < (__m128i*)dst_end);
}

BMFORCEINLINE 
__m128i sse2_and(__m128i a, __m128i b) BMNOEXCEPT
{
    return _mm_and_si128(a, b);
}

BMFORCEINLINE 
__m128i sse2_or(__m128i a, __m128i b) BMNOEXCEPT
{
    return _mm_or_si128(a, b);
}


BMFORCEINLINE 
__m128i sse2_xor(__m128i a, __m128i b) BMNOEXCEPT
{
    return _mm_xor_si128(a, b);
}

BMFORCEINLINE 
__m128i sse2_sub(__m128i a, __m128i b) BMNOEXCEPT
{
    return _mm_andnot_si128(b, a);
}


/*!
    @brief Gap block population count (array sum) utility
    @param pbuf - unrolled, aligned to 1-start GAP buffer
    @param sse_vect_waves - number of SSE vector lines to process
    @param sum - result acumulator
    @return tail pointer

    @internal
    @ingroup SSE2
*/
inline
const bm::gap_word_t* sse2_gap_sum_arr(
    const bm::gap_word_t* BMRESTRICT pbuf,
    unsigned sse_vect_waves,
    unsigned* sum) BMNOEXCEPT
{
    __m128i xcnt = _mm_setzero_si128();

    for (unsigned i = 0; i < sse_vect_waves; ++i)
    {
        __m128i mm0 = _mm_loadu_si128((__m128i*)(pbuf - 1));
        __m128i mm1 = _mm_loadu_si128((__m128i*)(pbuf + 8 - 1));
        __m128i mm_s2 = _mm_add_epi16(mm1, mm0);
        xcnt = _mm_add_epi16(xcnt, mm_s2);
        pbuf += 16;
    }
    xcnt = _mm_sub_epi16(_mm_srli_epi32(xcnt, 16), xcnt);

    unsigned short* cnt8 = (unsigned short*)&xcnt;
    *sum += (cnt8[0]) + (cnt8[2]) + (cnt8[4]) + (cnt8[6]);
    return pbuf;
}

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif


} // namespace



#endif
