#pragma once

#include <sstream>
#include <fstream>
#include <algorithm>

namespace tools {

    const uint16_t FLAG_PAIRED = 0x01;
    const uint16_t FLAG_PROPPER_ALIGNED = 0x02;
    const uint16_t FLAG_UNMAPPED = 0x04;
    const uint16_t FLAG_MATE_UNMAPPED = 0x08;
    const uint16_t FLAG_REVERSED = 0x10;
    const uint16_t FLAG_MATE_REVERSED = 0x20;
    const uint16_t FLAG_IS_READ1 = 0x40;
    const uint16_t FLAG_IS_READ2 = 0x80;
    const uint16_t FLAG_NOT_PRIMARY = 0x100;
    const uint16_t FLAG_QUAL_CHECK_FAILED = 0x200;
    const uint16_t FLAG_PCR_OR_DUP = 0x400;
    const uint16_t FLAG_SUPPLEMENTAL = 0x800;

    static uint16_t calc_flags( uint32_t ref_id, uint32_t mate_ref_id,
                        bool reversed, bool mate_reversed,
                        uint64_t mate_id, uint64_t mate_codes ) {
        uint16_t res = 0;

        bool paired   = ( mate_codes & 0x8 ) == 0x8;
        bool pcr_dub  = ( mate_codes & 0x4 ) == 0x4;
        bool bad_qual = ( mate_codes & 0x2 ) == 0x2;
        bool is_rd2   = ( mate_codes & 0x1 ) == 0x1;
        
        if ( paired ) { res |= tools::FLAG_PAIRED; }
        if ( reversed ) { res |= tools::FLAG_REVERSED; }
        if ( bad_qual ) { res |= tools::FLAG_QUAL_CHECK_FAILED; }
        if ( pcr_dub ) { res |= tools::FLAG_PCR_OR_DUP; }

        if ( mate_id > 0 ) {
            if ( ref_id == mate_ref_id ) { res |= tools::FLAG_PROPPER_ALIGNED; }
            if ( mate_reversed ) { res |= tools::FLAG_MATE_REVERSED; }
        } else {
            //the mate is unmapped...
            res |= tools::FLAG_MATE_UNMAPPED;
        }
        
        if ( is_rd2 ) {
            res |= tools::FLAG_IS_READ2; // because this is read2
        } else {
            res |= tools::FLAG_IS_READ1; // because this is read1
        }

        return res;
    }

    static int32_t calc_tlen( bool reversed, bool mate_reversed,
                    uint32_t ref_ofs, uint32_t mate_ref_ofs,
                    uint32_t ref_len, uint32_t mate_ref_len,
                    bool is_rd2 ) {
        int32_t res = 0;
        uint32_t self_left  = ref_ofs;
        uint32_t self_right = ref_ofs + ref_len;
        uint32_t mate_left  = mate_ref_ofs;
        uint32_t mate_right = mate_ref_ofs + mate_ref_len;
        uint32_t leftmost   = ( self_left  < mate_left ) ? self_left  : mate_left;
        uint32_t rightmost  = ( self_right > mate_right ) ? self_right : mate_right;
        uint32_t tlen = rightmost - leftmost;

        /* The standard says, "The leftmost segment has a plus sign and the rightmost has a minus sign." */
        if ( ( self_left <= mate_left && self_right >= mate_right ) || /* mate fully contained within self or */
            ( mate_left <= self_left && mate_right >= self_right ) ) { /* self fully contained within mate; */

            if ( self_left < mate_left || ( !is_rd2 == 1 && self_left == mate_left ) ) {
                res = tlen;
            } else {
                res = -( ( int32_t )tlen );
            }
        } else if ( ( self_right == mate_right && mate_left == leftmost ) || /* both are rightmost, but mate is leftmost */
                    ( self_right == rightmost ) ) {
            res = -( ( int32_t )tlen );
        } else {
            res = tlen;
        }
        return res;
    }

    static char complement( char c ) {
        switch( c ) {
            case 'A' : return 'T'; break;
            case 'C' : return 'G'; break;
            case 'G' : return 'C'; break;
            case 'T' : return 'A'; break;
        }
        return c;
    }

    static void reverse_complement( std::string& s ) {
        std::transform(
            std::begin( s ),
            std::end( s ),
            std::begin( s ),
            complement );
    }

    static void reverse( std::string& s ) { std::reverse( s.begin(), s.end() ); }
    
    class iter_func {
        public:
            virtual void operator()( uint64_t id, const std::string& s ) {}
    };

    static uint64_t for_each_line( std::istream& s, iter_func& f, uint64_t limit = 0 ) {
        uint64_t line_nr = 0;
        for ( std::string line; std::getline( s, line ); line_nr++ ) {
            if ( limit > 0 && line_nr >= limit )  break;
            f( line_nr, line );
        }
        return line_nr;
    }
    static uint64_t for_each_word( const std::string& s, char delim, iter_func& f ) {
        uint64_t line_nr = 0;
        std::istringstream is( s );
        for ( std::string line; std::getline( is, line, delim ); line_nr++ ) {
            f( line_nr, line );
        }
        return line_nr;
    }

    template < typename T1, typename T2 > T1 convert_str( T2 src ) {
        std::istringstream iss( src );
        try {
            T1 res;
            iss >> res;
            return res;
        } catch ( ... ) {};
        return 0;
    }

    static uint64_t str_2_uint64_t( const std::string& src ) { return convert_str<uint64_t>( src ); }
    static uint64_t str_2_uint64_t( const char * src ) { return convert_str<uint64_t>( src ); }
    static uint64_t str_2_uint32_t( const std::string& src ) { return convert_str<uint32_t>( src ); }
    static uint64_t str_2_uint32_t( const char * src ) { return convert_str<uint32_t>( src ); }
    static uint64_t str_2_uint16_t( const std::string& src ) { return convert_str<uint16_t>( src ); }
    static uint64_t str_2_uint16_t( const char * src ) { return convert_str<uint16_t>( src ); }
    static uint64_t str_2_uint8_t( const std::string& src ) { return convert_str<uint8_t>( src ); }
    static uint64_t str_2_uint8_t( const char * src ) { return convert_str<uint8_t>( src ); }

    static bool make_empty_file( const std::string& filename, uint64_t file_size ) {
        try {
            std::ofstream ofs( filename, std::ios::binary | std::ios::out );
            ofs.seekp( file_size - 1 );
            ofs.write( "", 1 );
            return true;
        } catch ( ... ) {
            return false;
        }
    }

    static bool make_empty_file( const char * filename, uint64_t file_size ) {
        return make_empty_file( std::string( filename ), file_size );
    }
    
    static std::string make_filename( const char * base, const char * ext ) {
        std::ostringstream os;
        os << base << "." << ext;
        return os.str();
    }
};

