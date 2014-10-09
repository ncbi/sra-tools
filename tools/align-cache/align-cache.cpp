/*==============================================================================
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

#include "align-cache.vers.h"
#include "helper.h"

#include <stdio.h>
#include <iostream>

#include <kapp/main.h>
#include <klib/rc.h>


namespace AlignCache
{
    struct Params
    {
        char const* dbPath;
        
        int64_t     id_spread_threshold;
        size_t      cursor_cache_size; 
    } g_Params =
    {
        "",
        50000,
#if _ARCH_BITS == 64
        128UL << 30  // 128 GB
#else
        1024UL << 20 // 1 GB
#endif
    };

    char const OPTION_ID_SPREAD_THRESHOLD[] = "threshold";
    char const ALIAS_ID_SPREAD_THRESHOLD[]  = "t";
    char const* USAGE_ID_SPREAD_THRESHOLD[]  = { "cache PRIMARY_ALIGNMENT records with difference between values of ALIGN_ID and MATE_ALIGN_ID >= the value of 'threshold' option", NULL };

    char const OPTION_CURSOR_CACHE_SIZE[] = "cursor-cache";
    //char const ALIAS_CURSOR_CACHE_SIZE[]  = "c";
    char const* USAGE_CURSOR_CACHE_SIZE[]  = { "the size of the read cursor in Megabytes", NULL };

    ::OptDef Options[] =
    {
        { OPTION_ID_SPREAD_THRESHOLD, ALIAS_ID_SPREAD_THRESHOLD, NULL, USAGE_ID_SPREAD_THRESHOLD, 1, true, false },
        { OPTION_CURSOR_CACHE_SIZE, NULL, NULL, USAGE_CURSOR_CACHE_SIZE, 1, true, false }
    };

    struct PrimaryAlignmentData
    {
        uint64_t                    prev_key;
        VDBObjects::CVCursor const* pCursorPA;
        uint32_t const*             pColumnIndex;
        uint32_t const*             pColumnIndexCache;
        size_t const                column_count;
        VDBObjects::CVCursor*       pCursorPACache;
        size_t                      count;
        size_t const                total_count;
    };

    size_t print_percent ( size_t count, size_t total_count )
    {
        size_t const points = 100;
        if ( total_count >= points && !(count % (total_count / points)) )
        {
            size_t percent = (size_t)(points*count/total_count);
            std::cout
                << percent
                << "%% ("
                << count
                << "/"
                << total_count
                << ")"
                << std::endl;
            return percent;
        }
        else
            return 0;
    }

    rc_t KVectorCallbackPrimaryAlignment ( uint64_t key, bool value, void *user_data )
    {
        assert ( value );
        PrimaryAlignmentData* p = (PrimaryAlignmentData*)user_data;
        int64_t prev_row_id = (int64_t)p->prev_key;
        int64_t row_id = (int64_t)key;
        VDBObjects::CVCursor& cur_cache = *p->pCursorPACache;

        ++p->count;
        print_percent (p->count, p->total_count);

        // Filling gaps between actually cached rows with zero-length records
        if ( p->prev_key )
        {
            if ( row_id - prev_row_id > 1)
            {
                cur_cache.OpenRow ();
                cur_cache.CommitRow ();
                if (row_id - prev_row_id > 2)
                    cur_cache.RepeatRow ( row_id - prev_row_id - 2 ); // -2 due to the first zero-row has been written in the previous line
                cur_cache.CloseRow ();
            }
        }

        // Caching (copying) actual record from PRIMARY_ALIGNMENT table
        {
            // The very first visin - need to set starting row_id
            if ( p->prev_key == 0 )
                cur_cache.SetRowId ( row_id );

            cur_cache.OpenRow ();

            uint32_t item_count;

            // MATE_ALIGN_ID
            size_t column_index = 0;
            int64_t mate_align_id;
            p->pCursorPA->ReadItems ( row_id, p->pColumnIndex [column_index], & mate_align_id, sizeof (mate_align_id) );
            cur_cache.Write ( p->pColumnIndexCache [column_index], & mate_align_id, 1 );

            // SAM_FLAGS
            ++ column_index;
            uint32_t sam_flags;
            p->pCursorPA->ReadItems ( row_id, p->pColumnIndex [column_index], & sam_flags, sizeof (sam_flags) );
            cur_cache.Write ( p->pColumnIndexCache [column_index], & sam_flags, 1 );

            // TEMPLATE_LEN
            ++ column_index;
            int32_t template_len;
            p->pCursorPA->ReadItems ( row_id, p->pColumnIndex [column_index], & template_len, sizeof (template_len) );
            cur_cache.Write ( p->pColumnIndexCache [column_index], & template_len, 1 );

            // MATE_REF_NAME
            ++ column_index;
            char mate_ref_name[512];
            item_count = p->pCursorPA->ReadItems ( row_id, p->pColumnIndex [column_index], mate_ref_name, sizeof (mate_ref_name) );
            cur_cache.Write ( p->pColumnIndexCache [column_index], mate_ref_name, item_count );

            // MATE_REF_POS
            ++ column_index;
            uint32_t mate_ref_pos;
            p->pCursorPA->ReadItems ( row_id, p->pColumnIndex [column_index], & mate_ref_pos, sizeof (mate_ref_pos) );
            cur_cache.Write ( p->pColumnIndexCache [column_index], & mate_ref_pos, 1 );

            // SAM_QUALITY
            ++ column_index;
            char sam_quality[4096];
            item_count = p->pCursorPA->ReadItems ( row_id, p->pColumnIndex [column_index], sam_quality, sizeof (sam_quality) );
            cur_cache.Write ( p->pColumnIndexCache [column_index], sam_quality, item_count );

            cur_cache.CommitRow ();
            cur_cache.CloseRow ();

            p->prev_key = key;
        }

        return 0;
    }

    bool ProcessSequenceRow ( int64_t idRow, VDBObjects::CVCursor const& cursor, KLib::CKVector& vect, uint32_t idxCol )
    {
        int64_t buf[3]; // TODO: find out the real type of this array
        uint32_t items_read_count = cursor.ReadItems ( idRow, idxCol, buf, countof(buf) );
        if ( items_read_count == 2 )
        {
            int64_t id1 = buf[0];
            int64_t id2 = buf[1];
            int64_t diff = id1 >= id2 ? id1 - id2 : id2 - id1;

            if (id1 && id2 && diff > g_Params.id_spread_threshold)
            {
                vect.SetBool(id1, true);
                vect.SetBool(id2, true);
                return true;
            }
        }
        return false;
    }

    size_t FillKVectorWithAlignIDs (VDBObjects::CVDatabase const& vdb, size_t cache_size, KLib::CKVector& vect )
    {
        char const* ColumnNamesSequence[] =
        {
            "PRIMARY_ALIGNMENT_ID"
        };
        uint32_t ColumnIndexSequence [ countof (ColumnNamesSequence) ];

        VDBObjects::CVTable table = vdb.OpenTable("SEQUENCE");

        VDBObjects::CVCursor cursor = table.CreateCursorRead ( cache_size );
        cursor.InitColumnIndex (ColumnNamesSequence, ColumnIndexSequence, countof(ColumnNamesSequence), false);
        cursor.Open();

        int64_t idRow = 0;
        uint64_t nRowCount = 0;
        size_t count = 0;

        cursor.GetIdRange (idRow, nRowCount);

        for (; (uint64_t)idRow < nRowCount/500; ++idRow )
        {
            if ( ProcessSequenceRow (idRow, cursor, vect, ColumnIndexSequence[0]) )
                ++count;
        }

        return count;
    }

    void CachePrimaryAlignment (VDBObjects::CVDBManager& mgr, VDBObjects::CVDatabase const& vdb, size_t cache_size, KLib::CKVector const& vect, size_t vect_size)
    {
        // Defining the set of columns to be copied from PRIMARY_ALIGNMENT table
        // to the new cache table
#define DECLARE_PA_COLUMNS( arrName, column_suffix ) char const* arrName[] =\
        {\
            "MATE_ALIGN_ID" column_suffix,\
            "SAM_FLAGS"     column_suffix,\
            "TEMPLATE_LEN"  column_suffix,\
            "MATE_REF_NAME" column_suffix,\
            "MATE_REF_POS"  column_suffix,\
            "SAM_QUALITY"   column_suffix\
        }

        DECLARE_PA_COLUMNS (ColumnNamesPrimaryAlignment, "");
        DECLARE_PA_COLUMNS (ColumnNamesPrimaryAlignmentCache, "_CACHE");
#undef DECLARE_PA_COLUMNS

        uint32_t ColumnIndexPrimaryAlignment [ countof (ColumnNamesPrimaryAlignment) ];
        uint32_t ColumnIndexPrimaryAlignmentCache [ countof (ColumnNamesPrimaryAlignmentCache) ];

        // Openning cursor to iterate through PRIMARY_ALIGNMENT table
        VDBObjects::CVTable tablePA = vdb.OpenTable("PRIMARY_ALIGNMENT");
        VDBObjects::CVCursor cursorPA = tablePA.CreateCursorRead ( cache_size );
        cursorPA.InitColumnIndex ( ColumnNamesPrimaryAlignment, ColumnIndexPrimaryAlignment, countof(ColumnNamesPrimaryAlignment), false );
        cursorPA.Open();

        // Creating new cache table (with the same name - PRIMARY_ALIGNMENT but in the separate DB file)
        char const schema_path[] = // TODO: specify schema path in a proper way
#ifdef _WIN32
            "Z:/projects/internal/asm-trace/interfaces/align/mate-cache.vschema";
#else
            "align/mate-cache.vschema";
#endif

        VDBObjects::CVSchema schema = mgr.MakeSchema ();
        schema.VSchemaParseFile ( schema_path );
        char szCacheDBName[256] = "";
        string_printf (szCacheDBName, countof (szCacheDBName), NULL, "%s_aux.sra", g_Params.dbPath );
        VDBObjects::CVDatabase dbCache = mgr.CreateDB ( schema, "NCBI:align:db:mate_cache #1", kcmParents | kcmInit, szCacheDBName );
        VDBObjects::CVTable tableCache = dbCache.CreateTable ( "PRIMARY_ALIGNMENT", kcmInit );

        VDBObjects::CVCursor cursorCache = tableCache.CreateCursorWrite ( kcmInsert );
        cursorCache.InitColumnIndex ( ColumnNamesPrimaryAlignmentCache, ColumnIndexPrimaryAlignmentCache, countof (ColumnNamesPrimaryAlignmentCache), true );
        cursorCache.Open ();

        PrimaryAlignmentData data = { 0, &cursorPA, ColumnIndexPrimaryAlignment, ColumnIndexPrimaryAlignmentCache, countof (ColumnNamesPrimaryAlignment), &cursorCache, 0, vect_size };

        // process each saved primary_alignment_id
        vect.VisitBool ( KVectorCallbackPrimaryAlignment, & data );
        cursorCache.Commit ();
    }

    void create_cache_db_impl()
    {
        VDBObjects::CVDBManager mgr;
        mgr.Make();

        VDBObjects::CVDatabase vdb = mgr.OpenDB (g_Params.dbPath);

        // Scan SEQUENCE table to find mate_alignment_ids that have to be cached
        KLib::CKVector vect;
        size_t count = FillKVectorWithAlignIDs ( vdb, g_Params.cursor_cache_size, vect );

        // For each id in vect cache the PRIMARY_ALIGNMENT record
        CachePrimaryAlignment ( mgr, vdb, g_Params.cursor_cache_size, vect, count );
    }


    void create_cache_db (int argc, char** argv)
    {
        try
        {
            KApp::CArgs args (argc, argv, Options, countof (Options));
            uint32_t param_count = args.GetParamCount ();
            if ( param_count < 1 )
            {
                MiniUsage (args.GetArgs());
                return;
            }

            if ( param_count > 1 )
            {
                printf ("Too many database parameters");
                return;
            }

            g_Params.dbPath = args.GetParamValue (0);

            if (args.GetOptionCount (OPTION_ID_SPREAD_THRESHOLD))
                g_Params.id_spread_threshold = args.GetOptionValueInt<int64_t> ( OPTION_ID_SPREAD_THRESHOLD, 0 );

            if (args.GetOptionCount (OPTION_CURSOR_CACHE_SIZE))
                g_Params.cursor_cache_size = args.GetOptionValueInt<size_t> ( OPTION_CURSOR_CACHE_SIZE, 0 );

            create_cache_db_impl ();
        }
        catch (VDBObjects::CErrorMsg const& e)
        {
            char szBufErr[512] = "";
            size_t rc = e.getRC();
            rc_t res = string_printf(szBufErr, countof(szBufErr), NULL, "ERROR: %s failed with error 0x%08x (%d) [%R]", e.what(), rc, rc, rc);
            if (res == rcBuffer || res == rcInsufficient)
                szBufErr[countof(szBufErr) - 1] = '\0';
            printf("%s\n", szBufErr);
        }
        catch (...)
        {
            printf("Unexpected exception occured\n");
        }
    }
}


extern "C"
{
    const char UsageDefaultName[] = "align-cache";
    ver_t CC KAppVersion()
    {
        return ALIGN_CACHE_VERS;
    }
    rc_t CC UsageSummary (const char * progname)
    {
        printf (
        "Usage:\n"
        "  %s [options] <db-path>\n"
        "\n"
        "Summary:\n"
        "  Create a cache file for given database PRIMARY_ALIGNMENT table\n"
        "\n", progname);
        return 0;
    }
    const char* param_usage[] = { "Path to the database" };
    rc_t CC Usage (::Args const* args)
    {
        rc_t rc = 0;
        const char* progname = UsageDefaultName;
        const char* fullpath = UsageDefaultName;

        if (args == NULL)
            rc = RC(rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
        else
            rc = ArgsProgram(args, &fullpath, &progname);

        UsageSummary (progname);

        printf("Parameters:\n");

        HelpParamLine ("db-path", param_usage);

        printf ("\nOptions:\n");

        HelpOptionLine (AlignCache::ALIAS_ID_SPREAD_THRESHOLD, AlignCache::OPTION_ID_SPREAD_THRESHOLD, "value", AlignCache::USAGE_ID_SPREAD_THRESHOLD);
        HelpOptionLine (NULL, AlignCache::OPTION_CURSOR_CACHE_SIZE, "value in MB", AlignCache::USAGE_CURSOR_CACHE_SIZE);

        printf ("\n");

        HelpOptionsStandard ();

        HelpVersion (fullpath, KAppVersion());

        return rc;
    }

    rc_t CC KMain(int argc, char* argv[])
    {
        AlignCache::create_cache_db (argc, argv);
        return 0;
    }
}
