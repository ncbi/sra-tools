#pragma once

#include<string>
#include<algorithm>
#include<memory>

using namespace std;

namespace sra_convert {

class Product;
typedef std::shared_ptr< Product > Product_ptr;
class Product {
    private :
        enum class Product_E { FLAT, NONE_CSRA, CSRA, TEST, UNKNOWN };
        Product_E f_type;

        Product( const string& p ) {
            string s;
            std::transform( p . begin(), p . end(), std::inserter( s, s . begin() ), ::tolower );
            if ( s == "flat" ) { f_type = Product_E::FLAT; }
            else if ( s == "none_csra" ) { f_type = Product_E::NONE_CSRA; }
            else if ( s == "db" ) { f_type = Product_E::NONE_CSRA; }
            else if ( s == "csra" ) { f_type = Product_E::CSRA; }
            else if ( s == "test" ) { f_type = Product_E::TEST; }
            else { f_type = Product_E::UNKNOWN; }
        }

    public:
        static Product_ptr make( const string& p ) {
            return Product_ptr( new Product( p ) );
        }

        string to_string( void ) const {
            switch( f_type ) {
                case Product_E::FLAT : return string( "FLAT" ); break;
                case Product_E::NONE_CSRA : return string( "NONE_CSRA" ); break;
                case Product_E::CSRA : return string( "CSRA" ); break;
                case Product_E::TEST : return string( "TEST" ); break;
                default : return string( "UNKNOWN" ); break;
            }
        }

        bool is_acc( void ) const {
            switch( f_type ) {
                case Product_E::FLAT : return true; break;
                case Product_E::NONE_CSRA : return true; break;
                case Product_E::CSRA : return true; break;
                default : return false; break;
            }
        }

        bool is_flat( void ) const {
            return ( f_type == Product_E::FLAT );
        }

        bool is_db( void ) const {
            return ( f_type == Product_E::NONE_CSRA );
        }

        bool is_cSRA( void ) const {
            return ( f_type == Product_E::CSRA );
        }

        bool is_tst( void ) const {
            return ( f_type == Product_E::TEST );
        }

        friend auto operator<<( ostream& os, Product_ptr o ) -> ostream& {
            os << o -> to_string();
            return os;
        }
};

} // end of namespace sra_convert
