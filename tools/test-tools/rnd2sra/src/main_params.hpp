#pragma once

#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <ostream>
#include "../util/str_tools.hpp"
#include "../util/values.hpp"
#include "../util/file_tool.hpp"

using namespace std;

namespace sra_convert {

/*
    * --ini INIFILE         -I INIFILE
    * --inidir INIDIR       -D INIDIR
    * --out OUTDIR          -O OUTDIR
    * --bin BINDIR          -B BINDIR
    * --testbin TESTBINDIR  -T TESTBINDIR
    * --filter FILTER
    * --help                -h HELP
*/

class CmdLineParser {
    private :
        uint32_t f_argnum;
        string f_prog;
        string f_ini_file;
        string f_ini_dir;
        string f_out_dir;
        string f_bin_dir;
        string f_testbin_dir;
        string f_filter;
        bool f_help;

        enum class State{ BASE, INI, INIDIR, OUT, BIN, TESTBIN, FILTER };

        State OnBase( const string& arg ) {
            if ( "--ini" == arg || "-I" == arg ) { return State::INI; }
            if ( "--inidir" == arg || "-D" == arg ) { return State::INIDIR; }
            if ( "--out" == arg || "-O" == arg ) { return State::OUT; }
            if ( "--bin" == arg || "-B" == arg ) { return State::BIN; }
            if ( "--testbin" == arg || "-T" == arg ) { return State::TESTBIN; }
            if ( "--filter" == arg || "-F" == arg ) { return State::FILTER; }
            if ( "--help" == arg || "-h" ==arg ) { f_help = true; return State::BASE; }
            switch ( f_argnum++ ) {
                case 0 : f_prog = arg; break;
                case 1 : f_ini_file = arg; break;
                case 2 : f_out_dir = arg; break;
                case 3 : f_bin_dir = arg; break;
            }
            return State::BASE;
        }

        State OnIni( const string& arg ) {
            f_ini_file = arg;
            return State::BASE;
        }

        State OnIniDir( const string& arg ) {
            f_ini_dir = arg;
            return State::BASE;
        }

        State OnOut( const string& arg ) {
            f_out_dir = arg;
            return State::BASE;
        }

        State OnBin( const string& arg ) {
            f_bin_dir = arg;
            return State::BASE;
        }

        State OnTestBin( const string& arg ) {
            f_testbin_dir = arg;
            return State::BASE;
        }

        State OnFilter( const string& arg ) {
            f_filter = arg;
            return State::BASE;
        }

    public :
        CmdLineParser( const vector< string >& args ) : f_argnum( 0 ) {
            State state{ State::BASE };
            f_help = false;
            // a little state-machine to capture the values...
            for ( auto& item : args ) {
                switch ( state ) {
                    case State::BASE    : state = OnBase( item ); break;
                    case State::INI     : state = OnIni( item ); break;
                    case State::INIDIR  : state = OnIniDir( item ); break;
                    case State::OUT     : state = OnOut( item ); break;
                    case State::BIN     : state = OnBin( item ); break;
                    case State::TESTBIN : state = OnTestBin( item ); break;
                    case State::FILTER  : state = OnFilter( item ); break;
                }
            }
            if ( f_ini_dir . empty() ) {
                f_ini_dir = FileTool::location( f_ini_file );
            }
            if ( f_bin_dir . empty() ) {
                f_bin_dir = FileTool::location( f_prog );
            }
        }

        const string& prog( void ) const { return f_prog; }
        const string& ini_file( void ) const { return f_ini_file; }
        const string& ini_dir( void ) const { return f_ini_dir; }
        const string& out_dir( void ) const { return f_out_dir; }
        const string& bin_dir( void ) const { return f_bin_dir; }
        const string& testbin_dir( void ) const { return f_testbin_dir; }
        const string& filter( void ) const { return f_filter; }
        const bool is_help( void ) const { return f_help; }
};

class MainParams;
typedef std::shared_ptr< MainParams > MainParamsPtr;
class MainParams {
    private:
        int f_sub_level;
        KV_Map_Ptr f_values;
        vector< string > f_args;
        string f_title;
        string f_ini_file;
        string f_ini_file_loc;
        string f_bin_loc;
        string f_ini_loc;
        string f_output;
        bool f_help;

        // Ctor(s)
        MainParams( int argc, const char** argv, int sub_level )
            : f_sub_level( sub_level ), f_values( KV_Map::make() ) {
                for ( int idx = 0; idx < argc; ++idx ) {
                    f_args . push_back( string( argv[ idx ]) );
                }
                parse_args();
        }

        MainParams( int sub_level )
            : f_sub_level( sub_level ), f_values( KV_Map::make() ) { }

        void set_output( const string& value ) {
            if ( !value . empty() ) {
                f_output = value + "/";
            }
            auto out = f_values -> get_values_of( "OUT" );
            if ( out . empty() ) { f_values -> insert( "OUT", f_output ); }
        }

    public :
        // Factory(s)
        static MainParamsPtr make( int argc, const char** argv, int sub_level ) {
            return MainParamsPtr( new MainParams( argc, argv, sub_level ) );
        }

        static MainParamsPtr make( int sub_level ) {
            return MainParamsPtr( new MainParams( sub_level ) );
        }

        const string& get_title( void ) const { return f_title; }
        void set_title( const string& value ) { f_title = value; }

        const string& get_ini_file_loc( void ) const { return f_ini_file_loc; }
        void set_ini_file_loc( const string& value ) {
            f_ini_file_loc = value;
            if ( ! value . empty() ) {
                f_ini_file = value + '/' + f_ini_file;
            }
        }

        const string& get_bin_loc( void ) const { return f_bin_loc; }
        void set_bin_loc( const string& value ) {
            if ( !value . empty() ) {
                f_bin_loc = value + "/";
                auto bin = f_values -> get_values_of( "BIN" );
                if ( bin . empty() ) { f_values -> insert( "BIN", f_bin_loc ); }
            }
        }

        const string& get_ini_file( void ) const { return f_ini_file; }

        const string& get_output( void ) const { return f_output; }

        int get_sub_level( void ) const { return f_sub_level; }

        void add_arg( const string_view& value ) {
            f_args . push_back( string( value ) );
        }

        void add_args( const vector< string >& args ) {
            for ( auto& item : args ) { f_args . push_back( item ); }
        }

        const KV_Map_Ptr get_values( void ) const { return f_values; }
        void add_values( const KV_Map_Ptr src ) {
            f_values -> import_values( src );
        }

        const bool is_help( void ) const { return f_help; }

        void parse_args( void ) {
            CmdLineParser parser( f_args );
            f_ini_file = parser . ini_file();
            f_ini_file_loc = parser . ini_dir();
            f_help = parser . is_help();
            set_output( parser . out_dir() );
            set_bin_loc( parser . bin_dir() );
        }

        void print_help( ostream& os ) const {
            os << "--ini INIFILE         -I INIFILE     (abs. or rel. path of ini-file)\n";
            os << "--inidir INIDIR       -D INIDIR      (location of ini-file)\n";
            os << "--out OUTDIR          -O OUTDIR      (where to produce sra-output)\n";
            os << "--bin BINDIR          -B BINDIR      (where are binaries to run)\n";
            os << "--testbin TESTBINDIR  -T TESTBINDIR  (where are test-binaries)\n";
            os << "--filter FILTER                      (what tests to run if filtering)\n";
            os << "--help                -h\n";
        }

        friend auto operator<<( ostream& os, MainParamsPtr o ) -> ostream& {
            os << "MainParams:\n";
            os << "sub    : " << o -> f_sub_level << endl;
            os << "args   :\n";
            StrTool::to_stream( os, o -> f_args, "   " );
            os << "values :\n";
            o -> f_values -> to_stream( os, "   " );
            return os;
        }
};

}
