#ifndef TOOLS_H
#define TOOLS_H

#include <iostream>
#include <sstream>
#include <bits/stdc++.h>
#include <map>

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
                
                case 'Y' : c = 'R'; break;
                case 'R' : c = 'Y'; break;
                case 'K' : c = 'M'; break;
                case 'M' : c = 'K'; break;

                case 'y' : c = 'r'; break;
                case 'r' : c = 'y'; break;
                case 'k' : c = 'k'; break;
                case 'm' : c = 'm'; break;
                
                case 'D' : c = 'H'; break;
                case 'V' : c = 'B'; break;
                case 'H' : c = 'D'; break;
                case 'B' : c = 'V'; break;

                case 'd' : c = 'h'; break;
                case 'v' : c = 'n'; break;
                case 'h' : c = 'd'; break;
                case 'b' : c = 'v'; break;
            }
        }
        return res;
    }
 
};

class json_doc_t {
    private :
        typedef std::map< std::string, uint64_t > str_u64_dict_t;
        typedef std::map< std::string, std::string > str_str_dict_t;

        str_u64_dict_t str_u64;
        str_str_dict_t str_str;
        
    public :
        json_doc_t() {}
        
        void add( const std::string& key, uint64_t value ) {
            str_u64 . insert( std::make_pair( key, value ) );
        }
        
        void add( const std::string& key, const std::string& value ) {
            str_str . insert( std::make_pair( key, value ) );            
        }

        void print( std::ostream * sink ) {
            bool print_comma = false;
            *sink << "{" << std::endl;
            for ( auto &entry : str_u64 ) {
                if ( print_comma ) { *sink << "," << std::endl; }
                *sink << "  \"" << entry.first << "\" : " << entry.second;
                print_comma = true;
            }
            for ( auto &entry : str_str ) {
                if ( print_comma ) { *sink << "," << std::endl; }
                *sink << "  \"" << entry.first << "\" : \"" << entry.second << "\"";
                print_comma = true;
            }
            if ( print_comma ) { *sink << std::endl; }
            *sink << "}" << std::endl;
        }
};

#endif
