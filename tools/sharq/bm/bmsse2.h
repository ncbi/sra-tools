#ifndef BMSSE2__H__INCLUDED__
#define BMSSE2__H__INCLUDED__
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

/*! \file bmsse2.h
    \brief Compute functions for SSE2 SIMD instruction set (internal)
*/

#ifndef BMWASMSIMDOPT
#include<mmintrin.h>
#endif
#include<emmintrin.h>

#include "bmdef.h"
#include "bmsse_util.h"
#include "bmutil.h"


#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif

namespace bm
{


/*!
    SSE2 optimized bitcounting function implements parallel bitcounting
    algorithm for SSE2 instruction set.

<pre>
unsigned CalcBitCount32(unsigned b)
{
    b = (b & 0x55555555) + (b >> 1 & 0x55555555);
    b = (b & 0x33333333) + (b >> 2 & 0x33333333);
    b = (b + (b >> 4)) & 0x0F0F0F0F;
    b = b + (b >> 8);
    b = (b + (b >> 16)) & 0x0000003F;
    return b;
}
</pre>

    @ingroup SSE2

*/
inline 
bm::id_t sse2_bit_count(const __m128i* block, const __m128i* block_end)
{
    const unsigned mu1 = 0x55555555;
    const unsigned mu2 = 0x33333333;
    const unsigned mu3 = 0x0F0F0F0F;
    const unsigned mu4 = 0x0000003F;

    // Loading masks
    __m128i m1 = _mm_set_epi32 (mu1, mu1, mu1, mu1);
    __m128i m2 = _mm_set_epi32 (mu2, mu2, mu2, mu2);
    __m128i m3 = _mm_set_epi32 (mu3, mu3, mu3, mu3);
    __m128i m4 = _mm_set_epi32 (mu4, mu4, mu4, mu4);
    __m128i mcnt;
    mcnt = _mm_xor_si128(m1, m1); // cnt = 0

    __m128i tmp1, tmp2;
    do
    {        
        __m128i b = _mm_load_si128(block);
        ++block;

        // b = (b & 0x55555555) + (b >> 1 & 0x55555555);
        tmp1 = _mm_srli_epi32(b, 1);                    // tmp1 = (b >> 1 & 0x55555555)
        tmp1 = _mm_and_si128(tmp1, m1); 
        tmp2 = _mm_and_si128(b, m1);                    // tmp2 = (b & 0x55555555)
        b    = _mm_add_epi32(tmp1, tmp2);               //  b = tmp1 + tmp2

        // b = (b & 0x33333333) + (b >> 2 & 0x33333333);
        tmp1 = _mm_srli_epi32(b, 2);                    // (b >> 2 & 0x33333333)
        tmp1 = _mm_and_si128(tmp1, m2); 
        tmp2 = _mm_and_si128(b, m2);                    // (b & 0x33333333)
        b    = _mm_add_epi32(tmp1, tmp2);               // b = tmp1 + tmp2

        // b = (b + (b >> 4)) & 0x0F0F0F0F;
        tmp1 = _mm_srli_epi32(b, 4);                    // tmp1 = b >> 4
        b = _mm_add_epi32(b, tmp1);                     // b = b + (b >> 4)
        b = _mm_and_si128(b, m3);                       //           & 0x0F0F0F0F

        // b = b + (b >> 8);
        tmp1 = _mm_srli_epi32 (b, 8);                   // tmp1 = b >> 8
        b = _mm_add_epi32(b, tmp1);                     // b = b + (b >> 8)

        // b = (b + (b >> 16)) & 0x0000003F;
        tmp1 = _mm_srli_epi32 (b, 16);                  // b >> 16
        b = _mm_add_epi32(b, tmp1);                     // b + (b >> 16)
        b = _mm_and_si128(b, m4);                       // (b >> 16) & 0x0000003F;

        mcnt = _mm_add_epi32(mcnt, b);                  // mcnt += b

    } while (block < block_end);


    bm::id_t BM_ALIGN16 tcnt[4] BM_ALIGN16ATTR;
    _mm_store_si128((__m128i*)tcnt, mcnt);

    return tcnt[0] + tcnt[1] + tcnt[2] + tcnt[3];
}



template<class Func>
bm::id_t sse2_bit_count_op(const __m128i* BMRESTRICT block, 
                           const __m128i* BMRESTRICT block_end,
                           const __m128i* BMRESTRICT mask_block,
                           Func sse2_func)
{
    const unsigned mu1 = 0x55555555;
    const unsigned mu2 = 0x33333333;
    const unsigned mu3 = 0x0F0F0F0F;
    const unsigned mu4 = 0x0000003F;

    // Loading masks
    __m128i m1 = _mm_set_epi32 (mu1, mu1, mu1, mu1);
    __m128i m2 = _mm_set_epi32 (mu2, mu2, mu2, mu2);
    __m128i m3 = _mm_set_epi32 (mu3, mu3, mu3, mu3);
    __m128i m4 = _mm_set_epi32 (mu4, mu4, mu4, mu4);
    __m128i mcnt;
    mcnt = _mm_xor_si128(m1, m1); // cnt = 0
    do
    {
        __m128i tmp1, tmp2;
        __m128i b = _mm_load_si128(block++);

        tmp1 = _mm_load_si128(mask_block++);
        
        b = sse2_func(b, tmp1);
                        
        // b = (b & 0x55555555) + (b >> 1 & 0x55555555);
        tmp1 = _mm_srli_epi32(b, 1);                    // tmp1 = (b >> 1 & 0x55555555)
        tmp1 = _mm_and_si128(tmp1, m1); 
        tmp2 = _mm_and_si128(b, m1);                    // tmp2 = (b & 0x55555555)
        b    = _mm_add_epi32(tmp1, tmp2);               //  b = tmp1 + tmp2

        // b = (b & 0x33333333) + (b >> 2 & 0x33333333);
        tmp1 = _mm_srli_epi32(b, 2);                    // (b >> 2 & 0x33333333)
        tmp1 = _mm_and_si128(tmp1, m2); 
        tmp2 = _mm_and_si128(b, m2);                    // (b & 0x33333333)
        b    = _mm_add_epi32(tmp1, tmp2);               // b = tmp1 + tmp2

        // b = (b + (b >> 4)) & 0x0F0F0F0F;
        tmp1 = _mm_srli_epi32(b, 4);                    // tmp1 = b >> 4
        b = _mm_add_epi32(b, tmp1);                     // b = b + (b >> 4)
        b = _mm_and_si128(b, m3);                       //           & 0x0F0F0F0F

        // b = b + (b >> 8);
        tmp1 = _mm_srli_epi32 (b, 8);                   // tmp1 = b >> 8
        b = _mm_add_epi32(b, tmp1);                     // b = b + (b >> 8)
        
        // b = (b + (b >> 16)) & 0x0000003F;
        tmp1 = _mm_srli_epi32 (b, 16);                  // b >> 16
        b = _mm_add_epi32(b, tmp1);                     // b + (b >> 16)
        b = _mm_and_si128(b, m4);                       // (b >> 16) & 0x0000003F;

        mcnt = _mm_add_epi32(mcnt, b);                  // mcnt += b

    } while (block < block_end);

    bm::id_t BM_ALIGN16 tcnt[4] BM_ALIGN16ATTR;
    _mm_store_si128((__m128i*)tcnt, mcnt);

    return tcnt[0] + tcnt[1] + tcnt[2] + tcnt[3];
}


inline
bm::id_t sse2_bit_block_calc_count_change(const __m128i* BMRESTRICT block,
                                          const __m128i* BMRESTRICT block_end,
                                               unsigned* BMRESTRICT bit_count)
{
   const unsigned mu1 = 0x55555555;
   const unsigned mu2 = 0x33333333;
   const unsigned mu3 = 0x0F0F0F0F;
   const unsigned mu4 = 0x0000003F;

   // Loading masks
   __m128i m1 = _mm_set_epi32 (mu1, mu1, mu1, mu1);
   __m128i m2 = _mm_set_epi32 (mu2, mu2, mu2, mu2);
   __m128i m3 = _mm_set_epi32 (mu3, mu3, mu3, mu3);
   __m128i m4 = _mm_set_epi32 (mu4, mu4, mu4, mu4);
   __m128i mcnt;//, ccnt;
   mcnt = _mm_xor_si128(m1, m1); // bit_cnt = 0
   //ccnt = _mm_xor_si128(m1, m1); // change_cnt = 0

   __m128i tmp1, tmp2;

   int count = (int)(block_end - block)*4; //0;//1;

   bm::word_t  w, w0, w_prev;//, w_l;
   const int w_shift = sizeof(w) * 8 - 1;
   bool first_word = true;
 
   // first word
   {
       const bm::word_t* blk = (const bm::word_t*) block;
       w = w0 = blk[0];
       w ^= (w >> 1);
       BM_INCWORD_BITCOUNT(count, w);
       count -= (w_prev = (w0 >> w_shift)); // negative value correction
   }

   bm::id_t BM_ALIGN16 tcnt[4] BM_ALIGN16ATTR;

   do
   {
       // compute bit-count
       // ---------------------------------------------------------------------
       {
       __m128i b = _mm_load_si128(block);

       // w ^(w >> 1)
       tmp1 = _mm_srli_epi32(b, 1);       // tmp1 = b >> 1
       tmp2 = _mm_xor_si128(b, tmp1);     // tmp2 = tmp1 ^ b;
       _mm_store_si128((__m128i*)tcnt, tmp2);
       

       // compare with zero
       // SSE4: _mm_test_all_zero()
       {
           // b = (b & 0x55555555) + (b >> 1 & 0x55555555);
           //tmp1 = _mm_srli_epi32(b, 1);                    // tmp1 = (b >> 1 & 0x55555555)
           tmp1 = _mm_and_si128(tmp1, m1);
           tmp2 = _mm_and_si128(b, m1);                    // tmp2 = (b & 0x55555555)
           b    = _mm_add_epi32(tmp1, tmp2);               //  b = tmp1 + tmp2

           // b = (b & 0x33333333) + (b >> 2 & 0x33333333);
           tmp1 = _mm_srli_epi32(b, 2);                    // (b >> 2 & 0x33333333)
           tmp1 = _mm_and_si128(tmp1, m2);
           tmp2 = _mm_and_si128(b, m2);                    // (b & 0x33333333)
           b    = _mm_add_epi32(tmp1, tmp2);               // b = tmp1 + tmp2

           // b = (b + (b >> 4)) & 0x0F0F0F0F;
           tmp1 = _mm_srli_epi32(b, 4);                    // tmp1 = b >> 4
           b = _mm_add_epi32(b, tmp1);                     // b = b + (b >> 4)
           b = _mm_and_si128(b, m3);                       //& 0x0F0F0F0F

           // b = b + (b >> 8);
           tmp1 = _mm_srli_epi32 (b, 8);                   // tmp1 = b >> 8
           b = _mm_add_epi32(b, tmp1);                     // b = b + (b >> 8)

           // b = (b + (b >> 16)) & 0x0000003F;
           tmp1 = _mm_srli_epi32 (b, 16);                  // b >> 16
           b = _mm_add_epi32(b, tmp1);                     // b + (b >> 16)
           b = _mm_and_si128(b, m4);                       // (b >> 16) & 0x0000003F;

           mcnt = _mm_add_epi32(mcnt, b);                  // mcnt += b
       }

       }
       // ---------------------------------------------------------------------
       {
           //__m128i b = _mm_load_si128(block);
           // TODO: SSE4...
           //w = _mm_extract_epi32(b, i);               

           const bm::word_t* BMRESTRICT blk = (const bm::word_t*) block;

           if (first_word)
           {
               first_word = false;
           }
           else
           {
               if (0!=(w0=blk[0]))
               {
                   BM_INCWORD_BITCOUNT(count, tcnt[0]);
                   count -= !(w_prev ^ (w0 & 1));
                   count -= w_prev = (w0 >> w_shift);
               }
               else
               {
                   count -= !w_prev; w_prev ^= w_prev;
               }  
           }
           if (0!=(w0=blk[1]))
           {
               BM_INCWORD_BITCOUNT(count, tcnt[1]);
               count -= !(w_prev ^ (w0 & 1));
               count -= w_prev = (w0 >> w_shift);                    
           }
           else
           {
               count -= !w_prev; w_prev ^= w_prev;
           }    
           if (0!=(w0=blk[2]))
           {
               BM_INCWORD_BITCOUNT(count, tcnt[2]);
               count -= !(w_prev ^ (w0 & 1));
               count -= w_prev = (w0 >> w_shift);                    
           }
           else
           {
               count -= !w_prev; w_prev ^= w_prev;
           }      
           if (0!=(w0=blk[3]))
           {
               BM_INCWORD_BITCOUNT(count, tcnt[3]);
               count -= !(w_prev ^ (w0 & 1));
               count -= w_prev = (w0 >> w_shift);                    
           }
           else
           {
               count -= !w_prev; w_prev ^= w_prev;
           }               
       }
   } while (++block < block_end);

   _mm_store_si128((__m128i*)tcnt, mcnt);
   *bit_count = tcnt[0] + tcnt[1] + tcnt[2] + tcnt[3];

   return unsigned(count);
}

#ifdef __GNUG__
// necessary measure to silence false warning from GCC about negative pointer arithmetics
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

/*!
SSE4.2 check for one to two (variable len) 128 bit SSE lines for gap search results (8 elements)
\internal
*/
inline
unsigned sse2_gap_find(const bm::gap_word_t* BMRESTRICT pbuf, const bm::gap_word_t pos, const unsigned size)
{
    BM_ASSERT(size <= 16);
    BM_ASSERT(size);

    const unsigned unroll_factor = 8;
    if (size < 4) // for very short vector use conventional scan
    {
        unsigned j;
        for (j = 0; j < size; ++j)
        {
            if (pbuf[j] >= pos)
                break;
        }
        return j;
    }

    __m128i m1, mz, maskF, maskFL;

    mz = _mm_setzero_si128();
    m1 = _mm_loadu_si128((__m128i*)(pbuf)); // load first 8 elements

    maskF = _mm_cmpeq_epi32(mz, mz); // set all FF
    maskFL = _mm_slli_si128(maskF, 4 * 2); // byle shift to make [0000 FFFF] 
    int shiftL = (64 - (unroll_factor - size) * 16);
    maskFL = _mm_slli_epi64(maskFL, shiftL); // additional bit shift to  [0000 00FF]

    m1 = _mm_andnot_si128(maskFL, m1); // m1 = (~mask) & m1
    m1 = _mm_or_si128(m1, maskFL);

    __m128i mp = _mm_set1_epi16(pos);  // broadcast pos into all elements of a SIMD vector
    __m128i  mge_mask = _mm_cmpeq_epi16(_mm_subs_epu16(mp, m1), mz); // unsigned m1 >= mp
    int mi = _mm_movemask_epi8(mge_mask);  // collect flag bits
    if (mi)
    {
        int bsr_i= bm::bit_scan_fwd(mi) >> 1;
        return bsr_i;   // address of first one element (target)
    }
    if (size == 8)
        return size;

    // inspect the next lane with possible step back (to avoid over-read the block boundaries)
    //   GCC gives a false warning for "- unroll_factor" here
    const bm::gap_word_t* BMRESTRICT pbuf2 = pbuf + size - unroll_factor;
    BM_ASSERT(pbuf2 > pbuf); // assert in place to make sure GCC warning is indeed false

    m1 = _mm_loadu_si128((__m128i*)(pbuf2)); // load next elements (with possible overlap)
    mge_mask = _mm_cmpeq_epi16(_mm_subs_epu16(mp, m1), mz); // m1 >= mp        
    mi = _mm_movemask_epi8(mge_mask);
    if (mi)
    {
        int bsr_i = bm::bit_scan_fwd(mi) >> 1;
        return size - (unroll_factor - bsr_i);
    }
    return size;
}

/**
    Hybrid binary search, starts as binary, then switches to linear scan

   \param buf - GAP buffer pointer.
   \param pos - index of the element.
   \param is_set - output. GAP value (0 or 1).
   \return GAP index.

    @ingroup SSE2
*/
inline
unsigned sse2_gap_bfind(const unsigned short* BMRESTRICT buf,
                         unsigned pos, unsigned* BMRESTRICT is_set)
{
    unsigned start = 1;
    unsigned end = 1 + ((*buf) >> 3);
    unsigned dsize = end - start;

    if (dsize < 17)
    {
        start = bm::sse2_gap_find(buf+1, (bm::gap_word_t)pos, dsize);
        *is_set = ((*buf) & 1) ^ (start & 1);
        BM_ASSERT(buf[start+1] >= pos);
        BM_ASSERT(buf[start] < pos || (start==0));

        return start+1;
    }
    unsigned arr_end = end;
    while (start != end)
    {
        unsigned curr = (start + end) >> 1;
        if (buf[curr] < pos)
            start = curr + 1;
        else
            end = curr;

        unsigned size = end - start;
        if (size < 16)
        {
            size += (end != arr_end);
            unsigned idx =
                bm::sse2_gap_find(buf + start, (bm::gap_word_t)pos, size);
            start += idx;

            BM_ASSERT(buf[start] >= pos);
            BM_ASSERT(buf[start - 1] < pos || (start == 1));
            break;
        }
    }

    *is_set = ((*buf) & 1) ^ ((start-1) & 1);
    return start;
}

/**
    Hybrid binary search, starts as binary, then switches to scan
    @ingroup SSE2
*/
inline
unsigned sse2_gap_test(const unsigned short* BMRESTRICT buf, unsigned pos)
{
    unsigned is_set;
    bm::sse2_gap_bfind(buf, pos, &is_set);
    return is_set;
}


#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif


#define VECT_XOR_ARR_2_MASK(dst, src, src_end, mask)\
    sse2_xor_arr_2_mask((__m128i*)(dst), (__m128i*)(src), (__m128i*)(src_end), (bm::word_t)mask)

#define VECT_ANDNOT_ARR_2_MASK(dst, src, src_end, mask)\
    sse2_andnot_arr_2_mask((__m128i*)(dst), (__m128i*)(src), (__m128i*)(src_end), (bm::word_t)mask)

#define VECT_BITCOUNT(first, last) \
    sse2_bit_count((__m128i*) (first), (__m128i*) (last))

#define VECT_BITCOUNT_AND(first, last, mask) \
    sse2_bit_count_op((__m128i*) (first), (__m128i*) (last), (__m128i*) (mask), sse2_and)

#define VECT_BITCOUNT_OR(first, last, mask) \
    sse2_bit_count_op((__m128i*) (first), (__m128i*) (last), (__m128i*) (mask), sse2_or)

#define VECT_BITCOUNT_XOR(first, last, mask) \
    sse2_bit_count_op((__m128i*) (first), (__m128i*) (last), (__m128i*) (mask), sse2_xor)

#define VECT_BITCOUNT_SUB(first, last, mask) \
    sse2_bit_count_op((__m128i*) (first), (__m128i*) (last), (__m128i*) (mask), sse2_sub)

#define VECT_INVERT_BLOCK(first) \
    sse2_invert_block((__m128i*)first);

#define VECT_AND_BLOCK(dst, src) \
    sse2_and_block((__m128i*) dst, (__m128i*) (src))

#define VECT_OR_BLOCK(dst, src) \
    sse2_or_block((__m128i*) dst, (__m128i*) (src))

#define VECT_OR_BLOCK_2WAY(dst, src1, src2) \
    sse2_or_block_2way((__m128i*) (dst), (__m128i*) (src1), (__m128i*) (src2))

#define VECT_OR_BLOCK_3WAY(dst, src1, src2) \
    sse2_or_block_3way((__m128i*) (dst), (__m128i*) (src1), (__m128i*) (src2))

#define VECT_OR_BLOCK_5WAY(dst, src1, src2, src3, src4) \
    sse2_or_block_5way((__m128i*) (dst), (__m128i*) (src1), (__m128i*) (src2), (__m128i*) (src3), (__m128i*) (src4))

#define VECT_SUB_BLOCK(dst, src) \
    sse2_sub_block((__m128i*) dst, (__m128i*) (src))

#define VECT_XOR_BLOCK(dst, src) \
    sse2_xor_block((__m128i*) dst, (__m128i*) (src))

#define VECT_XOR_BLOCK_2WAY(dst, src1, src2) \
    sse2_xor_block_2way((__m128i*) (dst), (const __m128i*) (src1), (const __m128i*) (src2))

#define VECT_COPY_BLOCK(dst, src) \
    sse2_copy_block((__m128i*) dst, (__m128i*) (src))

#define VECT_COPY_BLOCK_UNALIGN(dst, src) \
    sse2_copy_block_unalign((__m128i*) dst, (__m128i*) (src))

#define VECT_STREAM_BLOCK(dst, src) \
    sse2_stream_block((__m128i*) dst, (__m128i*) (src))

#define VECT_STREAM_BLOCK_UNALIGN(dst, src) \
    sse2_stream_block_unalign((__m128i*) dst, (__m128i*) (src))

#define VECT_SET_BLOCK(dst, value) \
    sse2_set_block((__m128i*) dst, value)

#define VECT_GAP_BFIND(buf, pos, is_set) \
    sse2_gap_bfind(buf, pos, is_set)


} // namespace


#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif


#endif
