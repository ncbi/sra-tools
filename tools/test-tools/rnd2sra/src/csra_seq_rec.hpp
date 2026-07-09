#pragma once

#include "../vdb/wvdb.hpp"
#include "../util/random_toolbox.hpp"

using namespace std;
using namespace vdb;

namespace sra_convert {

class SeqRec {
    private :
        static const uint32_t READ_START_BACKING_SIZE = 16;
        static const uint32_t READ_LEN_BACKING_SIZE = 16;
        static const uint32_t READ_TYPE_BACKING_SIZE = 16;
        static const uint32_t READ_FILTER_BACKING_SIZE = 16;

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
        uint32_t read_start_backing[ READ_START_BACKING_SIZE ];
        uint32_t read_len_backing[ READ_LEN_BACKING_SIZE ];
        uint8_t read_type_backing[ READ_TYPE_BACKING_SIZE ];
        uint8_t read_filter_backing[ READ_FILTER_BACKING_SIZE ];
        uint64_t u64data[ 2 ];

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
            f_read_start( { &( read_start_backing[ 0 ] ), 2 } ),
            f_read_len( { &( read_len_backing[ 0 ] ), 2 } ),
            f_prim_al_id( { &( u64data[ 0 ] ), 2 } ),
            f_read_type( { &( read_type_backing[ 0 ] ), 2 } ),
            f_read_filter( { &( read_filter_backing[ 0 ] ), 2 } ) {
                f_qual_buffer . data = ( uint8_t * ) malloc( 4096 );
                f_qual_buffer . len = 4096;
            }

        ~SeqRec( void ) {
            if ( f_qual . data != nullptr ) {
                free( ( void * ) f_qual . data );
                f_qual . data = nullptr;
            }
        }

        /* ===== ROW_ID ====================================================================================== */
        void set_row_id( int64_t value ) { f_row_id = value; }
        void inc_row_id( void ) { f_row_id++; }
        int64_t get_row_id( void ) const { return f_row_id; }

        /* ===== READ ======================================================================================== */
        const string& get_read( void ) const { return f_read; }
        void set_read( string_view value ) { f_read = value; }

        /* ===== CMP_READ ==================================================================================== */
        const string& get_cmp_read( void ) const { return f_cmp_read; }
        void set_cmp_read( string_view value ) { f_cmp_read = value; }
        void clear_cmp_read( void ) { f_cmp_read . clear(); }

        /* ===== QUALITY ===================================================================================== */
        const uint8_arr_ptr get_qual( void ) const { return ( uint8_arr_ptr )&f_qual; }
        void make_random_qual( RandomPtr rnd, size_t len ) {
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

        /* ===== NAME ========================================================================================= */
        const string& get_name( void ) const { return f_name; }
        void set_name( string_view value ) { f_name = value; }

        /* ===== SPOTGROUP ===================================================================================== */
        const string& get_spot_grp( void ) const { return f_spot_grp; }
        void set_spot_grp( string_view value ) { f_spot_grp = value; }

        /* ===== READ_START =================================================================================== */
        const uint32_arr_ptr get_read_start( void ) const { return ( uint32_arr_ptr)&f_read_start; }
        void set_read_start( uint32_t value1, uint32_t value2 ) { set_u32( &f_read_start, value1, value2 ); }
        void set_read_start( uint32_t value ) { set_u32( &f_read_start, value ); }
        void modify_read_start_element_count( int32_t diff ) {
            if ( diff < 0 ) {
                if ( f_read_start . len >= ( uint32_t )-diff ) {
                    f_read_start . len += diff;
                }
            } else if ( diff < ( int32_t ) READ_START_BACKING_SIZE ) {
                f_read_start . len += diff;
            }
        }

        /* ===== READ_LEN ===================================================================================== */
        const uint32_arr_ptr get_read_len( void ) const { return ( uint32_arr_ptr)&f_read_len; }
        void set_read_len( uint32_t value1, uint32_t value2 ) { set_u32( &f_read_len, value1, value2 ); }
        void set_read_len( uint32_t value ) { set_u32( &f_read_len, value ); }
        void modify_read_len_element_count( int32_t diff ) {
            if ( diff < 0 ) {
                if ( f_read_len . len >= ( uint32_t )-diff ) {
                    f_read_len . len += diff;
                }
            } else if ( diff < ( int32_t )READ_LEN_BACKING_SIZE ) {
                f_read_len . len += diff;
            }
        }

        /* ===== READ_TYPE ===================================================================================== */
        const uint8_arr_ptr get_read_type( void ) const { return ( uint8_arr_ptr)&f_read_type; }
        void set_read_type( uint8_t value1, uint8_t value2 ) { set_u8( &f_read_type, value1, value2 ); }
        void set_read_type( uint8_t value ) { set_u8( &f_read_type, value ); }
        void modify_read_type_element_count( int32_t diff ) {
            if ( diff < 0 ) {
                if ( f_read_type . len >= ( uint32_t )-diff ) {
                    f_read_type . len += diff;
                }
            } else if ( diff < ( int32_t )READ_TYPE_BACKING_SIZE ) {
                f_read_type . len += diff;
            }
        }

        /* ===== READ_FILTER =================================================================================== */
        const uint8_arr_ptr get_read_filter( void ) const { return ( uint8_arr_ptr)&f_read_filter; }
        void set_read_filter( uint8_t value1, uint8_t value2 ) { set_u8( &f_read_filter, value1, value2 ); }
        void set_read_filter( uint8_t value ) { set_u8( &f_read_filter, value ); }
        void modify_read_filter_element_count( int32_t diff ) {
            if ( diff < 0 ) {
                if ( f_read_filter . len >= ( uint32_t )-diff ) {
                    f_read_filter . len += diff;
                }
            } else {
                if ( diff < ( int32_t )READ_FILTER_BACKING_SIZE ) {
                    f_read_filter . len += diff;
                }
            }
        }

        /* ===== PRIM_ALIGNMENT_IDs ============================================================================ */
        const uint64_arr_ptr get_prim_al_id( void ) const { return ( uint64_arr_ptr)&f_prim_al_id; }
        void set_prim_al_id( uint64_t value1, uint64_t value2 ) { set_u64( &f_prim_al_id, value1, value2 ); }
        void set_prim_al_id( uint64_t value ) { set_u64( &f_prim_al_id, value ); }

};

}
