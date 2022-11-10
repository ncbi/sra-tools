#ifndef RESULT_H
#define RESULT_H

#include <iostream>
#include <sstream>
#include <vector>

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
        std::cerr << "IMPORT:" << std::endl;
        std::cerr << "\tsuccess    : " << BASE_RESULT::yes_no( success ) << std::endl;            
        std::cerr << "\ttotal lines: " << BASE_RESULT::with_ths( total_lines ) << std::endl;
        std::cerr << "\theaders    : " << BASE_RESULT::with_ths( header_lines ) << std::endl;
        std::cerr << "\talignments : " << BASE_RESULT::with_ths( alignment_lines ) << std::endl;
    }
    
};

struct ANALYZE_RESULT {
    typedef std::pair< uint64_t, uint64_t > t_u64_pair;
    typedef std::vector< t_u64_pair > t_u64_pair_vec;
    
    bool success;
    uint64_t spot_count;
    uint64_t refs_in_use;
    uint64_t unaligned;
    uint64_t half_aligned;
    uint64_t fully_aligned;
    t_u64_pair_vec spot_sizes;
    
    ANALYZE_RESULT() : 
        success( false ), spot_count( 0 ), refs_in_use( 0 ) {}

        void add_spot_size( uint64_t spot_size, uint64_t count ) {
        spot_sizes . push_back( std::make_pair( spot_size, count ) );
    }
    
    void report( void ) {
        std::cerr << "ANALYZE:" << std::endl;
        std::cerr << "\tsuccess    : " << BASE_RESULT::yes_no( success ) << std::endl;
        std::cerr << "\tspots      : " << BASE_RESULT::with_ths( spot_count ) << std::endl;
        std::cerr << "\tused refs  : " << BASE_RESULT::with_ths( refs_in_use ) << std::endl;
        std::cerr << "\tunalig.    : " << BASE_RESULT::with_ths( unaligned ) << std::endl;
        std::cerr << "\thalf alig. : " << BASE_RESULT::with_ths( half_aligned ) << std::endl;
        std::cerr << "\tfull alig. : " << BASE_RESULT::with_ths( fully_aligned ) << std::endl;        
        for( auto & element : spot_sizes ) {
            std::cerr << "\tspots with " << element . second << " alignment";
            if ( element . second > 1 ) { std::cerr << "s"; }
            std::cerr << " : " << BASE_RESULT::with_ths( element . first ) << std::endl;
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
        std::cerr << "EXPORT:" << std::endl;
        std::cerr << "\tsuccess    : " << BASE_RESULT::yes_no( success ) << std::endl;
        std::cerr << "\theaders    : " << BASE_RESULT::with_ths( header_lines ) << std::endl;
        std::cerr << "\talignments : " << BASE_RESULT::with_ths( alignment_lines ) << std::endl;
    }
};

#endif
