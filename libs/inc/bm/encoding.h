#ifndef BMENCODING_H__INCLUDED__
#define BMENCODING_H__INCLUDED__
/*
Copyright(c) 2002-2024 Anatoliy Kuznetsov(anatoliy_kuznetsov at yahoo.com)

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

/*! \file encoding.h
    \brief Encoding utilities for serialization (internal)
*/


#include <memory.h>
#include "bmutil.h"


#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4702)
#endif 

namespace bm
{


// ----------------------------------------------------------------

/*!
   \brief Memory encoding.
   
   Class for encoding data into memory. 
   Class handles aligment issues with the integer data types.
   
   \ingroup gammacode
*/
class encoder
{
public:
    typedef unsigned char* position_type;
public:
    encoder(unsigned char* buf, size_t size) BMNOEXCEPT;
    void put_8(unsigned char c) BMNOEXCEPT;
    void put_16(bm::short_t  s) BMNOEXCEPT;
    void put_16(const bm::short_t* s, unsigned count) BMNOEXCEPT;
    void put_24(bm::word_t  w) BMNOEXCEPT;
    void put_32(bm::word_t  w) BMNOEXCEPT;
    void put_32(const bm::word_t* w, unsigned count) BMNOEXCEPT;
    void put_48(bm::id64_t w) BMNOEXCEPT;
    void put_64(bm::id64_t w) BMNOEXCEPT;
    void put_h64(bm::id64_t w) BMNOEXCEPT;

    void put_8_16_32(unsigned w,
     unsigned char c8, unsigned char c16, unsigned char c32) BMNOEXCEPT;
    void put_prefixed_array_32(unsigned char c, 
                               const bm::word_t* w, unsigned count) BMNOEXCEPT;
    void put_prefixed_array_16(unsigned char c, 
                               const bm::short_t* s, unsigned count,
                               bool encode_count) BMNOEXCEPT;
    void put_short_array_16(const bm::short_t* s, unsigned count) BMNOEXCEPT;
    void memcpy(const unsigned char* src, size_t count) BMNOEXCEPT;
    void move_from(encoder& enc) BMNOEXCEPT;
    size_t size() const BMNOEXCEPT;
    unsigned char* get_pos() const BMNOEXCEPT;
    void set_pos(unsigned char* buf_pos) BMNOEXCEPT;
    void reset() BMNOEXCEPT;
private:
    unsigned char*  buf_;
    unsigned char*  start_;
    size_t          size_;
};

// ----------------------------------------------------------------
/**
    Base class for all decoding functionality
    \ingroup gammacode
*/
class decoder_base
{
public:
    decoder_base(const unsigned char* buf) BMNOEXCEPT { buf_ = start_ = buf; }
    
    /// Reads character from the decoding buffer. 
    unsigned char get_8() BMNOEXCEPT { return *buf_++; }
    
    /// Returns size of the current decoding stream.
    size_t size() const BMNOEXCEPT { return size_t(buf_ - start_); }
    
    /// change current position
    void seek(int delta) BMNOEXCEPT { buf_ += delta; }
    
    /// read bytes from the decode buffer
    void memcpy(unsigned char* dst, size_t count) BMNOEXCEPT;
    
    /// Return current buffer pointer
    const unsigned char* get_pos() const BMNOEXCEPT { return buf_; }

    /// Set current buffer pointer
    void set_pos(const unsigned char* pos) BMNOEXCEPT { buf_ = pos; }

    /// Read h-64-bit
    bm::id64_t get_h64() BMNOEXCEPT;

protected:
   const unsigned char*   buf_;
   const unsigned char*   start_;
};


// ----------------------------------------------------------------
/**
   Class for decoding data from memory buffer.
   Properly handles aligment issues with integer data types.
   \ingroup gammacode
*/
class decoder : public decoder_base
{
public:
    decoder(const unsigned char* buf) BMNOEXCEPT;
    bm::short_t get_16() BMNOEXCEPT;
    bm::word_t get_24() BMNOEXCEPT;
    bm::word_t get_32() BMNOEXCEPT;
    bm::id64_t get_48() BMNOEXCEPT;
    bm::id64_t get_64() BMNOEXCEPT;
    void get_32(bm::word_t* w, unsigned count) BMNOEXCEPT;
    bool get_32_OR(bm::word_t* w, unsigned count) BMNOEXCEPT;
    void get_32_AND(bm::word_t* w, unsigned count) BMNOEXCEPT;
    void get_16(bm::short_t* s, unsigned count) BMNOEXCEPT;
};

// ----------------------------------------------------------------
/**
   Class for decoding data from memory buffer.
   Properly handles aligment issues with integer data types.
   Converts data to big endian architecture 
   (presumed it was encoded as little endian)
   \ingroup gammacode
*/
typedef decoder decoder_big_endian;


// ----------------------------------------------------------------
/**
   Class for decoding data from memory buffer.
   Properly handles aligment issues with integer data types.
   Converts data to little endian architecture 
   (presumed it was encoded as big endian)
   \ingroup gammacode
*/
class decoder_little_endian : public decoder_base
{
public:
    decoder_little_endian(const unsigned char* buf);
    bm::short_t get_16();
    bm::word_t get_24();
    bm::word_t get_32();
    bm::id64_t get_48();
    bm::id64_t get_64();
    void get_32(bm::word_t* w, unsigned count);
    bool get_32_OR(bm::word_t* w, unsigned count);
    void get_32_AND(bm::word_t* w, unsigned count);
    void get_16(bm::short_t* s, unsigned count);
};


/** 
    Byte based writer for un-aligned bit streaming 

    @ingroup gammacode
    @sa encoder
*/
template<class TEncoder>
class bit_out
{
public:
    bit_out(TEncoder& dest)
        : dest_(dest), used_bits_(0), accum_(0)
    {}

    ~bit_out() { flush(); }
    
    /// issue single bit into encode bit-stream
    void put_bit(unsigned value) BMNOEXCEPT;

    /// issue count bits out of value
    void put_bits(unsigned value, unsigned count) BMNOEXCEPT;

    /// issue 0 into output stream
    void put_zero_bit() BMNOEXCEPT;

    /// issue specified number of 0s
    void put_zero_bits(unsigned count) BMNOEXCEPT;

    /// Elias Gamma encode the specified value
    void gamma(unsigned value) BMNOEXCEPT;

    /// Elias Gamma encode the specified value
    void gamma8(unsigned value) BMNOEXCEPT;

    /// Custome variant of delta encoding
    void delta16(unsigned value) BMNOEXCEPT;

    /// Custome variant of delta encoding (safe for 8-bit values)
    void delta16s(unsigned value) BMNOEXCEPT;

    /// put 16 bits neutral byte order
    void put_16_no(unsigned value) BMNOEXCEPT;

    /// put 24 bits neutral byte order
    void put_24_no(unsigned value) BMNOEXCEPT;

    /// put 32 bits neutral byte order
    void put_32_no(unsigned value) BMNOEXCEPT;

    /// put 64 bits neutral byte order
    void put_64_no(unsigned long long value) BMNOEXCEPT;

    /// Binary Interpolative array decode
    void bic_encode_u16(const bm::gap_word_t* arr, unsigned sz,
                        bm::gap_word_t lo, bm::gap_word_t hi) BMNOEXCEPT
    {
        bic_encode_u16_cm(arr, sz, lo, hi);
    }

    /// Binary Interpolative encoding (array of 16-bit ints)
    void bic_encode_u16_rg(const bm::gap_word_t* arr, unsigned sz,
                           bm::gap_word_t lo,
                           bm::gap_word_t hi) BMNOEXCEPT;
    
    /// Binary Interpolative encoding (array of 16-bit ints)
    /// cm - "center-minimal"
    void bic_encode_u16_cm(const bm::gap_word_t* arr, unsigned sz,
                           bm::gap_word_t lo,
                           bm::gap_word_t hi) BMNOEXCEPT;

    /// Binary Interpolative encoding (array of 32-bit ints)
    /// cm - "center-minimal"
    void bic_encode_u32_cm(const bm::word_t* arr, unsigned sz,
                           bm::word_t lo, bm::word_t hi) BMNOEXCEPT;


    /// Selective array serialization (BIC encoding is used)
    ///
    /// @param force_code  - (0 - no force code, 1 - gamma, 2 - delta gamma)
    ///
    void encode_array(const bm::gap_word_t* arr, bm::gap_word_t* recalc_arr,
                      bm::word_t* tb_wflags, bm::gap_word_t* tmp_arr,
                      unsigned sz,
                      bool one_flag,
                      bool EOC_flag,
                      unsigned force_code = 0,
                      bool save_size = true) BMNOEXCEPT;

    /// Flush the incomplete 32-bit accumulator word
    void flush() BMNOEXCEPT { if (used_bits_) flush_accum(); }

    void flush_if_full() BMNOEXCEPT
    {
        if (used_bits_ == 32)
            flush_accum();
    }

private:
    void flush_accum() BMNOEXCEPT
    {
        dest_.put_32(accum_);
        used_bits_ = accum_ = 0;
    }
private:
    bit_out(const bit_out&);
    bit_out& operator=(const bit_out&);

private:
    TEncoder&      dest_;      ///< Bit stream target
    unsigned       used_bits_; ///< Bits used in the accumulator
    unsigned       accum_;     ///< write bit accumulator 
};


/** 
    Byte based reader for un-aligned bit streaming 

    @ingroup gammacode
    @sa encoder
*/
template<class TDecoder>
class bit_in
{
public:
    bit_in(TDecoder& decoder) BMNOEXCEPT
        : src_(decoder),
          used_bits_(unsigned(sizeof(accum_) * 8)),
          accum_(0) 
    {}

    /// decode unsigned value using Elias Gamma coding
    unsigned gamma() BMNOEXCEPT;

    /// decode unsigned value using Elias Gamma coding
    unsigned gamma8() BMNOEXCEPT;

    /// Custome variant of delta decoding
    unsigned delta16() BMNOEXCEPT;

    /// Custome variant of delta decoding
    unsigned delta16s() BMNOEXCEPT;

    /// get 16 bits neutral order from the bit-stream
    unsigned get_16_no() BMNOEXCEPT;

    /// get 24 bits neutral order from the bit-stream
    unsigned get_24_no() BMNOEXCEPT;

    /// get 32 bits neutral order from the bit-stream
    unsigned get_32_no() BMNOEXCEPT;

    // get 64-bit neutral order from the bit-stream
    unsigned long long get_64_no() BMNOEXCEPT;

    /// read number of bits out of the stream
    unsigned get_bits(unsigned count) BMNOEXCEPT;

    /// read 1 bit
    unsigned get_bit() BMNOEXCEPT;

    /// Binary Interpolative array decode
    void bic_decode_u16(bm::gap_word_t* arr, unsigned sz,
                        bm::gap_word_t lo, bm::gap_word_t hi) BMNOEXCEPT
    {
        if (sz)
            bic_decode_u16_cm(arr, sz, lo, hi);
    }
    
    void bic_decode_u16_bitset(bm::word_t* block, unsigned sz,
                               bm::gap_word_t lo, bm::gap_word_t hi) BMNOEXCEPT
    {
        if (sz)
            bic_decode_u16_cm_bitset(block, sz, lo, hi);
    }
    void bic_decode_u16_dry(unsigned sz,
                            bm::gap_word_t lo, bm::gap_word_t hi) BMNOEXCEPT
    {
        if (sz)
            bic_decode_u16_cm_dry(sz, lo, hi);
    }


    /// Binary Interpolative array decode
    void bic_decode_u16_rg(bm::gap_word_t* arr, unsigned sz,
                           bm::gap_word_t lo, bm::gap_word_t hi) BMNOEXCEPT;
    /// Binary Interpolative array decode
    void bic_decode_u16_cm(bm::gap_word_t* arr, unsigned sz,
                           bm::gap_word_t lo, bm::gap_word_t hi) BMNOEXCEPT;

    /// Binary Interpolative array decode (32-bit)
    void bic_decode_u32_cm(bm::word_t* arr, unsigned sz,
                           bm::word_t lo, bm::word_t hi) BMNOEXCEPT;


    /// Binary Interpolative array decode into bitset (32-bit based)
    void bic_decode_u16_rg_bitset(bm::word_t* block, unsigned sz,
                                  bm::gap_word_t lo, bm::gap_word_t hi) BMNOEXCEPT;

    /// Binary Interpolative array decode into /dev/null
    void bic_decode_u16_rg_dry(unsigned sz,
                               bm::gap_word_t lo, bm::gap_word_t hi) BMNOEXCEPT;

    /// Binary Interpolative array decode into bitset (32-bit based)
    void bic_decode_u16_cm_bitset(bm::word_t* block, unsigned sz,
                                  bm::gap_word_t lo,
                                  bm::gap_word_t hi) BMNOEXCEPT;

    /// Binary Interpolative array decode into /dev/null
    void bic_decode_u16_cm_dry(unsigned sz,
                               bm::gap_word_t lo, bm::gap_word_t hi) BMNOEXCEPT;

    /// Selective array decode
    /// @return bit-flag of decoding parameters
    ///
    unsigned decode_array(bm::gap_word_t* arr, bm::word_t* tb_wflags,
                          unsigned* sz,
                          unsigned default_sz = 0) BMNOEXCEPT;


private:
    bit_in(const bit_in&);
    bit_in& operator=(const bit_in&);
private:
    TDecoder&           src_;        ///< Source of bytes
    unsigned            used_bits_;  ///< Bits used in the accumulator
    unsigned            accum_;      ///< read bit accumulator
};


/**
    Functor for Elias Gamma encoding
    @ingroup gammacode
*/
template<typename T, typename TBitIO>
class gamma_encoder
{
public:
    gamma_encoder(TBitIO& bout) : bout_(bout) 
    {}
        
    /** Encode word */
    void operator()(T value) { bout_.gamma(value); }
private:
    gamma_encoder(const gamma_encoder&);
    gamma_encoder& operator=(const gamma_encoder&);
private:
    TBitIO&  bout_;
};


/**
    Elias Gamma decoder
    @ingroup gammacode
*/
template<typename T, typename TBitIO>
class gamma_decoder
{
public:
    gamma_decoder(TBitIO& bin) : bin_(bin) {}
    
    /**
        Start encoding sequence
    */
    void start() {}
    
    /**
        Stop decoding sequence
    */
    void stop() {}
    
    /**
        Decode word
    */
    T operator()(void) { return (T)bin_.gamma(); }
private:
    gamma_decoder(const gamma_decoder&);
    gamma_decoder& operator=(const gamma_decoder&);
private:
    TBitIO&  bin_;
};


// ----------------------------------------------------------------
// Implementation details. 
// ----------------------------------------------------------------

/*!
    \brief Compute gamma size( in bytes)
*/
template<typename T>
unsigned gamma_size(const T v) BMNOEXCEPT
{
    BM_ASSERT(v);
    unsigned l = bm::bit_scan_reverse(v); // log2
    return (l*2) + 1;
}

/*!
    \brief Compute gamma size( in bytes)
*/
template<typename T>
unsigned gamma_size(const T* arr, unsigned len) BMNOEXCEPT
{
    BM_ASSERT(len);
    unsigned bits = 0;
    for (unsigned i = 0; i < len; ++i)
    {
        if (!arr[i])
            return ~0u; // impossible value
        bits += bm::gamma_size(arr[i]);
    }
    return (bits+7) / 8; // round up to byte
}

/*!
    \brief Compute gamma size( in bytes)
*/
template<typename T>
unsigned gamma_delta_size(const T* arr, unsigned len) BMNOEXCEPT
{
    BM_ASSERT(len);
    unsigned bits = bm::gamma_size(arr[0]+1);
    for (unsigned i = 1; i < len; ++i)
    {
        BM_ASSERT(arr[i] > arr[i-1]);
        T delta = arr[i] - arr[i-1];
        bits += bm::gamma_size(delta);
    } // for i
    return (bits+7) / 8; // round up to byte
}

/*!
    \brief Compute GAP delta-gamma size( in bytes)
*/
template<typename T>
unsigned gamma_gap_delta_size(const T* gap_buf) BMNOEXCEPT
{
    const T* pcurr = gap_buf;
    const T* pend = pcurr + (*pcurr >> 3);
    BM_ASSERT(*pend == 65535);
    ++pcurr;
    unsigned bits = 16;
    T prev = *pcurr;
    bits += bm::gamma_size(prev+1);

    ++pcurr;
    do
    {
        if (*pcurr == 65535)
            break;
        auto delta = (*pcurr - prev);
        BM_ASSERT(delta);
        bits += bm::gamma_size(delta);
        prev = *pcurr;
    } while (++pcurr < pend);
    return (bits+7) / 8; // round up to byte
}

/*!
    \brief Compute various GAP encoding characteristics
    @internal
*/
template<typename T>
void gap_survey(const T* gap_buf,
                unsigned& gamma_size_bits) BMNOEXCEPT
{
    const T* pcurr = gap_buf;
    unsigned len = (*pcurr >> 3);
    const T* pend = pcurr + len;
    BM_ASSERT(*pend == 65535);

    ++pcurr;
    gamma_size_bits = 2 + bm::gamma_size(len);

    T prev = *pcurr;
    gamma_size_bits += bm::gamma_size(prev+1);
    ++pcurr;
    do
    {
        if (*pcurr == 65535)
            break;
        auto delta = (*pcurr - prev);
        BM_ASSERT(delta);
        gamma_size_bits += bm::gamma_size(delta);
        prev = *pcurr;
    } while (++pcurr < pend);
}



/*! 
    \fn encoder::encoder(unsigned char* buf, unsigned size) 
    \brief Construction.
    \param buf - memory buffer pointer.
    \param size - size of the buffer
*/
inline encoder::encoder(unsigned char* buf, size_t a_size) BMNOEXCEPT
: buf_(buf), start_(buf)
{
    size_ = a_size;
}
/*!
    \brief Encode 8-bit prefix + an array
*/
inline void encoder::put_prefixed_array_32(unsigned char c, 
                                           const bm::word_t* w, 
                                           unsigned count) BMNOEXCEPT
{
    put_8(c);
    put_32(w, count);
}

/*!
    \brief Encode 8-bit prefix + an array 
*/
inline void encoder::put_prefixed_array_16(unsigned char c, 
                                           const bm::short_t* s, 
                                           unsigned count,
                                           bool encode_count) BMNOEXCEPT
{
    put_8(c);
    if (encode_count)
        put_16((bm::short_t) count);
    put_16(s, count);
}

/*!
    \brief Encode array of shorts less than 256 in length
*/
inline void encoder::put_short_array_16(const bm::short_t* s,
                                        unsigned count) BMNOEXCEPT
{
    BM_ASSERT(count < 256);
    put_8((unsigned char)count);
    put_16(s, count);
}

/*!
   \fn void encoder::put_8(unsigned char c) 
   \brief Puts one character into the encoding buffer.
   \param c - character to encode
*/
inline void encoder::put_8(unsigned char c) BMNOEXCEPT
{
    *buf_++ = c;
}

/*!
   \fn encoder::put_16(bm::short_t s)
   \brief Puts short word (16 bits) into the encoding buffer.
   \param s - short word to encode
*/
inline void encoder::put_16(bm::short_t s) BMNOEXCEPT
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(buf_, &s, sizeof(bm::short_t)); // optimizer takes care of it
	buf_ += sizeof(bm::short_t);
#else
    *buf_++ = (unsigned char) s;
    s >>= 8;
    *buf_++ = (unsigned char) s;
#endif
}

/*!
   \brief Method puts array of short words (16 bits) into the encoding buffer.
*/
inline void encoder::put_16(const bm::short_t* s, unsigned count) BMNOEXCEPT
{
    BM_ASSERT(count);
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(buf_, s, sizeof(bm::short_t)*count);
    buf_ += sizeof(bm::short_t) * count;
#else
    unsigned char* buf = buf_;
    const bm::short_t* s_end = s + count;
    do 
    {
        bm::short_t w16 = *s++;
        unsigned char a = (unsigned char)  w16;
        unsigned char b = (unsigned char) (w16 >> 8);
        
        *buf++ = a;
        *buf++ = b;
                
    } while (s < s_end);
    
    buf_ = (unsigned char*)buf;
#endif
}

/*!
    \brief but gat plus value based on its VBR evaluation
*/
inline
void encoder::put_8_16_32(unsigned w,
                          unsigned char c8,
                          unsigned char c16,
                          unsigned char c32) BMNOEXCEPT
{
    if (w < 256)
    {
        put_8(c8);
        put_8((unsigned char)w);
    }
    else
    {
        if (w < 65536)
        {
            put_8(c16);
            put_16((unsigned short) w);
        }
        else
        {
            put_8(c32);
            put_32(w);
        }
    }
}

/*!
    \brief rewind the encoder to the start buffer position
*/
inline void encoder::reset() BMNOEXCEPT
{
    buf_ = start_;
}

/*!
    \brief copy bytes into target buffer or just rewind if src is NULL
*/
inline
void encoder::memcpy(const unsigned char* src, size_t count) BMNOEXCEPT
{
    BM_ASSERT(src);
    BM_ASSERT((buf_ + count) < (start_ + size_) || (size_ == 0));
//    if (src)
    ::memcpy(buf_, src, count);
    buf_ += count;
}

/*!
    \brief Move content from another encoder
*/
inline
void encoder::move_from(encoder& enc) BMNOEXCEPT
{
    BM_ASSERT(&enc != this);
    auto sz = enc.size();
    this->memcpy(enc.start_, sz);
    enc.reset();
}

/*!
   \fn unsigned encoder::size() const
   \brief Returns size of the current encoding stream.
*/
inline size_t encoder::size() const BMNOEXCEPT
{
    return size_t(buf_ - start_);
}

/**
    \brief Get current memory stream position
*/
inline encoder::position_type encoder::get_pos() const BMNOEXCEPT
{
    return buf_;
}

/**
    \brief Set current memory stream position
*/
inline void encoder::set_pos(encoder::position_type buf_pos) BMNOEXCEPT
{
    buf_ = buf_pos;
}

/*!
   \fn void encoder::put_24(bm::word_t w)
   \brief Puts 24 bits word into encoding buffer.
   \param w - word to encode.
*/
inline void encoder::put_24(bm::word_t w) BMNOEXCEPT
{
    BM_ASSERT((w & ~(0xFFFFFFU)) == 0);

    buf_[0] = (unsigned char)w;
    buf_[1] = (unsigned char)(w >> 8);
    buf_[2] = (unsigned char)(w >> 16);
    buf_ += 3;
}


/*!
   \fn void encoder::put_32(bm::word_t w)
   \brief Puts 32 bits word into encoding buffer.
   \param w - word to encode.
*/
inline void encoder::put_32(bm::word_t w) BMNOEXCEPT
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(buf_, &w, sizeof(bm::word_t));
    buf_ += sizeof(bm::word_t);
#else
    *buf_++ = (unsigned char) w;
    *buf_++ = (unsigned char) (w >> 8);
    *buf_++ = (unsigned char) (w >> 16);
    *buf_++ = (unsigned char) (w >> 24);
#endif
}

/*!
   \fn void encoder::put_48(bm::id64_t w)
   \brief Puts 48 bits word into encoding buffer.
   \param w - word to encode.
*/
inline void encoder::put_48(bm::id64_t w) BMNOEXCEPT
{ 
    BM_ASSERT((w & ~(0xFFFFFFFFFFFFUL)) == 0);
    *buf_++ = (unsigned char)w;
    *buf_++ = (unsigned char)(w >> 8);
    *buf_++ = (unsigned char)(w >> 16);
    *buf_++ = (unsigned char)(w >> 24);
    *buf_++ = (unsigned char)(w >> 32);
    *buf_++ = (unsigned char)(w >> 40);
}


/*!
   \fn void encoder::put_64(bm::id64_t w)
   \brief Puts 64 bits word into encoding buffer.
   \param w - word to encode.
*/
inline void encoder::put_64(bm::id64_t w) BMNOEXCEPT
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(buf_, &w, sizeof(bm::id64_t));
    buf_ += sizeof(bm::id64_t);
#else
    *buf_++ = (unsigned char) w;
    *buf_++ = (unsigned char) (w >> 8);
    *buf_++ = (unsigned char) (w >> 16);
    *buf_++ = (unsigned char) (w >> 24);
    *buf_++ = (unsigned char) (w >> 32);
    *buf_++ = (unsigned char) (w >> 40);
    *buf_++ = (unsigned char) (w >> 48);
    *buf_++ = (unsigned char) (w >> 56);
#endif
}

/*!
   \fn void encoder::put_h64(bm::id64_t w)
   \brief Puts 64 bits word into encoding buffer with h-compression
   \param w - word to encode.
*/
inline void encoder::put_h64(bm::id64_t w) BMNOEXCEPT
{
    unsigned h_mask = bm::compute_h64_mask(w);
    put_8((unsigned char) h_mask);
    for (unsigned i = 0; w && (i < 8); ++i, w >>= 8)
    {
        if ((unsigned char) w)
            put_8((unsigned char) w);
    } // for i
}



/*!
    \brief Encodes array of 32-bit words
*/
inline void encoder::put_32(const bm::word_t* w, unsigned count) BMNOEXCEPT
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    // use memcpy() because compilers now understand it as an idiom and inline
    ::memcpy(buf_, w, sizeof(bm::word_t) * count);
    buf_ += sizeof(bm::word_t) * count;
#else
    unsigned char* buf = buf_;
    const bm::word_t* w_end = w + count;
    do 
    {
        bm::word_t w32 = *w++;
        unsigned char a = (unsigned char) w32;
        unsigned char b = (unsigned char) (w32 >> 8);
        unsigned char c = (unsigned char) (w32 >> 16);
        unsigned char d = (unsigned char) (w32 >> 24);

        *buf++ = a;
        *buf++ = b;
        *buf++ = c;
        *buf++ = d;
    } while (w < w_end);
    
    buf_ = (unsigned char*)buf;
#endif
}


// ---------------------------------------------------------------------


/*!
    Load bytes from the decode buffer
*/
inline
void decoder_base::memcpy(unsigned char* dst, size_t count) BMNOEXCEPT
{
    if (dst)
        ::memcpy(dst, buf_, count);
    buf_ += count;
}

/*!
   \fn bm::id64_t decoder_base::get_h64()
   \brief Reads 64-bit word from the decoding buffer.
*/
inline
bm::id64_t decoder_base::get_h64() BMNOEXCEPT
{
    bm::id64_t w = 0;
    unsigned h_mask = (unsigned char) *buf_++;
    for (unsigned i = 0; h_mask && (i < 8); ++i)
    {
        if (h_mask & (1u<<i))
        {
            h_mask &= ~(1u<<i);
            unsigned char a = (unsigned char) *buf_++;
            w |= (bm::id64_t(a) << (i*8));
        }
    } // for i
    return w;
}


/*!
   \fn decoder::decoder(const unsigned char* buf) 
   \brief Construction
   \param buf - pointer to the decoding memory. 
*/
inline decoder::decoder(const unsigned char* buf) BMNOEXCEPT
: decoder_base(buf)
{
}

/*!
   \fn bm::short_t decoder::get_16()
   \brief Reads 16-bit word from the decoding buffer.
*/
BMFORCEINLINE bm::short_t decoder::get_16() BMNOEXCEPT
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    bm::short_t a;
    ::memcpy(&a, buf_, sizeof(bm::short_t));
#else
    bm::short_t a = (bm::short_t)(buf_[0] + ((bm::short_t)buf_[1] << 8));
#endif
	buf_ += sizeof(a);
    return a;
}

/*!
   \fn bm::word_t decoder::get_24()
   \brief Reads 32-bit word from the decoding buffer.
*/
inline bm::word_t decoder::get_24() BMNOEXCEPT
{
    bm::word_t a =
        buf_[0] + ((unsigned)buf_[1] << 8) + ((unsigned)buf_[2] << 16);
    buf_ += 3;
    return a;
}


/*!
   \fn bm::word_t decoder::get_32()
   \brief Reads 32-bit word from the decoding buffer.
*/
BMFORCEINLINE bm::word_t decoder::get_32() BMNOEXCEPT
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    bm::word_t a;
    ::memcpy(&a, buf_, sizeof(bm::word_t));
#else
	bm::word_t a = buf_[0]+ ((unsigned)buf_[1] << 8) +
                   ((unsigned)buf_[2] << 16) + ((unsigned)buf_[3] << 24);
#endif
    buf_+=sizeof(a);
    return a;
}

/*!
   \fn bm::word_t decoder::get_48()
   \brief Reads 64-bit word from the decoding buffer.
*/
inline
bm::id64_t decoder::get_48() BMNOEXCEPT
{
    bm::id64_t a = buf_[0] +
        ((bm::id64_t)buf_[1] << 8) +
        ((bm::id64_t)buf_[2] << 16) +
        ((bm::id64_t)buf_[3] << 24) +
        ((bm::id64_t)buf_[4] << 32) +
        ((bm::id64_t)buf_[5] << 40);
    buf_ += 6;
    return a;
}

/*!
   \fn bm::id64_t decoder::get_64()
   \brief Reads 64-bit word from the decoding buffer.
*/
inline
bm::id64_t decoder::get_64() BMNOEXCEPT
{
#if (BM_UNALIGNED_ACCESS_OK == 1)
    bm::id64_t a;
    ::memcpy(&a, buf_, sizeof(bm::id64_t));
#else
	bm::id64_t a = buf_[0]+
                   ((bm::id64_t)buf_[1] << 8)  +
                   ((bm::id64_t)buf_[2] << 16) +
                   ((bm::id64_t)buf_[3] << 24) +
                   ((bm::id64_t)buf_[4] << 32) +
                   ((bm::id64_t)buf_[5] << 40) +
                   ((bm::id64_t)buf_[6] << 48) +
                   ((bm::id64_t)buf_[7] << 56);
#endif
    buf_ += sizeof(a);
    return a;
}


/*!
   \fn void decoder::get_32(bm::word_t* w, unsigned count)
   \brief Reads block of 32-bit words from the decoding buffer.
   \param w - pointer on memory block to read into.
   \param count - size of memory block in words.
*/
inline void decoder::get_32(bm::word_t* w, unsigned count) BMNOEXCEPT
{
    if (!w) 
    {
        seek(int(count * sizeof(bm::word_t)));
        return;
    }
#if (BM_UNALIGNED_ACCESS_OK == 1)
	::memcpy(w, buf_, count * sizeof(bm::word_t));
	seek(int(count * sizeof(bm::word_t)));
	return;
#else
    const unsigned char* buf = buf_;
    const bm::word_t* w_end = w + count;
    do 
    {
        bm::word_t a = buf[0]+ ((unsigned)buf[1] << 8) +
                   ((unsigned)buf[2] << 16) + ((unsigned)buf[3] << 24);
        *w++ = a;
        buf += sizeof(a);
    } while (w < w_end);
    buf_ = (unsigned char*)buf;
#endif
}
 
/*!
   \brief Reads block of 32-bit words from the decoding buffer and ORs
   to the destination
   \param w - pointer on memory block to read into
   \param count - should match bm::set_block_size
*/
inline
bool decoder::get_32_OR(bm::word_t* w, unsigned count) BMNOEXCEPT
{
    if (!w)
    {
        seek(int(count * sizeof(bm::word_t)));
        return false;
    }
#if defined(BMAVX2OPT)
        __m256i* buf_start = (__m256i*)buf_;
        seek(int(count * sizeof(bm::word_t)));
        __m256i* buf_end = (__m256i*)buf_;

        return bm::avx2_or_arr_unal((__m256i*)w, buf_start, buf_end);
#elif defined(BMSSE42OPT) || defined(BMSSE2OPT)
        __m128i* buf_start = (__m128i*)buf_;
        seek(int(count * sizeof(bm::word_t)));
        __m128i* buf_end = (__m128i*)buf_;

        return bm::sse2_or_arr_unal((__m128i*)w, buf_start, buf_end);
#else
        bm::word_t acc = 0;
        const bm::word_t not_acc = acc = ~acc;

        for (unsigned i = 0; i < count; i+=4)
        {
            acc &= (w[i+0] |= get_32());
            acc &= (w[i+1] |= get_32());
            acc &= (w[i+2] |= get_32());
            acc &= (w[i+3] |= get_32());
        }
        return acc == not_acc;
#endif
}

/*!
   \brief Reads block of 32-bit words from the decoding buffer and ANDs
   to the destination
   \param w - pointer on memory block to read into
   \param count - should match bm::set_block_size
*/
inline
void decoder::get_32_AND(bm::word_t* w, unsigned count) BMNOEXCEPT
{
    if (!w)
    {
        seek(int(count * sizeof(bm::word_t)));
        return;
    }
#if defined(BMAVX2OPT)
        __m256i* buf_start = (__m256i*)buf_;
        seek(int(count * sizeof(bm::word_t)));
        __m256i* buf_end = (__m256i*)buf_;

        bm::avx2_and_arr_unal((__m256i*)w, buf_start, buf_end);
#elif defined(BMSSE42OPT) || defined(BMSSE2OPT)
        __m128i* buf_start = (__m128i*)buf_;
        seek(int(count * sizeof(bm::word_t)));
        __m128i* buf_end = (__m128i*)buf_;

        bm::sse2_and_arr_unal((__m128i*)w, buf_start, buf_end);
#else
        for (unsigned i = 0; i < count; i+=4)
        {
            w[i+0] &= get_32();
            w[i+1] &= get_32();
            w[i+2] &= get_32();
            w[i+3] &= get_32();
        }
#endif
}



/*!
   \fn void decoder::get_16(bm::short_t* s, unsigned count)
   \brief Reads block of 32-bit words from the decoding buffer.
   \param s - pointer on memory block to read into.
   \param count - size of memory block in words.
*/
inline void decoder::get_16(bm::short_t* s, unsigned count) BMNOEXCEPT
{
    BM_ASSERT(count);
    if (!s) 
    {
        seek(int(count * sizeof(bm::short_t)));
        return;
    }
#if (BM_UNALIGNED_ACCESS_OK == 1)
    ::memcpy(s, buf_, sizeof(bm::short_t) * count);
    buf_ += sizeof(bm::short_t) * count;
#else
    const unsigned char* buf = buf_;
    const bm::short_t* s_end = s + count;
    do 
    {
        bm::short_t a = (bm::short_t)(buf[0] + ((bm::short_t)buf[1] << 8));
        *s++ = a;
        buf += sizeof(a);
    } while (s < s_end);
    buf_ = (unsigned char*)buf;
#endif
    
}



// ---------------------------------------------------------------------

inline decoder_little_endian::decoder_little_endian(const unsigned char* buf)
: decoder_base(buf)
{
}

inline
bm::short_t decoder_little_endian::get_16()
{
    bm::short_t v1 = bm::short_t(buf_[0]);
    bm::short_t v2 = bm::short_t(buf_[1]);
    bm::short_t a = bm::short_t((v1 << 8) + v2);
    buf_ += sizeof(a);
    return a;
}

inline bm::word_t decoder_little_endian::get_24()
{
    // TODO: validate if this is a correct for cross endian opts
    bm::word_t a = buf_[0] + ((unsigned)buf_[1] << 8) +
        ((unsigned)buf_[2] << 16);
    buf_ += 3;
    return a;
}

inline
bm::word_t decoder_little_endian::get_32()
{
    bm::word_t a = ((unsigned)buf_[0] << 24)+ ((unsigned)buf_[1] << 16) +
                   ((unsigned)buf_[2] << 8) + ((unsigned)buf_[3]);
    buf_+=sizeof(a);
    return a;
}

inline
bm::id64_t decoder_little_endian::get_48()
{
    bm::id64_t a = buf_[0] +
        ((bm::id64_t)buf_[1] << 8) +
        ((bm::id64_t)buf_[2] << 16) +
        ((bm::id64_t)buf_[3] << 24) +
        ((bm::id64_t)buf_[4] << 32) +
        ((bm::id64_t)buf_[5] << 40);
    buf_ += 6;
    return a;
}

inline
bm::id64_t decoder_little_endian::get_64()
{
    bm::id64_t a = buf_[0]+
                   ((bm::id64_t)buf_[1] << 56) +
                   ((bm::id64_t)buf_[2] << 48) +
                   ((bm::id64_t)buf_[3] << 40) +
                   ((bm::id64_t)buf_[4] << 32) +
                   ((bm::id64_t)buf_[5] << 24) +
                   ((bm::id64_t)buf_[6] << 16) +
                   ((bm::id64_t)buf_[7] << 8);
    buf_+=sizeof(a);
    return a;
}

inline
void decoder_little_endian::get_32(bm::word_t* w, unsigned count)
{
    if (!w) 
    {
        seek(int(count * sizeof(bm::word_t)));
        return;
    }

    const unsigned char* buf = buf_;
    const bm::word_t* w_end = w + count;
    do 
    {
        bm::word_t a = ((unsigned)buf[0] << 24)+ ((unsigned)buf[1] << 16) +
                       ((unsigned)buf[2] << 8) + ((unsigned)buf[3]);
        *w++ = a;
        buf += sizeof(a);
    } while (w < w_end);
    buf_ = (unsigned char*)buf;
}

inline
bool decoder_little_endian::get_32_OR(bm::word_t* w, unsigned count)
{
    if (!w)
    {
        seek(int(count * sizeof(bm::word_t)));
        return false;
    }

    bm::word_t acc = 0;
    const bm::word_t not_acc = acc = ~acc;

    for (unsigned i = 0; i < count; i+=4)
    {
        acc &= (w[i+0] |= get_32());
        acc &= (w[i+1] |= get_32());
        acc &= (w[i+2] |= get_32());
        acc &= (w[i+3] |= get_32());
    }
    return acc == not_acc;
}

inline
void decoder_little_endian::get_32_AND(bm::word_t* w, unsigned count)
{
    for (unsigned i = 0; i < count; i+=4)
    {
        w[i+0] &= get_32();
        w[i+1] &= get_32();
        w[i+2] &= get_32();
        w[i+3] &= get_32();
    }
}


inline
void decoder_little_endian::get_16(bm::short_t* s, unsigned count)
{
    if (!s) 
    {
        seek(int(count * sizeof(bm::short_t)));
        return;
    }

    const unsigned char* buf = buf_;
    const bm::short_t* s_end = s + count;
    do 
    {
        bm::short_t v1 = bm::short_t(buf_[0]);
        bm::short_t v2 = bm::short_t(buf_[1]);
        bm::short_t a = bm::short_t((v1 << 8) + v2);
        *s++ = a;
        buf += sizeof(a);
    } while (s < s_end);
    buf_ = (unsigned char*)buf;
}

// ----------------------------------------------------------------------
//

template<typename TEncoder>
void bit_out<TEncoder>::put_bit(unsigned value) BMNOEXCEPT
{
    BM_ASSERT(value <= 1);
    BM_ASSERT(used_bits_ < 32);
    accum_ |= (value << used_bits_);
    if (++used_bits_ == (sizeof(accum_) * 8))
        flush_accum();
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::put_bits(unsigned value, unsigned count) BMNOEXCEPT
{
    unsigned used = used_bits_;
    unsigned acc = accum_;
    if (used == (sizeof(accum_) * 8))
    {
        dest_.put_32(acc);
        used = acc = 0;
    }
    {
        unsigned mask = ~0u;
        mask >>= (sizeof(accum_) * 8) - count;
        value &= mask;
    }
    for (;count;)
    {
        unsigned free_bits = unsigned(sizeof(accum_) * 8) - used;
        BM_ASSERT(free_bits);
        acc |= value << used;

        if (count <= free_bits)
        {
            used += count;
            break;
        }
        else
        {
            value >>= free_bits;
            count -= free_bits;
            dest_.put_32(acc);
            acc = used = 0;
            continue;
        }
    }
    used_bits_ = used;
    accum_ = acc;
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::put_zero_bit() BMNOEXCEPT
{
    if (++used_bits_ == (sizeof(accum_) * 8))
        flush_accum();
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::put_zero_bits(unsigned count) BMNOEXCEPT
{
    unsigned used = used_bits_;
    unsigned free_bits = (sizeof(accum_) * 8) - used;
    if (count >= free_bits)
    {
        flush_accum();
        count -= free_bits;
        used = 0;

        for ( ;count >= sizeof(accum_) * 8; count -= sizeof(accum_) * 8)
        {
            dest_.put_32(0);
        }
        used += count;
    }
    else
    {
        used += count;
    }
    accum_ |= (1u << used);
    if (++used == (sizeof(accum_) * 8))
        flush_accum();
    else
        used_bits_ = used;
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::put_16_no(unsigned value) BMNOEXCEPT
{
    BM_ASSERT(value < 65536);
    unsigned lo = (unsigned char) value;
    unsigned hi = (unsigned char) (value >> 8);
    put_bits(lo, 8);
    put_bits(hi, 8);
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::put_24_no(unsigned w) BMNOEXCEPT
{
    BM_ASSERT(w < 0xFFFFFF);

    unsigned v = (unsigned char)w;
    put_bits(v, 8);
    v = (unsigned char)(w >> 8);
    put_bits(v, 8);
    v = (unsigned char)(w >> 16);
    put_bits(v, 8);
}


// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::put_32_no(unsigned value) BMNOEXCEPT
{
    unsigned lo = (unsigned char) value;
    unsigned hi = (unsigned char) (value >> 8);
    put_bits(lo, 8);
    put_bits(hi, 8);
    lo = (unsigned char) (value >> 16);
    hi = (unsigned char) (value >> 24);
    put_bits(lo, 8);
    put_bits(hi, 8);
 }

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::put_64_no(unsigned long long value) BMNOEXCEPT
{
    unsigned lo = (unsigned) value;
    unsigned hi = (unsigned) (value >> 32);
    put_32_no(lo);
    put_32_no(hi);
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::gamma8(unsigned value) BMNOEXCEPT
{
    BM_ASSERT(value < 65536);
    if (!value)
    {
        gamma(4);
        return;
    }
    if (value > 0)
    {
        if (value < 16)
        {
            gamma(1);
            gamma(value);
            return;
        }
        if (value < 256)
        {
            gamma(2);
            put_bits(value, 8);
            return;
        }
    }
    gamma(3);
    delta16(value);
    return;
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::delta16(unsigned value) BMNOEXCEPT
{
    BM_ASSERT(value > 255 && value < 65536);
    if (value <= 511)
    {
        gamma(1);
        unsigned delta = 511-value;
        BM_ASSERT(delta < 256);
        put_bits(delta, 8);
    }
    else
    if (value <= 512 + 255)
    {
        gamma(2);
        unsigned delta = 512+255-value;
        BM_ASSERT(delta < 256);
        put_bits(delta, 8);
    }
    else
    if (value <= 512 + 256 + 255)
    {
        gamma(3);
        unsigned delta = 512 + 256 + 255-value;
        BM_ASSERT(delta < 256);
        put_bits(delta, 8);
    }
    else
    {
        gamma(4);
        put_16_no(value);
    }
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::delta16s(unsigned value) BMNOEXCEPT
{
    BM_ASSERT(value < 65536);

    if (value < 256)
    {
        put_bits(0, 1); // code = 0
        put_bits(value, 8);
        return;
    }
    put_bits(1, 1); // code = 1
    delta16(value);
}


// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::gamma(unsigned value) BMNOEXCEPT
{
    BM_ASSERT(value);

    unsigned logv = bm::bit_scan_reverse32(value);

    // Put zeroes + 1 bit

    unsigned used = used_bits_;
    unsigned acc = accum_;
    const unsigned acc_bits = (sizeof(acc) * 8);
    unsigned free_bits = acc_bits - used;

    {
        unsigned count = logv;
        if (count >= free_bits)
        {
            dest_.put_32(acc);
            acc = used ^= used;
            count -= free_bits;

            for ( ;count >= acc_bits; count -= acc_bits)
            {
                dest_.put_32(0);
            }
            used += count;
        }
        else
        {
            used += count;
        }
        acc |= (1 << used);
        if (++used == acc_bits)
        {
            dest_.put_32(acc);
            acc = used ^= used;
        }
    }

    // Put the value bits
    //
    {
        unsigned mask = (~0u);
        mask >>= acc_bits - logv;
        value &= mask;
    }
    for (;logv;)
    {
        acc |= value << used;
        free_bits = acc_bits - used;
        if (logv <= free_bits)
        {
            used += logv;
            break;
        }
        else
        {
            value >>= free_bits;
            logv -= free_bits;
            dest_.put_32(acc);
            acc = used ^= used;
            continue;
        }
    } // for

    if (used ==  32)
    {
        dest_.put_32(acc);
        acc = used ^= used;
    }
    used_bits_ = used;
    accum_ = acc;
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::bic_encode_u16_rg(
                                const bm::gap_word_t* arr,
                                unsigned sz,
                                bm::gap_word_t lo, bm::gap_word_t hi) BMNOEXCEPT
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        unsigned mid_idx = sz >> 1;
        bm::gap_word_t val = arr[mid_idx];
        
        // write the interpolated value
        // write(x, r) where x=(arr[mid] - lo - mid) r=(hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned value = val - lo - mid_idx;
                unsigned logv = bm::bit_scan_reverse32(r);
                put_bits(value, logv+1);
            }
        }
        
        bic_encode_u16_rg(arr, mid_idx, lo, gap_word_t(val-1));
        // tail recursion
        //   bic_encode_u16(arr + mid_idx + 1, sz - mid_idx - 1, gap_word_t(val+1), hi);
        arr += mid_idx + 1;
        sz  -= mid_idx + 1;
        lo = gap_word_t(val + 1);
    } // for sz
}

// ----------------------------------------------------------------------

template<typename TEncoder>
void bit_out<TEncoder>::bic_encode_u32_cm(const bm::word_t* arr,
                                          unsigned sz,
                                          bm::word_t lo,
                                          bm::word_t hi) BMNOEXCEPT
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        unsigned mid_idx = sz >> 1;
        bm::word_t val = arr[mid_idx];
        
        // write the interpolated value
        // write(x, r) where x=(arr[mid] - lo - mid) r=(hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned value = val - lo - mid_idx;
                
                unsigned n = r + 1;
                unsigned logv = bm::bit_scan_reverse32(n);
                unsigned c = (unsigned)(1ull << (logv + 1)) - n;
                int64_t half_c = c >> 1; // c / 2;
                int64_t half_r = r >> 1; // r / 2;
                int64_t lo1 = half_r - half_c;
                int64_t hi1 = half_r + half_c + 1;
                lo1 -= (n & 1);
                logv += (value <= lo1 || value >= hi1);
                
                put_bits(value, logv);
            }
        }
        
        bic_encode_u32_cm(arr, mid_idx, lo, val-1);
        // tail recursive call:
        // bic_encode_u32_cm(arr + mid_idx + 1, sz - mid_idx - 1, val+1, hi);
        arr += mid_idx + 1;
        sz  -= mid_idx + 1;
        lo = val + 1;
    } // for sz
}

// ----------------------------------------------------------------------

#if 0
/**
    Shuffle structure for BIC
    @internal
*/
template<unsigned N>
struct bic_encode_stack_u16
{
    bm::gap_word_t  val_[N];
    bm::gap_word_t  mid_[N];
    bm::gap_word_t  lo_[N];
    bm::gap_word_t  r_[N];

    unsigned stack_size_ = 0;
    
    void bic_shuffle(const bm::gap_word_t* arr,
                     bm::gap_word_t sz, bm::gap_word_t lo, bm::gap_word_t hi)
    {
        for (;sz;)
        {
            BM_ASSERT(lo <= hi);
            bm::gap_word_t mid_idx = sz >> 1;
            bm::gap_word_t val = arr[mid_idx];
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned s = stack_size_++;
                r_[s] = (bm::gap_word_t)r;
                val_[s] = val;
                mid_[s] = mid_idx;
                lo_[s] = lo;
            }
            
            bic_shuffle(arr, mid_idx, lo, bm::gap_word_t(val-1));
            // tail recursive call:
            // bic_shuffle(arr + mid_idx + 1, sz - mid_idx - 1, val+1, hi);
            arr += mid_idx + 1;
            sz  -= mid_idx + 1;
            lo = bm::gap_word_t(val + 1);
        } // for sz
    }
};

template<typename TEncoder>
void bit_out<TEncoder>::bic_encode_u16_cm(const bm::gap_word_t* arr,
                                          unsigned sz_i,
                                          bm::gap_word_t lo_i,
                                          bm::gap_word_t hi_i) BMNOEXCEPT
{
    BM_ASSERT(sz_i <= 65535);

    bic_encode_stack_u16<bm::bie_cut_off> u16_stack;
    // BIC re-ordering
    u16_stack.bic_shuffle(arr, bm::gap_word_t(sz_i), lo_i, hi_i);
    
    BM_ASSERT(sz_i == u16_stack.stack_size_);
    for (unsigned i = 0; i < sz_i; ++i)
    {
        bm::gap_word_t val = u16_stack.val_[i];
        bm::gap_word_t mid_idx = u16_stack.mid_[i];
        bm::gap_word_t lo = u16_stack.lo_[i];
        unsigned r = u16_stack.r_[i];

        unsigned value = val - lo - mid_idx;
        unsigned n = r + 1;
        unsigned logv = bm::bit_scan_reverse32(n);
        unsigned c = (unsigned)(1ull << (logv + 1)) - n;
        
        int64_t half_c = c >> 1; // c / 2;
        int64_t half_r = r >> 1; // r / 2;
        int64_t lo1 = half_r - half_c;
        int64_t hi1 = half_r + half_c + 1;
        lo1 -= (n & 1);
        logv += (value <= lo1 || value >= hi1);

        put_bits(value, logv);
    } // for i
}
#endif


template<typename TEncoder>
void bit_out<TEncoder>::bic_encode_u16_cm(const bm::gap_word_t* arr,
                                          unsigned sz,
                                          bm::gap_word_t lo,
                                          bm::gap_word_t hi) BMNOEXCEPT
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        unsigned mid_idx = sz >> 1;
        bm::gap_word_t val = arr[mid_idx];

        // write the interpolated value
        // write(x, r) where x=(arr[mid] - lo - mid) r=(hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned value = val - lo - mid_idx;
                
                unsigned n = r + 1;
                unsigned logv = bm::bit_scan_reverse32(n);
                unsigned c = (unsigned)(1ull << (logv + 1)) - n;
                unsigned half_c = c >> 1; // c / 2;
                unsigned half_r = r >> 1; // r / 2;
                int64_t  lo1 = (int64_t(half_r) - half_c - (n & 1u));
                unsigned hi1 = (half_r + half_c);
                logv += (value <= lo1 || value > hi1);

                put_bits(value, logv);
            }
        }
        
        bic_encode_u16_cm(arr, mid_idx, lo, bm::gap_word_t(val-1));
        // tail recursive call:
        // bic_encode_u32_cm(arr + mid_idx + 1, sz - mid_idx - 1, val+1, hi);
        arr += ++mid_idx;
        sz  -= mid_idx;
        lo = bm::gap_word_t(val + 1);
    } // for sz
}

// ----------------------------------------------------------------------


// Array encoding(masks and bit-packed values)
//
// encoding types:
// 00 - delta-gamma code
// 01 - BIC with DR (0..65535)
// 10 - gamma code
// 11 - 0|1|2 value(s)
const unsigned char h3f_ex_upper2 = 1 | (1 << 1); // lower two bits mask for codec

// length types
// 00 - 8 bit < 256
// 01 - 8 bit < 511
// 10 - 16-bit value
// 11 - reserved (gamma length)

const unsigned char h3f_ex_arr_1 = (1 << 4); /// array of 1s (otherwise 0s)
const unsigned char h3f_use_gamma = (1 << 3); /// use gamma code for size
const unsigned char h3f_ex_arr_ex_EOC = (1 << 5); /// End-of-Chain
//
const unsigned char h3f_ex_arr_min0_0 = (1 << 6); /// min0 is zero somehow
const unsigned char h3f_ex_arr_arr0_0 = (1 << 6); /// arr[0] is zero
//
const unsigned char h3f_ex_minmax_v = (1 << 7); /// min-max range used to encode
const unsigned char h3f_ex_noop = (1 << 7); /// nothing encoded, no-operation
const unsigned char h3f_ex_gamma_zero_correct = (1 << 7); /// zero correction

const unsigned wcnt_cutoff = 15;


template<typename TEncoder>
void bit_out<TEncoder>::encode_array(const bm::gap_word_t* arr,
                                     bm::gap_word_t* recalc_arr,
                                     bm::word_t* tb_wflags,
                                     bm::gap_word_t* tmp_arr,
                                     unsigned sz,
                                     bool one_flag,
                                     bool EOC_flag,
                                     unsigned force_code,
                                     bool save_size) BMNOEXCEPT
{
#define BM_ENCODE_SIZE(b, s)  if (b) \
{ if (h3_flag & bm::h3f_use_gamma) this->gamma8(s-1); else this->delta16(s); }

    BM_ASSERT(sz < 65536);
    BM_ASSERT(arr && arr != recalc_arr);

    bm::gap_word_t min0;

    unsigned char h3_flag = 0;
    if (one_flag)
        h3_flag |= bm::h3f_ex_arr_1;
    if (EOC_flag)
        h3_flag |= bm::h3f_ex_arr_ex_EOC;

    if (sz == 0)
    {
        h3_flag |= bm::h3f_ex_upper2; // same flag as single value
        h3_flag |= bm::h3f_ex_noop; // (1<<7) uses same flag as h3f_ex_minmax_v
        this->put_bits(h3_flag, 8);
        return;
    }
    if (sz == 1) // single value encoding
    {
        h3_flag |= bm::h3f_ex_upper2; // same flag as single value
        if (arr[0] == 0)
        {
            h3_flag |= bm::h3f_ex_arr_min0_0; // (1<<6)
            this->put_bits(h3_flag, 8);
            return;
        }
        if (arr[0] < 16)
            h3_flag |= bm::h3f_use_gamma;
        this->put_bits(h3_flag, 8);
        if (h3_flag & bm::h3f_use_gamma)
            this->gamma(arr[0]);
        else
            this->put_16_no(arr[0]);
        return;
    } // single value encoding

    auto gamma_size = bm::gamma_size(sz-1);
    if (gamma_size < 16)
        h3_flag |= bm::h3f_use_gamma;

    // -----------------------------------------------------
    switch (force_code)
    {
    case 1: // gamma encoding
    {
        bool zero_correct = 0;
        min0 = 65535;
        for (unsigned i = 0; i < sz && min0; ++i)
        {
            if (arr[i] < min0)
                min0 = arr[i];
        } // for i

        h3_flag |= (1u << 1); // use gamma code
        if (!min0)
        {
            h3_flag |= bm::h3f_ex_gamma_zero_correct;
            zero_correct = 1;
        }
        if (min0)
            --min0;
        if (!min0)
            h3_flag |= bm::h3f_ex_arr_min0_0;

        this->put_bits(h3_flag, 8);
        BM_ENCODE_SIZE(save_size, sz)
        if (min0)
            this->gamma(min0);
        for (unsigned i = 0; i < sz; ++i)
        {
            auto v = arr[i];
            v -= min0;
            v += zero_correct;
            this->gamma(v);
        } // for i
        return;
    } // if force_code == 1 (gamma)
    case 2: // delta-gamma
    {
        bool zero_correct = (arr[0] == 0);
        min0 = 65535;
        for (unsigned i = 1; i < sz && min0; ++i)
        {
            BM_ASSERT(arr[i-1] < arr[i]);
            unsigned delta = arr[i] - arr[i-1];
            if (delta < min0)
                min0 = (bm::gap_word_t) delta;
        } // for i
        BM_ASSERT(min0 < 65535);
        if (zero_correct)
            h3_flag |= bm::h3f_ex_gamma_zero_correct;
        if (min0)
            --min0;
        if (!min0)
            h3_flag |= bm::h3f_ex_arr_min0_0;
        this->put_bits(h3_flag, 8);
        BM_ENCODE_SIZE(save_size, sz)
        if (min0)
            this->gamma(min0);
        if (zero_correct)
        {} // starts with 0, nothing to encode
        else
            this->gamma(arr[0]); // encode first element
        for (unsigned i = 1; i < sz; ++i)
        {
            unsigned delta = arr[i] - arr[i-1];
            BM_ASSERT(delta > min0);
            delta -= min0;
            this->gamma(delta);
        } // for i
        return;
    } // if force_code == 2 (delta-gamma)
    default: break;
    } // switch

    // -----------------------------------------------------
    // calculate DR compression parameter
    //
    if (sz <= 2)
        min0 = 0;
    else
    {
        bool r = bm::arr_calc_delta_min(arr, sz, min0);
        if (r)
        {
            if (min0 < 65535)
            {
                BM_ASSERT(min0 > 0); // impossible because min0==0 should be !r
            }
            else
                min0 = 0;
        }
        else // !r (no feasible delta found)
        {
            BM_ASSERT(min0==0);
        }
    }

    // evaluate feasibility for window-DR
    //
    unsigned win_size = 0; float best_save = 0.0f;
    bool use_wdr = bm::compute_wdr_params(arr, sz, tb_wflags, min0, win_size, best_save);

    unsigned max_wd = 0;
    if (best_save < 10.0f) // 16.0f
        use_wdr = false;
    if (use_wdr)
        max_wd = (sz / win_size) + 1;

    if (min0)
        --min0;
    unsigned wcnt = 0;

    // recalculate min using wdr or global min0
    if (use_wdr)
    {
        // recalculate again to restore flags
        bm::bit_block_set(tb_wflags, 0);

        wcnt = bm::arr_calc_delta_min_w(arr, sz, win_size, min0, tb_wflags);
        BM_ASSERT(wcnt);
        BM_ASSERT(wcnt == bm::bit_block_count(tb_wflags));

        auto delta_acc =
        bm::arr_recalc_min_w(recalc_arr, arr, sz, win_size, min0, tb_wflags);
        BM_ASSERT((tb_wflags[0] & 1) == 0);
        if (delta_acc < 10 || wcnt < wcnt_cutoff) // (300) if DR compression efficient?
        {
            use_wdr = false;
            goto use_min0; // quick fallback to old method
        }
    }
    else // non WDR range compression
    {
        if (min0)
        {
        use_min0:
            auto delta_acc = bm::arr_recalc_min(recalc_arr, arr, sz, min0);
            if (delta_acc < 10)
            {
                min0 = 0;
                recalc_arr = const_cast<bm::gap_word_t*>(arr);
            }
        }
        else
        {
            recalc_arr = const_cast<bm::gap_word_t*>(arr);
        }
    }
    if (!min0)
        h3_flag |= bm::h3f_ex_arr_min0_0;

    h3_flag |= 1; // BIC-DR
    bm::gap_word_t min_v = recalc_arr[0];
    bm::gap_word_t max_v = recalc_arr[sz-1];
//    bm::gap_word_t max_v_delta = 65536 - max_v;
    BM_ASSERT(min_v < max_v);

    unsigned range_reduct = min_v + (65536 - max_v);
    if ((sz > 10) && (range_reduct > (128+512)) /*&& (min_v >= 256) && (max_v_delta >= 256)*/)
        h3_flag |= bm::h3f_ex_minmax_v;

    this->put_bits(h3_flag, 8);
    BM_ENCODE_SIZE(save_size, sz)
    if (!(h3_flag & bm::h3f_ex_arr_min0_0))
        this->gamma(min0);

    if (h3_flag & bm::h3f_ex_minmax_v)
    {
        this->put_16_no(min_v); // delta16 is not efficient (for this use now)
        this->put_16_no(max_v);

        recalc_arr = &recalc_arr[1];
        sz-=2;
        if (!sz)
        {
            BM_ASSERT(!use_wdr);
            this->flush_if_full();
            return;
        }
        ++min_v; --max_v;
    }
    else // use default range
    {
        min_v = 0; max_v = 65535; // BIC dflt range [0..65535]
    }

    this->flush_if_full();
    this->bic_encode_u16(recalc_arr, sz, min_v, max_v);
    this->flush_if_full();

    this->put_bit(use_wdr);
    if (use_wdr)
    {
        BM_ASSERT(wcnt && win_size);
        BM_ASSERT((wcnt * win_size) <= (sz + win_size-1));
        BM_ASSERT(max_wd);
        {
            unsigned arr_len =
                bm::bit_block_convert_to_arr(tmp_arr, tb_wflags, false);
            BM_ASSERT(arr_len == wcnt);
            BM_ASSERT(max_wd >= tmp_arr[arr_len-1]);

            BM_ASSERT(win_size/2 > 9);
            this->gamma((win_size/2) - 9);
            this->gamma(wcnt - (wcnt_cutoff - 1));
            this->flush_if_full();

            // Note: saving min_v (1) does not improve compression here
            this->bic_encode_u16(tmp_arr, arr_len, 1, (bm::gap_word_t)max_wd);
        }
    }
    this->flush_if_full();

#undef BM_ENCODE_SIZE
}




// ----------------------------------------------------------------------
//
// ----------------------------------------------------------------------


template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_rg(bm::gap_word_t* arr, unsigned sz,
                                         bm::gap_word_t lo,
                                         bm::gap_word_t hi) BMNOEXCEPT
{
    BM_ASSERT(sz);
    do // for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        // read the value
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r) + 1;
                val = get_bits(logv);
                BM_ASSERT(val <= r);
            }
            else
            {
                val = 0;
            }
        }
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        
        BM_ASSERT(val < 65536);
        BM_ASSERT(mid_idx < 65536);
        
        arr[mid_idx] = bm::gap_word_t(val);
        if (sz <= 1)
            return;

        bic_decode_u16_rg(arr, mid_idx, lo, bm::gap_word_t(val - 1));
        arr += mid_idx + 1;
        sz  -= mid_idx + 1;
        lo = bm::gap_word_t(val + 1);
    } while (sz); // for sz
}

// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u32_cm(bm::word_t* arr, unsigned sz,
                                         bm::word_t lo,
                                         bm::word_t hi) BMNOEXCEPT
{
    BM_ASSERT(sz);
    do
    {
        BM_ASSERT(lo <= hi);
        // read the interpolated value
        // x = read(r)+ lo + mid,  where r = (hi - lo - sz + 1);
        unsigned val = hi - lo - sz + 1;
        if (val)
        {
            unsigned logv = bm::bit_scan_reverse32(val+1);
                
            unsigned c = unsigned((1ull << (logv + 1)) - val - 1);
            int64_t half_c = c >> 1; 
            int64_t half_r = val >> 1; 
            int64_t lo1 = half_r - half_c - ((val + 1) & 1);
            int64_t hi1 = half_r + half_c + 1;

            val = get_bits(logv);
            if (val <= lo1 || val >= hi1)
                val += (get_bit() << logv);
        }
        
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        arr[mid_idx] = val;

        if (sz <= 1)
            return;
        
        bic_decode_u32_cm(arr, mid_idx, lo, val-1);
        // tail recursive call:
        //  bic_decode_u32_cm(arr + mid_idx + 1, sz - mid_idx - 1, val + 1, hi);
        arr += ++mid_idx;
        sz -= mid_idx;
        lo = val + 1;
    } while (sz);// for sz
}

// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_cm(bm::gap_word_t* arr, unsigned sz,
                                         bm::gap_word_t lo,
                                         bm::gap_word_t hi) BMNOEXCEPT
{
    BM_ASSERT(sz);
    do
    {
        BM_ASSERT(lo <= hi);
        // read the interpolated value
        // x = read(r)+ lo + mid,  where r = (hi - lo - sz + 1);
        unsigned val = hi - lo - sz + 1;
        if (val)
        {
            unsigned logv = bm::bit_scan_reverse32(val+1);

            unsigned c = unsigned((1ull << (logv + 1)) - val - 1);
            int64_t half_c = c >> 1; // c / 2;
            int64_t half_r = val >> 1; // r / 2;
            int64_t lo1 = half_r - half_c - ((val + 1) & 1);
            int64_t hi1 = half_r + half_c + 1;
            val = get_bits(logv);
            if (val <= lo1 || val >= hi1)
                val += (get_bit() << logv);
        }
        
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        arr[mid_idx] = bm::gap_word_t(val);

        if (sz <= 1)
            return;
        
        bic_decode_u16_cm(arr, mid_idx, lo, bm::gap_word_t(val-1));
        // tail recursive call:
        //  bic_decode_u16_cm(arr + mid_idx + 1, sz - mid_idx - 1, val + 1, hi);
        arr += ++mid_idx;
        sz -= mid_idx;
        lo = bm::gap_word_t(val + 1);
    } while (sz);// for sz
}

// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_cm_bitset(bm::word_t* block, unsigned sz,
                              bm::gap_word_t lo,
                              bm::gap_word_t hi) BMNOEXCEPT
{
    BM_ASSERT(sz);
    do
    {
        BM_ASSERT(lo <= hi);
        // read the interpolated value
        // x = read(r)+ lo + mid,  where r = (hi - lo - sz + 1);
        unsigned val = hi - lo - sz + 1;
        if (val)
        {
            unsigned logv = bm::bit_scan_reverse32(val+1);

            unsigned c = unsigned((1ull << (logv + 1)) - val - 1);
            int64_t half_c = c >> 1; // c / 2;
            int64_t half_r = val >> 1; // r / 2;
            int64_t lo1 = half_r - half_c - ((val + 1) & 1);
            int64_t hi1 = half_r + half_c + 1;

            val = get_bits(logv);
            if (val <= lo1 || val >= hi1)
                val += (get_bit() << logv);
        }

        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        
        // set bit in the target block
        {
            unsigned nword = (val >> bm::set_word_shift);
            block[nword] |= (1u << (val & bm::set_word_mask));
        }
        
        if (sz <= 1)
            return;

        bic_decode_u16_cm_bitset(block, mid_idx, lo, bm::gap_word_t(val-1));
        // tail recursive call:
        //  bic_decode_u32_cm(block, sz - mid_idx - 1, val + 1, hi);
        sz -= ++mid_idx;
        lo = bm::gap_word_t(val + 1);
    } while (sz);
}

// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_cm_dry(unsigned sz,
                              bm::gap_word_t lo,
                              bm::gap_word_t hi) BMNOEXCEPT
{
    BM_ASSERT(sz);
    do
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        
        // read the interpolated value
        // x = read(r)+ lo + mid,  where r = (hi - lo - sz + 1);
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r+1);
                
                unsigned c = unsigned((1ull << (logv + 1)) - r - 1);
                int64_t half_c = c >> 1; // c / 2;
                int64_t half_r = r >> 1; // r / 2;
                int64_t lo1 = half_r - half_c - ((r + 1) & 1);
                int64_t hi1 = half_r + half_c + 1;
                r = get_bits(logv);
                if (r <= lo1 || r >= hi1)
                    r += (get_bit() << logv);
            }
            val = r;
        }
        
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        
        if (sz <= 1)
            return;
        
        bic_decode_u16_cm_dry(mid_idx, lo, bm::gap_word_t(val-1));
        // tail recursive call:
        //  bic_decode_u32_cm_dry(sz - mid_idx - 1, val + 1, hi);
        sz -= mid_idx + 1;
        lo = bm::gap_word_t(val + 1);
    } while (sz); // for sz
}


// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_rg_bitset(bm::word_t* block, unsigned sz,
                                                bm::gap_word_t lo,
                                                bm::gap_word_t hi) BMNOEXCEPT
{
    BM_ASSERT(sz);
    do //for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        // read the value
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r) + 1;
                val = get_bits(logv);
                BM_ASSERT(val <= r);
            }
            else
            {
                val = 0;
            }
        }
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        BM_ASSERT(val < 65536);
        BM_ASSERT(mid_idx < 65536);

        // set bit in the target block
        {
            unsigned nword = (val >> bm::set_word_shift);
            block[nword] |= (1u << (val & bm::set_word_mask));
        }
        
        if (sz == 1)
            return;
        bic_decode_u16_rg_bitset(block, mid_idx, lo, bm::gap_word_t(val - 1));
        // tail recursion of:
        //bic_decode_u16_bitset(block, sz - mid_idx - 1, bm::gap_word_t(val + 1), hi);
        sz  -= mid_idx + 1;
        lo = bm::gap_word_t(val + 1);
    } while(sz); // for sz
}

// ----------------------------------------------------------------------

template<class TDecoder>
void bit_in<TDecoder>::bic_decode_u16_rg_dry(unsigned sz,
                                   bm::gap_word_t lo,
                                   bm::gap_word_t hi) BMNOEXCEPT
{
    for (;sz;)
    {
        BM_ASSERT(lo <= hi);
        
        unsigned val;
        // read the value
        {
            unsigned r = hi - lo - sz + 1;
            if (r)
            {
                unsigned logv = bm::bit_scan_reverse32(r) + 1;
                val = get_bits(logv);
                BM_ASSERT(val <= r);
            }
            else
            {
                val = 0;
            }
        }
        unsigned mid_idx = sz >> 1;
        val += lo + mid_idx;
        BM_ASSERT(val < 65536);
        BM_ASSERT(mid_idx < 65536);

        if (sz == 1)
            return;
        bic_decode_u16_rg_dry(mid_idx, lo, bm::gap_word_t(val - 1));
        sz  -= mid_idx + 1;
        lo = bm::gap_word_t(val + 1);
    } // for sz
}
// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::gamma8() BMNOEXCEPT
{
    auto c = gamma();
    switch (c)
    {
    case 1:
        return gamma();
    case 2:
        return get_bits(8);
    case 3:
        return delta16(); //get_16_no();
    case 4:
        return 0;
    default:
        BM_ASSERT(0);
    }
    return 0;
}

// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::delta16() BMNOEXCEPT
{
    auto order = gamma();
    unsigned delta;
    switch (order)
    {
    case 1:
        delta = get_bits(8);
        return 511 - delta;
    case 2:
        delta = get_bits(8);
        return 512 + 255 - delta;
    case 3:
        delta = get_bits(8);
        return 512 + 256 + 255 - delta;
    default:
        break;
    }
    BM_ASSERT(order == 4);
    return get_16_no();
}

// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::delta16s() BMNOEXCEPT
{
    auto c = get_bit();
    switch (c)
    {
    case 1:
        return delta16();
    case 0:
        return get_bits(8);
    default:
        BM_ASSERT(0);
    }
    return 0;
}

// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::gamma() BMNOEXCEPT
{
    unsigned acc = accum_;
    unsigned used = used_bits_;

    if (used == (sizeof(acc) * 8))
    {
        acc = src_.get_32();
        used ^= used;
    }
    unsigned zero_bits = 0;
    while (true)
    {
        if (acc == 0)
        {
            zero_bits = unsigned(zero_bits +(sizeof(acc) * 8) - used);
            used = 0;
            acc = src_.get_32();
            continue;
        }
        unsigned first_bit_idx =
            #if defined(BM_x86) && (defined(__GNUG__) || defined(_MSC_VER)) && !(defined(__arm64__) || defined(__arm__))
                bm::bsf_asm32(acc);
            #else
                bm::bit_scan_fwd(acc);
            #endif
        acc >>= first_bit_idx;
        zero_bits += first_bit_idx;
        used += first_bit_idx;
        break;
    } // while

    // eat the border bit
    //
    if (used == (sizeof(acc) * 8))
    {
        acc = src_.get_32();
        used = 1;
    }
    else
    {
        ++used;
    }
    acc >>= 1;

    // get the value
    unsigned current;
    
    unsigned free_bits = unsigned((sizeof(acc) * 8) - used);
    if (zero_bits <= free_bits)
    {
    take_accum:
        current =
            (acc & block_set_table<true>::_left[zero_bits]) | (1 << zero_bits);
        acc >>= zero_bits;
        used += zero_bits;
        goto ret;
    }

    if (used == (sizeof(acc) * 8))
    {
        acc = src_.get_32();
        used ^= used;
        goto take_accum;
    }

    // take the part
    current = acc;
    // read the next word
    acc = src_.get_32();
    used = zero_bits - free_bits;
    current |=
        ((acc & block_set_table<true>::_left[used]) << free_bits) |
        (1 << zero_bits);

    acc >>= used;
ret:
    accum_ = acc;
    used_bits_ = used;
    return current;
}

// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::get_16_no() BMNOEXCEPT
{
    unsigned lo = get_bits(8);
    unsigned hi = get_bits(8);
    unsigned v = lo | (hi << 8);
    return v;
}

// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::get_24_no() BMNOEXCEPT
{
    unsigned v0 = get_bits(8);
    unsigned v1 = get_bits(8);
    unsigned v2 = get_bits(8);
    unsigned w = v0 + (v1 << 8) + (v2 << 16);
    return w;
}

// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::get_32_no() BMNOEXCEPT
{
    unsigned lo = get_bits(8);
    unsigned hi = get_bits(8);
    unsigned v = lo | (hi << 8);
    lo = get_bits(8);
    hi = get_bits(8);
    v |= (lo << 16) | (hi << 24);
    return v;
}

// ----------------------------------------------------------------------

template<class TDecoder>
unsigned long long bit_in<TDecoder>::get_64_no() BMNOEXCEPT
{
    unsigned long long lo = get_32_no();
    unsigned long long hi = get_32_no();
    return (hi << 32) | lo;
}

// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::get_bits(unsigned count) BMNOEXCEPT
{
    BM_ASSERT(count);
    const unsigned maskFF = ~0u;
    unsigned acc = accum_;
    unsigned used = used_bits_;

    unsigned value = 0;
    unsigned free_bits = unsigned((sizeof(acc) * 8) - used);
    if (count <= free_bits)
    {
    take_accum:
        value = acc & (maskFF >> (32 - count));
        acc >>= count;
        used += count;
        goto ret;
    }
    if (used == (sizeof(acc) * 8))
    {
        acc = src_.get_32();
        used = 0;
        goto take_accum;
    }
    value = acc;
    acc = src_.get_32();
    used = count - free_bits;
    value |= ((acc & (maskFF >> (32 - used))) << free_bits);
    acc >>= used;
ret:
    accum_ = acc;
    used_bits_ = used;
    return value;
}

// ----------------------------------------------------------------------

template<class TDecoder>
unsigned bit_in<TDecoder>::get_bit() BMNOEXCEPT
{
    const unsigned mask = (~0u) >> (32 - 1); // 100000...
    unsigned value = accum_ & mask;
    if (unsigned free_bits = unsigned(32u - used_bits_); free_bits)
    {
        accum_ >>= 1; ++used_bits_;
    }
    else
    {
        BM_ASSERT(used_bits_ == (sizeof(accum_) * 8));
        unsigned a = src_.get_32();
        value = a & mask;
        used_bits_ = 1;
        accum_ = (a >> 1);
    }
    return value;
}

// ----------------------------------------------------------------------


template<class TDecoder>
unsigned bit_in<TDecoder>::decode_array(bm::gap_word_t* arr,
                                        bm::word_t*     tb_wflags,
                                        unsigned*       sz,
                                        unsigned        default_sz) BMNOEXCEPT
{
    BM_ASSERT(sz && arr);
    unsigned h3_flag = this->get_bits(8);

    if (((h3_flag & bm::h3f_ex_upper2)== bm::h3f_ex_upper2) &&
        (h3_flag & bm::h3f_ex_noop)) // 0 len (1 << 7)
    {
        *sz = 0;
    }
    else
    if ((h3_flag & bm::h3f_ex_upper2) == bm::h3f_ex_upper2) // single value
    {
        *sz = 1;
        arr[0] = (h3_flag & bm::h3f_ex_arr_min0_0) ? // (1<<6)
            0 :
            (h3_flag & bm::h3f_use_gamma) ?
                  (bm::gap_word_t) this->gamma()
                : (bm::gap_word_t) this->get_16_no();
    }
    else // BIC-DR or gamma
    {
        *sz = default_sz ? default_sz :
                (h3_flag & bm::h3f_use_gamma) ? (this->gamma8()+1)
                                              : this->delta16();
        bm::gap_word_t min0 =
                (h3_flag & bm::h3f_ex_arr_min0_0) ?
                0 :
                (bm::gap_word_t) this->gamma();

        if ((h3_flag & bm::h3f_ex_upper2) == 0) // delta-gamma
        {
            bool zero_correct = (h3_flag & bm::h3f_ex_gamma_zero_correct);
            if (zero_correct)
                arr[0] = 0;
            else
                arr[0] = (bm::gap_word_t) this->gamma();
            for (unsigned i = 1; i < *sz; ++i)
            {
                unsigned delta = this->gamma();
                arr[i] = (bm::gap_word_t) (arr[i-1] + delta + min0);
            } // for i
        }
        else
        if (h3_flag & (1<<1)) // gamma decode
        {
            BM_ASSERT((h3_flag & 1) == 0);
            bool zero_correct = (h3_flag & bm::h3f_ex_gamma_zero_correct);
            for (unsigned i = 0; i < *sz; ++i)
            {
                bm::gap_word_t v = (bm::gap_word_t)this->gamma();
                v -= zero_correct;
                v += min0;
                arr[i] = v;
            } // for i
        }
        else // BIC-DR decode
        {
            bm::gap_word_t min_v, max_v;
            auto s = *sz;
            bm::gap_word_t* ptr = arr;
            if (h3_flag & h3f_ex_minmax_v)
            {
                min_v = (bm::gap_word_t) this->get_16_no();
                max_v = (bm::gap_word_t) this->get_16_no();
                arr[0] = min_v; arr[*sz-1] = max_v;
                if (*sz == 2)
                    return h3_flag;
                ++min_v; --max_v; s -= 2; ptr++;
            }
            else
            {
                min_v = 0; max_v = 65535;
            }
            this->bic_decode_u16(ptr, s, min_v, max_v);

            auto use_wdr = this->get_bit();
            if (use_wdr)
            {
                bm::bit_block_set(tb_wflags, 0);
                unsigned win_size = this->gamma();
                unsigned wcnt = this->gamma();
                wcnt += bm::wcnt_cutoff - 1;
                win_size = (win_size+9) * 2;
                unsigned max_wd = (*sz / win_size) + 1;
                this->bic_decode_u16_bitset(tb_wflags, wcnt, 1, (bm::gap_word_t)max_wd);

                BM_ASSERT(wcnt == bm::bit_block_count(tb_wflags));

                bm::arr_restore_min_w(arr, *sz, win_size, min0, tb_wflags);
            }
            else
                if (min0)
                    bm::arr_restore_min(arr, *sz, min0);
        }
    }
    return h3_flag;
}


// ----------------------------------------------------------------------

} // namespace bm

#ifdef _MSC_VER
#pragma warning(pop)
#endif 

#endif
