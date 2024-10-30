#pragma once

#include <cstdint>
#include <string>
#include <memory>

using namespace std;

namespace sra_convert {

class RdFilter;
typedef std::shared_ptr< RdFilter > RdFilter_ptr;
class RdFilter {
    private:
        enum class RdFilter_E: uint8_t { PASS = 0, REJECT = 1, CRITERIA = 2, REDACTED = 3 };
        RdFilter_E f_type;

        RdFilter ( void ) {
            f_type = RdFilter_E::PASS;
        }

        RdFilter ( char c ) {
            switch( c ) {
                case 'P' : f_type = RdFilter_E::PASS; break;
                case 'J' : f_type = RdFilter_E::REJECT; break;
                case 'C' : f_type = RdFilter_E::CRITERIA; break;
                case 'A' : f_type = RdFilter_E::REDACTED; break;
                default  : f_type = RdFilter_E::PASS; break;
            }
        }

        RdFilter ( const string& s ) {
            if ( string::npos != s . find( 'P' ) ) {
                f_type = RdFilter_E::PASS;
            } else if ( string::npos != s . find( 'J' ) ) {
                f_type = RdFilter_E::REJECT;
            } else if ( string::npos != s . find( 'C' ) ) {
                f_type = RdFilter_E::CRITERIA;
            } else if ( string::npos != s . find( 'A' ) ) {
                f_type = RdFilter_E::REDACTED;
            } else {
                f_type = RdFilter_E::PASS;
            }
        }

    public:
        static RdFilter_ptr make( char c ) {
            return RdFilter_ptr( new RdFilter( c ) );
        }

        static RdFilter_ptr make( const string& s ) {
            return RdFilter_ptr( new RdFilter( s ) );
        }

        static RdFilter_ptr make( void ) {
            return RdFilter_ptr( new RdFilter() );
        }

        string to_str( void ) const {
            switch( f_type ) {
                case RdFilter_E::PASS     : return "P"; break;
                case RdFilter_E::REJECT   : return "J"; break;
                case RdFilter_E::CRITERIA : return "C"; break;
                case RdFilter_E::REDACTED : return "A"; break;
                default : return ""; break;
            }
            return "";
        }

        uint8_t as_u8( void ) const { return ( uint8_t ) f_type; }
};

} // end of namespace sra_convert
