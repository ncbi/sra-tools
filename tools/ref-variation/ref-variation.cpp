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

#define IMPLEMENT_IN_NGS 1
#define SECRET_OPTION 0

#include "ref-variation.vers.h"

#include "helper.h"

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <string>
#include <vector>

#include <kapp/main.h>
#include <klib/rc.h>

#if IMPLEMENT_IN_NGS != 0 || SECRET_OPTION != 0
#include <ngs/ncbi/NGS.hpp>
#include <ngs/ReferenceSequence.hpp>
#endif

namespace RefVariation
{
    struct Params
    {
        // command line params

        // command line options
        char const* ref_acc;
        int64_t ref_pos_var;
        char const* query;
        size_t var_len_on_ref;
        int verbosity;

    } g_Params =
    {
        "",
        -1,
        "",
        0,
        0
    };

#if SECRET_OPTION != 0
    char const OPTION_SECRET[] = "alt-ctrl-shift-f12";
    char const* USAGE_SECRET[] = { "activate secret mode", NULL };
#endif

    char const OPTION_REFERENCE_ACC[] = "reference-accession";
    char const ALIAS_REFERENCE_ACC[]  = "r";
    char const* USAGE_REFERENCE_ACC[] = { "look for the variation in this reference", NULL };

    char const OPTION_REF_POS[] = "position";
    char const ALIAS_REF_POS[]  = "p";
    char const* USAGE_REF_POS[] = { "look for the variation at this position on the reference", NULL };

    char const OPTION_QUERY[] = "query";
    char const ALIAS_QUERY[]  = "q";
    char const* USAGE_QUERY[] = { "query to find in the given reference (\"-\" is treated as empty string - deletion)", NULL };

    char const OPTION_VAR_LEN_ON_REF[] = "variation-length";
    char const ALIAS_VAR_LEN_ON_REF[]  = "l";
    char const* USAGE_VAR_LEN_ON_REF[] = { "the length of the variation on the reference (0 - insertion)", NULL };

    char const OPTION_VERBOSITY[] = "verbose";
    char const ALIAS_VERBOSITY[]  = "v";
    char const* USAGE_VERBOSITY[] = { "increase the verbosity of the program. use multiple times for more verbosity.", NULL };

    ::OptDef Options[] =
    {
        { OPTION_REFERENCE_ACC, ALIAS_REFERENCE_ACC, NULL, USAGE_REFERENCE_ACC, 1, true, true },
        { OPTION_REF_POS,       ALIAS_REF_POS,       NULL, USAGE_REF_POS,       1, true, true },
        { OPTION_QUERY,         ALIAS_QUERY,         NULL, USAGE_QUERY,         1, true, true },
        { OPTION_VAR_LEN_ON_REF,ALIAS_VAR_LEN_ON_REF,NULL, USAGE_VAR_LEN_ON_REF,1, true, true }
#if SECRET_OPTION != 0
        ,{ OPTION_SECRET,        NULL,                NULL, USAGE_SECRET,        1, true, false }
#endif
        //,{ OPTION_VERBOSITY,     ALIAS_VERBOSITY,     NULL, USAGE_VERBOSITY,     0, false, false }
    };

    void print_indel (char const* name, char const* text, size_t text_size, size_t indel_start, size_t indel_len)
    {
        int prefix_count = indel_start < 5 ? indel_start : 5;
        int suffix_count = (text_size - (indel_start + indel_len)) < 5 ?
                (text_size - (indel_start + indel_len)) : 5;

        printf ( "%s: %s%.*s[%.*s]%.*s%s\n",
                    name,
                    indel_start < 5 ? "" : "...",
                    prefix_count, text + indel_start - prefix_count, //(int)indel_start, text,
                    (int)indel_len, text + indel_start,
                    suffix_count, text + indel_start + indel_len, //(int)(text_size - (indel_start + indel_len)), text + indel_start + indel_len
                    (text_size - (indel_start + indel_len)) > 5 ? "..." : ""
               );
    }

#if SECRET_OPTION != 0 || IMPLEMENT_IN_NGS == 0
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
#endif

#if 0
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
#endif


#if IMPLEMENT_IN_NGS != 0

    ngs::String compose_query_adjusted (ngs::String const& ref,
        size_t ref_start, size_t ref_len,
        char const* query, size_t query_len, int64_t ref_pos_var, size_t var_len_on_ref)
    {
        ngs::String ret;
        ret.reserve (ref_len + query_len - var_len_on_ref); // TODO: not always correct

        char const* query_adj = query;
        size_t query_len_adj = query_len;

        //size_t ref_len

        if ( (size_t)ref_pos_var > ref_start )
        {
            // if extended window starts to the left from initial reported variation start
            // then include preceding bases into adjusted variation
            ret.assign ( ref.c_str() + ref_start, (size_t)ref_pos_var - ref_start );
        }
        else if ( (size_t)ref_pos_var < ref_start )
        {
            // the real window of ambiguity actually starts to the right from
            // the reported variation start
            // let's not to include the left unambigous part into
            // adjusted variation (?)

            query_adj += ref_start - ref_pos_var;
            query_len_adj -= ref_start - ref_pos_var;
        }

        //if ( (int64_t)ret.length() > (int64_t)ref_len - (int64_t)var_len_on_ref )
        //    query_len_adj -= ret.length() - (ref_len - var_len_on_ref);

        if ( query_len_adj > 0 )
            ret.append ( query_adj, query_len_adj );

        if ( (int64_t)(ref_len - ((size_t)ref_pos_var - ref_start) - var_len_on_ref) > 0 )
        {
            ret.append ( ref.c_str() + (size_t)ref_pos_var + var_len_on_ref,
                ref_len - ((size_t)ref_pos_var - ref_start) - var_len_on_ref );
        }

        return ret;
    }

    enum PileupColumnNameIndices
    {
        idx_DELETION_COUNT,
        idx_DEPTH,
        idx_INSERTION_COUNTS,
        idx_MISMATCH_COUNTS,
        idx_REFERENCE_SPEC,
        idx_REF_POS,
        idx_RUN_NAME
    };
    char const* g_PileupColumnNames[] =
    {
        "DELETION_COUNT",
        "DEPTH",
        "INSERTION_COUNTS",
        "MISMATCH_COUNTS",
        "REFERENCE_SPEC",
        "REF_POS",
        "RUN_NAME"
    };
    uint32_t g_PileupColumnIndex [ countof (g_PileupColumnNames) ];


    bool filter_pileup_db ( char const* acc, char const* ref_name,
                size_t ref_pos, char const* query, size_t query_len,
                std::vector <std::string>& vec)
    {
        std::cout << "Processing " << acc << "... ";// << std::endl;

        VDBObjects::CVDBManager mgr;
        mgr.Make();

        try
        {
        VDBObjects::CVDatabase db = mgr.OpenDB ( acc );
        VDBObjects::CVTable table = db.OpenTable ( "STATS" );
        VDBObjects::CVCursor cursor = table.CreateCursorRead ();

        cursor.InitColumnIndex (g_PileupColumnNames, g_PileupColumnIndex, countof(g_PileupColumnNames));
        cursor.Open();

        int64_t id_first = 0;
        uint64_t row_count = 0;

        cursor.GetIdRange (id_first, row_count);

        // Find Reference beginning

#if REF_LINEAR_SEARCH != 0
        //int64_t ref_pos_;
        //uint32_t depth;
        //uint32_t count;
        char ref_spec[64];
        int64_t ref_id_start = -1;
        int64_t row_id;
        for ( row_id = id_first; row_id < id_first + (int64_t)row_count; ++ row_id )
        {
            uint32_t count = cursor.ReadItems ( row_id, g_PileupColumnIndex[idx_REFERENCE_SPEC], ref_spec, countof(ref_spec)-1 );
            assert (count < countof(ref_spec));
            ref_spec [count] = '\0';

            if ( strcmp (ref_spec, ref_name) == 0 && ref_id_start == -1 )
            {
                ref_id_start = row_id;
                break;
            }
        }
        if ( row_id == id_first + (int64_t)row_count )
            return false;

#else
        KDBObjects::CKTable ktbl = table.OpenKTableRead();
        KDBObjects::CKIndex kindex = ktbl.OpenIndexRead("ref_spec");

        int64_t ref_id_start;
        uint64_t id_count;

        bool found = kindex.FindText ( ref_name, & ref_id_start, & id_count, NULL, NULL );
        std::cout
            << (found ? "" : " not") << " found " << ref_name << " row_id=" << ref_id_start
            << ", id_count=" << id_count << " "; // <<  std::endl;
        if ( !found )
        {
            std::cout << std::endl;
            return false;
        }

#endif


        // check depth > 0 for every position of the region
        for ( int64_t pos = (int64_t)ref_pos; pos < (int64_t)( ref_pos + query_len ); ++pos )
        {
            if ( pos + ref_id_start >= id_first + (int64_t)row_count )
            {
                std::cout << "OUT OF BOUNDS! filtering out" << std::endl;
                return false; // went beyond the end of db, probably, it's a bug in db
            }
            uint32_t depth;
            uint32_t count = cursor.ReadItems ( pos + ref_id_start, g_PileupColumnIndex[idx_DEPTH], & depth, sizeof depth );

            if ( count == 0 || depth == 0 )
            {
                std::cout << "depth=0 at the ref_pos=" << pos
                    << " (id=" << pos + ref_id_start << ") filtering out" << std::endl;
                return false;
            }
        }

        char run_name[64];
        uint32_t count = cursor.ReadItems ( id_first, g_PileupColumnIndex[idx_RUN_NAME], run_name, countof(run_name)-1 );
        assert (count < countof(run_name));
        run_name [count] = '\0';


        std::cout << run_name << " is suspicious" << std::endl;
        char const* p = run_name[0] == '/' ? run_name + 1 : run_name;

        vec.push_back ( std::string(p) );

        return true;
        }
        catch ( Utils::CErrorMsg const& e )
        {
            if ( e.getRC() == SILENT_RC(rcDB,rcMgr,rcOpening,rcDatabase,rcIncorrect))
            {
                std::cout
                    << "BAD db, filtering out" << std::endl;
                return false;
            }
            else
                throw;
        }
    }

    std::vector <std::string> get_acc_list (KApp::CArgs const& args, char const* ref_name, size_t ref_pos, char const* query, size_t query_len)
    {
        size_t param_count = args.GetParamCount();
        
        std::cout << param_count << " pileup database" << (param_count == 1 ? "" : "s")
            << " to search for" << std::endl;

        std::vector <std::string> vec_acc;

        for ( size_t i = 0; i < param_count; ++i)
        {
            char const* acc = args.GetParamValue( i );
            filter_pileup_db ( acc, ref_name, ref_pos, query, query_len, vec_acc );
        }
        
        return vec_acc;
    }

    void find_alignments ( KApp::CArgs const& args, char const* ref_name, size_t ref_pos, char const* query, size_t query_len )
    {
        std::vector <std::string> vec_acc = get_acc_list ( args, ref_name, ref_pos, query, query_len );

        std::cout << "Looking for \""  << query << "\" in the selected runs (" << vec_acc.size() << ")" << std::endl;
        for ( std::vector <std::string>::const_iterator cit = vec_acc.begin(); cit != vec_acc.end(); ++cit )
        {
            std::string const& acc = (*cit);
            ncbi::ReadCollection run = ncbi::NGS::openReadCollection ( acc );

            ngs::Reference reference = run.getReference( ref_name );
            ngs::AlignmentIterator ai = reference.getAlignmentSlice ( ref_pos, query_len, ngs::Alignment::all );

            while ( ai.nextAlignment() )
            {
                ngs::String id = ai.getAlignmentId ().toString();
                ngs::String bases = ai.getFragmentBases( ref_pos - ai.getAlignmentPosition(), query_len ).toString();
                bool match = strncmp (query, bases.c_str(), query_len) == 0;
                std::cout << "id=" << id
                    << ": "
                    << bases
                    << (match ? " MATCH!" : "")
                    << std::endl;
            }
        }
    }

    void finish_find_variation_region_impl ( KApp::CArgs const & args, size_t var_len,
        const ngs::String & ref, size_t bases_start, size_t ref_start, size_t ref_len )
    {
        std::cout << "Found indel box at pos=" << ref_start << ", length=" << ref_len << std::endl;
        assert ( ref_start >= bases_start );
        print_indel ( "reference", ref.c_str(), ref.size(), ref_start - bases_start, ref_len );

        ngs::String var_query = compose_query_adjusted ( ref, ref_start - bases_start, ref_len,
            g_Params.query, var_len, g_Params.ref_pos_var - bases_start, g_Params.var_len_on_ref );

        std::cout << "var_query=" << var_query << std::endl;

        std::cout
            << "Input variation spec: "
            << g_Params.ref_acc << ":"
            << g_Params.ref_pos_var << ":"
            << g_Params.var_len_on_ref << ":"
            << g_Params.query
            << std::endl;

        size_t ver_len_on_ref_adj = g_Params.var_len_on_ref;
        if ( (int64_t)ref_start > g_Params.ref_pos_var )
            ver_len_on_ref_adj -= ref_start - g_Params.ref_pos_var;

        std::cout
            << "Adjusted variation spec: "
            << g_Params.ref_acc << ":"
            << ref_start << ":"
            << ver_len_on_ref_adj << ":"
            << var_query
            << std::endl;

        find_alignments (args, g_Params.ref_acc, ref_start, var_query.c_str(), var_query.length());
    }

    void find_variation_region_impl (KApp::CArgs const& args)
    {
        ngs::ReferenceSequence ref_seq = ncbi::NGS::openReferenceSequence ( g_Params.ref_acc );

        size_t var_len = strlen (g_Params.query);

        size_t bases_start = g_Params.ref_pos_var > 1000 ? g_Params.ref_pos_var - 1000 : 0;
        ngs::StringRef ref1 = ref_seq.getReferenceChunk ( bases_start );

        size_t ref_start, ref_len;

        bool failed = true;
        if ( bases_start + ref1 . size () >= (size_t)g_Params.ref_pos_var + 1000 )
        {
            failed = false;
            try
            {
                KSearch::FindRefVariationRegionAscii ( ref1.data(), ref1.size(), g_Params.ref_pos_var - bases_start,
                    g_Params.query, var_len, g_Params.var_len_on_ref, & ref_start, & ref_len );
                ref_start += bases_start;
            }
            catch ( ... )
            {
                failed = true;
            }
        }

        if ( ! failed )
        {
            finish_find_variation_region_impl ( args, var_len, ref1.toString (), bases_start, ref_start, ref_len );
        }

        else
        {
            failed = false;
            ngs::String ref2;

            try
            {
                bases_start = g_Params.ref_pos_var > 5000 ? g_Params.ref_pos_var - 5000 : 0;
                ref2 = ref_seq.getReferenceBases( bases_start, 15000 );
                KSearch::FindRefVariationRegionAscii ( ref2.data(), ref2.size(), g_Params.ref_pos_var - bases_start,
                    g_Params.query, var_len, g_Params.var_len_on_ref, & ref_start, & ref_len );
                ref_start += bases_start;
            }
            catch ( ... )
            {
                failed = true;
            }

            if ( failed )
            {
                bases_start = 0;
                ref2 = ref_seq.getReferenceBases( 0 );
                KSearch::FindRefVariationRegionAscii ( ref2.data(), ref2.size(), g_Params.ref_pos_var,
                    g_Params.query, var_len, g_Params.var_len_on_ref, & ref_start, & ref_len );
            }

            finish_find_variation_region_impl ( args, var_len, ref2, bases_start, ref_start, ref_len );
        }
    }

#else // VDB implementation
    std::string get_ref_chunk ( int64_t ref_row_id, VDBObjects::CVCursor const& cursor )
    {
        char const* pREAD;
        uint32_t count =  cursor.CellDataDirect ( ref_row_id, g_ColumnIndex[idx_READ], & pREAD );
        return std::string ( pREAD, count );
    }


    void find_variation_region_impl (KApp::CArgs const& args)
    {
        VDBObjects::CVDBManager mgr;
        mgr.Make();

        VDBObjects::CVTable table = mgr.OpenTable ( g_Params.ref_acc );
        VDBObjects::CVCursor cursor = table.CreateCursorRead ();

        cursor.InitColumnIndex (g_ColumnNames, g_ColumnIndex, countof(g_ColumnNames));
        cursor.Open();

        int64_t id_first = 0;
        uint64_t row_count = 0;

        cursor.GetIdRange (id_first, row_count);

        uint32_t max_seq_len;
        uint64_t total_seq_len;
        cursor.ReadItems ( id_first, g_ColumnIndex[idx_MAX_SEQ_LEN], & max_seq_len, 1 );
        cursor.ReadItems ( id_first, g_ColumnIndex[idx_TOTAL_SEQ_LEN], & total_seq_len, 1 );

        if ( (uint64_t) g_Params.ref_pos_var > total_seq_len )
        {
            throw Utils::CErrorMsg ("reference position (%ld) is greater than total sequence length (%ud)",
                                    g_Params.ref_pos_var, total_seq_len);
        }

        int64_t var_len = strlen (g_Params.query);

        std::string ref;
        ref.reserve( max_seq_len + 1);
        int64_t id = g_Params.ref_pos_var / max_seq_len + id_first;
        size_t ref_pos_adj = g_Params.ref_pos_var % max_seq_len;

        int64_t id_start = id, id_end = id;

        ref = get_ref_chunk ( id, cursor );
        size_t ref_start, ref_len;
        while ( true )
        {
            KSearch::FindRefVariationRegionAscii ( ref, ref_pos_adj,
                g_Params.query, var_len, g_Params.var_len_on_ref, ref_start, ref_len );

            std::cout
                << "id_start=" << id_start
                << ", id_end=" << id_end
                << ", ref_pos_adj=" << ref_pos_adj
                << std::endl;
            std::cout << "Found indel box at pos=" << ref_start << ", length=" << ref_len << std::endl;
            print_indel ( "reference", ref.c_str(), ref.size(), ref_start, ref_len );

            bool cont = false;
            if ( ref_start == 0 && id > id_first )
            {
                --id_start;
                ref_pos_adj += max_seq_len;

                char const* pREAD;
                uint32_t count_read =  cursor.CellDataDirect ( id_start, g_ColumnIndex[idx_READ], & pREAD );
                ref.insert( 0, pREAD, count_read );
                //ref.insert( 0, get_ref_chunk ( id_start, cursor ).c_str() );

                cont = true;
                std::cout
                    << "extending to the left, id_start == " << id_start
                    << "; adjusting ref_pos to " << ref_pos_adj << std::endl;

            }
            if ( ref_start + ref_len == ref.size() && id_end < id_first + (int64_t)row_count - 1 )
            {
                ++id_end;

                char const* pREAD;
                uint32_t count_read =  cursor.CellDataDirect ( id_end, g_ColumnIndex[idx_READ], & pREAD );
                ref.append ( pREAD, count_read );
//                ref.append ( get_ref_chunk ( id_end, cursor ) );

                cont = true;
                std::cout
                    << "extending to the right, id_end == " << id_end << std::endl;

            }
            if ( !cont )
                break;
        }

        // adjust ref_start to absolute zero-based value
        if ( id_start > id_first )
            ref_start += max_seq_len * ( id_start - id_first );
        

#if 0
        {
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
            KSearch::FindRefVariationRegionAscii ( ref_slice, g_Params.ref_pos_var - new_slice_start, g_Params.query, var_len, ref_start, ref_len );
            
            printf ("indel box found lib: ref: (%lu, %lu), query: (%lu, %lu)\n",
                ref_start, ref_len, query_start, query_len );
            print_indel ( "reference", ref_slice.c_str(), ref_slice.size(), ref_start, ref_len );
            //print_indel ( "query    ", query.c_str(), query.size(), query_start, query_len );

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
                break;

            slice_start = new_slice_start;
            slice_end = new_slice_end;
        }
        }
#endif

    }
#endif

#if SECRET_OPTION != 0 || IMPLEMENT_IN_NGS == 0
    void get_ref_bases ( int64_t offset, uint64_t len, std::string & ret,
        VDBObjects::CVCursor const& cursor,
        int64_t id_first, uint64_t row_count,
        uint32_t max_seq_len, uint64_t total_seq_len
        )
    {
        if ( offset + len > total_seq_len )
            len = total_seq_len - offset;

        ret.clear();
        if ( ret.capacity() < len )
            ret.reserve ( ret.capacity() * 2 );

        int64_t id_start = offset / max_seq_len + id_first;
        int64_t id_end = (offset + len - 1) / max_seq_len + id_first;
        if ( id_start == id_end ) // optimizing: have a separate branch for single-row slice
        {
            char const* pREAD;
            cursor.CellDataDirect ( id_start, g_ColumnIndex[idx_READ], & pREAD );
            int64_t offset_adj = offset % max_seq_len;
            ret.assign( pREAD + offset_adj, len );
        }
        else // multi-row reference slice
        {
            // first id
            char const* pREAD;
            uint32_t count_read =  cursor.CellDataDirect ( id_start, g_ColumnIndex[idx_READ], & pREAD );
            int64_t offset_adj = offset % max_seq_len;
            ret.assign( pREAD + offset_adj, count_read - offset_adj );

            // intermediate id (full max_seq_len chunks)
            for (int64_t id = id_start + 1; id < id_end; ++id )
            {
                count_read = cursor.CellDataDirect ( id, g_ColumnIndex[idx_READ], & pREAD );
                ret.append ( pREAD, count_read );
            }

            // last id
            count_read =  cursor.CellDataDirect ( id_end, g_ColumnIndex[idx_READ], & pREAD );
            ret.append( pREAD, len - ret.length() );
        }
    }

#endif

#if SECRET_OPTION != 0
    void test_ngs ()
    {
        std::cout << "Starting NGS test..." << std::endl;
        ngs::ReferenceSequence ref = ncbi::NGS::openReferenceSequence ( g_Params.ref_acc );

        uint64_t len = 1;
        int64_t offset = ref.getLength()/2 - len;
        std::string slice;
        for ( ; offset >= 0; --offset, len += 2 )
        {
            slice = ref.getReferenceBases( offset, len );
            // making something that prevents the optimizer from eliminating the code
            if ( slice.length() != len )
                std::cout << "slice.length() != len" << std::endl;
        }
        std::cout << "NGS test has SUCCEEDED" << std::endl;
    }

    void test_vdb ()
    {
        std::cout << "Starting VDB test..." << std::endl;

        VDBObjects::CVDBManager mgr;
        mgr.Make();

        VDBObjects::CVTable table = mgr.OpenTable ( g_Params.ref_acc );
        VDBObjects::CVCursor cursor = table.CreateCursorRead ();

        cursor.InitColumnIndex (g_ColumnNames, g_ColumnIndex, countof(g_ColumnNames));
        cursor.Open();

        int64_t id_first = 0;
        uint64_t row_count = 0;

        cursor.GetIdRange (id_first, row_count);

        uint32_t max_seq_len;
        uint64_t total_seq_len;
        cursor.ReadItems ( id_first, g_ColumnIndex[idx_MAX_SEQ_LEN], & max_seq_len, 1 );
        cursor.ReadItems ( id_first, g_ColumnIndex[idx_TOTAL_SEQ_LEN], & total_seq_len, 1 );

        uint64_t len = 1;
        int64_t offset = total_seq_len/2 - len;
        std::string slice;
        int64_t offset_increment = 1;
        for ( ; offset >= 0; offset -= offset_increment, len += 2 * offset_increment )
        {
            get_ref_bases ( offset, len, slice, cursor, id_first, row_count, max_seq_len, total_seq_len );
            // making something that prevents the optimizer from eliminating the code
            if ( slice.length() != len )
                std::cout << "slice.length() != len" << std::endl;
        }
        std::cout << "VDB test has SUCCEEDED" << std::endl;

    }

    void test_correctness ()
    {
        std::cout << "Running correctness test..." << std::endl;
        ngs::ReferenceSequence ref_ngs = ncbi::NGS::openReferenceSequence ( g_Params.ref_acc );

        VDBObjects::CVDBManager mgr;
        mgr.Make();

        VDBObjects::CVTable table = mgr.OpenTable ( g_Params.ref_acc );
        VDBObjects::CVCursor cursor = table.CreateCursorRead ();

        cursor.InitColumnIndex (g_ColumnNames, g_ColumnIndex, countof(g_ColumnNames));
        cursor.Open();

        int64_t id_first = 0;
        uint64_t row_count = 0;

        cursor.GetIdRange (id_first, row_count);

        uint32_t max_seq_len;
        uint64_t total_seq_len;
        cursor.ReadItems ( id_first, g_ColumnIndex[idx_MAX_SEQ_LEN], & max_seq_len, 1 );
        cursor.ReadItems ( id_first, g_ColumnIndex[idx_TOTAL_SEQ_LEN], & total_seq_len, 1 );

        if ( ref_ngs.getLength() != total_seq_len )
        {
            std::cout
                << "Correctness test FAILED: length discrepancy: ngs="
                << ref_ngs.getLength() << ", vdb=" << total_seq_len << std::endl;
            return;
        }

        uint64_t len = 1;
        int64_t offset = total_seq_len/2 - len;
        std::string slice_vdb;
        for ( ; offset >= 0; --offset, len += 2 )
        {
            ngs::String slice_ngs = ref_ngs.getReferenceBases( offset, len );
            get_ref_bases ( offset, len, slice_vdb, cursor, id_first, row_count, max_seq_len, total_seq_len );

            if (slice_ngs != slice_vdb)
            {
                std::cout
                    << "Correctness test FAILED: slice discrepancy at "
                    "offset=" << offset << ", length=" << len << std::endl
                    << "ngs=" << slice_ngs << std::endl
                    << "vdb=" << slice_vdb << std::endl;
                return;
            }
        }
        std::cout << "Correctness test has SUCCEEDED" << std::endl;
    }
#endif

    void find_variation_region (int argc, char** argv)
    {
        try
        {
            KApp::CArgs args (argc, argv, Options, countof (Options));
            /*uint32_t param_count = args.GetParamCount ();
            if ( param_count != 0 )
            {
                MiniUsage (args.GetArgs());
                return;
            }*/

            // Actually GetOptionCount check is not needed here since
            // CArgs checks that exactly 1 option is required

            if (args.GetOptionCount (OPTION_REFERENCE_ACC) == 1)
                g_Params.ref_acc = args.GetOptionValue ( OPTION_REFERENCE_ACC, 0 );

            if (args.GetOptionCount (OPTION_REF_POS) == 1)
                g_Params.ref_pos_var = args.GetOptionValueInt<int64_t> ( OPTION_REF_POS, 0 );

            if (args.GetOptionCount (OPTION_QUERY) == 1)
            {
                g_Params.query = args.GetOptionValue ( OPTION_QUERY, 0 );
                // TODO: maybe CArgs should allow for empty option value
                if (g_Params.query [0] == '-' && g_Params.query [1] == '\0' )
                    g_Params.query = "";
            }

            if (args.GetOptionCount (OPTION_VAR_LEN_ON_REF) == 1)
                g_Params.var_len_on_ref = args.GetOptionValueUInt<size_t>( OPTION_VAR_LEN_ON_REF, 0 );

            g_Params.verbosity = (int)args.GetOptionCount (OPTION_VERBOSITY);

#if SECRET_OPTION != 0
            if ( args.GetOptionCount (OPTION_SECRET) > 0 )
            {
                std::cout << "Running ref-variation in secret mode" << std::endl;

                switch ( args.GetOptionValueInt<int> (OPTION_SECRET, 0) )
                {
                case 1:
                    test_correctness ();
                    break;
                case 2:
                    test_ngs ();
                    break;
                case 3:
                    test_vdb ();
                    break;
                default:
                    std::cout
                        << "specify value for this option:" << std::endl
                        << "1 - run correctness test" << std::endl
                        << "2 - run ngs performance test" << std::endl
                        << "3 - run vdb performance test" << std::endl;
                }
            }
            else
#endif
            {
                find_variation_region_impl (args);
            }

        }
#if IMPLEMENT_IN_NGS != 0 || SECRET_OPTION != 0
        catch ( ngs::ErrorMsg const& e )
        {
            std::cout << "ngs::ErrorMsg: " << e.what() << std::endl;
        }
#endif
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
        "  %s -r <reference accession> -p <position on reference> -q <query to look for> -l 0 [<parameters>]\n"
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


        printf ("\nParameters: optional space-separated list of pileup stats in which the query will be looked for\n\n");

        printf ("\nOptions:\n");

        HelpOptionLine (RefVariation::ALIAS_REFERENCE_ACC, RefVariation::OPTION_REFERENCE_ACC, "acc", RefVariation::USAGE_REFERENCE_ACC);
        HelpOptionLine (RefVariation::ALIAS_REF_POS, RefVariation::OPTION_REF_POS, "value", RefVariation::USAGE_REF_POS);
        HelpOptionLine (RefVariation::ALIAS_QUERY, RefVariation::OPTION_QUERY, "string", RefVariation::USAGE_QUERY);
        HelpOptionLine (RefVariation::ALIAS_VAR_LEN_ON_REF, RefVariation::OPTION_VAR_LEN_ON_REF, "value", RefVariation::USAGE_VAR_LEN_ON_REF);
        //HelpOptionLine (RefVariation::ALIAS_VERBOSITY, RefVariation::OPTION_VERBOSITY, "", RefVariation::USAGE_VERBOSITY);
#if SECRET_OPTION != 0
        HelpOptionLine (NULL, RefVariation::OPTION_SECRET, NULL, RefVariation::USAGE_SECRET);
#endif

        HelpOptionsStandard ();

        HelpVersion (fullpath, KAppVersion());

        return rc;
    }

    rc_t CC KMain ( int argc, char *argv [] )
    {
        /* command line examples:
          -r NC_011752.1 -p 2018 --query CA -l 0
          -r NC_011752.1 -p 2020 --query CA -l 0
          -r NC_011752.1 -p 5000 --query CA -l 0
       
       find insertion:
          ref-variation -r NC_000013.10 -p 100635036 --query 'ACC' -l 0 /netmnt/traces04/sra33/SRZ/000793/SRR793062/SRR793062.pileup /netmnt/traces04/sra33/SRZ/000795/SRR795251/SRR795251.pileup
          
       */

        RefVariation::find_variation_region ( argc, argv );
        return 0;
    }
}
