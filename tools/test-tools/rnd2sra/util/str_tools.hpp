#pragma once

#include <cstdint>
#include <cstdio>
#include <list>
#include <map>
#include <string_view>
#include <vector>
#include <string>
#include <sstream>
#include <cctype>
#include <algorithm>

using namespace std;

namespace sra_convert {

typedef multimap< const string, string > str2strmap;
typedef pair< const string, string > strpair;

class StrTool {
    public :

        static void ltrim( string &s ) {
            s . erase( s . begin(),
                       find_if( s . begin(),
                                s . end(),
                                []( unsigned char ch ) { return !::isspace( ch ); } ) );
        }

        static void rtrim( string &s ) {
            s . erase( find_if( s . rbegin(),
                                s . rend(),
                                [](unsigned char ch) { return !::isspace( ch ); } ) . base(),
                                s . end() );
        }

        static void trim_line( string& s ) {
            ltrim( s );
            rtrim( s );
        }

        static string trim( const string& s ) {
            string tmp( s );
            trim_line( tmp );
            return tmp;
        }

        static bool is_comment( const string_view& line ) {
            if ( startsWith( line, "/" ) ) return true;
            if ( startsWith( line, "#" ) ) return true;
            return false;
        }

        static bool startsWith( const string_view& s, const string& prefix ) {
            return ( s . rfind( prefix, 0 ) == 0 );
        }

        static bool startsWith( const string_view& s, char c ) {
            if ( s . empty() ) return false;
            return ( s . at( 0 ) == c );
        }

        static bool startsWith( const string_view& s, char c1, char c2 ) {
            if ( s . empty() ) return false;
            char c = s . at( 0 );
            return ( c == c1 ) || ( c == c2 );
        }

        static bool endsWith( const string_view &s, string const &postfix ) {
            if ( s . length() >= postfix . length() ) {
                return ( 0 == s . compare( s . length() - postfix . length(), postfix . length(), postfix ) );
            } else {
                return false;
            }
        }

        static bool endsWith( const string_view &s, char c ) {
            if ( s . empty() ) return false;
            return ( s . at( s . length() - 1 ) == c );
        }

        static bool endsWith( const string_view &s, char c1, char c2 ) {
            if ( s . empty() ) return false;
            char c = s . at( s . length() - 1 );
            return ( c == c1 ) || ( c == c2 );
        }

        static string unquote( const string_view& s, char c1, char c2 ) {
            if ( startsWith( s, c1, c2 ) ) {
                if ( endsWith( s, c1, c2 ) ) {
                    string tmp{ s };
                    return tmp . substr( 1, tmp . length() - 2 );
                }
            }
            return string( s );
        }

        static vector< string > tokenize( const string& s, char delimiter = ' ' ) {
            vector< string > res;
            stringstream ss( s );
            string token, temp;
            while ( getline( ss, token, delimiter ) ) {
                string trimmed = trim( token );
                if ( temp . empty() ) {
                    // we ARE NOT in a quoted list of tokens...
                    if ( startsWith( trimmed, '"', '\'' ) ) {
                        temp = trimmed;
                    } else {
                        res . push_back( trimmed );
                    }
                } else {
                    // we ARE in a quoted list of tokens...
                    temp += delimiter;
                    temp . append( trimmed );
                    if ( endsWith( trimmed, '"', '\'' ) ) {
                        res . push_back( temp );
                        temp . clear();
                    }
                }
            }
            if ( ! temp . empty() ) {
                res . push_back( trim( temp ) );
            }
            return res;
        }

        static void to_stream( ostream& os, const vector< string >& src, const string& prefix ) {
            uint32_t idx = 0;
            for ( auto& item : src ) {
                os << prefix << "[ " << idx++ << " ] = " << item << endl;
            }
        }

        template <typename T> static T convert( const string_view& s, T dflt ) {
            if ( s . empty() ) { return dflt; }
            stringstream ss;
            ss << s;
            T res;
            ss >> res;
            return res;
        }

}; // end of class StrTool

} // end of namespace sra_convert
