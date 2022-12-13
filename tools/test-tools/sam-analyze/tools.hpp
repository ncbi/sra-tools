#ifndef TOOLS_H
#define TOOLS_H

#include <iostream>
#include <sstream>
#include <bits/stdc++.h>

struct tools_t {
    static std::string yes_no( bool flag ) {
        if ( flag ) { return std::string( "YES" ); }
        return std::string( "NO" );
    }
    
    static std::string with_ths( uint64_t x ) {
        std::ostringstream ss;
        ss . imbue( std::locale( "" ) );
        ss << x;
        return ss.str();
    }
    
    static void reverse_string( std::string& s ) {
        size_t n = s . length();
        for ( size_t i = 0; i < n / 2; i++ )
            std::swap( s[ i ], s[ n - i - 1 ] ); 
    }
    
    static std::string reverse_complement( const std::string& s ) {
        std::string res( s );
        reverse_string( res );
        for ( char& c : res ) {
            switch( c ) {
                case 'A' : c = 'T'; break;
                case 'C' : c = 'G'; break;
                case 'G' : c = 'C'; break;
                case 'T' : c = 'A'; break;

                case 'a' : c = 't'; break;
                case 'c' : c = 'g'; break;
                case 'g' : c = 'c'; break;
                case 't' : c = 'a'; break;                
            }
        }
        return res;
    }
 
};

#endif
