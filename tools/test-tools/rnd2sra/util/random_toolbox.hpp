#pragma once

#include <cstdint>
#include <random>
#include <memory>

using namespace std;

namespace sra_convert {

class Random;
typedef std::shared_ptr< Random > RandomPtr;
class Random {
    private:
        default_random_engine f_generator;
        uniform_int_distribution< uint8_t > f_bio_base_distribution;
        uniform_int_distribution< uint8_t > f_bio_qual_distribution;
        uniform_int_distribution< int8_t >  f_bio_qual_diff_distribution;
        uniform_int_distribution< uint8_t >  f_string_distribution;
        uniform_int_distribution< uint8_t >  f_lower_char_distribution;
        uniform_int_distribution< uint8_t >  f_upper_char_distribution;

        char f_bases[ 4 ];
        uint8_t f_last_qual;

        Random( int seed ) : f_generator( seed ),
            f_bio_base_distribution( 0, 3 ),
            f_bio_qual_distribution( 0, 63 ),
            f_bio_qual_diff_distribution( -5, +5 ),
            f_string_distribution( (uint8_t)'A', (uint8_t)'z' ),
            f_lower_char_distribution( (uint8_t)'a', (uint8_t)'z' ),
            f_upper_char_distribution( (uint8_t)'A', (uint8_t)'Z' )
        { f_bases[ 0 ] = 'A';
          f_bases[ 1 ] = 'C';
          f_bases[ 2 ] = 'G';
          f_bases[ 3 ] = 'T';
          f_last_qual = random_qual();
        }

        uint8_t random_qual( void ) {
            return f_bio_qual_distribution( f_generator );
        }

        uint8_t random_qual_diff( void ) {
            int8_t diff = f_bio_qual_diff_distribution( f_generator );
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

        char random_char( void ) {
            return f_string_distribution( f_generator );
        }

        char random_lower_char( void ) {
            return f_lower_char_distribution( f_generator );
        }

        char random_upper_char( void ) {
            return f_upper_char_distribution( f_generator );
        }

        string random_string( size_t length ) {
            string res;
            for ( size_t i = 0; i < length; ++i ) { res += random_char(); }
            return res;
        }

        char random_base( void ) {
            uint8_t x = f_bio_base_distribution( f_generator );
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
            uniform_int_distribution< uint32_t > dist( min, max );
            return dist( f_generator );
        }
};

} // end of namespace sra_convert
