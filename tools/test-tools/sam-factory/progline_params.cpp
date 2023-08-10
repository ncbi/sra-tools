
#include "progline_params.hpp"

const std::string empty_string( "" );

void t_params::split_kv( const std::string &kv ) {
    int eq_pos = kv.find( '=' );
    if ( eq_pos != -1 ) {
        std::string key = kv.substr( 0, eq_pos );
        std::transform( key.begin(), key.end(), key.begin(), ::tolower);
        std::string value = kv.substr( eq_pos + 1, kv.length() );
        values.insert( { key, value } );
    } else {
        values.insert( { kv, empty_string } );
    }
}
    
t_params::t_params( const std::string &line ) : values( {} ) {
    int colon_pos = line.find( ':' );
    if ( colon_pos != -1 ) {
        std::string rem = line.substr( colon_pos + 1, line.length() );
        int start = 0;
        int end = rem.find( ',' );
        while ( end != -1 ) {
            std::string param = rem.substr( start, end - start );
            split_kv( param );
            start = end + 1;
            end = rem.find( ',', start );
        }
        std::string param = rem.substr( start, end - start );
        split_kv( param );
    }
}
    
t_params_ptr t_params::make( const std::string &line ) {
    return std::make_shared< t_params >( t_params( line ) );
}
    
const std::string& t_params::get_string( const std::string& dflt ) {
    auto entry = values . begin();
    if ( entry != values . end() ) { return entry -> first; }
    return dflt;
}
    
const std::string& t_params::get_string_key( const std::string& key,
                                             const std::string& dflt ) const {
    auto found = values . find( key );
    if ( found != values . end() ) { return found -> second; }
    return dflt;
}
