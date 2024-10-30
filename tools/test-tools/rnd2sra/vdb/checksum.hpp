#pragma once

#include<string>
#include<algorithm>
#include<memory>

#include "VdbObj.hpp"

using namespace std;

namespace vdb {

class Checksum;
typedef std::shared_ptr< Checksum > Checksum_ptr;
class Checksum {
    private :
        enum class Checksum_E { NONE, CRC32, MD5 };
        Checksum_E f_type;

        Checksum( const string& p ) {
            string s;
            std::transform( p . begin(), p . end(), std::inserter( s, s . begin() ), ::tolower );
            if ( s == "crc32" ) { f_type = Checksum_E::CRC32; }
            else if ( s == "md5" ) { f_type = Checksum_E::MD5; }
            else { f_type = Checksum_E::NONE; }
        }

    public:
        static Checksum_ptr make( const string& p ) {
            return Checksum_ptr( new Checksum( p ) );
        }

        string to_string( void ) const {
            switch( f_type ) {
                case Checksum_E::NONE  : return string( "NONE" ); break;
                case Checksum_E::CRC32 : return string( "CRC32" ); break;
                case Checksum_E::MD5   : return string( "MD5" ); break;
            }
            return string( "NONE" );
        }

        bool is_none( void ) const {
            return ( f_type == Checksum_E::NONE );
        }

        bool is_crc32( void ) const {
            return ( f_type == Checksum_E::CRC32 );
        }

        bool is_md5( void ) const {
            return ( f_type == Checksum_E::MD5 );
        }

        KCreateMode mode( KCreateMode in ) const {
            KCreateMode res( in );
            if ( is_md5() ) res |= kcmMD5;
            return res;
        }

        KCreateMode mode_mask( KCreateMode in ) const {
            KCreateMode res( in );
            if ( is_md5() ) res |= kcmMD5;
            return res;
        }

        friend auto operator<<( ostream& os, Checksum_ptr o ) -> ostream& {
            os << o -> to_string();
            return os;
        }
};

} // end of namespace sra_convert
