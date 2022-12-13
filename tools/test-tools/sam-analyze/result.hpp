#ifndef RESULT_H
#define RESULT_H

#include <iostream>
#include <sstream>
#include <vector>
#include <map>

#include "tools.hpp"
#include "alig.hpp"

struct import_result_t {
    bool success;
    uint64_t total_lines;
    uint64_t header_lines;
    uint64_t alignment_lines;

    import_result_t() : 
        success( false ), total_lines( 0 ), header_lines( 0 ), alignment_lines( 0 ) {}

    void report( void ) {
        std::cerr << "\ttotal lines: " << tools_t::with_ths( total_lines ) << std::endl;
        std::cerr << "\theaders    : " << tools_t::with_ths( header_lines ) << std::endl;
        std::cerr << "\talignments : " << tools_t::with_ths( alignment_lines ) << std::endl;
    }
    
};

class counter_dict_t {
    private :
        typedef std::map< uint64_t, uint64_t > dict_t;
        typedef dict_t::iterator dict_iter_t;

        bool at_start;
        dict_t dict;
        dict_iter_t dict_iter;

    public:
        counter_dict_t() : at_start( true ) {}
        
        void add( uint64_t value ) {
            auto found = dict . find( value );
            if ( found == dict . end() ) {
                dict . insert( std::make_pair( value, 1 ) );
            } else {
                found -> second += 1;
            }
        }

        void reset( void ) { at_start = true; }
        
        bool next( uint64_t& value, uint64_t& count ) {
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
};

struct flag_count_t {
    uint64_t paired_in_seq;
    uint64_t propper_mapped;
    uint64_t unmapped;
    uint64_t mate_unmapped;
    uint64_t reversed;
    uint64_t mate_reversed;
    uint64_t first_in_pair;
    uint64_t second_in_pair;
    uint64_t not_primary;
    uint64_t bad_qual;
    uint64_t duplicate;
    
    flag_count_t() : paired_in_seq( 0 ), propper_mapped( 0 ), unmapped( 0 ), mate_unmapped( 0 ),
        reversed( 0 ), mate_reversed( 0 ), first_in_pair( 0 ), second_in_pair( 0 ),
        not_primary( 0 ), bad_qual( 0 ), duplicate( 0 ) {}
    
    void report( void ) {
        std::cerr << "\tpaired in seq  : " << tools_t::with_ths( paired_in_seq ) << std::endl;
        std::cerr << "\tpropper mapped : " << tools_t::with_ths( propper_mapped ) << std::endl;
        std::cerr << "\tunmapped       : " << tools_t::with_ths( unmapped ) << std::endl;
        std::cerr << "\tmate unmapped  : " << tools_t::with_ths( mate_unmapped ) << std::endl;
        std::cerr << "\treversed       : " << tools_t::with_ths( reversed ) << std::endl;
        std::cerr << "\tmate reversed  : " << tools_t::with_ths( mate_reversed ) << std::endl;
        std::cerr << "\tfirst in pair  : " << tools_t::with_ths( first_in_pair ) << std::endl;
        std::cerr << "\tsecond in pair : " << tools_t::with_ths( second_in_pair ) << std::endl;
        std::cerr << "\tnot primary    : " << tools_t::with_ths( not_primary ) << std::endl;
        std::cerr << "\tbad quality    : " << tools_t::with_ths( bad_qual ) << std::endl;
        std::cerr << "\tduplicate      : " << tools_t::with_ths( duplicate ) << std::endl;
    }
};

struct analyze_result_t {
    bool success;
    uint64_t spot_count;
    uint64_t refs_in_use;
    uint64_t unaligned;
    uint64_t half_aligned;
    uint64_t fully_aligned;
    uint64_t flag_problems;
    flag_count_t flag_counts;
    counter_dict_t counter_dict;
    IUPAC_counts_t counts;
    
    analyze_result_t() : 
        success( false ), spot_count( 0 ), refs_in_use( 0 ),
        unaligned( 0 ), half_aligned( 0 ), fully_aligned( 0 ), flag_problems( 0 ) {}

    void report( bool produce_fingerprint ) {
        std::cerr << "\tspots      : " << tools_t::with_ths( spot_count ) << std::endl;
        std::cerr << "\tused refs  : " << tools_t::with_ths( refs_in_use ) << std::endl;
        std::cerr << "\tunalig.    : " << tools_t::with_ths( unaligned ) << std::endl;
        std::cerr << "\thalf alig. : " << tools_t::with_ths( half_aligned ) << std::endl;
        std::cerr << "\tfull alig. : " << tools_t::with_ths( fully_aligned ) << std::endl;
        std::cerr << "\tflag-probl.: " << tools_t::with_ths( flag_problems ) << std::endl;
        flag_counts . report();
        uint64_t value, count;
        counter_dict . reset();
        while( counter_dict . next( value, count ) ) {
            std::cerr << "\tspots with " << value << " alignment";
            if ( value ) { std::cerr << "s"; }
            std::cerr << " : " << tools_t::with_ths( count ) << std::endl;
            
        }
        counts . report( produce_fingerprint );
    }
};

struct export_result_t {
    bool success;
    uint64_t header_lines;
    uint64_t alignment_lines;

    export_result_t() : 
        success( false ), header_lines( 0 ), alignment_lines( 0 ) {}

    void report( void ) {
        std::cerr << "\theaders    : " << tools_t::with_ths( header_lines ) << std::endl;
        std::cerr << "\talignments : " << tools_t::with_ths( alignment_lines ) << std::endl;
    }
};

#endif
