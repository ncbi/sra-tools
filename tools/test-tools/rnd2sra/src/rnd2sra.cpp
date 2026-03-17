#include "main_params.hpp"
#include "rnd2sra_main.hpp"
#include <cstdlib>

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
    IniPtr ini = Ini::make( ini_file_name );
    Tool_MainPtr tool = Tool_Main::make( params, ini );
    return tool -> run(); // rnd2sra_main.hpp
}

} // end of namespace sra_convert

int main( int argc, char* argv[] ) {

    sra_convert::MainParamsPtr params = sra_convert::MainParams::make( argc, ( const char ** )argv, 0 );
    if ( params -> print_help() ) {
        params -> print_help( cout );
        return EXIT_SUCCESS;
    } else if ( params -> print_version() ) {
        params -> print_version( cout );
        return EXIT_SUCCESS;
    } else if ( params -> print_platforms() ) {
        cout << sra_convert::Platform::list_all();
        return EXIT_SUCCESS;
    } else {
        bool res = sra_convert::run_tool( params ); // above
        return res ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}
