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
#include <klib/checksum.h>

using namespace std;
namespace hashing {
    using bvector_type = bm::bvector<>;
    using hash_bucket_type = vector<bvector_type>;
    using hash_function_type = function<bool(hash_bucket_type&, const char*, size_t)>;
    static
    //unsigned long long LargestPrime48 = 281474976710597u;  // FFFFFFFFFFC5

    // unsigned long  crc = crc32(0L, Z_NULL, 0);


    // For 32 bit machines:
    // const std::size_t fnv_prime = 16777619u;
    // const std::size_t fnv_offset_basis = 2166136261u;

    // For 64 bit machines:
    // const std::size_t fnv_prime = 1099511628211u;
    // const std::size_t fnv_offset_basis = 14695981039346656037u;

    // For 128 bit machines:
    // const std::size_t fnv_prime = 309485009821345068724781401u;
    // const std::size_t fnv_offset_basis =
    //     275519064689413815358837431229664493455u;

    // For 256 bit machines:
    // const std::size_t fnv_prime =
    //     374144419156711147060143317175368453031918731002211u;
    // const std::size_t fnv_offset_basis =
    //     100029257958052580907070968620625704837092796014241193945225284501741471925557u;

    uint64_t fnv_1a(const char* v, size_t sz)
    {
        static const size_t fnv_prime = 1099511628211u;
        static const size_t fnv_offset_basis = 14695981039346656037u;
        size_t hash = fnv_offset_basis;
        while (sz > 0) {
            hash ^= *v;
            hash *= fnv_prime;
            --sz;
            ++v;
        }
        return hash;
    };


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

    int _fnv_1a_hash(hash_bucket_type& buckets, const char* value, size_t sz) 
    {
        auto const hash = fnv_1a(value, sz);
        auto const lo = decltype(bm::id_max){(uint32_t)(hash)};
        auto const hi = decltype(bm::id_max){(uint32_t)(hash >> 32)};
        int hits = 0;

        assert(buckets.size() >= 2);

        if (lo == bm::id_max || !buckets[0].set_bit_conditional(lo, true, false)) 
            ++hits;

        if (hi == bm::id_max || !buckets[1].set_bit_conditional(hi, true, false))
            ++hits;

        return hits;
    }

    bool fnv_1a_hash(hash_bucket_type& buckets, const char* value, size_t sz) 
    {
        return _fnv_1a_hash(buckets, value, sz) == 2;
    }

    int _fnv_murmur_hash(hash_bucket_type& buckets, const char* value, size_t sz) 
    {
        assert(buckets.size() == 4);
        int hits = _fnv_1a_hash(buckets, value, sz);

        auto const hash = MurmurHash(value, sz);
        auto const lo = decltype(bm::id_max){(uint32_t)(hash)};
        auto const hi = decltype(bm::id_max){(uint32_t)(hash >> 32)};

        if (lo == bm::id_max || !buckets[2].set_bit_conditional(lo, true, false)) 
            ++hits;
        if (hi == bm::id_max || !buckets[3].set_bit_conditional(hi, true, false))
            ++hits;

        return hits;
    }

    bool fnv_murmur_hash(hash_bucket_type& buckets, const char* value, size_t sz) 
    {
        return (size_t)_fnv_murmur_hash(buckets, value, sz) == buckets.size();
    }

    bool sha1_hash(hash_bucket_type& buckets, const char* value, size_t sz) 
    {
        assert(buckets.size() == 5);
        uint8_t hits = 0;
        auto words = std::array<uint32_t, 5>();

        SHA1State state;
        SHA1StateInit(&state);
        SHA1StateAppend(&state, value, sz);
        SHA1StateFinish(&state, reinterpret_cast<uint8_t *>(words.data()));        

        for (auto && hash : words) {
            auto const i = &hash - &words[0];
            auto const hash32 = decltype(bm::id_max){hash};
            if (hash32 == bm::id_max || !buckets[i].set_bit_conditional(hash32, true, false))
                ++hits;
        }
        return hits == buckets.size();
    }
};


class spot_name_check
{
public:
    spot_name_check(size_t spot_name_number) 
    : hash_buckets(bucket_count(spot_name_number), hashing::bvector_type(bm::BM_GAP, bm::gap_len_table_min<true>::_len))
    , hash_func(function(spot_name_number))
    {};
    
    bool seen_before(const char* value, size_t sz) 
    {
        return hash_func(hash_buckets, value, sz);   
    }
    
    size_t memory_used() const {
        size_t memory_used = 0;
        for (const auto& hb : hash_buckets) {
            hashing::bvector_type::statistics st;
            hb.calc_stat(&st);
            memory_used += st.memory_used;
        }
        return memory_used;
    }

private:
    spot_name_check() = default;
    hashing::hash_bucket_type hash_buckets;
    hashing::hash_function_type hash_func;

    static constexpr auto threshold = 9e8;
    static size_t bucket_count(size_t const spot_count) {
        return spot_count < threshold ? 4 : 5;
    }
    static hashing::hash_function_type function(size_t const spot_count) {
        return spot_count < threshold ? hashing::fnv_murmur_hash : hashing::sha1_hash;
    }
};

#endif // __HASHING_HPP__
