#ifndef RESULT_H
#define RESULT_H

#include <iostream>
#include <sstream>
#include <vector>
#include <map>

struct BASE_RESULT {
    static std::string yes_no( bool flag ) {
        if ( flag ) { return std::string( "YES" ); }
        return std::string( "NO" );
    }

    static std::string with_ths( uint64_t x ) {
        std::ostringstream ss;
        ss . imbue( std::locale( "" ) );
        ss << x;
        return ss.str();
    }
};

struct IMPORT_RESULT {
    bool success;
    uint64_t total_lines;
    uint64_t header_lines;
    uint64_t alignment_lines;

    IMPORT_RESULT() : 
        success( false ), total_lines( 0 ), header_lines( 0 ), alignment_lines( 0 ) {}

    void report( void ) {
        std::cerr << "\tsuccess    : " << BASE_RESULT::yes_no( success ) << std::endl;            
        std::cerr << "\ttotal lines: " << BASE_RESULT::with_ths( total_lines ) << std::endl;
        std::cerr << "\theaders    : " << BASE_RESULT::with_ths( header_lines ) << std::endl;
        std::cerr << "\talignments : " << BASE_RESULT::with_ths( alignment_lines ) << std::endl;
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

struct FLAG_COUNTS {
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
    
    FLAG_COUNTS() : paired_in_seq( 0 ), propper_mapped( 0 ), unmapped( 0 ), mate_unmapped( 0 ),
        reversed( 0 ), mate_reversed( 0 ), first_in_pair( 0 ), second_in_pair( 0 ),
        not_primary( 0 ), bad_qual( 0 ), duplicate( 0 ) {}
    
    void report( void ) {
        std::cerr << "\tpaired in seq  : " << BASE_RESULT::with_ths( paired_in_seq ) << std::endl;
        std::cerr << "\tpropper mapped : " << BASE_RESULT::with_ths( propper_mapped ) << std::endl;
        std::cerr << "\tunmapped       : " << BASE_RESULT::with_ths( unmapped ) << std::endl;
        std::cerr << "\tmate unmapped  : " << BASE_RESULT::with_ths( mate_unmapped ) << std::endl;
        std::cerr << "\treversed       : " << BASE_RESULT::with_ths( reversed ) << std::endl;
        std::cerr << "\tmate reversed  : " << BASE_RESULT::with_ths( mate_reversed ) << std::endl;
        std::cerr << "\tfirst in pair  : " << BASE_RESULT::with_ths( first_in_pair ) << std::endl;
        std::cerr << "\tsecond in pair : " << BASE_RESULT::with_ths( second_in_pair ) << std::endl;
        std::cerr << "\tnot primary    : " << BASE_RESULT::with_ths( not_primary ) << std::endl;
        std::cerr << "\tbad quality    : " << BASE_RESULT::with_ths( bad_qual ) << std::endl;
        std::cerr << "\tduplicate      : " << BASE_RESULT::with_ths( duplicate ) << std::endl;
    }
};

struct ANALYZE_RESULT {
    bool success;
    uint64_t spot_count;
    uint64_t refs_in_use;
    uint64_t unaligned;
    uint64_t half_aligned;
    uint64_t fully_aligned;
    uint64_t flag_problems;
    FLAG_COUNTS flag_counts;
    counter_dict_t counter_dict;
    
    ANALYZE_RESULT() : 
        success( false ), spot_count( 0 ), refs_in_use( 0 ),
        unaligned( 0 ), half_aligned( 0 ), fully_aligned( 0 ), flag_problems( 0 ) {}

    void report( void ) {
        std::cerr << "\tsuccess    : " << BASE_RESULT::yes_no( success ) << std::endl;
        std::cerr << "\tspots      : " << BASE_RESULT::with_ths( spot_count ) << std::endl;
        std::cerr << "\tused refs  : " << BASE_RESULT::with_ths( refs_in_use ) << std::endl;
        std::cerr << "\tunalig.    : " << BASE_RESULT::with_ths( unaligned ) << std::endl;
        std::cerr << "\thalf alig. : " << BASE_RESULT::with_ths( half_aligned ) << std::endl;
        std::cerr << "\tfull alig. : " << BASE_RESULT::with_ths( fully_aligned ) << std::endl;
        std::cerr << "\tflag-probl.: " << BASE_RESULT::with_ths( flag_problems ) << std::endl;        
        flag_counts . report();
        uint64_t value, count;
        counter_dict . reset();
        while( counter_dict . next( value, count ) ) {
            std::cerr << "\tspots with " << value << " alignment";
            if ( value ) { std::cerr << "s"; }
            std::cerr << " : " << BASE_RESULT::with_ths( count ) << std::endl;
            
        }
    }
};

struct EXPORT_RESULT {
    bool success;
    uint64_t header_lines;
    uint64_t alignment_lines;

    EXPORT_RESULT() : 
        success( false ), header_lines( 0 ), alignment_lines( 0 ) {}

    void report( void ) {
        std::cerr << "\tsuccess    : " << BASE_RESULT::yes_no( success ) << std::endl;
        std::cerr << "\theaders    : " << BASE_RESULT::with_ths( header_lines ) << std::endl;
        std::cerr << "\talignments : " << BASE_RESULT::with_ths( alignment_lines ) << std::endl;
    }
};

#endif
