#pragma once

#include <memory>
#include "../util/ini.hpp"
#include "../util/values.hpp"

using namespace std;

namespace sra_convert {

class runner_ini;
typedef std::shared_ptr< runner_ini > runner_ini_ptr;
class runner_ini {
    private:
        KV_Map_Ptr f_values;
        string f_prefix;
        string f_executable;
        string f_title;
        string f_caption;
        string f_exclude;
        vector< string > f_args;
        string f_stdout;
        string f_stderr;
        bool f_silent;
        bool f_ignore_err;

        // >>>>> Ctor <<<<<
        runner_ini( const IniPtr ini, const string_view& prefix )
            : f_values( KV_Map::make() ), f_prefix( prefix ) {
            int idx = 0;
            auto key = prefixed( "exe" );
            for ( const auto& s : StrTool::tokenize( ini -> get( key ) ) ) {
                if ( idx++ == 0 ) {
                    f_executable = s;
                } else {
                    f_args . push_back( s );
                }
            }
            for ( const auto& s : ini -> get_definitions_of( prefixed( "args" ) ) ) {
                auto p = StrTool::tokenize( s );
                copy( p . begin(), p . end(), back_inserter( f_args ) );
            }

            f_values -> parse_values( ini -> get_definitions_of( prefixed( "values" ) ) );

            f_title = ini -> get( prefixed( "title" ), f_prefix );
            f_caption = ini -> get( prefixed( "cap" ), f_prefix );
            f_exclude = ini -> get( prefixed( "excl" ), f_prefix );
            f_stdout = ini -> get( prefixed( "stdout" ) );
            f_stderr = ini -> get( prefixed( "stderr" ) );
            f_silent = !ini -> has_value( prefixed( "echo" ), "yes" );
            f_ignore_err = ini -> has_value( prefixed( "ignore" ), "yes" );
        }

        string prefixed( const string& key ) const {
            if ( f_prefix . empty() ) { return key; }
            return f_prefix + "." + key;
        }

    public:
        static runner_ini_ptr make( const IniPtr ini, const string_view& prefix = "" ) {
            return runner_ini_ptr( new runner_ini( ini, prefix ) );
        }

        const string_view get_executable( void ) const { return f_executable; }
        const vector< string >& get_args( void ) const { return f_args; }
        const KV_Map_Ptr get_values( void ) const { return f_values; }
        const string_view get_title( void ) const { return f_title; }
        const string_view get_caption( void ) const { return f_caption; }
        const string_view get_exclude( void ) const { return f_exclude; }
        const string_view get_stdout( void ) const { return f_stdout; }
        const string_view get_stderr( void ) const { return f_stderr; }
        bool get_silent( void ) const { return f_silent; }
        bool get_ignore_err( void ) const { return f_ignore_err; }

        bool valid( void ) const { return !f_executable . empty(); }

        friend auto operator<<( ostream& os, runner_ini_ptr o ) -> ostream& {
            os << o -> prefixed( "title" ) << "   = " << o -> get_title() << endl;
            os << o -> prefixed( "capion" ) << "  = " << o -> get_caption() << endl;
            os << o -> prefixed( "exe" ) << "      = " << o -> get_executable() << endl;
            os << o -> prefixed( "excl" ) << "     = " << o -> get_exclude() << endl;
            uint32_t i = 0;
            for ( const auto& arg : o -> f_args ) {
                os << o -> prefixed( "arg" );
                os << "[ " << i++ << " ] = " << arg << endl;
            }
            os << o -> prefixed( "stdout" ) << "   = " << o -> get_stdout() << endl;
            os << o -> prefixed( "stderr" ) << "   = " << o -> get_stderr() << endl;
            os << o -> prefixed( "silent" ) << "   = " << o -> get_silent() << endl;
            os << o -> prefixed( "ignore" ) << "   = " << o -> get_ignore_err() << endl;
            return os;
        }

};

}
