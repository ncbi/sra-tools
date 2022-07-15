#include <iostream>
#include <sstream>
#include "tools.hpp"

/* ================================================================== */
/* produce reference-lookup : name-start-end from stdin               */
/* ================================================================== */

struct refline_t {
    std::string name;
    uint64_t start;
    uint64_t len;
};

class ref_event : public tools::iter_func {
    public:
        ref_event( void ) : f_cur_pos( 1 ), f_refline( { "", 0, 0 } ) {}

        virtual void operator() ( uint64_t id, const std::string& line ) {
            try {
                std::istringstream iss( line );
                std::string name;
                uint32_t len = 0;
                
                iss >> name >> len;
                if ( ( !name.empty() ) && ( len > 0 ) ) {

                    if ( f_refline . name . empty() ) { f_refline . name = name; }
                    if ( f_refline . start == 0 ) { f_refline . start = f_cur_pos; }

                    if ( name != f_refline . name ) {
                        // switch to a new reference...
                        print_refline();
                        f_refline . name = name;
                        f_refline . start = f_cur_pos;
                        f_refline . len = 0;
                    }
                    f_cur_pos += 5000;
                    f_refline . len += len;
                }
                
            } catch ( ... ) {}
        }

        void print_refline( void ) {
            uint64_t end = f_refline.start + f_refline.len;
            std::cout << f_refline.name << "\t" << f_refline.start << "\t" << end << std::endl;    
        }
        
    private:
        uint64_t f_cur_pos;
        refline_t f_refline;
};

int main( int argc, char *argv[] ) {
    ref_event e;
    tools::for_each_line( std::cin, e );
    e.print_refline();
    return 0;
}
