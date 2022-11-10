#ifndef LINE_SPLIT_H
#define LINE_SPLIT_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

class FILE_READER {
    private :
        std::istream* data;
        uint64_t line_limit;
        uint64_t line_nr;
        bool stdin_mode;

    public:
        FILE_READER( const std::string& filename, uint64_t a_line_limit = 0 ) 
            : data( nullptr ), line_limit( a_line_limit ), line_nr( 0 ) {
            stdin_mode = ( filename == "stdin" );
            if ( stdin_mode ) {
                data = &std::cin;
            } else {
                data = new std::ifstream( filename );
            }
        }
        FILE_READER() = delete;  // no copy ctor!

        ~FILE_READER( void ) { if ( !stdin_mode ) { delete data; }
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

class FILE_WRITER {
    private :
        std::ostream* data;
        bool stdout_mode;

    public :
        FILE_WRITER( const std::string& filename ) : data( nullptr ) {
            stdout_mode = ( filename == "stdout" );
            if ( stdout_mode ) {
                data = &std::cout;
            } else {
                data = new std::ofstream( filename );
            }
        }
        FILE_WRITER() = delete; // not copy ctor!
        
        ~FILE_WRITER( void ) { if ( !stdout_mode ) { delete data; } }
        
        std::ostream* get( void ) { return data; }
};

class STRING_PARTS {
    private :
        char delim;
        std::vector< std::string > v;
        std::string empty;

    public:
        STRING_PARTS( char a_delim ) : delim( a_delim ) {}

        int split( const std::string& s ) {
            std::stringstream ss( s );
            std::string item;
            v . clear();
            while( getline( ss, item, delim ) ) {
                v . push_back( item );
            }
            return v.size();
        }
        
        size_t size( void ) const { return v . size(); }
        
        std::string& get( uint16_t idx ) {
            if ( idx > v . size() ) { return empty; }
            return v[ idx ];
        }
};

#endif
