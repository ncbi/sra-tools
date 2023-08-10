#include <string>
#include <sstream>
#include <cstring>

#include "helper.hpp"

bool is_numeric( const std::string &s ) {
    if ( s.empty() ) return false;
    return s.find_first_not_of( "0123456789" ) == std::string::npos;
}

int str_to_int( const std::string &s, int dflt ) {
    if ( is_numeric( s ) ) {
        std::stringstream ss;
        ss << s;
        int res;
        ss >> res;
        return res;
    }
    return dflt;
}

#ifdef _MSC_VER 
//not #if defined(_WIN32) || defined(_WIN64) because we have strncasecmp in mingw
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

bool str_to_bool( const std::string &s ) {
    return( strcasecmp ( s.c_str (), "true" ) == 0 ||
    strcasecmp ( s.c_str (), "yes" ) == 0 ||
    atoi( s.c_str() ) != 0 );
}

int random_int( int min, int max ) {
    int spread = max - min;
    return min + rand() % spread;
}

std::string random_chars( int length, const std::string &charset ) {
    std::string result;
    result.resize( length );
    
    for ( int i = 0; i < length; i++ ) {
        result[ i ] = charset[ rand() % charset.length() ];
    }
    return result;
}

static std::string bases_charset = "ACGTN";
std::string random_bases( int length ) { return random_chars( length, bases_charset ); }

static std::string qual_charset = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
static char min_qual_char = '!';
static char max_qual_char = '~';

std::string random_quality( int length ) { return random_chars( length, qual_charset ); }

std::string random_div_quality( int length, int max_div ) {
    std::string res;
    if ( length > 0 ) {
        res = random_chars( 1, qual_charset );
        if ( length > 1 ) {
            for ( int i = 1; i < length; i++  ) {
                int diff = random_int( -max_div, max_div );
                char c = res[ i - 1 ] + diff;
                if ( c < min_qual_char ) {
                    c = min_qual_char;
                } else if ( c > max_qual_char ) {
                    c = max_qual_char;
                }
                res += c;
            }
        }
    }
    return res;
}

static std::string string_charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
std::string random_string( int length ) { return random_chars( length, string_charset ); }

std::string pattern_quality( std::string &pattern, size_t len ) {
    std::string res;
    while ( res.length() < len ) { res += pattern; }
    if ( res.length() > len ) { res = res.substr( 0, len ); }
    return res;
}

/* -----------------------------------------------------------------
 * a list of errors
 * ----------------------------------------------------------------- */

const bool t_errors::empty( void ) const { return errors.empty(); }
    
void t_errors::msg( const char * msg, const std::string &org ) {
    std::stringstream ss;
    ss << msg << org;
    errors . push_back( ss.str() );
}
    
bool t_errors::print( void ) const {
    for ( const std::string &err : errors ) {
        std::cerr << err << std::endl;
    }
    return ( errors.size() == 0 );
}
