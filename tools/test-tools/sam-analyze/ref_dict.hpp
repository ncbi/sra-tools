#ifndef REFLIST_H
#define REFLIST_H

#include <string>
#include <map>

class ref_dict_t {
    private :
        typedef std::map< std::string, uint64_t > dict_t;
        typedef dict_t::iterator dict_iter_t;
        
        bool at_start;
        dict_t dict;
        dict_iter_t dict_iter;
        
    public:
        ref_dict_t() : at_start( true ) {}
        
        void add( std::string& value ) {
            auto found = dict . find( value );
            if ( found == dict . end() ) {
                dict . insert( std::make_pair( value, 1 ) );
            } else {
                found -> second += 1;
            }
        }
        
        void reset( void ) { at_start = true; }

        size_t size( void ) const { return dict . size(); }

        bool next( std::string& value, uint64_t& count ) {
            if ( at_start ) {
                dict_iter = dict . begin();
                at_start = false;
            }
            bool res = ( dict_iter != dict . end() );
            if ( res ) {
                value = dict_iter -> first;
                count = dict_iter -> second;
                dict_iter ++;
            }
            return res;
        }
        
        bool contains( const std::string& name ) const {
            auto found = dict . find( name );
            bool res = ( found != dict . end() );
            return res;
        }
};

#endif
