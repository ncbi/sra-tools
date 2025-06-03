/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include <kapp/main.h>

#include "args.hpp"
#include "params.hpp"
#include "sam_db.hpp"
#include "importer.hpp"
#include "analyzer.hpp"
#include "exporter.hpp"
#include "ref_dict.hpp"

MAIN_DECL(argc, argv)
{
    VDB::Application app(argc, argv);

    int res = 0;
    args_t::str_vec_t hints;
    params_t::populate_hints( hints );
    const args_t args( argc, app.getArgV(), hints); // in args.hpp
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

    app.setRc( res );
    return app.getExitCode();
}
