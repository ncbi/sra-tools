#pragma once

#include <cstdint>
#include <cstdio>
#include <memory>
#include <list>
#include <map>
#include <vector>
#include <sstream>
#include <fstream>
#include <cctype>
#include <algorithm>
#include "str_tools.hpp"
#include "values.hpp"

using namespace std;

namespace sra_convert {

class Ini;
typedef std::shared_ptr< Ini > IniPtr;
class Ini {
    private:
        KV_Map_Ptr f_values;
        KV_Map_Ptr f_definitions;

        // Ctor
        Ini( void ) : f_values( KV_Map::make() ), f_definitions( KV_Map::make() ) { }

        void load_line( const string& line ) {
            size_t colon_pos = line . find( ':' );
            size_t equ_pos = line . find( '=' );
            if ( colon_pos != string::npos ) {
                // we do have a colon
                if ( equ_pos != string::npos ) {
                    // we have a colon and a equ
                    if ( colon_pos < equ_pos ) {
                        // colon comes first :
                        f_values -> insert_tokens( line, ':' );
                    } else {
                        // equ comes first :
                        f_definitions -> insert_tokens( line, '=' );
                    }
                } else {
                    // we only have a colon
                    f_values -> insert_tokens( line, ':' );
                }
            } else {
                // we do NOT have a colon
                if ( equ_pos != string::npos ) {
                    // we only have a equ
                    f_definitions -> insert_tokens( line, '=' );
                } else {
                    // we have neither a colon nor a equ
                }
            }
        }

        void load( const string_view& filename ) {
            string fn{ filename };
            ifstream f( fn );
            if ( f ) {
                string line;
                while ( getline( f, line ) ) {
                    StrTool::trim_line( line );
                    if ( ! StrTool::is_comment( line ) ) {
                        load_line( line );
                    }
                }
            }
        }

    public:
        static IniPtr make( void ) { return IniPtr( new Ini ); }

        static IniPtr make( const string_view& filename ) {
            auto res = IniPtr( new Ini );
            res -> load( filename );
            return res;
        }

        KV_Map_Ptr get_values( void ) const { return f_values; }

        vector< string > get_definitions_of( const string& key ) const {
            return f_definitions -> get_values_of( key );
        }

        const string& get( const string& key, const string& dflt = "" ) const {
            return f_definitions -> get( key, dflt );
        }

        void insert_definition( const string& key, const string& value ) {
            f_definitions -> insert( key, value );
        }

        bool has( const string& key ) const { return f_definitions -> has( key ); }

        bool has_value( const string& key, const string& value ) const {
            return ( 0 == get( key ) . compare( value ) );
        }

        uint8_t get_u8( const string& key, uint8_t dflt ) const {
            return StrTool::convert< uint8_t >( get( key, "" ), dflt );
        }

        int8_t get_i8( const string& key, int8_t dflt ) const {
            return StrTool::convert< int8_t >( get( key, "" ), dflt );
        }

        uint16_t get_u16( const string& key, uint16_t dflt ) const {
            return StrTool::convert< uint16_t >( get( key, "" ), dflt );
        }

        int16_t get_i16( const string& key, int16_t dflt ) const {
            return StrTool::convert< int16_t >( get( key, "" ), dflt );
        }

        uint32_t get_u32( const string& key, uint32_t dflt ) const {
            return StrTool::convert< uint32_t >( get( key, "" ), dflt );
        }

        int32_t get_i32( const string& key, int32_t dflt ) const {
            return StrTool::convert< int32_t >( get( key, "" ), dflt );
        }

        uint64_t get_u64( const string& key, uint64_t dflt ) const {
            return StrTool::convert< uint64_t >( get( key, "" ), dflt );
        }

        int64_t get_i64( const string& key, int64_t dflt ) const {
            return StrTool::convert< int64_t >( get( key, "" ), dflt );
        }

        friend auto operator<<( ostream& os, IniPtr const& o ) -> ostream& {
            os << "DEFS : \n" << o -> f_definitions;
            os << "VALS : \n" << o -> f_values;
            return os;
        }
}; // end of class Ini

} // end of namespace sra_convert
