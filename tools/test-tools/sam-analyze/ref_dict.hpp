#ifndef REFLIST_H
#define REFLIST_H

#include <string>
#include <map>
#include <vector>
#include <algorithm>

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

struct ref_freq_t {
    std::string name;
    uint64_t count;
};

class ref_by_freq_t {
    private :
        typedef std::vector< ref_freq_t > ref_freq_vec_t;
        typedef ref_freq_vec_t::iterator ref_freq_vec_iter_t;
        ref_freq_vec_t ref_freq_vec;
        ref_freq_vec_iter_t iter;
        
        static bool cmp( ref_freq_t& a, ref_freq_t& b ) {
            return a . count > b . count;
        }
        
    public :
        ref_by_freq_t( ref_dict_t& ref_dict ) {
            ref_freq_t entry;
            ref_dict . reset();
            while ( ref_dict . next( entry . name, entry . count ) ) {
                ref_freq_vec . push_back( entry );
            }
            sort( ref_freq_vec . begin(), ref_freq_vec . end(), cmp );
            reset();
        }
        
        void reset( void ) { iter = ref_freq_vec . begin(); }
        size_t size( void ) const { return ref_freq_vec . size(); }
  
        bool next( ref_freq_t& ref_freq ) {
            bool res = ( iter != ref_freq_vec . end() );
            if ( res ) {
                ref_freq = *iter;
                iter ++;
            }
            return res;
        }
  
};

#endif
