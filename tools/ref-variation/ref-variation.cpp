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

#include "ref-variation.vers.h"
#include "helper.h"

#include <iostream>
#include <string.h>
#include <string>
#include <stdio.h>
#include <stdint.h>

#include <kapp/main.h>
#include <klib/rc.h>

namespace RefVariation
{
    struct Params
    {
        // command line params

        // command line options
        char const* ref_acc;
        int64_t ref_pos_var;
        char const* query;
        int verbosity;

    } g_Params =
    {
        "",
        -1,
        "",
        0
    };
    char const OPTION_REFERENCE_ACC[] = "reference-accession";
    char const ALIAS_REFERENCE_ACC[]  = "r";
    char const* USAGE_REFERENCE_ACC[] = { "look for the variation in this reference", NULL };

    char const OPTION_REF_POS[] = "position";
    char const ALIAS_REF_POS[]  = "p";
    char const* USAGE_REF_POS[] = { "look for the variation at this position on the reference", NULL };

    char const OPTION_QUERY[] = "query";
    char const ALIAS_QUERY[]  = "q";
    char const* USAGE_QUERY[] = { "query to find in the given reference", NULL };

    char const OPTION_VERBOSITY[] = "verbose";
    char const ALIAS_VERBOSITY[]  = "v";
    char const* USAGE_VERBOSITY[] = { "increase the verbosity of the program. use multiple times for more verbosity.", NULL };

    ::OptDef Options[] =
    {
        { OPTION_REFERENCE_ACC, ALIAS_REFERENCE_ACC, NULL, USAGE_REFERENCE_ACC, 1, true, true },
        { OPTION_REF_POS,       ALIAS_REF_POS,       NULL, USAGE_REF_POS,       1, true, true },
        { OPTION_QUERY,         ALIAS_QUERY,     NULL, USAGE_QUERY,         1, true, true }
        //{ OPTION_VERBOSITY,     ALIAS_VERBOSITY,     NULL, USAGE_VERBOSITY,     0, false, false }
    };

    void print_indel (char const* name, char const* text, size_t text_size, size_t indel_start, size_t indel_len)
    {
        printf ( "%s: %.*s[%.*s]%.*s\n",
                    name,
                    (int)indel_start, text,
                    (int)indel_len, text + indel_start,
                    (int)(text_size - (indel_start + indel_len)), text + indel_start + indel_len
               );
    }

    enum ColumnNameIndices
    {
//        idx_SEQ_LEN,
//        idx_SEQ_START,
        idx_MAX_SEQ_LEN,
        idx_TOTAL_SEQ_LEN,
        idx_READ
    };
    char const* g_ColumnNames[] =
    {
//        "SEQ_LEN",
//        "SEQ_START", // 1-based
        "MAX_SEQ_LEN",
        "TOTAL_SEQ_LEN",
        "READ"
    };
    uint32_t g_ColumnIndex [ countof (g_ColumnNames) ];


    std::string get_ref_slice_int (
                            VDBObjects::CVCursor const& cursor,
                            int64_t ref_start,
                            int64_t ref_end,
                            uint32_t max_seq_len
                           )
    {
        int64_t ref_start_id = ref_start / max_seq_len + 1;
        int64_t ref_start_chunk_pos = ref_start % max_seq_len;

        int64_t ref_end_id = ref_end / max_seq_len + 1;
        int64_t ref_end_chunk_pos = ref_end % max_seq_len;

        std::string ret;
        ret.reserve( ref_end - ref_start + 2 );

        for ( int64_t id = ref_start_id; id <= ref_end_id; ++id )
        {
            int64_t chunk_pos_start = id == ref_start_id ?
                        ref_start_chunk_pos : 0;
            int64_t chunk_pos_end = id == ref_end_id ?
                        ref_end_chunk_pos : max_seq_len - 1;

            char const* pREAD;
            uint32_t count =  cursor.CellDataDirect ( id, g_ColumnIndex[idx_READ], & pREAD );
            assert (count > ref_end_chunk_pos);
            (void)count;

            ret.append (pREAD + chunk_pos_start, pREAD + chunk_pos_end + 1);
        }

        return ret;
    }

    std::string get_ref_slice ( VDBObjects::CVCursor const& cursor,
                                int64_t ref_pos_var, // reported variation position on the reference
                                int64_t var_len,     // the length of the reported variation
                                int64_t slice_expand_left,
                                int64_t slice_expand_right,
                                int64_t* ref_slice_start, // returned value
                                int64_t* ref_slice_end    // returned value, inclusive
                              )
    {
        int64_t idRow = 0;
        uint64_t nRowCount = 0;

        cursor.GetIdRange (idRow, nRowCount);

        uint32_t max_seq_len;
        uint64_t total_seq_len;
        cursor.ReadItems ( idRow, g_ColumnIndex[idx_MAX_SEQ_LEN], & max_seq_len, 1 );
        cursor.ReadItems ( idRow, g_ColumnIndex[idx_TOTAL_SEQ_LEN], & total_seq_len, 1 );

        if ( (uint64_t) g_Params.ref_pos_var > total_seq_len )
        {
            throw Utils::CErrorMsg ("reference position (%ld) is greater than total sequence length (%ud)",
                                    g_Params.ref_pos_var, total_seq_len);
        }

        int64_t var_half_len = var_len / 2 + 1;

        int64_t ref_start = ref_pos_var - var_half_len - slice_expand_left >= 0 ?
                            ref_pos_var - var_half_len - slice_expand_left : 0;
        int64_t ref_end = (uint64_t)(ref_pos_var + var_half_len + slice_expand_right - 1) < total_seq_len ?
                          ref_pos_var + var_half_len + slice_expand_right - 1 : total_seq_len - 1;

        std::string ret = get_ref_slice_int ( cursor, ref_start, ref_end, max_seq_len );

        if ( ref_slice_start != NULL )
            *ref_slice_start = ref_start;
        if ( ref_slice_end != NULL )
            *ref_slice_end = ref_end;

        return ret;
    }

    // now it works for pure insertions (no mismatches/deletions)
    std::string make_query ( std::string const& ref_slice,
                             char const* variation, size_t var_len,
                             int64_t var_start_pos_adj // ref_pos adjusted to the beginning of ref_slice (in the simplest case - the middle of ref_slice)
                           )
    {
        std::string ret;
        ret.reserve ( ref_slice.size() + var_len );

        ret.append ( ref_slice.begin(), ref_slice.begin() + var_start_pos_adj );
        ret.append ( variation, variation + var_len );
        ret.append ( ref_slice.begin() + var_start_pos_adj, ref_slice.end() );

        return ret;
    }

    std::string find_variation_region_impl ()
    {
        VDBObjects::CVDBManager mgr;
        mgr.Make();

        VDBObjects::CVTable table = mgr.OpenTable ( g_Params.ref_acc );
        VDBObjects::CVCursor cursor = table.CreateCursorRead ();

        cursor.InitColumnIndex (g_ColumnNames, g_ColumnIndex, countof(g_ColumnNames));
        cursor.Open();

        int64_t var_len = strlen (g_Params.query);
        int64_t slice_start = -1, slice_end = -1;
        int64_t add_l = 0, add_r = 0;

        while ( true )
        {
            int64_t new_slice_start, new_slice_end;
            std::string ref_slice = get_ref_slice (cursor, g_Params.ref_pos_var, var_len, add_l, add_r, & new_slice_start, & new_slice_end );
            std::string query = make_query ( ref_slice, g_Params.query, var_len, g_Params.ref_pos_var - new_slice_start );

            std::cout
                << "Looking for query \"" << query
                << "\" at the reference around pos=" << g_Params.ref_pos_var
                << " [" << new_slice_start << ", " << new_slice_end << "]"
                << ": \"" << ref_slice << "\"..." << std::endl;

            size_t ref_start, ref_len, query_start, query_len;
            KSearch::FindRefVariationBounds ( ref_slice, query, ref_start, ref_len, query_start, query_len );
            
            printf ("indel box found lib: ref: (%lu, %lu), query: (%lu, %lu)\n",
                ref_start, ref_len, query_start, query_len );
            print_indel ( "reference", ref_slice.c_str(), ref_slice.size(), ref_start, ref_len );
            print_indel ( "query    ", query.c_str(), query.size(), query_start, query_len );

            bool cont = false;

            if ( ref_start == 0)
            {
                if ( slice_start != -1 && new_slice_start == slice_start )
                    std::cout << "cannot expand to the left anymore" <<std::endl;
                else
                {
                    std::cout << "expanding the window to the left..." << std::endl;
                    add_l += 2;
                    cont = true;
                }
            }
            if ( ref_start + ref_len == ref_slice.size() )
            {
                if ( slice_end != -1 && new_slice_end == slice_end )
                    std::cout << "cannot expand to the right anymore" <<std::endl;
                else
                {
                    std::cout << "expanding the window to the right..." << std::endl;
                    add_r += 2;
                    cont = true;
                }
            }

            if ( !cont )
                return query.substr( query_start, query_len );

            slice_start = new_slice_start;
            slice_end = new_slice_end;
        }

        //printf ("Reference slice [%ld, %ld]: %s\n", slice_start, slice_end, ref_slice.c_str() );

    } 

    void find_variation_region (int argc, char** argv)
    {
        try
        {
            KApp::CArgs args (argc, argv, Options, countof (Options));
            uint32_t param_count = args.GetParamCount ();
            if ( param_count != 0 )
            {
                MiniUsage (args.GetArgs());
                return;
            }

            // Actually GetOptionCount check is not needed here since
            // CArgs checks that exactly 1 option is required

            if (args.GetOptionCount (OPTION_REFERENCE_ACC) == 1)
                g_Params.ref_acc = args.GetOptionValue ( OPTION_REFERENCE_ACC, 0 );

            if (args.GetOptionCount (OPTION_REF_POS) == 1)
                g_Params.ref_pos_var = args.GetOptionValueInt<int64_t> ( OPTION_REF_POS, 0 );

            if (args.GetOptionCount (OPTION_QUERY) == 1)
                g_Params.query = args.GetOptionValue ( OPTION_QUERY, 0 );

            g_Params.verbosity = (int)args.GetOptionCount (OPTION_VERBOSITY);

            std::string full_var = find_variation_region_impl ();

            std::cout << "Full variation: " << full_var << std::endl;
        }
        catch (...)
        {
            Utils::HandleException ();
        }
    }
}

extern "C"
{
    const char UsageDefaultName[] = "ref-variation";
    ver_t CC KAppVersion ()
    {
        return REF_VARIATION_VERS;
    }

    rc_t CC UsageSummary (const char * progname)
    {
        printf (
        "Usage example:\n"
        "  %s -r <reference accession> -p <position on reference> -q <query to look for>\n"
        "\n"
        "Summary:\n"
        "  Find a possible indel window\n"
        "\n", progname);
        return 0;
    }

    rc_t CC Usage ( struct Args const * args )
    {
        rc_t rc = 0;
        const char* progname = UsageDefaultName;
        const char* fullpath = UsageDefaultName;

        if (args == NULL)
            rc = RC(rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
        else
            rc = ArgsProgram(args, &fullpath, &progname);

        UsageSummary (progname);

        printf ("\nOptions:\n");

        HelpOptionLine (RefVariation::ALIAS_REFERENCE_ACC, RefVariation::OPTION_REFERENCE_ACC, "acc", RefVariation::USAGE_REFERENCE_ACC);
        HelpOptionLine (RefVariation::ALIAS_REF_POS, RefVariation::OPTION_REF_POS, "value", RefVariation::USAGE_REF_POS);
        HelpOptionLine (RefVariation::ALIAS_QUERY, RefVariation::OPTION_QUERY, "string", RefVariation::USAGE_QUERY);
        //HelpOptionLine (RefVariation::ALIAS_VERBOSITY, RefVariation::OPTION_VERBOSITY, "", RefVariation::USAGE_VERBOSITY);

        printf ("\n");

        HelpOptionsStandard ();

        HelpVersion (fullpath, KAppVersion());

        return rc;
    }

    rc_t CC KMain ( int argc, char *argv [] )
    {
        /* command line examples:
          -r NC_011752.1 -p 2018 -q CA
          -r NC_011752.1 -p 2020 -q CA
          -r NC_011752.1 -p 5000 -q CA*/

        RefVariation::find_variation_region ( argc, argv );
        return 0;
    }
}
