#ifndef FILE_WRITER_H
#define FILE_WRITER_H

#include <iostream>
#include <fstream>

class file_writer_t {
private :
    std::ostream* data;
    bool stdout_mode;
    
public :
    file_writer_t( const std::string& filename ) : data( nullptr ) {
        stdout_mode = ( filename == "stdout" );
        if ( stdout_mode ) {
            data = &std::cout;
        } else {
            data = new std::ofstream( filename );
        }
    }
    file_writer_t() = delete; // not copy ctor!
    
    ~file_writer_t( void ) { if ( !stdout_mode ) { delete data; } }
    
    std::ostream* get( void ) { return data; }
};

#endif
