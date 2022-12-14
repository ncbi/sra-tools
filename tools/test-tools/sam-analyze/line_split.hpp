#ifndef LINE_SPLIT_H
#define LINE_SPLIT_H

#include <sstream>
#include <vector>

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
