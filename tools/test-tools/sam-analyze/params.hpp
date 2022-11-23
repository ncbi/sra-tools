#ifndef PARAMS_H
#define PARAMS_H

#include <fstream>
#include "args.hpp"
#include "alig.hpp"
#include "sam_db.hpp"

struct base_params_t {
    static std::string yes_no( bool flag ) {
        if ( flag ) { return std::string( "YES" ); }
        return std::string( "NO" );
    }
    
    static bool FileExists( const std::string& filename ) {
        bool res = false;
        std::ifstream f( filename );
        if ( f.is_open() ) {
            res = true;
            f.close();
        }
        return res;
    }
    
    static sam_alig_iter_t::ALIG_ORDER encode_alig_order( bool by_pos, bool by_name ) {
        sam_alig_iter_t::ALIG_ORDER res = sam_alig_iter_t::ALIG_ORDER_NONE;
        if ( by_pos ) { res = sam_alig_iter_t::ALIG_ORDER_REFPOS; }
        if ( by_name ) { res = sam_alig_iter_t::ALIG_ORDER_NAME; }
        return res;
    }
    
    static std::string alig_order_to_string( sam_alig_iter_t::ALIG_ORDER spot_order ) {
        switch( spot_order ) {
            case sam_alig_iter_t::ALIG_ORDER_NONE : return std::string( "NONE" ); break;
            case sam_alig_iter_t::ALIG_ORDER_NAME : return std::string( "NAME" ); break;
            case sam_alig_iter_t::ALIG_ORDER_REFPOS : return std::string( "REFPOS" ); break;
        }
        return std::string( "UNKNOWN" );
    }
    
    static std::string with_ths( uint64_t x ) {
        std::ostringstream ss;
        ss . imbue( std::locale( "" ) );
        ss << x;
        return ss.str();
    }
};

struct cmn_params_t {
    const std::string db_filename;
    const uint32_t transaction_size;
    const bool help;
    const bool report;
    const bool progress;

    cmn_params_t( const args_t& args ) :
        db_filename( args . get_str( "-d", "--db", "sam.db" ) ),
        transaction_size( args . get_int<uint32_t>( "-t", "--trans", 50000 ) ),
        help( args . has( "-h", "--help" ) ),
        report( args . has( "-r", "--report" ) ),
        progress( args . has( "-p", "--progress" ) ) { 
    }

    static void populate_hints( args_t::str_vec_t& hints ) {
        hints . push_back( "-d" );
        hints . push_back( "--db" );        
        hints . push_back( "-t" );
        hints . push_back( "--trans" );        
    }

    void show_report( void ) const {
        std::cerr << "database-file : '" << db_filename << "'" << std::endl;
        std::cerr << "progress      : " << base_params_t::yes_no( progress ) << std::endl;
    }
    
    void show_help( void ) const {
        std::cout << "bam_analyze OPTIONS" << std::endl;
        std::cout << "\t-h --help ... show this help-text" << std::endl;
        std::cout <<  std::endl;
        std::cout << "\t-d=file --db=file ....... sqlite3-database for storage ( can be ':memory:' ), dflf:none" << std::endl;        
        std::cout << "\t-t=count --trans=count .. transaction size for writing to sqlite3-db ( dflt: 50,000 )" << std::endl;
        std::cout << "\t-r --report ............. produce report about import/analysis/export on stderr" << std::endl;
        std::cout << "\t-p --progress ........... show progress of import/export on stderr" << std::endl;
    }
};

struct import_params_t {
    const cmn_params_t &cmn;
    const std::string import_filename;
    const uint64_t align_limit;

    import_params_t( const args_t& args, const cmn_params_t& a_cmn ) :
        cmn( a_cmn ),
        import_filename( args . get_str( "-i", "--import" ) ),
        align_limit( args . get_int< uint64_t >( "-l", "--import-alig" ) ) { 
    }

    static void populate_hints( args_t::str_vec_t& hints ) {
        hints . push_back( "-i" );
        hints . push_back( "--import" );        
        hints . push_back( "-l" );
        hints . push_back( "--import-alig" );        
    }

    void show_report( void ) const {
        std::cerr << "import file   : '" << import_filename << "'" << std::endl;
        if ( align_limit > 0 ) {
            std::cerr << "align-limit   : " << align_limit << std::endl;
        }
    }

    bool requested( void ) const { return ! import_filename . empty (); }

    bool check( void ) const {
        bool res = true;
        if ( !( import_filename == "stdin" ) ) {
            res = base_params_t::FileExists( import_filename );
        }
        if ( !res ) {
            std::cerr << "import-file not found: '" << import_filename << "'" << std::endl;
        }
        return res;
    }

    void show_help( void ) const {
        std::cout << "\t-i=file --import=file ... import from this SAM-file ( can be 'stdin' )" << std::endl;
        std::cout << "\t-l=count --import-alig=count ... limit number of alignments to import" << std::endl;        
    }
};

struct analyze_params_t {
    const bool analyze;

    analyze_params_t( const args_t& args ) :
        analyze( args . has( "-a", "--analyze" ) ) { }

    static void populate_hints( args_t::str_vec_t& hints ) {
    }

    void show_report( void ) const {
        std::cerr << "analyze data  : " << base_params_t::yes_no( analyze ) << std::endl;
    }

    bool requested( void ) const { return analyze; }
    
    bool check( sam_database_t& db ) const {
        bool res = ( db . alig_count() > 0 );
        if ( !res ) {
            std::cerr << "database is empty!" << std::endl;            
        }
        return res;
    }
    
    void show_help( void ) const {
        std::cout << "\t-a --analyze          ... analyze imported data" << std::endl;
    }

};

struct export_params_t {
    const cmn_params_t &cmn;
    const std::string export_filename;
    const std::string ref_report_filename;
    const bool only_used_refs;
    const bool fix_names;
    const sam_alig_iter_t::ALIG_ORDER alig_order;
    const uint64_t spot_limit;
    const bool min_refs;
    
    export_params_t( const args_t& args, const cmn_params_t& a_cmn ) :
        cmn( a_cmn ),
        export_filename( args . get_str( "-e", "--export" ) ),
        ref_report_filename( args . get_str( "-R", "--ref-report" ) ),
        only_used_refs( args . has( "-u", "--only-used-refs" ) ),
        fix_names( args . has( "-f", "--fix-names" ) ),
        alig_order( base_params_t::encode_alig_order( 
            args . has( "-s", "--sort-by-refpos" ),
            args . has( "-n", "--sort-by-name" ) ) ),
        spot_limit( args . get_int<uint64_t>( "-E", "--export-spot-limit" ) ),
        min_refs( args . has( "-M", "--export-min-refs" ) )  { }
        
    static void populate_hints( args_t::str_vec_t& hints ) {
        hints . push_back( "-e" );
        hints . push_back( "--export" );
        hints . push_back( "-R" );
        hints . push_back( "--ref-file" );
        hints . push_back( "-E" );
        hints . push_back( "--export-spot-limit" );        
    }

    void show_report( void ) const {
        std::cerr << "export-file   : '" << export_filename << "'" << std::endl;
        std::cerr << "ref-rep.-file : '" << ref_report_filename << "'" << std::endl;
        std::cerr << "used-refs     : " << base_params_t::yes_no( only_used_refs ) << std::endl;
        std::cerr << "fix-names     : " << base_params_t::yes_no( fix_names ) << std::endl;
        std::cerr << "sort-by       : " << base_params_t::alig_order_to_string( alig_order ) << std::endl;
        std::cerr << "spot-limit    : " << base_params_t::with_ths( spot_limit ) << std::endl;
        std::cerr << "min-refs      : " << base_params_t::yes_no( min_refs ) << std::endl;        
    }

    bool requested( void ) const { return ! export_filename . empty (); }

    bool check( sam_database_t& db ) const {
        bool res = ( db . alig_count() > 0 );
        if ( !res ) {
            std::cerr << "database is empty!" << std::endl;            
        }
        return res;
    }

    void show_help( void ) const {
        std::cout << "\t-e=file --export=file ..... export into this SAM-file ( can be 'stdout' )" << std::endl;
        std::cout << "\t-R=file --ref-report=file . produce report of used references" << std::endl;
        std::cout << "\t-u --only-used-refs ....... export only used reference-header-lines" << std::endl;
        std::cout << "\t-f --fix-names ............ fix names in alignments ( remove space )" << std::endl;
        std::cout << "\t-s --sort ................. sort alignments by ref-position in export" << std::endl;
        std::cout << "\t-E --export-spot-limit=N... limit how many spots are exported" << std::endl;
        std::cout << "\t-M --export-min-refs ...... used smallest number of refs for export" << std::endl;
    }
};

struct params_t {
    const cmn_params_t cmn;
    const import_params_t imp;
    const analyze_params_t ana;
    const export_params_t exp;
    
    params_t( const args_t& args ) : cmn( args ), imp( args, cmn ), ana( args ), exp( args, cmn ) { }

    void show_report( void ) const {
        cmn . show_report();
        imp . show_report();
        ana . show_report();
        exp . show_report();
    }

    void show_help( void ) const  {
        cmn . show_help();
        imp . show_help();
        ana . show_help();
        exp . show_help();
    }
    
    static void populate_hints( args_t::str_vec_t& hints ) {
        cmn_params_t::populate_hints( hints );
        import_params_t::populate_hints( hints );
        analyze_params_t::populate_hints( hints );
        export_params_t::populate_hints( hints );        
    }
};

#endif
