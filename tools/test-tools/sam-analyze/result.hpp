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

    static std::string with_ths( unsigned long x ) {
        std::ostringstream ss;
        ss . imbue( std::locale( "" ) );
        ss << x;
        return ss.str();
    }
};

struct IMPORT_RESULT {
    bool success;
    unsigned long total_lines;
    unsigned long header_lines;
    unsigned long alignment_lines;

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
    typedef std::pair< unsigned long, unsigned long > t_long_pair;
    typedef std::vector< t_long_pair > t_long_pair_vec;
    
    bool success;
    unsigned long spot_count;
    unsigned long refs_in_use;
    unsigned long unaligned;
    unsigned long half_aligned;
    unsigned long fully_aligned;
    t_long_pair_vec spot_sizes;
    
    ANALYZE_RESULT() : 
        success( false ), spot_count( 0 ), refs_in_use( 0 ) {}

    void add_spot_size( unsigned long spot_size, unsigned long count ) {
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
    unsigned long header_lines;
    unsigned long alignment_lines;

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
