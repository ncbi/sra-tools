#include <iostream>
#include <map>
#include <functional>
#include "tools.hpp"

typedef std::map< std::string, std::string > values;

std::string make_key( const std::string& s, uint32_t nr ) {
    std::ostringstream os;
    os << s << "." << nr;
    return os.str();
}

std::string flip_key( const std::string& s ) {
    std::size_t dot = s.find( "." );
    if ( dot != std::string::npos ) {
        std::string before_dot( s, 0, dot );
        std::string after_dot( s, dot + 1, 1 );
        if ( after_dot == "1" ) {
            return make_key( before_dot, 2 );
        } else if ( after_dot == "2" ) {
            return make_key( before_dot, 1 );            
        }
    }
    return s;
}

class line_event : public tools::iter_func {

    class tab_event : public tools::iter_func {
        public :
            tab_event( uint16_t col, values& v ) : f_col( col ), f_values( v ) {}
            
            virtual void operator()( uint64_t id, const std::string& s ) {
                if ( id == 0 ) {
                    f_key = s;
                } else if ( id == f_col ) {
                    std::string k1 = make_key( f_key, 1 );
                    auto found = f_values . find( k1 );
                    if ( found == f_values.end() ) {
                        f_values.insert( { k1, s } );
                    } else {
                        f_values.insert( { make_key( f_key, 2 ), s } );                        
                    }
                }
            }
        private:
            uint16_t f_col;
            values& f_values;
            std::string f_key;
    };
    
    public:
        line_event( uint16_t col, values& v ) : f_col( col ), f_values( v ) {}

        virtual void operator()( uint64_t id, const std::string& s ) {
            tab_event e( f_col, f_values );
            tools::for_each_word( s, '\t', e );
        }

    private:
        uint16_t f_col;
        values& f_values;
};

void fill_values( std::ifstream& is, uint16_t col, uint64_t row_limit, values& v ) {
    line_event e( col, v );
    tools::for_each_line( is, e, row_limit );
}

class comp {
    public :
        comp( const values& v1, const values& v2, const bool verbose )
            : m1( v1 ), m2( v2 ), f_verbose( verbose ) {}
        
        
        bool compare() const {
            uint64_t matches = 0;
            uint64_t mismatches = 0;
            uint64_t not_found = 0;
            for ( auto i = m1.begin(); i != m1.end(); ++i ) {
                const std::string& key( i -> first );
                const std::string& value( i -> second );
                auto found = m2 . find( key );
                if ( found != m2 . end() ) {
                    const std::string& found_value( found -> second );
                    if ( found_value  == value ) {
                        matches++;
                    } else {
                        std::string flipped_key = flip_key( key );
                        auto found_flipped = m2 . find( flipped_key );
                        bool mismatched = true;
                        if ( found_flipped != m2 . end() ) {
                            const std::string& flipped_value( found_flipped -> second );
                            mismatched = !( flipped_value == value );
                        }
                        if ( mismatched ) {
                            mismatches++;
                            if ( f_verbose && mismatches < 4 ) {
                                std::cout << "mis-match : " << i -> first << " - " << i -> second << " - " << found_value << std::endl;
                            }
                        } else {
                            matches++;
                        }
                    }
                } else {
                    not_found++;
                    if ( f_verbose ) {
                        std::cout << i -> first << " not found" << std::endl;
                    }
                }
            }
            std::cout << "matches : " << matches << std::endl;
            if ( mismatches > 0 ) { std::cout << "mis-matches : " << mismatches << std::endl; }
            if ( not_found > 0 ) { std::cout << "not-found : " << not_found << std::endl; }
            return ( mismatches == 0 );
        }

    private :
        const values& m1;
        const values& m2;
        const bool f_verbose;
};

/*
 * argv[ 1 ] ... filename of first SAM-file
 * argv[ 2 ] ... filename of second SAM-file
 * argv[ 3 ] ... column to compare, zero-based, but 0 is the key! ( cannot use this one )
 * argv[ 4 ] ... row-limit for first SAM-file
 * argv[ 5 ] ... row-limit for second SAM-file
 * argv[ 6 ] ... verbose ( 0...off, 1...on )
 */

int main( int argc, char *argv[] ) {
    int res = 1;
    if ( argc > 3 ) {
        std::ifstream sam1( argv[ 1 ] );
        if ( sam1.is_open() ) {
            std::ifstream sam2( argv[ 2 ] );
            if ( sam2.is_open() ) {
                uint16_t col = tools::str_2_uint16_t( argv[ 3 ] );
                if ( col > 0 ) {
                    uint64_t row_limit1 = 0;
                    uint64_t row_limit2 = 0;                    
                    if ( argc > 4 ) row_limit1 = tools::str_2_uint64_t( argv[ 4 ] );
                    if ( argc > 5 ) row_limit2 = tools::str_2_uint64_t( argv[ 5 ] );
                    
                    values m1, m2;
                    fill_values( sam1, col, row_limit1, m1 );
                    fill_values( sam2, col, row_limit2, m2 );
                    
                    bool verbose = false;
                    if ( argc > 6 ) verbose = ( tools::str_2_uint8_t( argv[ 6 ] ) > 0 );
                    comp c( m1, m2, verbose );
                    if ( c.compare() ) res = 0;
                }
            }
        }
    }
    return res;
}
