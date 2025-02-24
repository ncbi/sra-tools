#pragma once

#include "../vdb/wvdb.hpp"
#include "../util/utils.hpp"

using namespace std;
using namespace vdb;

namespace sra_convert {

class SeqRec {
    private :
        uint8_arr f_qual;
        uint8_arr f_qual_buffer;
        uint32_arr f_read_start;
        uint32_arr f_read_len;
        uint64_arr f_prim_al_id;
        uint8_arr f_read_type;
        uint8_arr f_read_filter;

        string f_name;
        string f_spot_grp;
        string f_read;
        string f_cmp_read;
        int64_t f_row_id;
        uint32_t u32data[ 4 ];
        uint64_t u64data[ 2 ];
        uint8_t u8data[ 4 ];

        void set_u8( uint8_arr_ptr dst, uint8_t value1, uint8_t value2 ) {
            uint8_t * p = ( uint8_t * ) dst -> data;
            dst -> len = 2;
            p[ 0 ] = value1;
            p[ 1 ] = value2;
        }

        void set_u8( uint8_arr_ptr dst, uint8_t value ) {
            uint8_t * p = ( uint8_t * ) dst -> data;
            dst -> len = 1;
            p[ 0 ] = value;
        }

        void set_u32( uint32_arr_ptr dst, uint32_t value1, uint32_t value2 ) {
            uint32_t * p = ( uint32_t * ) dst -> data;
            dst -> len = 2;
            p[ 0 ] = value1;
            p[ 1 ] = value2;
        }

        void set_u32( uint32_arr_ptr dst, uint32_t value ) {
            uint32_t * p = ( uint32_t * ) dst -> data;
            dst -> len = 1;
            p[ 0 ] = value;
        }

        void set_u64( uint64_arr_ptr dst, uint64_t value1, uint64_t value2 ) {
            uint64_t * p = ( uint64_t * ) dst -> data;
            dst -> len = 2;
            p[ 0 ] = value1;
            p[ 1 ] = value2;
        }

        void set_u64( uint64_arr_ptr dst, uint64_t value ) {
            uint64_t * p = ( uint64_t * ) dst -> data;
            dst -> len = 1;
            p[ 0 ] = value;
        }

    public :
        SeqRec( void ) : f_qual( { nullptr, 0 } ),
            f_qual_buffer( { nullptr, 0 } ),
            f_read_start( { &( u32data[ 0 ] ), 2 } ),
            f_read_len( { &( u32data[ 2 ] ), 2 } ),
            f_prim_al_id( { &( u64data[ 0 ] ), 2 } ),
            f_read_type( { &( u8data[ 0 ] ), 2 } ),
            f_read_filter( { &( u8data[ 2 ] ), 2 } ) {
                f_qual_buffer . data = ( uint8_t * ) malloc( 4096 );
                f_qual_buffer . len = 4096;
            }

        ~SeqRec( void ) {
            if ( f_qual . data != nullptr ) {
                free( ( void * ) f_qual . data );
                f_qual . data = nullptr;
            }
        }

        const uint8_arr_ptr get_qual( void ) const { return ( uint8_arr_ptr )&f_qual; }
        const string& get_name( void ) const { return f_name; }
        const string& get_spot_grp( void ) const { return f_spot_grp; }
        const string& get_read( void ) const { return f_read; }
        const string& get_cmp_read( void ) const { return f_cmp_read; }
        const uint32_arr_ptr get_read_start( void ) const { return ( uint32_arr_ptr)&f_read_start; }
        const uint32_arr_ptr get_read_len( void ) const { return ( uint32_arr_ptr)&f_read_len; }
        const uint64_arr_ptr get_prim_al_id( void ) const { return ( uint64_arr_ptr)&f_prim_al_id; }
        const uint8_arr_ptr get_read_type( void ) const { return ( uint8_arr_ptr)&f_read_type; }
        const uint8_arr_ptr get_read_filter( void ) const { return ( uint8_arr_ptr)&f_read_filter; }
        int64_t get_row_id( void ) const { return f_row_id; }

        void set_name( string_view value ) { f_name = value; }
        void set_spot_grp( string_view value ) { f_spot_grp = value; }
        void set_read( string_view value ) { f_read = value; }
        void set_cmp_read( string_view value ) { f_cmp_read = value; }
        void clear_cmp_read( void ) { f_cmp_read . clear(); }

        void set_row_id( int64_t value ) { f_row_id = value; }
        void inc_row_id( void ) { f_row_id++; }
        void set_read_start( uint32_t value1, uint32_t value2 ) { set_u32( &f_read_start, value1, value2 ); }
        void set_read_start( uint32_t value ) { set_u32( &f_read_start, value ); }
        void set_read_len( uint32_t value1, uint32_t value2 ) { set_u32( &f_read_len, value1, value2 ); }
        void set_read_len( uint32_t value ) { set_u32( &f_read_len, value ); }
        void set_prim_al_id( uint64_t value1, uint64_t value2 ) { set_u64( &f_prim_al_id, value1, value2 ); }
        void set_prim_al_id( uint64_t value ) { set_u64( &f_prim_al_id, value ); }
        void set_read_type( uint8_t value1, uint8_t value2 ) { set_u8( &f_read_type, value1, value2 ); }
        void set_read_type( uint8_t value ) { set_u8( &f_read_type, value ); }
        void set_read_filter( uint8_t value1, uint8_t value2 ) { set_u8( &f_read_filter, value1, value2 ); }
        void set_read_filter( uint8_t value ) { set_u8( &f_read_filter, value ); }

        void make_random_qual( util::RandomPtr rnd, size_t len ) {
            if ( len > f_qual_buffer . len ) {
                if ( nullptr != f_qual_buffer . data ) {
                    free( ( void * ) f_qual_buffer . data );
                }
                f_qual_buffer . data = ( uint8_t * )malloc( len );
                f_qual_buffer . len = len;
            }
            rnd -> random_diff_quals( ( uint8_t * )f_qual_buffer . data, len );
            f_qual . data = f_qual_buffer . data;
            f_qual . len = len;
        }

};

}
