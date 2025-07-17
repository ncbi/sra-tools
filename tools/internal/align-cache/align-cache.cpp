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

#include "helper.h"

#include <stdio.h>
#include <iostream>

#include <kapp/main.h>
#include <klib/rc.h>
#include <klib/log.h>


namespace AlignCache
{
    struct Params
    {
        // Command line Params:
        char const* dbPathSrc;
        char const* dbPathDst;

        // Command line options
        int64_t     id_spread_threshold;
        size_t      cursor_cache_size;
        size_t      min_cache_count;

        // Internal parameters
        bool cache_alignment_count;
    } g_Params =
    {
        // Command line Params:
        "",
        "",
        // Command line options
        50000,
#if _ARCH_BITS == 64
        16UL << 30,  // 16 GB
#else
        1024UL << 20, // 1 GB
#endif
        100000,
        // Internal parameters
        true
    };

    char const OPTION_ID_SPREAD_THRESHOLD[] = "threshold";
    char const ALIAS_ID_SPREAD_THRESHOLD[]  = "t";
    char const* USAGE_ID_SPREAD_THRESHOLD[]  = { "cache PRIMARY_ALIGNMENT records with difference between values of ALIGN_ID and MATE_ALIGN_ID >= the value of 'threshold' option", NULL };

    char const OPTION_CURSOR_CACHE_SIZE[] = "cursor-cache";
    //char const ALIAS_CURSOR_CACHE_SIZE[]  = "c";
    char const* USAGE_CURSOR_CACHE_SIZE[]  = { "the size of the read cursor in Megabytes", NULL };

    char const OPTION_MIN_CACHE_COUNT[] = "min-cache-count";
    //char const ALIAS_MIN_CACHE_COUNT[]  = "";
    char const* USAGE_MIN_CACHE_COUNT[]  = { "if the number of primary alignment ids in the src db selected for caching is less than <min-cache-count>, the cache db will not be created at all", NULL };

    ::OptDef Options[] =
    {
        { OPTION_ID_SPREAD_THRESHOLD, ALIAS_ID_SPREAD_THRESHOLD, NULL, USAGE_ID_SPREAD_THRESHOLD, 1, true, false },
        { OPTION_CURSOR_CACHE_SIZE, NULL, NULL, USAGE_CURSOR_CACHE_SIZE, 1, true, false },
        { OPTION_MIN_CACHE_COUNT, NULL, NULL, USAGE_MIN_CACHE_COUNT, 1, true, false },
    };

    struct PrimaryAlignmentData
    {
        uint64_t                    prev_key;
        VDBObjects::CVCursor const* pCursorPA;
        uint32_t const*             pColumnIndex;
        uint32_t const*             pColumnIndexCache;
        size_t const                column_count;
        VDBObjects::CVCursor*       pCursorPACache;
        //size_t                      count;
        //size_t const                total_count;
        KApp::CProgressBar*         pProgressBar;
    };

    //size_t print_percent ( size_t count, size_t total_count )
    //{
    //    size_t const total_points = 10000;
    //    if ( total_count >= total_points && !(count % (total_count / total_points)) )
    //    {
    //        size_t points = (size_t)(total_points*count/total_count);
    //        std::cout
    //            << (100*points/total_points)
    //            << "."
    //            << ( 1000*points/total_points % 10 )
    //            << ( 10000*points/total_points % 10 )
    //            << "% ("
    //            << count
    //            << "/"
    //            << total_count
    //            << ")"
    //            << std::endl;
    //        return points;
    //    }
    //    else
    //        return 0;
    //}

    template <typename T>
    void copy_single_int_field (
        VDBObjects::CVCursor const& curFrom,
        VDBObjects::CVCursor& curTo,
        int64_t row_id,
        uint32_t column_index_from,
        uint32_t column_index_to
        )
    {
        T val;
        curFrom.ReadItems ( row_id, column_index_from, & val, sizeof (T) );
        curTo.Write ( column_index_to, & val, 1 );
    }

    void copy_str_field (
        VDBObjects::CVCursor const& curFrom,
        VDBObjects::CVCursor& curTo,
        int64_t row_id,
        uint32_t column_index_from,
        uint32_t column_index_to
        )
    {
        char val[4096];
        uint32_t item_count = curFrom.ReadItems ( row_id, column_index_from, val, sizeof (val) );
        curTo.Write ( column_index_to, val, item_count );
    }


    rc_t KVectorCallbackPrimaryAlignment ( uint64_t key, bool value, void *user_data )
    {
        if ( ::Quitting() )
        {
            LOGMSG ( klogWarn, "Interrupted" );
            return 1;
        }

        assert ( value );
        PrimaryAlignmentData* p = (PrimaryAlignmentData*)user_data;
        int64_t prev_row_id = (int64_t)p->prev_key;
        int64_t row_id = (int64_t)key;
        VDBObjects::CVCursor& cur_cache = *p->pCursorPACache;

        p->pProgressBar->Process ( 1, false );

        //++p->count;
        //print_percent (p->count, p->total_count);

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
            // The very first visit - need to set starting row_id
            if ( p->prev_key == 0 )
                cur_cache.SetRowId ( row_id );

            VDBObjects::CVCursor const& cur_pa = *p->pCursorPA;
            uint32_t const* ColIndexPA = p->pColumnIndex;
            uint32_t const* ColIndexCache = p->pColumnIndexCache;

            cur_cache.OpenRow ();

            // MATE_ALIGN_ID
            size_t column_index = 0;
            copy_single_int_field <int64_t> (cur_pa, cur_cache, row_id, ColIndexPA[column_index], ColIndexCache[column_index] );

            // SAM_FLAGS
            ++ column_index;
            copy_single_int_field <uint32_t> (cur_pa, cur_cache, row_id, ColIndexPA[column_index], ColIndexCache[column_index] );

            // TEMPLATE_LEN
            ++ column_index;
            copy_single_int_field <int32_t> (cur_pa, cur_cache, row_id, ColIndexPA[column_index], ColIndexCache[column_index] );

            // MATE_REF_NAME
            ++ column_index;
            copy_str_field (cur_pa, cur_cache, row_id, ColIndexPA[column_index], ColIndexCache[column_index] );

            // MATE_REF_POS
            ++ column_index;
            copy_single_int_field <uint32_t> (cur_pa, cur_cache, row_id, ColIndexPA[column_index], ColIndexCache[column_index] );

            // SAM_QUALITY
            ++ column_index;
            copy_str_field (cur_pa, cur_cache, row_id, ColIndexPA[column_index], ColIndexCache[column_index] );

            // RD_FILTER
            ++ column_index;
            copy_single_int_field <uint8_t> (cur_pa, cur_cache, row_id, ColIndexPA[column_index], ColIndexCache[column_index] );

            // SPOT_GROUP
            ++ column_index;
            copy_str_field (cur_pa, cur_cache, row_id, ColIndexPA[column_index], ColIndexCache[column_index] );

            if ( g_Params.cache_alignment_count )
            {
                // ALIGNMENT_COUNT
                ++ column_index;
                copy_single_int_field <uint8_t> (cur_pa, cur_cache, row_id, ColIndexPA[column_index], ColIndexCache[column_index] );
            }

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

        for (; (uint64_t)idRow < nRowCount; ++idRow )
        {
            if ( ::Quitting() )
            {
                LOGMSG ( klogWarn, "Interrupted" );
                return 0;
            }

            if ( ProcessSequenceRow (idRow, cursor, vect, ColumnIndexSequence[0]) )
                ++count;
        }

        return count;
    }

    void CachePrimaryAlignment (VDBObjects::CVDBManager& mgr, VDBObjects::CVDatabase const& vdb, size_t cache_size, KLib::CKVector const& vect, size_t vect_size, KApp::CProgressBar& progress_bar)
    {
        // Defining the set of columns to be copied from PRIMARY_ALIGNMENT table
        // to the new cache table
        // ALIGNMENT_COUNT is the optional column, it must be at the end of this array
#define DECLARE_PA_COLUMNS( arrName, column_suffix ) char const* arrName[] =\
        {\
            "MATE_ALIGN_ID"     column_suffix,\
            "SAM_FLAGS"         column_suffix,\
            "TEMPLATE_LEN"      column_suffix,\
            "MATE_REF_NAME"     column_suffix,\
            "MATE_REF_POS"      column_suffix,\
            "SAM_QUALITY"       column_suffix,\
            "RD_FILTER"         column_suffix,\
            "SPOT_GROUP"        column_suffix,\
            "ALIGNMENT_COUNT"   column_suffix\
        }

        DECLARE_PA_COLUMNS (ColumnNamesPrimaryAlignment, "");
        DECLARE_PA_COLUMNS (ColumnNamesPrimaryAlignmentCache, "_CACHE");
#undef DECLARE_PA_COLUMNS

        uint32_t ColumnIndexPrimaryAlignment [ countof (ColumnNamesPrimaryAlignment) ];
        uint32_t ColumnIndexPrimaryAlignmentCache [ countof (ColumnNamesPrimaryAlignmentCache) ];

        // Openning cursor to iterate through PRIMARY_ALIGNMENT table
        VDBObjects::CVTable tablePA = vdb.OpenTable("PRIMARY_ALIGNMENT");
        VDBObjects::CVCursor cursorPA = tablePA.CreateCursorRead ( cache_size );
        cursorPA.PermitPostOpenAdd();
        cursorPA.InitColumnIndex ( ColumnNamesPrimaryAlignment, ColumnIndexPrimaryAlignment, countof(ColumnNamesPrimaryAlignment) - 1, false );
        cursorPA.Open();

        // Check if we can read ALIGNMENT_COUNT parameter
        try
        {
            cursorPA.InitColumnIndex(
                ColumnNamesPrimaryAlignment + countof(ColumnNamesPrimaryAlignment) - 1,
                ColumnIndexPrimaryAlignment + countof(ColumnIndexPrimaryAlignment) - 1,
                1, false
            );
        }
        catch (Utils::CErrorMsg const& e)
        {
            if (e.getRC() == RC ( rcVDB, rcCursor, rcUpdating, rcColumn, rcNotFound ) )
                g_Params.cache_alignment_count = false;
            else
                throw;
        }

        // Creating new cache table (with the same name - PRIMARY_ALIGNMENT but in the separate DB file)
        char const schema_path[] = "align/mate-cache.vschema";

        VDBObjects::CVSchema schema = mgr.MakeSchema ();
        schema.VSchemaParseFile ( schema_path );
        char szCacheDBName[256] = "";
        string_printf (szCacheDBName, countof (szCacheDBName), NULL, "%s", g_Params.dbPathDst );
        VDBObjects::CVDatabase dbCache = mgr.CreateDB ( schema, "NCBI:align:db:mate_cache #1", kcmParents | kcmInit | kcmMD5, szCacheDBName );
        VDBObjects::CVTable tableCache = dbCache.CreateTable ( "PRIMARY_ALIGNMENT" );

        VDBObjects::CVCursor cursorCache = tableCache.CreateCursorWrite ( kcmInsert );
        cursorCache.InitColumnIndex ( ColumnNamesPrimaryAlignmentCache, ColumnIndexPrimaryAlignmentCache, countof (ColumnNamesPrimaryAlignmentCache) - (size_t) (!g_Params.cache_alignment_count), true );
        cursorCache.Open ();

        //PrimaryAlignmentData data = { 0, &cursorPA, ColumnIndexPrimaryAlignment, ColumnIndexPrimaryAlignmentCache, countof (ColumnNamesPrimaryAlignment), &cursorCache, 0, vect_size };
        progress_bar.Append (vect_size);
        PrimaryAlignmentData data =
        {
            0,
            &cursorPA,
            ColumnIndexPrimaryAlignment,
            ColumnIndexPrimaryAlignmentCache,
            countof (ColumnNamesPrimaryAlignment),
            &cursorCache,
            &progress_bar
        };

        // process each saved primary_alignment_id
        vect.VisitBool ( KVectorCallbackPrimaryAlignment, & data );
        cursorCache.Commit ();
    }

    int create_cache_db_impl()
    {
        // Adding 0% mark at the very beginning of the program
        KApp::CProgressBar progress_bar(1);
        progress_bar.Process ( 0, true );

        VDBObjects::CVDBManager mgr;
        mgr.Make();

        VDBObjects::CVDatabase vdb = mgr.OpenDB (g_Params.dbPathSrc);

        // Scan SEQUENCE table to find mate_alignment_ids that have to be cached
        KLib::CKVector vect;
        size_t count = FillKVectorWithAlignIDs ( vdb, g_Params.cursor_cache_size, vect );

        if ( count*2 >= g_Params.min_cache_count )
        {
            // For each id in vect cache the PRIMARY_ALIGNMENT record
            CachePrimaryAlignment ( mgr, vdb, g_Params.cursor_cache_size, vect, count*2, progress_bar );
        }
        else
        {
            if ( ::Quitting() == 0 )
            {
                PLOGMSG ( klogWarn,
                    ( klogWarn,
                    "The cache db will not be created because there is not "
                    "enough records to cache: $(COUNT) is found and minimum $(MIN_COUNT) is required. "
                    "The minimum required number can be changed via $(OPTION_MIN_CACHE_COUNT) parameter.",
                    "COUNT=%zu,MIN_COUNT=%zu,OPTION_MIN_CACHE_COUNT=%s",
                    count*2, g_Params.min_cache_count, OPTION_MIN_CACHE_COUNT
                    ));
            }
        }

        mgr.Release (); // should not be necessary - destructor should do job
        return 0;
    }

    // interpret exception processed by Utils::HandleException:
    // filter out some errors like invalid db - users don't want to
    // see such errors as actual errors
    int InterpretException ( int64_t rcCodeUtil, bool bSilent, char const* szErrDesc )
    {
        if ( rcCodeUtil == Utils::rcUnknown ||
             rcCodeUtil == Utils::rcErrorStdExc ||
             rcCodeUtil == Utils::rcInvalid)
        {
            if ( ! bSilent )
                LOGMSG ( klogErr, szErrDesc );

            return 3;
        }
        else if (rcCodeUtil == SILENT_RC( rcDB,rcMgr,rcOpening,rcDatabase,rcIncorrect ) ||
            rcCodeUtil == SILENT_RC( rcVFS,rcTree,rcResolving,rcPath,rcNotFound ))
        {
            if ( ! bSilent )
                LOGMSG ( klogWarn, szErrDesc );
            return 0;
        }
        else
        {
            if ( ! bSilent )
                LOGMSG ( klogErr, szErrDesc );
            return 3;
        }
    }

    int create_cache_db_impl_safe ()
    {
        try
        {
            return create_cache_db_impl ();
        }
        catch (...)
        {
            char szErrDesc [512];
            int64_t rc = Utils::HandleException ( true, szErrDesc, countof(szErrDesc) );
            return InterpretException ( rc, false, szErrDesc );
        }
    }

    int create_cache_db (int argc, char** argv)
    {
        try
        {
            KApp::CArgs args (argc, argv, Options, countof (Options), ::XMLLogger_Args, ::XMLLogger_ArgsQty);
            KApp::CXMLLogger xml_logger ( args );
            uint32_t param_count = args.GetParamCount ();
            if ( param_count != 2 )
            {
                MiniUsage (args.GetArgs());
                return 0;
            }

            g_Params.dbPathSrc = args.GetParamValue (0);
            g_Params.dbPathDst = args.GetParamValue (1);

            if (args.GetOptionCount (OPTION_ID_SPREAD_THRESHOLD))
                g_Params.id_spread_threshold = args.GetOptionValueUInt<int64_t> ( OPTION_ID_SPREAD_THRESHOLD, 0 );

            if (args.GetOptionCount (OPTION_CURSOR_CACHE_SIZE))
                g_Params.cursor_cache_size = 1024*1024 * args.GetOptionValueUInt<size_t> ( OPTION_CURSOR_CACHE_SIZE, 0 );

            if (args.GetOptionCount (OPTION_MIN_CACHE_COUNT))
                g_Params.min_cache_count = args.GetOptionValueUInt <size_t> ( OPTION_MIN_CACHE_COUNT, 0 );

            return create_cache_db_impl_safe ();
        }
        catch (...) // here we handle only exceptions in CArgs or CXMLLogger
        {
            return Utils::HandleException ( false, NULL, 0 );
        }
    }
}


extern "C"
{
    const char UsageDefaultName[] = "align-cache";
    rc_t CC UsageSummary (const char * progname)
    {
        printf (
        "Usage:\n"
        "  %s [options] <src-db-path> <new-cache-db-path>\n"
        "\n"
        "Summary:\n"
        "  Create a cache file for given database <src-db-path>\n"
        "  PRIMARY_ALIGNMENT table and save it into <new-cache-db-path>\n"
        "\n", progname);
        return 0;
    }
    const char* param_usage_src[] = { "Path to the database", NULL };
    const char* param_usage_dst[] = { "Path to the new cache database to be created", NULL };
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

        HelpParamLine ("src-db-path", param_usage_src);
        HelpParamLine ("new-cache-db-path", param_usage_dst);

        printf ("\nOptions:\n");

        HelpOptionLine (AlignCache::ALIAS_ID_SPREAD_THRESHOLD, AlignCache::OPTION_ID_SPREAD_THRESHOLD, "value", AlignCache::USAGE_ID_SPREAD_THRESHOLD);
        HelpOptionLine (NULL, AlignCache::OPTION_CURSOR_CACHE_SIZE, "value in MB", AlignCache::USAGE_CURSOR_CACHE_SIZE);
        HelpOptionLine (NULL, AlignCache::OPTION_MIN_CACHE_COUNT, "count", AlignCache::USAGE_MIN_CACHE_COUNT);
        XMLLogger_Usage();

        printf ("\n");

        HelpOptionsStandard ();

        HelpVersion (fullpath, KAppVersion());

        return rc;
    }

    MAIN_DECL(argc, argv)
    {
        VDB::Application app(argc, argv);

        SetUsage( Usage );
        SetUsageSummary( UsageSummary );

        return AlignCache::create_cache_db (argc, app.getArgV() );
    }
}
