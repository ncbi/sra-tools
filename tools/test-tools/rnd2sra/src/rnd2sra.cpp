#include "main_params.hpp"
#include "platform.hpp"
#include "rnd2sra_main.hpp"
#include <cstdlib>
#include "../vdb/kapp.hpp"

namespace sra_convert {

static bool check_ini_file( const string& ini_file_name ) {
    if ( ini_file_name == "stdin" ) return true;
    return FileTool::exists( ini_file_name );
}

// this is in a separate function, because it is called as recursion from within runner.hpp
bool run_tool( MainParamsPtr params ) {
    // MainParams ... main_params.hpp
    // Tool_Main  ... rnd2sra_main.hpp
    string ini_file_name = params -> get_ini_file();
    if ( ini_file_name . empty() ) {
        // without a config/ini - file we cannot do anything ...
        cerr << "missing: config/ini-file" << endl;
        return false;
    } else if ( ! check_ini_file( ini_file_name ) ) {
        // there is a config-file specified, but it cannot be found
        cerr << "config-file '" << ini_file_name << "' not found" << endl;
        cerr << "ini-dir: " << params -> get_ini_file_loc() << endl;
        return false;
    }
    // read the given INI-file init an instance if the Ini-class ( ../util/ini.hpp )
    IniPtr ini = Ini::make( ini_file_name );
    // use the commandline-parameters and the ini-insance to create an instance
    // of the Tool_Main-class
    Tool_MainPtr tool = Tool_Main::make( params, ini );
    // let the Tool_Main-instance run
    return tool -> run(); // rnd2sra_main.hpp
}

} // end of namespace sra_convert

rc_t rnd2sra_usage( const Args * args ) {
    sra_convert::TheHelp::print_help( cout );
    return 0;
}

rc_t rnd2sra_usage_summary( const char * prog_name ) {
    sra_convert::TheHelp::print_usage( cout );
    return 0;
}

int main( int argc, char* argv[] ) {

    auto app = VDB::Application( argc, argv, "-" );

    rc_t rc = app . HandleStandardOptions( rnd2sra_usage, rnd2sra_usage_summary );
    if ( 0 != rc ) { return 3; }

    int app_argc = app . getArgC();
    const char** app_argv = ( const char ** ) app . getArgV();

    sra_convert::MainParamsPtr params = sra_convert::MainParams::make( app_argc, app_argv, 0 );
    if ( app_argc == 1 ) {
        sra_convert::TheHelp::print_usage( cout );
        return EXIT_SUCCESS;
    } else if ( params -> print_platforms() ) {
        /* -platforms | -p | print all platforms, that can be used in the INI-file(s) */
        cout << sra_convert::Platform::list_all();
        return EXIT_SUCCESS;
    } else {
        bool res = sra_convert::run_tool( params ); // above
        return res ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}
