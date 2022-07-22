#include <iostream>
#include <algorithm>

#include "tools.hpp"

/* extract the row-count of a row-range answer from vdb-dump
    vdb-dump $ACC -C PRIMARY_ALIGNMENT_ID -r
    ---> 'id-range: first-row = 1, row-count = 7,549,706'
    transform this into 7549706 on stdout
 */

class token_event : public tools::iter_func {
    public:
        token_event( void ) {}

        virtual void operator() ( uint64_t id, const std::string& s )
        {
            if ( 6 == id ) {
                auto token = s;
                /* remove thousand separator : */
                token.erase( std::remove( token.begin(), token.end(), ',' ), token.end() );
                std::cout << token << std::endl;
            }
        }
};

class line_event : public tools::iter_func {
    public:
        line_event( void ) {}

        virtual void operator() ( uint64_t id, const std::string& line )
        {
            if ( 0 == id ) {
                token_event e;
                tools::for_each_word( line, ' ', e );                
            }
        }
};

int main() {
    line_event e;
    tools::for_each_line( std::cin, e );
    return 0;
}
