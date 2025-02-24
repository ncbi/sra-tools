#include <cstdlib>
#include <iostream>
#include <memory>

#include "../util/utils.hpp"

int main( int argc, char* argv[] ) {

    std::cout << "testing rnd2sra\n";

    const fs::path to_iterate{ argv[ 1 ] };
    auto iter = util::dir_iter::make( to_iterate );
    for( auto e : util::file_iter_range( iter ) ) {
        std::cout << e . value() . path() . string() << std::endl;
    };

    return EXIT_SUCCESS;
}
