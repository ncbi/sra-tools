#ifndef LINE_SPLIT_H
#define LINE_SPLIT_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

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

class string_parts_t {
    private :
        char delim;
        std::vector< std::string > v;
        std::string empty;

    public:
        string_parts_t( char a_delim ) : delim( a_delim ) {}

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
