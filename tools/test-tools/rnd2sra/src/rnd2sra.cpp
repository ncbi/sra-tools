#include "main_params.hpp"
#include "rnd2sra_main.hpp"
#include <cstdlib>

namespace sra_convert {

// this is in a separate function, because it is called as recursion from within runner.hpp
bool run_tool( MainParamsPtr params ) {
    // MainParams ... main_params.hpp
    // Tool_Main  ... rnd2sra_main.hpp
    Tool_MainPtr tool = Tool_Main::make( params );
    return tool -> run(); // rnd2sra_main.hpp
}

} // end of namespace sra_convert

int main( int argc, char* argv[] ) {

    sra_convert::MainParamsPtr params = sra_convert::MainParams::make( argc, ( const char ** )argv, 0 );
    if ( params -> is_help() ) {
        params -> print_help( cout );
        return EXIT_SUCCESS;
    } else {
        bool res = sra_convert::run_tool( params ); // above
        return res ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}
