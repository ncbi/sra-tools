#pragma once

#include "VCursor.hpp"

using namespace std;

namespace vdb {

class VDB_Column;
typedef std::shared_ptr< VDB_Column > VDB_Column_Ptr;
class VDB_Column {
    private :
        const string f_name;
        VCurPtr f_cur;
        uint32_t f_id;
        bool f_ok;

        VDB_Column( const string& name, VCurPtr cur )
            : f_name( name ), f_cur( cur ) {
                f_id = f_cur -> next_col_id( );
                f_ok = f_cur -> add_col( f_id, name . c_str() );
                if ( f_ok ) { f_cur -> inc_next_col_id(); }
        }

    public :
        static VDB_Column_Ptr make( const string& name, VCurPtr cur ) {
            return VDB_Column_Ptr( new VDB_Column( name, cur ) );
        }

        operator bool() const { return f_ok; }

        RowRange row_range( void ) const { return f_cur -> row_range( f_id ); }

        template <typename T> bool read( int64_t row, T* v ) {
            bool res = f_ok;
            if ( res ) { res = f_cur -> read< T >( f_id, row, v ); }
            return res;
        }

        template <typename T, typename A> bool read_arr( int64_t row, A* v ) {
            bool res = f_ok;
            if ( res ) { res = f_cur -> read_arr< T, A >( f_id, row, v ); }
            return res;
        }

        bool read_u8( int64_t row, uint8_t* v ) { return read< uint8_t >( row, v ); }
        bool read_u8_arr( int64_t row, uint8_arr * v ) { return read_arr< uint8_t, uint8_arr >( row, v ); }
        bool read_i8( int64_t row, int8_t* v ) { return read< int8_t >( row, v ); }
        bool read_i8_arr( int64_t row, int8_arr * v ) { return read_arr< int8_t, int8_arr >( row, v ); }
        bool read_string_view( int64_t row, string_view& v ) {
            char_arr a;
            bool res  = read_arr< char, char_arr >( row, &a );
            if ( res ) { v = string_view{ a . data, a . len }; }
            return res;
        }

        bool read_u16( int64_t row, uint16_t* v ) { return read< uint16_t >( row, v ); }
        bool read_u16_arr( int64_t row, uint16_arr * v ) { return read_arr< uint16_t, uint16_arr >( row, v ); }
        bool read_i16( int64_t row, int16_t* v ) { return read< int16_t >( row, v ); }
        bool read_i16_arr( int64_t row, int16_arr * v ) { return read_arr< int16_t, int16_arr >( row, v ); }

        bool read_u32( int64_t row, uint32_t* v ) { return read< uint32_t >( row, v ); }
        bool read_u32_arr( int64_t row, uint32_arr * v ) { return read_arr< uint32_t, uint32_arr >( row, v ); }
        bool read_i32( int64_t row, int32_t* v ) { return read< int32_t >( row, v ); }
        bool read_i32_arr( int64_t row, int32_arr * v ) { return read_arr< int32_t, int32_arr >( row, v ); }

        bool read_u64( int64_t row, uint64_t* v ) { return read< uint64_t >( row, v ); }
        bool read_u64_arr( int64_t row, uint64_arr * v ) { return read_arr< uint64_t, uint64_arr >( row, v ); }
        bool read_i64( int64_t row, int64_t* v ) { return read< int64_t >( row, v ); }
        bool read_i64_arr( int64_t row, int64_arr * v ) { return read_arr< int64_t, int64_arr >( row, v ); }

#ifdef VDB_WRITE
        // set a single default value
        template <typename T> bool write_dflt( const T* v ) {
            return f_cur -> write_dflt< T >( f_id, v );
        }

        bool dflt_u8( uint8_t v ) { return write_dflt< uint8_t >( &v ); }
        bool dflt_i8( int8_t v )  { return write_dflt< int8_t >( &v ); }
        bool dflt_u16( uint16_t v ) { return write_dflt< uint16_t >( &v ); }
        bool dlft_i16( int16_t v )  { return write_dflt< int16_t >( &v ); }
        bool dflt_u32( uint32_t v ) { return write_dflt< uint32_t >( &v ); }
        bool dlft_i32( int32_t v )  { return write_dflt< int32_t >( &v ); }
        bool dflt_u64( uint64_t v ) { return write_dflt< uint64_t >( &v ); }
        bool dflt_i64( int64_t v )  { return write_dflt< int64_t >( &v ); }
        bool dlft_f32( float v ) { return write_dflt< float >( &v ); }
        bool dlft_f64( double v )  { return write_dflt< double >( &v ); }


        // write a single value
        template <typename T> bool write( const T* v ) {
            return f_cur -> write< T >( f_id, v );
        }

        bool write_u8( uint8_t v ) { return write< uint8_t >( &v ); }
        bool write_i8( int8_t v )  { return write< int8_t >( &v ); }
        bool write_u16( uint16_t v ) { return write< uint16_t >( &v ); }
        bool write_i16( int16_t v )  { return write< int16_t >( &v ); }
        bool write_u32( uint32_t v ) { return write< uint32_t >( &v ); }
        bool write_i32( int32_t v )  { return write< int32_t >( &v ); }
        bool write_u64( uint64_t v ) { return write< uint64_t >( &v ); }
        bool write_i64( int64_t v )  { return write< int64_t >( &v ); }
        bool write_f32( float v ) { return write< float >( &v ); }
        bool write_f64( double v )  { return write< double >( &v ); }

        // write multiple values
        template <typename T, typename A> bool write_arr( const A* arr ) {
            return f_cur -> write_arr< T, A >( f_id, arr );
        }

        bool write_u8arr( const uint8_t * v, size_t len ) {
            uint8_arr a{ v, len }; return write_arr< uint8_t, uint8_arr >( &a );
        }
        bool write_u8arr( const uint8_arr_ptr v ) { return write_arr< uint8_t, uint8_arr >( v ); }

        bool write_i8arr( const int8_t * v, size_t len ) {
            int8_arr a{ v, len }; return write_arr< int8_t, int8_arr >( &a );
        }
        bool write_i8arr( const int8_arr_ptr v ) { return write_arr< int8_arr >( v ); }

        bool write_u16arr( uint16_t * v, size_t len ) {
            uint16_arr a{ v, len }; return write_arr< uint16_t, uint16_arr >( &a );
        }
        bool write_u16arr( uint16_arr_ptr v ) { return write_arr< uint16_t, uint16_arr >( v ); }

        bool write_i16arr( const int16_t * v, size_t len ) {
            int16_arr a{ v, len }; return write_arr< int16_t, int16_arr >( &a );
        }
        bool write_i16arr( const int16_arr_ptr v ) { return write_arr< int16_t, int16_arr >( v ); }

        bool write_u32arr( const uint32_t * v, size_t len ) {
            uint32_arr a{ v, len };  return write_arr< uint32_t, uint32_arr >( &a );
        }
        bool write_u32arr( const uint32_arr_ptr v ) { return write_arr< uint32_t, uint32_arr >( v ); }

        bool write_i32arr( const int32_t * v, size_t len ) {
            int32_arr a{ v, len }; return write_arr< int32_t, int32_arr >( &a );
        }
        bool write_i32arr( const int32_arr_ptr v ) { return write_arr< int32_t, int32_arr >( v ); }

        bool write_u64arr( const uint64_t * v, size_t len ) {
            uint64_arr a{ v, len }; return write_arr< uint64_t, uint64_arr >( &a );
        }
        bool write_u64arr( const uint64_arr_ptr v ) { return write_arr< uint64_t, uint64_arr >( v ); }

        bool write_i64arr( const int64_t * v, size_t len ) {
            int64_arr a{ v, len }; return write_arr< int64_t, int64_arr >( &a );
        }
        bool write_i64arr( const int64_arr_ptr v ) { return write_arr< int32_t, int64_arr >( v ); }

        bool write_f32arr( const float * v, size_t len ) {
            f32_arr a{ v, len }; return write_arr< float, f32_arr >( &a );
        }
        bool write_f32arr( const f32_arr_ptr v ) { return write_arr< float, f32_arr >( v ); }

        bool write_f64arr( const double * v, size_t len ) {
            f64_arr a{ v, len }; return write_arr< double, f64_arr >( &a );
        }
        bool write_f64arr( const f64_arr_ptr v ) { return write_arr< double, f64_arr >( v ); }

        bool write_string( const string& v ) {
            return f_cur -> WriteString( f_id, v );
        }

#endif

};

}; // end of namespace sra_convert
