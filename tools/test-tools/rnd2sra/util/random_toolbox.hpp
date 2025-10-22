#pragma once

#include <cstdint>
#include <random>
#include <memory>

using namespace std;

namespace sra_convert {

class Custom_Randomizer {
    private :
        std::mt19937_64 f_generator;

    public :
        Custom_Randomizer( int seed ) : f_generator( seed ) {}
        template< class T > T value( void ) { return f_generator(); }
};

template < class T >
class Custom_Range {
    private :
        const T f_start, f_span;

        const T span( T t1, T t2 ) const {
            return 1 + std::max( t1, t2 ) - std::min( t1, t2 );
        }

        const T abs( const T x ) const { return x < 0 ? -x : x; }

    public :
        Custom_Range( T t1, T t2 ) : f_start( std::min( t1, t2 ) ), f_span( span( t1, t2 ) ) {}

        const T get( Custom_Randomizer& src ) const {
            return f_start + abs( src . value< T >() % f_span );
        }
};

class Random;
typedef std::shared_ptr< Random > RandomPtr;
class Random {
    private:
        Custom_Randomizer f_random_src;
        Custom_Range< uint8_t > f_base_range;
        Custom_Range< uint8_t > f_qual_range;
        Custom_Range< int8_t > f_qual_diff_range;
        Custom_Range< uint8_t > f_string_range;
        Custom_Range< uint8_t > f_lower_range;
        Custom_Range< uint8_t > f_upper_range;

        char f_bases[ 4 ];
        uint8_t f_last_qual;

        Random( int seed ) : f_random_src( seed ),
            f_base_range( 0, 3 ),
            f_qual_range( 0, 63 ),
            f_qual_diff_range( -5, +5 ),
            f_string_range( (uint8_t)'A', (uint8_t)'z' ),
            f_lower_range( (uint8_t)'a', (uint8_t)'z' ),
            f_upper_range( (uint8_t)'A', (uint8_t)'Z' )
        { f_bases[ 0 ] = 'A';
          f_bases[ 1 ] = 'C';
          f_bases[ 2 ] = 'G';
          f_bases[ 3 ] = 'T';
          f_last_qual = random_qual();
        }

        uint8_t random_qual( void ) { return f_qual_range . get( f_random_src ); }

        uint8_t random_qual_diff( void ) {
            int8_t diff = f_qual_diff_range . get( f_random_src );
            if ( diff > 0 ) {
                if ( f_last_qual + diff > 63 ) { diff = -diff; }
            }
            if ( diff < 0 ) {
                if ( f_last_qual < abs( diff ) ) { diff = -diff; }
            }
            f_last_qual += diff;
            return f_last_qual;
        }

    public:
        static RandomPtr make( int seed ) { return RandomPtr( new Random( seed ) ); }

        char random_char( void ) { return f_string_range . get( f_random_src ); }
        char random_lower_char( void ) { return f_lower_range . get( f_random_src ); }
        char random_upper_char( void ) { return f_upper_range . get ( f_random_src ); }
        string random_string( size_t length ) {
            string res;
            for ( size_t i = 0; i < length; ++i ) { res += random_char(); }
            return res;
        }

        char random_base( void ) {
            uint8_t x = f_base_range . get( f_random_src );
            return f_bases[ x ];
        }

        string random_bases( size_t length ) {
            string res;
            for ( size_t i = 0; i < length; ++i ) { res += random_base(); }
            return res;
        }

        void random_quals( uint8_t * buffer, size_t length ) {
            for ( size_t i = 0; i < length; ++i ) { buffer[ i ] = random_qual(); }
        }

        void random_diff_quals( uint8_t * buffer, size_t length ) {
            for ( size_t i = 0; i < length; ++i ) { buffer[ i ] = random_qual_diff(); }
        }

        uint32_t random_u32( uint32_t min, uint32_t max ) {
            Custom_Range< uint32_t > u32_range( min, max );
            return u32_range . get( f_random_src );
        }
};

} // end of namespace sra_convert
