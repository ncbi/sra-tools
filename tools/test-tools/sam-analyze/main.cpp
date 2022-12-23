#include "args.hpp"
#include "params.hpp"
#include "sam_db.hpp"
#include "importer.hpp"
#include "analyzer.hpp"
#include "exporter.hpp"
#include "ref_dict.hpp"

extern "C" {

int KMain( int argc, char* argv[] ) {
    int res = 0;
    args_t::str_vec_t hints;
    params_t::populate_hints( hints );
    const args_t args( argc, argv, hints ); // in args.hpp
    const params_t params( args ); // in params.hpp
    if ( params . cmn . help ) {
        params . show_help();
    } else {
        if ( params . cmn . report ) {
            params . show_report();
        }

        sam_database_t db( params . cmn . db_filename ); // in sam_db.hpp

        if ( params . imp . requested() ) {     // import was requested?
            if ( params . imp . check() ) {     // import can be done?
                /* ==============================================================
                *  import a sam-file into the sqlite-database-file 
                * ============================================================== */
                importer_t importer( db, params . imp ); // in importer.hpp
                res = importer . run() ? 0 : 3;
                /* ============================================================== */
            } else {
                res = 3;    // import requested, but cannot be done
            }
        }

        ref_dict_t ref_dict;    // either filled by analyze-step, or in export if needed

        if ( 0 == res && params . ana . requested() ) {     // analyze was requested?
            if ( params . ana . check( db ) ) {             // analyze can be done?
                /* ==============================================================
                *  analyze SAM-database ( sqlite )
                * ============================================================== */
                analyzer_t analyzer( db, params . ana, ref_dict ); // in analyzer.hpp
                res = analyzer . run() ? 0 : 3;
                /* ============================================================== */
            } else {
                res = 3;    // analyze requested, but cannot be done
            }
        }

        if ( 0 == res && params . exp . requested() ) {     // export was requested?
            if ( params . exp . check( db ) ) {             // export can be done?
                /* ==============================================================
                *  export from the sqlite-database into a sam-file
                *  omit reference-headers which are not used by alignments
                * ============================================================== */
                exporter_t exporter( db, params . exp, ref_dict ); // in exporter.hpp
                res = exporter . run() ? 0 : 3;
            } else {
                res =  3;    // export requested, but cannot be done
            }
        }
    }
    return res;
}

}