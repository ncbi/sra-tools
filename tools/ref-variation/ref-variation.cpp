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

#define SECRET_OPTION 0

#include "ref-variation.vers.h"

#include "helper.h"

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include <vector>
#include <string>
#include <fstream>

#include <kapp/main.h>
#include <klib/rc.h>
#include <atomic.h>

#include <ngs/ncbi/NGS.hpp>
#include <ngs/ReferenceSequence.hpp>

#define CPP_THREADS 0

#if CPP_THREADS != 0
#include <thread>
#include <mutex>

#define LOCK_GUARD std::lock_guard<std::mutex>
#define LOCK std::mutex

#else

#define LOCK_GUARD KProc::CLockGuard<KProc::CKLock>
#define LOCK KProc::CKLock

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
        size_t thread_count;
        bool calc_coverage;
        char const* input_file;

    } g_Params =
    {
        "",
        -1,
        "",
        0,
        0,
        1,
        false,
        ""
    };

    class CInputRun
    {
    public:

        CInputRun (char const* run_name) : m_run_name (run_name == NULL ? "" : run_name)
        {}

        CInputRun (std::string const& run_name, std::string const& run_path, std::string const& pileup_stats_path)
            : m_run_name (run_name),
              m_run_path (run_path),
              m_pileup_stats_path (pileup_stats_path)
        {}

        std::string const& GetRunName() const { return m_run_name; }
        std::string const& GetRunPath() const { return m_run_path; }
        std::string const& GetPileupStatsPath() const { return m_pileup_stats_path; }

        bool IsValid () const { return m_run_name.size() > 0; }

    private:

        std::string m_run_name;
        std::string m_run_path;
        std::string m_pileup_stats_path;
    };

    class CInputRuns
    {
    public:
        CInputRuns ();
        CInputRuns (KApp::CArgs const& args);
        CInputRuns (CInputRuns const& ); // no copy
        CInputRuns& operator= (CInputRuns const& ); // no assignment

        void Init ( KApp::CArgs const& args ); // not thread-safe!

        size_t GetCount() const;
        CInputRun GetNext () const;
        size_t GetCurrentIndex() const;
    private:

        std::vector <CInputRun> m_input_runs;
        mutable atomic_t m_param_index;
        mutable size_t m_current_index;
    };

    enum
    {
        VERBOSITY_DEFAULT           = 0,
        VERBOSITY_PRINT_VAR_SPEC    = 1,
        VERBOSITY_SOME_DETAILS      = 2,
        VERBOSITY_MORE_DETAILS      = 3
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
    //char const ALIAS_QUERY[]  = "q"; // -q is deprecated
    char const* USAGE_QUERY[] = { "query to find in the given reference (\"-\" is treated as an empty string, or deletion)", NULL };

    char const OPTION_VAR_LEN_ON_REF[] = "variation-length";
    char const ALIAS_VAR_LEN_ON_REF[]  = "l";
    char const* USAGE_VAR_LEN_ON_REF[] = { "the length of the variation on the reference (0 means pure insertion)", NULL };

    char const OPTION_VERBOSITY[] = "verbose";
    char const ALIAS_VERBOSITY[]  = "v";
    char const* USAGE_VERBOSITY[] = { "increase the verbosity of the program. use multiple times for more verbosity.", NULL };

    char const OPTION_THREADS[] = "threads";
    char const ALIAS_THREADS[]  = "t";
    char const* USAGE_THREADS[] = { "the number of threads to run", NULL };

    char const OPTION_COVERAGE[] = "coverage";
    char const ALIAS_COVERAGE[]  = "c";
    char const* USAGE_COVERAGE[] = { "output coverage (the nubmer of alignments matching the given variation query) for each run", NULL };

    char const OPTION_INPUT_FILE[] = "input-file";
    char const ALIAS_INPUT_FILE[]  = "i";
    char const* USAGE_INPUT_FILE[] = { "take runs from input file rather than from command line. The file must be in text format, each line should contain three tab-separated values: <run-accession> <run-path> <pileup-stats-path>, the latter two are optional", NULL };

    ::OptDef Options[] =
    {
        { OPTION_REFERENCE_ACC, ALIAS_REFERENCE_ACC, NULL, USAGE_REFERENCE_ACC, 1, true, true },
        { OPTION_REF_POS,       ALIAS_REF_POS,       NULL, USAGE_REF_POS,       1, true, true },
        { OPTION_QUERY,         /*ALIAS_QUERY*/NULL, NULL, USAGE_QUERY,         1, true, true },
        { OPTION_VAR_LEN_ON_REF,ALIAS_VAR_LEN_ON_REF,NULL, USAGE_VAR_LEN_ON_REF,1, true, true },
        { OPTION_THREADS,       ALIAS_THREADS,       NULL, USAGE_THREADS,       1, true, false },
        { OPTION_COVERAGE,      ALIAS_COVERAGE,      NULL, USAGE_COVERAGE,      1, false,false },
        { OPTION_INPUT_FILE,    ALIAS_INPUT_FILE,    NULL, USAGE_INPUT_FILE,    1, true, false }
#if SECRET_OPTION != 0
        ,{ OPTION_SECRET,        NULL,                NULL, USAGE_SECRET,        1, true, false }
#endif
        //,{ OPTION_VERBOSITY,     ALIAS_VERBOSITY,     NULL, USAGE_VERBOSITY,     0, false, false }
    };

    void print_indel (char const* name, char const* text, size_t text_size, size_t indel_start, size_t indel_len)
    {
        int prefix_count = (int)(indel_start < 5 ? indel_start : 5);
        int suffix_count = (int)((text_size - (indel_start + indel_len)) < 5 ?
                (text_size - (indel_start + indel_len)) : 5);

        printf ( "%s: %s%.*s[%.*s]%.*s%s\n",
                    name,
                    indel_start < 5 ? "" : "...",
                    prefix_count, text + indel_start - prefix_count, //(int)indel_start, text,
                    (int)indel_len, text + indel_start,
                    suffix_count, text + indel_start + indel_len, //(int)(text_size - (indel_start + indel_len)), text + indel_start + indel_len
                    (text_size - (indel_start + indel_len)) > 5 ? "..." : ""
               );
    }

#if SECRET_OPTION != 0
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

    uint32_t base2na_to_index ( char const ch )
    {
        switch ( ch )
        {
        case 'A':
        case 'a':
            return 0;
        case 'C':
        case 'c':
            return 1;
        case 'G':
        case 'g':
            return 2;
        case 'T':
        case 't':
            return 3;
        default:
            assert ( false );
            return (uint32_t) -1;
        }
    }

    enum
    {
        PILEUP_DEFINITELY_NOT_FOUND,
        PILEUP_DEFINITELY_FOUND,
        PILEUP_MAYBE_FOUND
    };

    int find_alignment_in_pileup_db ( char const* acc_pileup, char const* ref_name,
                KSearch::CVRefVariation const& obj, size_t bases_start )
    {
        if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
            std::cout << "Processing " << acc_pileup << "... " << std::endl;

        size_t ref_pos = bases_start + obj.GetVarStart();
        VDBObjects::CVDBManager mgr;
        mgr.Make();

        try
        {
            VDBObjects::CVDatabase db = mgr.OpenDB ( acc_pileup );
            VDBObjects::CVTable table = db.OpenTable ( "STATS" );
            VDBObjects::CVCursor cursor = table.CreateCursorRead ();

            uint32_t PileupColumnIndex [ countof (g_PileupColumnNames) ];
            cursor.InitColumnIndex (g_PileupColumnNames, PileupColumnIndex, countof(g_PileupColumnNames));
            cursor.Open();

            int64_t id_first = 0;
            uint64_t row_count = 0;

            cursor.GetIdRange (id_first, row_count);

            // Find Reference beginning

            KDBObjects::CKTable ktbl = table.OpenKTableRead();
            KDBObjects::CKIndex kindex = ktbl.OpenIndexRead("ref_spec");

            int64_t ref_id_start;
            uint64_t id_count;

            bool found = kindex.FindText ( ref_name, & ref_id_start, & id_count, NULL, NULL );
            if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
            {
                std::cout
                    << (found ? "" : " not") << " found " << ref_name << " row_id=" << ref_id_start
                    << ", id_count=" << id_count << " " <<  std::endl;
            }
            if ( !found )
                return PILEUP_DEFINITELY_NOT_FOUND;

            int64_t indel_cnt = (int64_t)obj.GetVarSize() - (int64_t)obj.GetVarLenOnRef();
            //int64_t indel_check_cnt = indel_cnt > 0 ? indel_cnt : -indel_cnt;

            // check depth > 0 for every position of the region
            for ( int64_t pos = (int64_t)ref_pos; pos < (int64_t)( ref_pos + obj.GetVarLenOnRef() ); ++pos )
            {
                if ( pos + ref_id_start >= id_first + (int64_t)row_count )
                {
                    if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                        std::cout << "OUT OF BOUNDS! filtering out" << std::endl;
                    return PILEUP_DEFINITELY_NOT_FOUND; // went beyond the end of db, probably, it's a bug in db
                }

                if ( indel_cnt == 0 ) // pure mismatch optimization
                {
                    uint32_t mismatch[4];
                    uint32_t count = cursor.ReadItems ( pos + ref_id_start, PileupColumnIndex[idx_MISMATCH_COUNTS], mismatch, sizeof mismatch );

                    assert ( count == 0 || count == 4 );

                    if ( count == 0 ||
                        mismatch [base2na_to_index (obj.GetVariation()[pos - ref_pos])] == 0 )
                    {
                        return PILEUP_DEFINITELY_NOT_FOUND;
                    }
                }

                uint32_t depth;
                uint32_t count = cursor.ReadItems ( pos + ref_id_start, PileupColumnIndex[idx_DEPTH], & depth, sizeof depth );

                if ( count == 0 || depth == 0 )
                {
                    if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                    {
                        std::cout << "depth=0 at the ref_pos=" << pos
                            << " (id=" << pos + ref_id_start << ") filtering out" << std::endl;
                    }
                    return PILEUP_DEFINITELY_NOT_FOUND;
                }
            }

            char run_name[64];
            uint32_t count = cursor.ReadItems ( id_first, PileupColumnIndex[idx_RUN_NAME], run_name, countof(run_name)-1 );
            assert (count < countof(run_name));
            run_name [count] = '\0';

            if ( g_Params.verbosity >= RefVariation::VERBOSITY_SOME_DETAILS )
                std::cout << run_name << " is suspicious" << std::endl;
            char const* p = run_name[0] == '/' ? run_name + 1 : run_name;

            int ret;

            if ( indel_cnt == 0 && ! g_Params.calc_coverage )
            {
                // if we reached this point in the case of pure mismatch
                // this means this run definitely has some hits.
                // due to quantization of pileup stats counters we
                // cannot rely on the exact value of those counters
                // So, we can only report this SRR in the case when we aren't
                // calculating a coverage. For the coverage we have to
                // look into SRR itself anyway.

                std::cout << p << std::endl; // report immediately
                ret = PILEUP_DEFINITELY_FOUND;
            }
            else
                ret = PILEUP_MAYBE_FOUND;

            return ret;
        }
        catch ( Utils::CErrorMsg const& e )
        {
            if ( e.getRC() == SILENT_RC(rcVFS,rcMgr,rcOpening,rcDirectory,rcNotFound)
                || e.getRC() == SILENT_RC(rcVFS,rcTree,rcResolving,rcPath,rcNotFound))
            {
                if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                    std::cout << "pileup db NOT FOUND, need to look into run itself" << std::endl;

                return PILEUP_MAYBE_FOUND;
            }
            else if ( e.getRC() == SILENT_RC(rcDB,rcMgr,rcOpening,rcDatabase,rcIncorrect))
            {
                if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                    std::cout << "BAD pileup db, need to look into run itself" << std::endl;

                return PILEUP_MAYBE_FOUND;
            }
            else
                throw;
        }
    }

    int find_alignment_in_pileup_db_mt ( char const* acc_pileup, char const* ref_name,
                KSearch::CVRefVariation const* pobj, size_t bases_start,
                LOCK* lock_cout, size_t thread_num )
    {
        if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
        {
            LOCK_GUARD l(*lock_cout);
            std::cout
                << "[" << thread_num << "] "
                << "Processing " << acc_pileup << "... " << std::endl;
        }

        KSearch::CVRefVariation const& obj = *pobj;
        size_t ref_pos = bases_start + obj.GetVarStart();
        VDBObjects::CVDBManager mgr;
        mgr.Make();

        try
        {
            VDBObjects::CVDatabase db = mgr.OpenDB ( acc_pileup );
            VDBObjects::CVTable table = db.OpenTable ( "STATS" );
            VDBObjects::CVCursor cursor = table.CreateCursorRead ();

            uint32_t PileupColumnIndex [ countof (g_PileupColumnNames) ];
            cursor.InitColumnIndex (g_PileupColumnNames, PileupColumnIndex, countof(g_PileupColumnNames));
            cursor.Open();

            int64_t id_first = 0;
            uint64_t row_count = 0;

            cursor.GetIdRange (id_first, row_count);

            // Find Reference beginning

            KDBObjects::CKTable ktbl = table.OpenKTableRead();
            KDBObjects::CKIndex kindex = ktbl.OpenIndexRead("ref_spec");

            int64_t ref_id_start;
            uint64_t id_count;

            bool found = kindex.FindText ( ref_name, & ref_id_start, & id_count, NULL, NULL );
            if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
            {
                LOCK_GUARD l(*lock_cout);
                std::cout
                    << "[" << thread_num << "] "
                    << (found ? "" : " not") << " found " << ref_name << " row_id=" << ref_id_start
                    << ", id_count=" << id_count << " " <<  std::endl;
            }
            if ( !found )
                return PILEUP_DEFINITELY_NOT_FOUND;

            int64_t indel_cnt = (int64_t)obj.GetVarSize() - (int64_t)obj.GetVarLenOnRef();
            //int64_t indel_check_cnt = indel_cnt > 0 ? indel_cnt : -indel_cnt;

            // check depth > 0 for every position of the region
            for ( int64_t pos = (int64_t)ref_pos; pos < (int64_t)( ref_pos + obj.GetVarLenOnRef() ); ++pos )
            {
                if ( pos + ref_id_start >= id_first + (int64_t)row_count )
                {
                    if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                    {
                        LOCK_GUARD l(*lock_cout);
                        std::cout
                            << "[" << thread_num << "] "
                            << "OUT OF BOUNDS! filtering out" << std::endl;
                    }
                    return PILEUP_DEFINITELY_NOT_FOUND; // went beyond the end of db, probably, it's a bug in db
                }

                if ( indel_cnt == 0 ) // pure mismatch optimization
                {
                    uint32_t mismatch[4];
                    uint32_t count = cursor.ReadItems ( pos + ref_id_start, PileupColumnIndex[idx_MISMATCH_COUNTS], mismatch, sizeof mismatch );

                    assert ( count == 0 || count == 4 );

                    if ( count == 0 ||
                        mismatch [base2na_to_index (obj.GetVariation()[pos - ref_pos])] == 0 )
                    {
                        return PILEUP_DEFINITELY_NOT_FOUND;
                    }
                }

                uint32_t depth;
                uint32_t count = cursor.ReadItems ( pos + ref_id_start, PileupColumnIndex[idx_DEPTH], & depth, sizeof depth );

                if ( count == 0 || depth == 0 )
                {
                    if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                    {
                        LOCK_GUARD l(*lock_cout);
                        std::cout
                            << "[" << thread_num << "] "
                            << "depth=0 at the ref_pos=" << pos
                            << " (id=" << pos + ref_id_start << ") filtering out" << std::endl;
                    }
                    return PILEUP_DEFINITELY_NOT_FOUND;
                }
            }

            char run_name[64];
            uint32_t count = cursor.ReadItems ( id_first, PileupColumnIndex[idx_RUN_NAME], run_name, countof(run_name)-1 );
            assert (count < countof(run_name));
            run_name [count] = '\0';

            if ( g_Params.verbosity >= RefVariation::VERBOSITY_SOME_DETAILS )
            {
                LOCK_GUARD l(*lock_cout);
                std::cout
                    << "[" << thread_num << "] "
                    << run_name << " is suspicious" << std::endl;
            }
            char const* p = run_name[0] == '/' ? run_name + 1 : run_name;

            int ret;

            if ( indel_cnt == 0 && ! g_Params.calc_coverage )
            {
                // if we reached this point in the case of pure mismatch
                // this means this run definitely has some hits.
                // due to quantization of pileup stats counters we
                // cannot rely on the exact value of those counters
                // So, we can only report this SRR in the case when we aren't
                // calculating a coverage. For the coverage we have to
                // look into SRR itself anyway.

                std::cout << p << std::endl; // report immediately
                ret = PILEUP_DEFINITELY_FOUND;
            }
            else
                ret = PILEUP_MAYBE_FOUND;

            return ret;
        }
        catch ( Utils::CErrorMsg const& e )
        {
            if ( e.getRC() == SILENT_RC(rcVFS,rcMgr,rcOpening,rcDirectory,rcNotFound)
                || e.getRC() == SILENT_RC(rcVFS,rcTree,rcResolving,rcPath,rcNotFound))
            {
                if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                {
                    LOCK_GUARD l(*lock_cout);
                    std::cout
                        << "[" << thread_num << "] "
                        << "pileup db NOT FOUND, need to look into run itself" << std::endl;
                }
                return PILEUP_MAYBE_FOUND;
            }
            else if ( e.getRC() == SILENT_RC(rcDB,rcMgr,rcOpening,rcDatabase,rcIncorrect))
            {
                if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                {
                    LOCK_GUARD l(*lock_cout);
                    std::cout
                        << "[" << thread_num << "] "
                        << "BAD pileup db, need to look into run itself" << std::endl;
                }
                return PILEUP_MAYBE_FOUND;
            }
            else
                throw;
        }
    }

    void find_alignments_in_run_db ( char const* acc, char const* path,
        char const* ref_name, KSearch::CVRefVariation const& obj, size_t bases_start,
        char const* variation, size_t var_size )
    {
        size_t ref_start = bases_start + obj.GetVarStart();

        if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
            std::cout << "Processing " << acc << std::endl;

        ncbi::ReadCollection run = ncbi::NGS::openReadCollection ( path && path[0] ? path : acc );

        ngs::Reference reference = run.getReference( ref_name );
        ngs::AlignmentIterator ai = reference.getAlignmentSlice ( ref_start, var_size, ngs::Alignment::all );

        size_t alignments_total = 0;
        size_t alignments_matched = 0;
        while ( ai.nextAlignment() )
        {
            ++ alignments_total;
            ngs::String id = ai.getAlignmentId ().toString();
            int64_t align_pos = (ai.getReferencePositionProjectionRange (ref_start) >> 32);
            ngs::String bases = ai.getFragmentBases( align_pos, var_size ).toString();
            bool match = strncmp (variation, bases.c_str(), var_size) == 0;
            if ( match )
            {
                if ( ! g_Params.calc_coverage )
                {
                    std::cout << acc << std::endl;
                    break; // -c option is for speed-up, so we sacrifice verbose output
                }
                ++ alignments_matched;
                //if ( ! g_Params.calc_coverage )
                //    break; // -c option is for speed-up, so we sacrifice verbose output
            }
            if ( g_Params.verbosity >= RefVariation::VERBOSITY_SOME_DETAILS )
            {
                std::cout << "id=" << id
                    << ": "
                    << bases
                    << (match ? " MATCH!" : "")
                    << std::endl;
            }
        }

        if (alignments_total > 0)
        {
            std::cout
                << acc
                << '\t' << alignments_matched
                << '\t' << alignments_total
                << std::endl;
        }
    }

    void find_alignments_in_run_db_mt ( char const* acc, char const* path,
        char const* ref_name, KSearch::CVRefVariation const* pobj, size_t bases_start,
        char const* variation, size_t var_size,
        LOCK* lock_cout, size_t thread_num )
    {
        KSearch::CVRefVariation const& obj = *pobj;
        size_t ref_start = bases_start + obj.GetVarStart();

        if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
        {
            LOCK_GUARD l(*lock_cout);
            std::cout
                << "[" << thread_num << "] "
                << "Processing " << acc << std::endl;
        }

        ncbi::ReadCollection run = ncbi::NGS::openReadCollection ( path && path[0] ? path : acc );

        ngs::Reference reference = run.getReference( ref_name );
        ngs::AlignmentIterator ai = reference.getAlignmentSlice ( ref_start, var_size, ngs::Alignment::all );

        size_t alignments_total = 0;
        size_t alignments_matched = 0;
        while ( ai.nextAlignment() )
        {
            ++ alignments_total;
            ngs::String id = ai.getAlignmentId ().toString();
            int64_t align_pos = (ai.getReferencePositionProjectionRange (ref_start) >> 32);
            ngs::String bases = ai.getFragmentBases( align_pos, var_size ).toString();
            bool match = strncmp (variation, bases.c_str(), var_size) == 0;
            if ( match )
            {
                if ( ! g_Params.calc_coverage )
                {
                    LOCK_GUARD l(*lock_cout);
                    std::cout << acc << std::endl;
                    break; // -c option is for speed-up, so we sacrifice verbose output
                }
                ++ alignments_matched;
                //if ( ! g_Params.calc_coverage )
                //    break; // -c option is for speed-up, so we sacrifice verbose output
            }
            if ( g_Params.verbosity >= RefVariation::VERBOSITY_SOME_DETAILS )
            {
                LOCK_GUARD l(*lock_cout);
                std::cout
                    << "[" << thread_num << "] "
                    << "id=" << id
                    << ": "
                    << bases
                    << (match ? " MATCH!" : "")
                    << std::endl;
            }
        }

        if (alignments_total > 0)
        {
            LOCK_GUARD l(*lock_cout);
            std::cout
                << acc
                << '\t' << alignments_matched
                << '\t' << alignments_total
                << std::endl;
        }
    }

    void find_alignments_in_single_run ( char const* acc, char const* path,
        char const* pileup_path, char const* ref_name,
        KSearch::CVRefVariation const& obj, size_t bases_start,
        char const* variation, size_t var_size )
    {
        char const pileup_suffix[] = ".pileup";
        char acc_pileup [ 128 ];

        if ( pileup_path == NULL || pileup_path [0] == '\0' )
        {
            size_t acc_len = strlen(acc);
            assert ( countof(acc_pileup) >= acc_len + countof(pileup_suffix) );
            strncpy ( acc_pileup, acc, countof(acc_pileup) );
            strncpy ( acc_pileup + acc_len, pileup_suffix, countof(acc_pileup) - acc_len );
            pileup_path = acc_pileup;
        }

        int res = find_alignment_in_pileup_db ( pileup_path, ref_name, obj, bases_start );

        if ( res == PILEUP_MAYBE_FOUND )
        {
            find_alignments_in_run_db ( acc, path, ref_name, obj, bases_start,
                variation, var_size );
        }
    }

    void find_alignments_in_single_run_mt ( char const* acc, char const* path,
        char const* pileup_path, char const* ref_name,
        KSearch::CVRefVariation const* pobj, size_t bases_start,
        char const* variation, size_t var_size,
        LOCK* lock_cout, size_t thread_num )
    {
        char const pileup_suffix[] = ".pileup";
        char acc_pileup [ 128 ];

        if ( pileup_path == NULL || pileup_path [0] == '\0' )
        {
            size_t acc_len = strlen(acc);
            assert ( countof(acc_pileup) >= acc_len + countof(pileup_suffix) );
            strncpy ( acc_pileup, acc, countof(acc_pileup) );
            strncpy ( acc_pileup + acc_len, pileup_suffix, countof(acc_pileup) - acc_len );
            pileup_path = acc_pileup;
        }

        int res = find_alignment_in_pileup_db_mt ( pileup_path, ref_name,
            pobj, bases_start, lock_cout, thread_num );

        if ( res == PILEUP_MAYBE_FOUND )
        {
            find_alignments_in_run_db_mt( acc, path, ref_name, pobj, bases_start,
                variation, var_size, lock_cout, thread_num );
        }
    }

    void find_alignments ( char const* ref_name,
        KSearch::CVRefVariation const& obj, size_t bases_start,
        CInputRuns const* p_input_runs )
    {
        size_t ref_start = bases_start + obj.GetVarStart();
        char query_del[3];

        char const* variation;
        size_t var_size;
        if ( obj.GetVarSize() == 0 && obj.GetVarLenOnRef() > 0 )
        {
            variation = obj.GetQueryForPureDeletion( query_del, sizeof query_del );
            var_size = 2;
            -- ref_start;
        }
        else
        {
            variation = obj.GetVariation();
            var_size = obj.GetVarSize();
        }

        for ( ; ; )
        {
            CInputRun const& input_run = p_input_runs -> GetNext();

            if ( ! input_run.IsValid() )
                break;

            char const* acc = input_run.GetRunName().c_str();
            char const* path = input_run.GetRunPath().c_str();
            char const* pileup_path = input_run.GetPileupStatsPath().c_str();

            // TODO: this ugly try-catch is here because
            // we can't effectively (non-linearly and with no exceptions thrown)
            // check if the read collection has the given reference in it
            try
            {
                find_alignments_in_single_run ( acc, path, pileup_path, ref_name,
                    obj, bases_start, variation, var_size );
            }
            catch ( ngs::ErrorMsg const& e )
            {
                if ( strstr (e.what(), "Reference not found") == e.what() )
                {
                    if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                    {
                        std::cout
                            << "reference " << ref_name
                            << " NOT FOUND in " << acc
                            << ", skipping" << std::endl;
                    }
                }
                else
                    throw;
            }
        }
    }

    void find_alignments_mt ( char const* ref_name,
        KSearch::CVRefVariation const* pobj, size_t bases_start,
        LOCK* lock_cout, size_t thread_num,
        CInputRuns const* p_input_runs )
    {
        try
        {
            KSearch::CVRefVariation const& obj = *pobj;
            size_t ref_start = bases_start + obj.GetVarStart();

            char query_del[3];

            char const* variation;
            size_t var_size;
            if ( obj.GetVarSize() == 0 && obj.GetVarLenOnRef() > 0 )
            {
                variation = obj.GetQueryForPureDeletion( query_del, sizeof query_del );
                var_size = 2;
                -- ref_start;
            }
            else
            {
                variation = obj.GetVariation();
                var_size = obj.GetVarSize();
            }

            for ( ; ; )
            {
                CInputRun const& input_run = p_input_runs -> GetNext();

                if ( ! input_run.IsValid() )
                    break;

                char const* acc = input_run.GetRunName().c_str();
                char const* path = input_run.GetRunPath().c_str();
                char const* pileup_path = input_run.GetPileupStatsPath().c_str();

                if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                {
                    LOCK_GUARD l(*lock_cout);
                    std::cout
                        << "[" << thread_num << "] "
                        << "Processing parameter # " << p_input_runs -> GetCurrentIndex()
                        << ": " << acc
                        << ", path=[" << path << "]"
                        << ", pileup path=[" << pileup_path << "]"
                        << std::endl;
                }


                // TODO: this ugly try-catch is here because
                // we can't effectively (non-linearly and with no exceptions thrown)
                // check if the read collection has the given reference in it
                try
                {
                    find_alignments_in_single_run_mt ( acc, path, pileup_path,
                        ref_name, pobj, bases_start, variation, var_size,
                        lock_cout, thread_num );
                }
                catch ( ngs::ErrorMsg const& e )
                {
                    if ( strstr (e.what(), "Reference not found") == e.what() )
                    {
                        if ( g_Params.verbosity >= RefVariation::VERBOSITY_MORE_DETAILS )
                        {
                            LOCK_GUARD l(*lock_cout);
                            std::cout
                                << "[" << thread_num << "] "
                                << "reference " << ref_name
                                << " NOT FOUND in " << acc
                                << ", skipping" << std::endl;
                        }
                    }
                    else
                        throw;
                }
            }
        }
        catch ( ngs::ErrorMsg const& e )
        {
            LOCK_GUARD l(*lock_cout); // reuse cout mutex
            std::cerr
                << "[" << thread_num << "] "
                << "ngs::ErrorMsg: " << e.what() << std::endl;
        }
        catch (...)
        {
            LOCK_GUARD l(*lock_cout); // reuse cout mutex
            Utils::HandleException ();
        }
    }

#if CPP_THREADS == 0
    struct AdapterFindAlignment
    {
        KProc::CKThread thread;

        size_t param_start, param_count;
        char const* ref_name;
        KSearch::CVRefVariation const* pobj;
        size_t bases_start;
        LOCK* lock_cout;
        size_t thread_num;
        CInputRuns const* p_input_runs;
    };

    void AdapterFindAlignment_Init (AdapterFindAlignment & params,
            size_t param_start, size_t param_count,
            char const* ref_name,
            KSearch::CVRefVariation const* pobj,
            size_t bases_start,
            LOCK* lock_cout,
            size_t thread_num,
            CInputRuns const* p_input_runs
        )
    {
        params.param_start = param_start;
        params.param_count = param_count;
        params.ref_name = ref_name;
        params.pobj = pobj;
        params.bases_start = bases_start;
        params.lock_cout = lock_cout;
        params.thread_num = thread_num;
        params.p_input_runs = p_input_runs;
    }

    rc_t AdapterFindAlignmentFunc ( void* data )
    {
        AdapterFindAlignment& p = * (static_cast<AdapterFindAlignment*>(data));
        find_alignments_mt ( p.ref_name, p.pobj, p.bases_start,
            p.lock_cout, p.thread_num, p.p_input_runs );
        return 0;
    }
#endif

    bool find_variation_core_step (KSearch::CVRefVariation& obj,
        char const* ref_slice, size_t ref_slice_size,
        size_t& ref_pos_in_slice,
        char const* var, size_t var_len, size_t var_len_on_ref,
        size_t chunk_size, size_t chunk_no_last,
        size_t& bases_start, size_t& chunk_no_start, size_t& chunk_no_end)
    {
        bool cont = false;
        obj = KSearch::VRefVariationIUPACMake (
            ref_slice, ref_slice_size,
            ref_pos_in_slice, var, var_len, var_len_on_ref );

        if ( obj.GetVarStart() == 0 && chunk_no_start > 0 )
        {
            cont = true;
            --chunk_no_start;
            ref_pos_in_slice += chunk_size;
            bases_start -= chunk_size;
        }
        if (obj.GetVarStart() + obj.GetVarLenOnRef() == ref_slice_size &&
            chunk_no_end < chunk_no_last )
        {
            cont = true;
            ++chunk_no_end;
        }

        return cont;
    }

    void finish_find_variation_region ( KApp::CArgs const & args, size_t var_len,
        char const* ref_slice, size_t ref_slice_size, size_t bases_start,
        KSearch::CVRefVariation const& obj )
    {
        size_t ref_start = obj.GetVarStart() + bases_start;
        size_t ref_len = obj.GetVarLenOnRef();
        if ( g_Params.verbosity >= RefVariation::VERBOSITY_SOME_DETAILS )
        {
            std::cout << "Found indel box at pos=" << ref_start << ", length=" << ref_len << std::endl;
            print_indel ( "reference", ref_slice, ref_slice_size, obj.GetVarStart(), ref_len );

            std::cout << "var_query=" << obj.GetVariation() << std::endl;
        }

        if ( g_Params.verbosity >= RefVariation::VERBOSITY_PRINT_VAR_SPEC )
        {
            std::cout
                << "Input variation spec   : "
                << g_Params.ref_acc << ":"
                << g_Params.ref_pos_var << ":"
                << g_Params.var_len_on_ref << ":"
                << g_Params.query
                << std::endl;
        }

        size_t var_len_on_ref_adj = obj.GetVarLenOnRef();
        if ( (int64_t)ref_start > g_Params.ref_pos_var )
            var_len_on_ref_adj -= ref_start - g_Params.ref_pos_var;

        if ( g_Params.verbosity >= RefVariation::VERBOSITY_PRINT_VAR_SPEC )
        {
            std::cout
                << "Adjusted variation spec: "
                << g_Params.ref_acc << ":"
                << obj.GetVarStart() + bases_start << ":"
                << obj.GetVarLenOnRef() << ":"
                << obj.GetVariation()
                << std::endl;
        }

        // Split further processing into multiple threads if there too many params
        CInputRuns input_runs ( args );

        size_t param_count = input_runs.GetCount();

        if ( param_count >= 1 && param_count < g_Params.thread_count )
            g_Params.thread_count = param_count;

        size_t thread_count = g_Params.thread_count;

        if ( thread_count == 1 )
            find_alignments (g_Params.ref_acc, obj, bases_start, & input_runs);
        else
        {
            // split
            if ( g_Params.verbosity >= RefVariation::VERBOSITY_SOME_DETAILS )
            {
                std::cout
                    << "Splitting " << param_count
                    << " jobs into " << thread_count << " threads..." << std::endl;
            }

            LOCK mutex_cout;

#if CPP_THREADS != 0
            std::vector<std::thread> vec_threads;
#else
            std::vector<AdapterFindAlignment> vec_threads ( thread_count );
#endif

            size_t param_chunk_size = param_count / thread_count;
            for (size_t i = 0; i < thread_count; ++i)
            {
                size_t current_chunk_size = i == thread_count - 1 ?
                    param_chunk_size + param_count % thread_count : param_chunk_size;
#if CPP_THREADS != 0
                vec_threads.push_back(
                    std::thread( find_alignments_mt,
                                    i * param_chunk_size,
                                    current_chunk_size, g_Params.ref_acc, & obj, bases_start,
                                    & mutex_cout, i + 1, & input_runs
                               ));
#else
                AdapterFindAlignment & params = vec_threads [ i ];

                AdapterFindAlignment_Init ( params, i * param_chunk_size,
                    current_chunk_size, g_Params.ref_acc, & obj, bases_start,
                    & mutex_cout, i + 1, & input_runs );

                params.thread.Make ( AdapterFindAlignmentFunc, & params );
#endif
            }
#if CPP_THREADS != 0
            for (std::thread& th : vec_threads)
                th.join();
#else
            for (std::vector<AdapterFindAlignment>::iterator it = vec_threads.begin(); it != vec_threads.end(); ++it)
            {
                AdapterFindAlignment & params = *it;
                params.thread.Wait();
            }
#endif

        }
    }

    #include <search/grep.h>

    bool check_ref_slice ( char const* ref, size_t ref_size )
    {
        for ( size_t i = 0; i < ref_size; ++i )
        {
            if ( !(ref [i] == 'N' || ref [i] == 'n' || ref [i] == '.') )
                return true; // at least one non-N base is OK
        }
        return false;
    }

    void find_variation_region_impl (KApp::CArgs const& args)
    {
        ngs::ReferenceSequence ref_seq = ncbi::NGS::openReferenceSequence ( g_Params.ref_acc );

        size_t var_len = strlen (g_Params.query);

        size_t chunk_size = 5000; // TODO: add the method Reference[Sequence].getChunkSize() to the API
        size_t chunk_no = g_Params.ref_pos_var / chunk_size;
        size_t ref_pos_in_slice = g_Params.ref_pos_var % chunk_size;
        size_t bases_start = chunk_no * chunk_size;
        size_t chunk_no_last = ref_seq.getLength() / chunk_size;

        KSearch::CVRefVariation obj;
        bool cont = false;
        size_t chunk_no_start = chunk_no, chunk_no_end = chunk_no;

        // optimization: first look into the current chunk only (using ngs::StringRef)
        {
            ngs::StringRef ref_chunk = ref_seq.getReferenceChunk ( bases_start );

            if ( ! check_ref_slice (ref_chunk.data() + ref_pos_in_slice, g_Params.var_len_on_ref) )
            {
                throw Utils::CErrorMsg (
                    "The selected reference region [%.*s] does not contain valid bases, "
                    "exiting...",
                    (int)g_Params.var_len_on_ref, ref_chunk.data() + ref_pos_in_slice );
            }
            
            cont = find_variation_core_step ( obj,
                ref_chunk.data(), ref_chunk.size(), ref_pos_in_slice,
                g_Params.query, var_len, g_Params.var_len_on_ref,
                chunk_size, chunk_no_last, bases_start, chunk_no_start, chunk_no_end );

            if ( !cont )
            {
                finish_find_variation_region ( args, var_len,
                    ref_chunk.data(), ref_chunk.size(), bases_start, obj);
            }
        }

        // general case - expanding ref_slice to multiple chunks
        if ( cont )
        {
            ngs::String ref_slice;
            while ( cont )
            {
                ref_slice = ref_seq.getReferenceBases (
                    bases_start, (chunk_no_end - chunk_no_start + 1)*chunk_size );

                cont = find_variation_core_step ( obj,
                    ref_slice.c_str(), ref_slice.size(), ref_pos_in_slice,
                    g_Params.query, var_len, g_Params.var_len_on_ref,
                    chunk_size, chunk_no_last, bases_start, chunk_no_start, chunk_no_end );
            }

            finish_find_variation_region ( args, var_len,
                ref_slice.c_str(), ref_slice.size(), bases_start, obj);
        }
    }

#if SECRET_OPTION != 0
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

    void test_getReferenceProjectionRange()
    {
        std::cout << "Starting getReferenceProjectionRange test..." << std::endl;
        ngs::ReadCollection read_coll = ncbi::NGS::openReadCollection( "SRR1597772" );
        int64_t ref_pos = 11601;
        uint64_t stop = ref_pos + 92;

        ngs::Reference ref = read_coll.getReference( "CM000663.1" );
        ngs::Alignment align = read_coll.getAlignment("SRR1597772.PA.6");

        ngs::String ref_str = ref.getReferenceBases( ref_pos, stop - ref_pos );
        ngs::StringRef align_str = align.getFragmentBases();

        std::cout
            << "ref: " << ref_str << std::endl
            << "seq: " << align_str << std::endl;

        for ( ; ref_pos < (int64_t)stop; ++ ref_pos )
        {
            ngs::String ref_base = ref.getReferenceBases( ref_pos, 1 );
            uint64_t range = align.getReferencePositionProjectionRange ( ref_pos );
            uint32_t align_pos = range >> 32;
            uint32_t range_len = range & 0xFFFFFFFF;

            std::cout
                << ref_pos << " "
                << ref_base
                << " (" << (int32_t)align_pos << ", " << range_len << ") "
                << align.getFragmentBases( align_pos, range_len == 0 ? 1 : range_len )
                << std::endl;
        }

        std::cout << "getReferenceProjectionRange test has SUCCEEDED" << std::endl;
    }
#endif

    void find_variation_region (int argc, char** argv)
    {
        try
        {
            KApp::CArgs args;
            args.MakeAndHandle (argc, argv, Options, countof (Options));
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

            if (args.GetOptionCount (OPTION_THREADS) == 1)
                g_Params.thread_count = args.GetOptionValueUInt<size_t>( OPTION_THREADS, 0 );

            g_Params.calc_coverage = args.GetOptionCount (OPTION_COVERAGE) != 0;

            g_Params.verbosity = (int)args.GetOptionCount (OPTION_VERBOSITY);

            if (args.GetOptionCount (OPTION_INPUT_FILE) == 1)
            {
                g_Params.input_file = args.GetOptionValue ( OPTION_INPUT_FILE, 0 );
                if (args.GetParamCount() > 0)
                {
                    std::cerr << argv [0] << OPTION_INPUT_FILE
                        << " option must not be provided along with parameters,"
                        " use either command line parameters or input file but"
                        " not both" << std::endl;
                    return;
                }
            }

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
                case 4:
                    test_getReferenceProjectionRange ();
                    break;
                default:
                    std::cout
                        << "specify value for this option:" << std::endl
                        << "1 - run correctness test" << std::endl
                        << "2 - run ngs performance test" << std::endl
                        << "3 - run vdb performance test" << std::endl
                        << "4 - run getReferencePositionProjectionRange test" << std::endl;
                }
            }
            else
#endif
            {
                find_variation_region_impl (args);
            }

        }
        catch ( ngs::ErrorMsg const& e )
        {
            std::cerr << "ngs::ErrorMsg: " << e.what() << std::endl;
        }
        catch (...)
        {
            Utils::HandleException ();
        }
    }

/////////////////////////////
    // class CInputRuns

    CInputRuns::CInputRuns (KApp::CArgs const& args)
    {
        m_param_index.counter = 0;
        m_current_index = 0;

        Init ( args );
    }

    CInputRuns::CInputRuns ()
    {
        m_param_index.counter = 0;
        m_current_index = 0;
    }
    

    bool is_eol (char ch)
    {
        return ch == '\0' || ch == '\r' || ch == '\n';
    }

    bool is_sep (char ch)
    {
        return ch == '\t';
    }

    enum {PARSE_OK, PARSE_EMPTY, PARSE_ERROR};

    int parse_input_line ( char const* line,
        char const** p_acc, size_t* p_size_acc,
        char const** p_run_path, size_t* p_size_run_path,
        char const** p_pileup_path, size_t* p_size_pileup_path)
    {
        *p_acc = *p_run_path = *p_pileup_path = "";
        *p_size_acc = *p_size_run_path = *p_size_pileup_path = 0;

        char const* p = line;

        if ( is_eol (*p) )
            return PARSE_EMPTY;

        *p_acc = p;
        for ( ; ! is_sep (*p) && ! is_eol (*p); ++ p, ++ (*p_size_acc) );

        if ( *p_size_acc == 0 )
            return PARSE_ERROR;

        if ( is_eol ( *p ) )
            return PARSE_OK;

        ++ p;
        *p_run_path = p;
        for ( ; ! is_sep (*p) && ! is_eol (*p); ++p, ++ (*p_size_run_path) );

        if ( is_eol ( *p ) )
            return PARSE_OK;

        ++ p;
        *p_pileup_path = p;
        for ( ; ! is_sep (*p) && ! is_eol (*p); ++p, ++ (*p_size_pileup_path) );

        return PARSE_OK;
    }

    void CInputRuns::Init ( KApp::CArgs const& args ) // not thread-safe!
    {
        if ( g_Params.input_file != NULL && g_Params.input_file [0] != '\0' )
        {
            std::ifstream input_file( g_Params.input_file );
            if ( !input_file.good() )
                throw Utils::CErrorMsg( "Failed to open file %s", g_Params.input_file );

            std::string line;
            size_t count = 0;

            while ( std::getline ( input_file, line) )
            {
                ++ count;
                if ( m_input_runs.capacity() < count )
                    m_input_runs.reserve ( m_input_runs.capacity() * 2 );

                char const* p_acc, *p_path, *p_pileup_path;
                size_t size_acc, size_path, size_pileup_path;

                int res = parse_input_line ( line.c_str(),
                    & p_acc, & size_acc,
                    & p_path, & size_path,
                    & p_pileup_path, & size_pileup_path );
                if ( res == PARSE_ERROR )
                {
                    throw Utils::CErrorMsg(
                        "Failed to parse line # %lu from file %s",
                        count, g_Params.input_file );
                }
                else if ( res == PARSE_OK )
                {
                    m_input_runs.push_back(
                        CInputRun(
                            std::string(p_acc, size_acc),
                            std::string(p_path, size_path),
                            std::string(p_pileup_path, size_pileup_path)));
                }
                else // res == PARSE_EMPTY
                    -- count;
            }
        }
        else
        {
            m_input_runs.reserve (args.GetParamCount());

            for ( uint32_t i = 0; i < args.GetParamCount(); ++i )
                m_input_runs.push_back( CInputRun ( args.GetParamValue( i ) ) );
        }
    }

    size_t CInputRuns::GetCount() const
    {
        return m_input_runs.size();
    }

    CInputRun CInputRuns::GetNext () const
    {
        m_current_index = atomic_read_and_add( & m_param_index, 1 );
        return m_current_index < m_input_runs.size() ? m_input_runs[m_current_index] : CInputRun("");
    }
    size_t CInputRuns::GetCurrentIndex () const
    {
        return m_current_index;
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


        printf ("\nParameters: optional space-separated list of run accessions in which the query will be looked for\n\n");

        printf ("\nOptions:\n");

        HelpOptionLine (RefVariation::ALIAS_REFERENCE_ACC, RefVariation::OPTION_REFERENCE_ACC, "acc", RefVariation::USAGE_REFERENCE_ACC);
        HelpOptionLine (RefVariation::ALIAS_REF_POS, RefVariation::OPTION_REF_POS, "value", RefVariation::USAGE_REF_POS);
        HelpOptionLine (NULL, RefVariation::OPTION_QUERY, "string", RefVariation::USAGE_QUERY);
        HelpOptionLine (RefVariation::ALIAS_VAR_LEN_ON_REF, RefVariation::OPTION_VAR_LEN_ON_REF, "value", RefVariation::USAGE_VAR_LEN_ON_REF);
        HelpOptionLine (RefVariation::ALIAS_THREADS, RefVariation::OPTION_THREADS, "value", RefVariation::USAGE_THREADS);
        HelpOptionLine (RefVariation::ALIAS_COVERAGE, RefVariation::OPTION_COVERAGE, "", RefVariation::USAGE_COVERAGE);
        HelpOptionLine (RefVariation::ALIAS_INPUT_FILE, RefVariation::OPTION_INPUT_FILE, "string", RefVariation::USAGE_INPUT_FILE);
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

       windows example: -r NC_000002.11 -p 73613067 --query "-" -l 3 ..\..\..\tools\ref-variation\SRR618508.pileup

       -r NC_000002.11 -p 73613071 --query "C" -l 1
       -vv -t 16 -r NC_000007.13 -p 117292900 --query "-" -l 4          

       -vv -c -t 16 -r NC_000002.11 -p 73613067 --query '-' -l 3 /netmnt/traces04/sra33/SRZ/000867/SRR867061/SRR867061.pileup /netmnt/traces04/sra33/SRZ/000867/SRR867131/SRR867131.pileup
       -vv -c -t 16 -r NC_000002.11 -p 73613067 --query "-" -l 3 ..\..\..\tools\ref-variation\SRR867061.pileup ..\..\..\tools\ref-variation\SRR867131.pileup

       old problem cases (didn't stop):
       -v -c -r CM000671.1 -p 136131022 --query 'T' -l 1 SRR1601768
       -v -c -r NC_000001.11 -p 136131022 --query 'T' -l 1 SRR1601768

       new problebm:

       -t 16 -v -c -r CM000671.1 -p 136131021 --query "T" -l 1 SRR1596639

       */

        RefVariation::find_variation_region ( argc, argv );
        return 0;
    }
}
