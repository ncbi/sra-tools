/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 */

#ifndef __HASHING_HPP__
#define __HASHING_HPP__

#include <bm/bm64.h>
#include <vector>
#include <functional>
#include <openssl/sha.h>
#include <bitset>

using namespace std;
namespace hashing {
    using bvector_type = bm::bvector<>;

    uint64_t fnv1a(const char* key, std::size_t key_size, size_t hash = 14695981039346656037u) 
    {
        static const std::size_t FnvPrime = 1099511628211u;
        unsigned char *bp = (unsigned char *)key;
        unsigned char *be = bp + key_size;
        while (bp < be) {        
            hash ^= *bp++;
            hash *= FnvPrime;
        }
        return hash;
    }

    struct fnv_1a_hash
    {
        std::size_t operator()(const char* key, std::size_t key_size) const
        {
            return fnv1a(key, key_size);
        }
    };

    /* copied from  https://github.com/explosion/murmurhash/blob/master/murmurhash/MurmurHash2.cpp */
    #define BIG_CONSTANT(x) (x##LLU)

    uint64_t MurmurHash ( const void * key, int len, uint64_t seed = 0)
    {
        const uint64_t m = BIG_CONSTANT(0xc6a4a7935bd1e995);
        const int r = 47;

        uint64_t h = seed ^ (len * m);

        const uint64_t * data = (const uint64_t *)key;
        const uint64_t * end = data + (len/8);

        while(data != end)
        {
            uint64_t k = *data++;

            k *= m; 
            k ^= k >> r; 
            k *= m; 

            h ^= k;
            h *= m; 
        }

        const unsigned char * data2 = (const unsigned char*)data;

        switch(len & 7)
        {
            case 7: h ^= ((uint64_t) data2[6]) << 48;
            case 6: h ^= ((uint64_t) data2[5]) << 40;
            case 5: h ^= ((uint64_t) data2[4]) << 32;
            case 4: h ^= ((uint64_t) data2[3]) << 24;
            case 3: h ^= ((uint64_t) data2[2]) << 16;
            case 2: h ^= ((uint64_t) data2[1]) << 8;
            case 1: h ^= ((uint64_t) data2[0]);
                    h *= m;
        };

        h ^= h >> r;
        h *= m;
        h ^= h >> r;

        return h;
    } 
}

class spot_name_filter
{
public:
    spot_name_filter() = default;
    virtual ~spot_name_filter() = default;

    virtual bool seen_before(const char* value, size_t sz) = 0;
    virtual size_t memory_used() const = 0;
    uint64_t get_name_hash() const { return m_name_hash; }

protected:
    uint64_t m_name_hash = 0;
};

class fnv_murmur_filter : public spot_name_filter
{
public:    
    fnv_murmur_filter() {
        for (auto& hb : hash_buckets) {
            hb = hashing::bvector_type(bm::BM_GAP, bm::gap_len_table_min<true>::_len);
            hb.init();
        }
    }
    virtual bool seen_before(const char* value, size_t sz) 
    {
        bool hits[4];
        m_name_hash = hashing::fnv1a(value, sz);
        auto murmur_hash = hashing::MurmurHash(value, sz);
        uint32_t hash32 = m_name_hash;
        hits[0] = (hash32 == bm::id_max || !hash_buckets[0].set_bit_conditional(hash32, true, false)); 
        hash32 = (m_name_hash >> 32);
        hits[1] = (hash32 == bm::id_max || !hash_buckets[1].set_bit_conditional(hash32, true, false));
        hash32 = murmur_hash;
        hits[2] = (hash32 == bm::id_max || !hash_buckets[2].set_bit_conditional(hash32, true, false));
        hash32 = (murmur_hash >> 32);
        hits[3] = (hash32 == bm::id_max || !hash_buckets[3].set_bit_conditional(hash32, true, false));
        return hits[0] && hits[1] && hits[2] && hits[3];
    }
    size_t memory_used() const override {
        size_t memory_used = 0;
        for (const auto& hb : hash_buckets) {
            hashing::bvector_type::statistics st;
            hb.calc_stat(&st);
            memory_used += st.memory_used;
        }
        return memory_used;
    }

private:    
    
    array<hashing::bvector_type, 4> hash_buckets;

};

template<unsigned char* ShaFunc(const unsigned char *data, size_t count, unsigned char *md_buf), int NUM_BUCKETS>
class sha_filter : public spot_name_filter
{
public:
    virtual bool seen_before(const char* value, size_t sz) override 
    {
        m_name_hash = hashing::fnv1a(value, sz);
        ShaFunc((const unsigned char*)value, sz, (unsigned char*)m_md_buf.data());
        uint8_t hits = 0;
        for (int i = 0; i < NUM_BUCKETS; ++i) {
            m_hits[i] = hash_buckets[i].test(m_md_buf[i]);
            m_hits[i] ? ++hits : hash_buckets[i].set(m_md_buf[i]);
        }
        return hits == NUM_BUCKETS;
    }
    size_t memory_used() const override {
        return (4294967296/8) * NUM_BUCKETS;
    };

private:
    array<uint32_t, NUM_BUCKETS> m_md_buf;
    array<bool, NUM_BUCKETS> m_hits;
    array<std::bitset<4294967296>, NUM_BUCKETS> hash_buckets;
};

class sha1_filter : public sha_filter<SHA1, 5>
{
};

class sha224_filter : public sha_filter<SHA224, 7>
{
};

class sha256_filter : public sha_filter<SHA256, 8>
{
};



#endif // __HASHING_HPP__
