
#ifndef helper_hdr
#define helper_hdr

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

const int FLAG_MULTI = 0x01;
const int FLAG_PROPPER = 0x02;
const int FLAG_UNMAPPED = 0x04;
const int FLAG_NEXT_UNMAPPED = 0x08;
const int FLAG_REVERSED = 0x010;
const int FLAG_NEXT_REVERSED = 0x020;
const int FLAG_FIRST = 0x040;
const int FLAG_LAST = 0x080;
const int FLAG_SECONDARY = 0x0100;
const int FLAG_BAD = 0x0200;
const int FLAG_PCR = 0x0400;

const std::string empty_string( "" );
const std::string tab_string( "\t" );
const std::string star_string( "*" );

bool is_numeric( const std::string &s );
int str_to_int( const std::string &s, int dflt );
bool str_to_bool( const std::string &s );

int random_int( int min, int max );
std::string random_bases( int length );
std::string random_quality( int length );
std::string random_div_quality( int length, int max_div );
std::string random_string( int length );
std::string pattern_quality( std::string &pattern, size_t len );

int string_to_flag( const std::string& s );

class t_errors {
    private :
        std::vector< std::string > errors;
        
    public :
        const bool empty( void ) const;
        void msg( const char * msg, const std::string &org );
        bool print( void ) const;
};

#endif
