#pragma once

#include <memory>
#include <iostream>
#include "str_tools.hpp"

using namespace std;

namespace sra_convert {

class KV_Map;
typedef std::shared_ptr< KV_Map > KV_Map_Ptr;
class KV_Map {
    private:
        str2strmap f_kv;

        // Ctor
        KV_Map( void ) {}

        string replace_string_items( const string_view src,
                                     const string_view pattern,
                                     const string_view replacement ) {
            string tmp{ src };
            bool done = false;
            while ( !done ) {
                auto pos = tmp . find( pattern );
                done = ( string::npos == pos );
                if ( !done ) {
                    tmp . replace( pos, pattern . length(), replacement );
                }
            }
            return tmp;
        }

    public:
        // Factory
        static KV_Map_Ptr make( void ) { return KV_Map_Ptr( new KV_Map() ); }

        bool empty( void ) const { return f_kv . empty(); }

        void insert( const string& key, const string& value ) {
            strpair p( key, value );
            f_kv .insert( p );
        }

        bool insert_tokens( const string& line, char separator ) {
            bool res = false;
            if ( string::npos != line . find( separator ) ) {
                auto tokens = StrTool::tokenize( line, separator );
                switch ( tokens . size() ) {
                    case 0  : break;

                    case 1  : insert( tokens[ 0 ], string( "" ) );
                              res = true;
                              break;

                    default : insert( tokens[ 0 ], tokens[ 1 ] );
                              res = true;
                              break;
                }
            }
            return res;
        }

        void parse_values( const vector< string >& src, char separator = ':' ) {
            for ( auto& item : src ) { insert_tokens( item, separator ); }
        }

        void import_values( KV_Map_Ptr other ) {
            for ( const auto& item : other -> f_kv ) {
                insert( item . first, item . second );
            }
        }

        const string& get( const string& key, const string& dflt = "" ) const {
            auto it = f_kv . find( key );
            if ( it == f_kv . end() ) { return dflt; }
            return it -> second;
        }

        bool has( const string& key ) const {
            return ( f_kv . find( key ) != f_kv . end() );
        }

        vector< string > get_values_of( const string& key ) const {
            vector< string > res;
            auto range = f_kv . equal_range( key );
            for ( auto i = range . first; i != range . second; ++i ) {
                res . push_back( i -> second );
            }
            return res;
        }

        string replace1( const string_view src, char key = '$' ) {
            string tmp{ src };
            for ( const auto& kv : f_kv ) {
                string pattern{ key };
                pattern += '{';
                pattern += kv . first;
                pattern += '}';
                tmp = replace_string_items( tmp, pattern, kv . second );
            }
            return tmp;
        }

        string replace2( const string_view src, char key = '$' ) {
            string tmp{ src };
            for ( const auto& kv : f_kv ) {
                string pattern{ key };
                pattern += kv . first;
                tmp = replace_string_items( tmp, pattern, kv . second );
            }
            return tmp;
        }

        string replace( const string_view src, char key = '$' ) {
            return replace2( replace1( src, key ), key );
        }

        vector< string > replace( const vector< string >& src, char key = '$' ) {
            vector< string > res;
            for ( auto& item : src ) { res . push_back( replace( item, key ) ) ; }
            return res;
        }

        void to_stream( ostream& os, const string& prefix ) {
            for ( auto& item : f_kv ) {
                os << prefix << "[ " << item . first << " ] = " << item . second << endl;
            }
        }

        friend auto operator<<( ostream& os, KV_Map_Ptr o ) -> ostream& {
            o -> to_stream( os, "" );
            return os;
        }
};

}
