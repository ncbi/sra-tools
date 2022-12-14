#ifndef FILE_READER_H
#define FILE_READER_H

#include <iostream>
#include <fstream>

class file_reader_t {
private :
    std::istream* data;
    uint64_t line_limit;
    uint64_t line_nr;
    bool stdin_mode;
    
public:
    file_reader_t( const std::string& filename, uint64_t a_line_limit = 0 ) 
    : data( nullptr ), line_limit( a_line_limit ), line_nr( 0 ) {
        stdin_mode = ( filename == "stdin" );
        if ( stdin_mode ) {
            data = &std::cin;
        } else {
            data = new std::ifstream( filename );
        }
    }
    file_reader_t() = delete;  // no copy ctor!
    
    ~file_reader_t( void ) { if ( !stdin_mode ) { delete data; }
    }
    
    uint64_t get_line_nr( void ) const { return line_nr; }
    
    bool next( std::string& line ) {
        bool res = data -> good();
        if ( res && ( line_limit > 0 ) ){
            res = ( line_nr <= line_limit );
        }
        if ( res ) {
            try {
                std::getline( *data, line );
                res = data -> good();
            } catch( const std::exception& e ) {
                res = false;
            }
        }
        if ( res ) { line_nr += 1; }
        return res;
    }
};

#endif
