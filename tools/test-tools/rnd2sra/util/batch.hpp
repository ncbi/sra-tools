#pragma once

#include <string>
#include <sstream>
#include <memory>

using namespace std;

namespace sra_convert {
    
struct Data {
    uint64_t id;
    std::string read;
    
    void set_ids( int64_t seq_spot_id, uint32_t seq_read_id ) {
        id = compress_lookup_id( seq_spot_id, seq_read_id );
    }

    void get_idx( int64_t& seq_spot_id, uint32_t& seq_read_id ) {
        decompress_lookup_id( id, seq_spot_id, seq_read_id );
    }
    
    // seq_read_id is 1-based ( valid values are 1 and 2 )
    static uint64_t compress_lookup_id( int64_t seq_spot_id, uint32_t seq_read_id ) {
        uint64_t x = seq_spot_id;
        x <<= 1;
        if ( 1 == seq_read_id ) { return ( x & 0xFFFFFFFFFFFFFFFE ); }
        return ( x | 1 );
    }

    static void decompress_lookup_id( uint64_t id, int64_t& seq_spot_id, uint32_t& seq_read_id ) {
        seq_spot_id = id >> 1;
        if ( 1 == ( id & 1 ) ) {
            seq_read_id = 2;
        } else {
            seq_read_id = 1;
        }
    }
};

struct Batch;
typedef shared_ptr< Batch > BatchPtr;
struct Batch {
    size_t curr;
    size_t entries;
    Data * data;

    Batch( size_t a_entries ) : curr( 0 ), entries( a_entries ), data( nullptr ) { 
        data = new Data[ a_entries ];
    }

    ~Batch() { if ( nullptr != data ) { delete[] data; } }
    
    bool is_full( void ) const { return curr >= entries; }
    
    void reset( void ) { curr = 0; }
    
    static BatchPtr make( size_t a_entries ) { return BatchPtr( new Batch( a_entries ) ); }
};

} // namespace sra_convert
