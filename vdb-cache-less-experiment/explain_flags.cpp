#include <iostream>
#include <iomanip>

#include "tools.hpp"

void flag_bit( uint16_t flags, uint16_t mask, const char * txt ) {
    bool bit = ( flags & mask ) == mask;
    uint16_t v = bit ? 1 : 0;
    std::cout << std::setw( 20 ) << txt << ": " << v << std::endl;
}

void explain( uint16_t flags ) {
    std::cout << "FLAGS: " << flags << " - 0x" << std::hex << flags << std::dec << " :" << std::endl;
    flag_bit( flags, tools::FLAG_PAIRED, "PAIRED" );
    flag_bit( flags, tools::FLAG_PROPPER_ALIGNED, "PROPPER_ALIGNED" );
    flag_bit( flags, tools::FLAG_UNMAPPED, "UNMAPPED" );
    flag_bit( flags, tools::FLAG_MATE_UNMAPPED, "MATE_UNMAPPED" );
    flag_bit( flags, tools::FLAG_REVERSED, "REVERSED" );
    flag_bit( flags, tools::FLAG_MATE_REVERSED, "MATE_REVERSED" );
    flag_bit( flags, tools::FLAG_IS_READ1, "IS_READ1" );
    flag_bit( flags, tools::FLAG_IS_READ2, "IS_READ2" );
    flag_bit( flags, tools::FLAG_NOT_PRIMARY, "NOT_PRIMARY" );
    flag_bit( flags, tools::FLAG_QUAL_CHECK_FAILED, "QUAL_CHECK_FAILED" );
    flag_bit( flags, tools::FLAG_PCR_OR_DUP, "PCR_OR_DUP" );
    flag_bit( flags, tools::FLAG_SUPPLEMENTAL, "SUPPLEMENTAL" );
    std::cout << std::endl;    
}

int main( int argc, char *argv[] ) {
    if ( argc == 2 ) {
        explain( tools::str_2_uint16_t( argv[ 1 ] ) );
    } else if ( argc == 3 ) {
        explain( tools::str_2_uint16_t( argv[ 1 ] ) );
        explain( tools::str_2_uint16_t( argv[ 2 ] ) );
    }
    return 0;
}
