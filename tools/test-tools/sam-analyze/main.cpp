#include "args.hpp"
#include "params.hpp"
#include "result.hpp"
#include "sam_db.hpp"
#include "importer.hpp"
#include "analyzer.hpp"
#include "exporter.hpp"

int main( int argc, char* argv[] ) {
    int res = 0;
    ARGS::str_vec hints;
    PARAMS::populate_hints( hints );
    const ARGS args( argc, argv, hints ); // in args.hpp
    const PARAMS params( args ); // in params.hpp
    if ( params . cmn . help ) {
        params . show_help();
    } else {
        if ( params . cmn . report ) {
            params . show_report();
        }

        SAM_DB db( params . cmn . db_filename ); // in sam_db.hpp

        if ( params . imp . requested() ) {
            if ( params . imp . check() ) {
                /* ==============================================================
                *  import a sam-file into the sqlite-database-file 
                * ============================================================== */
                IMPORT_RESULT result; // in result.hpp
                IMPORTER importer( db, params . imp, result ); // in importer.hpp
                importer . run();
                if ( params . cmn . report ) { result . report(); }
                res = result . success ? 0 : 3;
                /* ============================================================== */
            } else {
                res = 3;
            }
        }

        if ( 0 == res && params . ana . requested() ) {
            if ( params . ana .check( db ) ) {
                /* ==============================================================
                *  analyze SAM-database ( sqlite )
                * ============================================================== */
                ANALYZE_RESULT result;
                ANALYZER analyzer( db, params . ana, result ); // in analyzer.hpp
                analyzer . run();
                result . report();
                res = result . success ? 0 : 3;
                /* ============================================================== */
            } else {
                res = 3;
            }
        }
        
        if ( 0 == res && params . exp . requested() ) {
            if ( params . exp . check( db ) ) {
                /* ==============================================================
                *  export from the sqlite-database into a sam-file
                *  omit reference-headers which are not used by alignments
                * ============================================================== */
                EXPORT_RESULT result;
                EXPORTER exporter( db, params . exp, result ); // in exporter.hpp
                exporter . run();
                if ( params . cmn . report ) { result . report(); }
                res = result . success ? 0 : 3;
            } else {
                res =  3;
            }
        }
    }
    return res;
}
