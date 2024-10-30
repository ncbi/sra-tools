#pragma once

#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <thread>
#include "../util/values.hpp"
#include "../util/file_deleter.hpp"
#include "../util/process.hpp"
#include "../util/file_rename.hpp"
#include "../util/file_diff.hpp"
#include "runner_ini.hpp"
#include "main_params.hpp"
#include "normalizer.hpp"

using namespace std;

namespace sra_convert {

// to be found in rnd2sra.cpp
bool run_tool( MainParamsPtr params );

class runner;
typedef std::shared_ptr< runner > runner_ptr;
class runner {
    private :
        const IniPtr f_ini;
        MainParamsPtr f_params;
        KV_Map_Ptr f_values;

        // >>>>> Ctor <<<<<
        runner( const IniPtr ini, MainParamsPtr params )
            : f_ini( ini ), f_params( params ), f_values( KV_Map::make() ) { }

/* -------------------------------------------------------------------------------------------- */
        bool compare_values( vector< string >& args, bool silent ) const {
            if ( args . size() < 2 ) { return false; }
            const string_view value1{ f_values -> get( args[ 0 ] ) };
            const string_view value2{ f_values -> get( args[ 1 ] ) };
            if ( !silent ) {
                cout << "cmp: >" << value1 << "< vs. >" << value2 << "<\n";
            }
            return 0 == value1 . compare( value2 );
        }

/* -------------------------------------------------------------------------------------------- */
        bool echo( vector< string >& args ) {
            string tmp;
            for ( auto& arg : args ) {
                if ( ! tmp . empty() ) { tmp += " "; }
                tmp += arg;
            }
            cout << tmp << endl;
            return true;
        }

/* -------------------------------------------------------------------------------------------- */
        bool sleep_ms( vector< string >& args ) {
            bool res = true;
            if ( ! args . empty() ) {
                try {
                    const int ms { stoi( args[ 0 ] ) };
                    this_thread::sleep_for( chrono::milliseconds( ms ) );
                }
                catch ( invalid_argument const& ex ) {
                    cerr << ex . what() << endl;
                    res = false;
                }
            }
            return res;
        }

/* -------------------------------------------------------------------------------------------- */
        bool normalize( vector< string >& args ) {
            if ( args . size() > 2 ) {
                Normalizer::run( args[ 0 ], args[ 1 ], args[ 2 ] );
            }
            return true;
        }

/* -------------------------------------------------------------------------------------------- */
        bool change_dir( vector< string >& args ) {
            bool res = args . size() > 0;
            if ( res ) {
                auto path = args[ 0 ];
                res = ! path . empty();
                if ( res ) {
                    error_code ec;
                    if ( ! fs::exists( path, ec ) ) {
                        res = fs::create_directories( path, ec );
                    }
                }
                if ( res ) {
                    fs::current_path( path );
                }
            }
            return res;
        }

/* -------------------------------------------------------------------------------------------- */
        bool run_sub( const runner_ini_ptr section_ini, vector< string >& args ) {
            // make a new MainParams-instance for the sub-process ( one level deeper )
            auto sub_params = MainParams::make( f_params -> get_sub_level() + 1 );

            // add a fake arg[ 0 ] for the sub-process
            sub_params -> add_arg( "rnd2sra" );

            // transfer values found in the section ( SECTION.values = xxxx ) to the sub-process
            sub_params -> add_values( section_ini -> get_values() );

            // transfer values found globaly in the ini-file
            sub_params -> add_values( f_values );

            // replace args starting with '$' with a value from f_values, if found in there
            if ( !args . empty() ) { sub_params -> add_args( args ); }

            // take the parent title and add the section-title
            // and use this as the parent title of the sub-process
            string title { f_params -> get_title() };
            if ( !title . empty() ) { title += '.'; }
            title += section_ini -> get_title();
            sub_params -> set_title( title );

            // let sub-params parse its args-vector
            sub_params -> parse_args();

            // if there is no special location the ini-file in the args
            // take it from the parent-params
            if ( sub_params -> get_ini_file_loc() . empty() ) {
                // if there is no path to the ini-file
                sub_params -> set_ini_file_loc( f_params -> get_ini_file_loc() );
            }

            // now run the sub-process
            return run_tool( sub_params );  // !!! recursive call back to rnd2sra.cpp !!!
        }

/* -------------------------------------------------------------------------------------------- */
        bool run_special( const runner_ini_ptr section_ini, const string_view& executable ) {
            bool res = true;
            bool silent = section_ini -> get_silent();
            bool ignore_err = section_ini -> get_ignore_err();
            auto args = f_values -> replace( section_ini -> get_args() );
            if ( executable . compare( ":values" ) == 0 ) {
                cout << f_values;
            } else if ( executable . compare( ":echo" ) == 0 ) {
                res = echo( args );
            } else if ( executable . compare( ":cmp" ) == 0 ) {
                res = compare_values( args, silent );
            } else if ( executable . compare( ":rm" ) == 0 ) {
                res = deleter::del( args, silent, ignore_err );
            } else if ( executable . compare( ":mv" ) == 0 ) {
                res = FileRename::move_files( args, ignore_err );
            } else if ( executable . compare( ":diff" ) == 0 ) {
                res = FileDiff::diff( args, ignore_err );
            } else if ( executable . compare( ":sleep" ) == 0 ) {
                res = sleep_ms( args );
            } else if ( executable . compare( ":norm" ) == 0 ) {
                res = normalize( args );
            } else if ( executable . compare( ":cd" ) == 0 ) {
                res = change_dir( args );
            } else if ( executable . compare( ":run" ) == 0 ) {
                res = run_sub( section_ini, args );
            } else {
                if ( section_ini -> get_silent() ) {
                    cout << "unknown: >" << executable << "<\n";
                }
                res = false;
            }
            return res;
        }

        bool run_process( const runner_ini_ptr section_ini, const string_view& executable ) {
            auto proc = process::make( f_values );  // pass in values from previous processes...
            // replace args starting with '$' with a value from f_values, if found in there
            proc -> set_exe( f_values -> replace( executable ) );
            proc -> set_args( f_values -> replace( section_ini -> get_args() ) );
            proc -> set_stdout_file( f_values -> replace( section_ini -> get_stdout() ) );
            proc -> set_stderr_file( f_values -> replace( section_ini -> get_stderr() ) );
            proc -> set_silent( section_ini -> get_silent() );
            return ( proc -> run() == EXIT_SUCCESS );
        }

        bool is_excluded( const string_view exclude, string_view title ) const {
            if ( exclude . empty() ) return false;
            string s_exclude{ exclude };    // we have to make a string out of it...
            // let's see if any of the tokens of exclude match in the title...
            bool found = false;
            auto tokens = StrTool::tokenize( s_exclude );
            for ( auto& item : tokens ) {
                if ( !found ) {
                    found = ( title . find( item ) != string::npos );
                }
            }
            return found;
        }

        void print_title( uint32_t id, uint32_t count, bool excl,
                          string_view executable,
                          string_view title,
                          string_view caption ) const {
            bool is_echo = StrTool::startsWith( executable, ":echo" );
            string prefix;
            for ( int i = 0; i < f_params -> get_sub_level(); ++i ) { prefix += "  "; }

            if ( is_echo ) {
                if ( !excl ) {
                    cout << prefix << "running : >" << title <<
                        "< [" << id << " of " << count << "] " << " ";
                }
            } else {
                if ( excl ) {
                    cout << prefix << "running : >" << title <<
                        "< [" << id << " of " << count << "] " << caption << " | excluded" << endl;
                } else {
                    cout << prefix << "running : >" << title <<
                        "< [" << id << " of " << count << "] " << caption << endl;
                }
            }
        }

        bool run_section( const string_view& section, uint32_t id, uint32_t count ) {
            auto section_ini = runner_ini::make( f_ini, section );
            string_view title{ f_params -> get_title() };
            bool excl = is_excluded( section_ini -> get_exclude(), title );
            string_view executable{ section_ini -> get_executable() };
            string caption{ f_values -> replace( section_ini -> get_caption() ) };

            print_title( id, count, excl, executable, title, caption );

            bool res = true;
            if ( !excl ) {
                if ( StrTool::startsWith( executable, ':' ) ) {
                    // 'special' commands like rm, cmp etc..
                    res = run_special( section_ini, executable );
                } else {
                    // external binary is run
                    res = run_process( section_ini, executable );
                }
                if ( !res ) {
                    cout << "err     : >" << title << " for " << caption << endl;
                }
            }
            return res;
        }

        bool tokenized( vector< string >& dst, const string& key ) const {
            bool res = f_ini -> has( key ); // breaks recursion
            if ( res ) {
                for ( const auto& item : StrTool::tokenize( f_ini -> get( key ) ) ) {
                    if ( !tokenized( dst, item ) ) {    // recursion!!!
                        dst . push_back( item );
                    }
                }
            }
            return res;
        }

    public:
        static runner_ptr make( const IniPtr ini, MainParamsPtr params ) {
            return runner_ptr( new runner( ini, params ) );
        }

        bool run( void ) {
            bool res = true;
            if ( f_ini -> get( "echo" ) == "yes" ) {
                cout << f_ini;
            }

            // transfer the values comming from the parent-script into the local values
            f_values -> import_values( f_params -> get_values() );

            // transfer the script-gobal values comming from the ini-file
            f_values -> import_values( f_ini -> get_values() );

            vector< string > sections;
            if ( tokenized( sections, "run" ) ) {
                // we have at least one "run" entry in the ini-file
                uint32_t count = sections . size();
                uint32_t id = 1;
                for ( const auto& section : sections ) {
                    if ( res ) { // breaks the sections on failure!
                        res = run_section( section, id++, count );
                    }
                }
            } else {
                // run a none-sections binary
                res = run_section( "1", 1, 1 );
            }
            return res;
        }

        friend auto operator<<( ostream& os, runner_ptr o ) -> ostream& {
            os << "INI:" << endl;
            os << o -> f_ini;
            os << o -> f_values;
            return os;
        }
};

}
