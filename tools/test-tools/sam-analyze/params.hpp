#ifndef PARAMS_H
#define PARAMS_H

#include <fstream>
#include "args.hpp"
#include "alig.hpp"
#include "sam_db.hpp"

struct BASE_PARAMS {
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
    
    static ALIG_ITER::ALIG_ORDER encode_alig_order( bool by_pos, bool by_name ) {
        ALIG_ITER::ALIG_ORDER res = ALIG_ITER::ALIG_ORDER_NONE;
        if ( by_pos ) { res = ALIG_ITER::ALIG_ORDER_REFPOS; }
        if ( by_name ) { res = ALIG_ITER::ALIG_ORDER_NAME; }
        return res;
    }
    
    static std::string alig_order_to_string( ALIG_ITER::ALIG_ORDER spot_order ) {
        switch( spot_order ) {
            case ALIG_ITER::ALIG_ORDER_NONE : return std::string( "NONE" ); break;
            case ALIG_ITER::ALIG_ORDER_NAME : return std::string( "NAME" ); break;
            case ALIG_ITER::ALIG_ORDER_REFPOS : return std::string( "REFPOS" ); break;
        }
        return std::string( "UNKNOWN" );
    }
};

struct COMMON_PARAMS {
    const std::string db_filename;
    const uint32_t transaction_size;
    const bool help;
    const bool report;
    const bool progress;

    COMMON_PARAMS( const ARGS& args ) :
        db_filename( args . get_str( "-d", "--db", "sam.db" ) ),
        transaction_size( args . get_int<uint32_t>( "-t", "--trans", 50000 ) ),
        help( args . has( "-h", "--help" ) ),
        report( args . has( "-r", "--report" ) ),
        progress( args . has( "-p", "--progress" ) ) { }

    static void populate_hints( ARGS::str_vec& hints ) {
        hints.push_back( "-d" );
        hints.push_back( "--db" );        
        hints.push_back( "-t" );
        hints.push_back( "--trans" );        
    }

    void show_report( void ) const {
        std::cerr << "database-file : '" << db_filename << "'" << std::endl;
        std::cerr << "progress      : " << BASE_PARAMS::yes_no( progress ) << std::endl;
    }
    
    void show_help( void ) const {
        std::cout << "bam_analyze OPTIONS" << std::endl;
        std::cout << "\t-h --help ... show this help-text" << std::endl;
        std::cout <<  std::endl;
        std::cout << "\t-d=file --db=file ....... sqlite3-database for storage ( can be ':memory:' ), dflf:none" << std::endl;        
        std::cout << "\t-t=count --trans=count .. transaction size for writing to sqlite3-db" << std::endl;
        std::cout << "\t-r --report ............. produce report about import/export on stderr" << std::endl;
        std::cout << "\t-p --progress ........... show progress of import/export on stderr" << std::endl;
    }
};

struct IMPORT_PARAMS {
    const COMMON_PARAMS &cmn;
    const std::string import_filename;
    const uint64_t align_limit;

    IMPORT_PARAMS( const ARGS& args, const COMMON_PARAMS& a_cmn ) :
        cmn( a_cmn ),
        import_filename( args . get_str( "-i", "--import" ) ),
        align_limit( args . get_int< uint64_t >( "-l", "--import-alig" ) ) { }

    static void populate_hints( ARGS::str_vec& hints ) {
        hints.push_back( "-i" );
        hints.push_back( "--import" );        
        hints.push_back( "-l" );
        hints.push_back( "--import-alig" );        
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
            res = BASE_PARAMS::FileExists( import_filename );
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

struct ANALYZE_PARAMS {
    const bool analyze;

    ANALYZE_PARAMS( const ARGS& args ) :
        analyze( args . has( "-a", "--analyze" ) ) { }

    static void populate_hints( ARGS::str_vec& hints ) {
    }

    void show_report( void ) const {
        std::cerr << "analyze data  : " << BASE_PARAMS::yes_no( analyze ) << std::endl;
    }

    bool requested( void ) const { return analyze; }
    
    bool check( SAM_DB& db ) const {
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

struct EXPORT_PARAMS {
    const COMMON_PARAMS &cmn;
    const std::string export_filename;
    const bool only_used_refs;
    const bool fix_names;
    const ALIG_ITER::ALIG_ORDER alig_order;
    
    EXPORT_PARAMS( const ARGS& args, const COMMON_PARAMS& a_cmn ) :
        cmn( a_cmn ),
        export_filename( args . get_str( "-e", "--export" ) ),
        only_used_refs( args . has( "-u", "--only-used-refs" ) ),
        fix_names( args . has( "-f", "--fix-names" ) ),
        alig_order( BASE_PARAMS::encode_alig_order( 
            args . has( "-s", "--sort-by-refpos" ),
            args . has( "-n", "--sort-by-name" ) ) ) { }
        
    static void populate_hints( ARGS::str_vec& hints ) {
        hints.push_back( "-e" );
        hints.push_back( "--export" );        
    }

    void show_report( void ) const {
        std::cerr << "export-file   : '" << export_filename << "'" << std::endl;        
        std::cerr << "used-refs     : " << BASE_PARAMS::yes_no( only_used_refs ) << std::endl;
        std::cerr << "fix-names     : " << BASE_PARAMS::yes_no( fix_names ) << std::endl;
        std::cerr << "sort-by       : " << BASE_PARAMS::alig_order_to_string( alig_order ) << std::endl;
    }

    bool requested( void ) const { return ! export_filename . empty (); }

    bool check( SAM_DB& db ) const {
        bool res = ( db . alig_count() > 0 );
        if ( !res ) {
            std::cerr << "database is empty!" << std::endl;            
        }
        return res;
    }

    void show_help( void ) const {
        std::cout << "\t-e=file --export=file ... export into this SAM-file ( can be 'stdout' )" << std::endl;
        std::cout << "\t-u --only-used-refs ..... export only used reference-header-lines" << std::endl;
        std::cout << "\t-f --fix-names .......... fix names in alignments ( remove space )" << std::endl;
        std::cout << "\t-s --sort ............... sort alignments by ref-position in export" << std::endl;
    }
};

struct PARAMS {
    const COMMON_PARAMS cmn;
    const IMPORT_PARAMS imp;
    const ANALYZE_PARAMS ana;
    const EXPORT_PARAMS exp;
    
    PARAMS( const ARGS& args ) : cmn( args ), imp( args, cmn ), ana( args ), exp( args, cmn ) { }

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
    
    static void populate_hints( ARGS::str_vec& hints ) {
        COMMON_PARAMS::populate_hints( hints );
        IMPORT_PARAMS::populate_hints( hints );
        ANALYZE_PARAMS::populate_hints( hints );
        EXPORT_PARAMS::populate_hints( hints );        
    }
};

#endif
