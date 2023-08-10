
#include "factory.hpp"

extern "C" {
    int KMain( int argc, char *argv[] ) {
        int res = 3;
        try {
            t_proglines proglines;
            t_progline::consume_lines( argc, argv, proglines );
            if ( !proglines.empty() ) {
                t_errors errors;
                t_factory factory( proglines, errors );
                res = factory . produce();
                errors . print();
            }
        } catch ( std::bad_alloc &e ) {
            std::cerr << "error: " << e.what() << std::endl;
        }
        return res;
    }
}
