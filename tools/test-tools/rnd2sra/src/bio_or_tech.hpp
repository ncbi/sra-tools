#pragma once

#include <cstdint>
#include <string>
#include <memory>

using namespace std;

namespace sra_convert {

class Bio_or_Tech;
typedef std::shared_ptr< Bio_or_Tech > Bio_or_Tech_ptr;
class Bio_or_Tech {
    private:
        enum class BioTech_E : uint8_t { TECH = 0, BIO = 1, INVALID };
        BioTech_E f_type;

        Bio_or_Tech ( char c ) {
            switch( c ) {
                case 'B' : f_type = BioTech_E::BIO; break;
                case 'T' : f_type = BioTech_E::TECH; break;
                default  : f_type = BioTech_E::INVALID; break;
            }
        }

    public:
        static Bio_or_Tech_ptr make( char c ) {
            return Bio_or_Tech_ptr( new Bio_or_Tech( c ) );
        }

        char to_char( void ) const {
            switch( f_type ) {
                case BioTech_E::BIO  : return 'B'; break;
                case BioTech_E::TECH : return 'T'; break;
                default : return '?'; break;
            }
            return '?';
        }

        size_t dflt_length( void ) const {
            switch( f_type ) {
                case BioTech_E::BIO  : return 50; break;
                case BioTech_E::TECH : return 8; break;
                default : return 0; break;
            }
            return 0;
        }

        uint8_t as_u8( void ) const { return ( uint8_t ) f_type; }
        bool is_bio( void ) const { return f_type == BioTech_E::BIO; }
};

} // end of namespace sra_convert
