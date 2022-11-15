#include <iostream>

#include "mmap.hpp"
#include "tools.hpp"

int main( int argc, char *argv[] ) {
    if ( argc > 2 ) {
        std::string qual_filename = tools::make_filename( argv[ 1 ], "qual" );
        std::string qual_idx_filename = tools::make_filename( argv[ 1 ], "qual.idx" );
        uint64_t id = tools::str_2_uint64_t( argv[ 2 ] );
        std::cout << "id: " << id << std::endl;
        MemoryMapped qual( qual_filename.c_str(), 0 );
        if ( qual.isValid() ) {
            MemoryMapped qual_idx( qual_idx_filename.c_str(), 0 );
            if ( qual_idx.isValid() ) {
                uint64_t qual_ofs = qual_idx[ id ];
                std::cout << "qual_of : " << qual_ofs << std::endl;

                uint16_t q_len = *( qual . u8_ptr( qual_ofs ) );
                uint16_t q_len_h = *( qual . u8_ptr( qual_ofs + 1 ) );
                q_len += q_len_h << 8;
                std::cout << "q-len : " << q_len << std::endl;

                char * qual_ptr = ( char * )qual . u8_ptr( qual_ofs + 2 );
                std::string q( qual_ptr, q_len );

                std::cout << "qual : " << q << std::endl;
            }
        }
    }
    return 1;
}
