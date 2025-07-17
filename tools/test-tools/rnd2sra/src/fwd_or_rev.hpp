#pragma once

#include <cstdint>
#include <string>
#include <memory>

using namespace std;

namespace sra_convert {

class Fwd_or_Rev;
typedef std::shared_ptr< Fwd_or_Rev > Fwd_or_Rev_ptr;
class Fwd_or_Rev {
    private:
        enum class FwdRev_E : uint8_t { UNKNOWN = 0, FWD = 2, REV = 4 };
        FwdRev_E f_type;

        Fwd_or_Rev ( void ) {
            f_type = FwdRev_E::UNKNOWN;
        }

        Fwd_or_Rev ( char c ) {
            switch( c ) {
                case 'F' : f_type = FwdRev_E::FWD; break;
                case 'R' : f_type = FwdRev_E::REV; break;
                default  : f_type = FwdRev_E::UNKNOWN; break;
            }
        }

        Fwd_or_Rev ( const string& s ) {
            if ( string::npos != s . find( 'F' ) ) {
                f_type = FwdRev_E::FWD;
            } else if ( string::npos != s . find( 'R' ) ) {
                f_type = FwdRev_E::REV;
            } else {
                f_type = FwdRev_E::UNKNOWN;
            }
        }

    public:
        static Fwd_or_Rev_ptr make( char c ) {
            return Fwd_or_Rev_ptr( new Fwd_or_Rev( c ) );
        }

        static Fwd_or_Rev_ptr make( const string& s ) {
            return Fwd_or_Rev_ptr( new Fwd_or_Rev( s ) );
        }

        static Fwd_or_Rev_ptr make( void ) {
            return Fwd_or_Rev_ptr( new Fwd_or_Rev() );
        }

        string to_str( void ) const {
            switch( f_type ) {
                case FwdRev_E::FWD : return "F"; break;
                case FwdRev_E::REV : return "R"; break;
                default : return ""; break;
            }
            return "";
        }

        uint8_t as_u8( void ) const { return ( uint8_t ) f_type; }
};

} // end of namespace sra_convert
