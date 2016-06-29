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

#include "helper.h"
#include "common.h"

#define CPP_THREADS 0

class CNoMutex // empty class for singlethreaded lock_guard (no actions)
{
public:
    CNoMutex() {}
    ~CNoMutex() {}
    CNoMutex(CNoMutex const& x);
    CNoMutex const& operator=(CNoMutex const& x);

    void lock() {}
    void unlock() {}
};

#if CPP_THREADS != 0
#include <thread>
#include <mutex>

#define LOCK_GUARD std::lock_guard<TLock>
#define LOCK std::mutex

#else

#define LOCK_GUARD KProc::CLockGuard<TLock>
#define LOCK KProc::CKLock

#endif

namespace NSRefVariation
{

#define COUNT_STRAND_NONE_STR           "none"
#define COUNT_STRAND_COUNTERALIGNED_STR "counteraligned"
#define COUNT_STRAND_COALIGNED_STR      "coaligned"
#define PARAM_ALG_SW "sw"
#define PARAM_ALG_RA "ra"

    enum EnumCountStrand
    {
        COUNT_STRAND_NONE,
        COUNT_STRAND_COUNTERALIGNED,
        COUNT_STRAND_COALIGNED
    };

    struct Params
    {
        // command line params

        // command line options
        char const* ref_acc;
        int64_t ref_pos_var;
        char query[256];
        size_t var_len_on_ref;
        int verbosity;
        size_t thread_count;
        bool calc_coverage;
        char const* input_file;
        EnumCountStrand count_strand;
        uint32_t query_min_rep;
        uint32_t query_max_rep;
        ::RefVarAlg alg;
    } g_Params =
    {
        "",
        -1,
        "",
        0,
        0,
        1,
        false,
        "",
        COUNT_STRAND_NONE,
        0,
        0,
        ::refvarAlgSW
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

        size_t GetNextIndex() const;
        CInputRun Get(size_t index) const;
    private:

        std::vector <CInputRun> m_input_runs;
        mutable atomic_t m_param_index;
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
    char const* USAGE_QUERY[] = { "query to find in the given reference (\"-\" is treated as an empty string, or deletion)."
        " Optionally, for non-empty query, the variable number of repetitions "
        "can be specified in the following way: "
        "\"<query>[<min_rep>-<max_rep>]\" where <query> is the pattern which "
        "should be repeated, <min_rep> is the minimum number of repetiotions and "
        "<max_rep> is the maximum number of repetiotions to produce from query pattern. "
        "E.g.: \"AT[1-3]\" produces 3 queries: \"AT\", \"ATAT\" and \"ATATAT\". "
        "In this case the output counts will contain as many columns for matched "
        "counts as many variations is produced from the given query.", NULL };

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

    char const OPTION_COUNT_STRAND[] = "count-strand";
    //char const ALIAS_COUNT_STRAND[]  = "s";
    char const* USAGE_COUNT_STRAND[] = { "controls relative orientation of 3' and 5' fragments. "
        "\""COUNT_STRAND_NONE_STR"\" - do not count (default). "
        "\""COUNT_STRAND_COUNTERALIGNED_STR"\" - as in Illumina. "
        "\""COUNT_STRAND_COALIGNED_STR"\" - as in 454 or IonTorrent. ", NULL };

    char const OPTION_ALG[] = "algorithm";
    //char const ALIAS_ALG[]  = "a";
    char const* USAGE_ALG[] = { "the algorithm to use for searching. "
        "\""PARAM_ALG_SW"\" means Smith-Waterman. "
        "\""PARAM_ALG_RA"\" means Rolling bulldozer algorithm\n", NULL };

    ::OptDef Options[] =
    {
        { OPTION_REFERENCE_ACC, ALIAS_REFERENCE_ACC, NULL, USAGE_REFERENCE_ACC, 1, true, true }
        ,{ OPTION_REF_POS,       ALIAS_REF_POS,       NULL, USAGE_REF_POS,       1, true, true }
        ,{ OPTION_QUERY,         /*ALIAS_QUERY*/NULL, NULL, USAGE_QUERY,         1, true, true }
        ,{ OPTION_VAR_LEN_ON_REF,ALIAS_VAR_LEN_ON_REF,NULL, USAGE_VAR_LEN_ON_REF,1, true, true }
        ,{ OPTION_THREADS,       ALIAS_THREADS,       NULL, USAGE_THREADS,       1, true, false }
        ,{ OPTION_COVERAGE,      ALIAS_COVERAGE,      NULL, USAGE_COVERAGE,      1, false,false }
        ,{ OPTION_INPUT_FILE,    ALIAS_INPUT_FILE,    NULL, USAGE_INPUT_FILE,    1, true, false }
        ,{ OPTION_COUNT_STRAND,  NULL,                NULL, USAGE_COUNT_STRAND,  1, true, false }
        ,{ OPTION_ALG,           NULL,                NULL, USAGE_ALG,           1, true, false }
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

        char buf[512] = "";
        rc_t res = string_printf(buf, countof(buf), NULL, "%s: %s%.*s[%.*s]%.*s%s",
            name,
            indel_start < 5 ? "" : "...",
            prefix_count, text + indel_start - prefix_count, //(int)indel_start, text,
            (int)indel_len, text + indel_start,
            suffix_count, text + indel_start + indel_len, //(int)(text_size - (indel_start + indel_len)), text + indel_start + indel_len
            (text_size - (indel_start + indel_len)) > 5 ? "..." : "");
        if (res == rcBuffer || res == rcInsufficient)
            buf[countof(buf) - 1] = '\0';

        LOGMSG ( klogInfo, buf );
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

    // TODO: remove unused from here:
    // idx_RUN_NAME - logging only for now
    // idx_MISMATCH_COUNTS, idx_INSERTION_COUNTS, idx_DELETION_COUNT
    //     - intended for optimization but not used now
    // idx_REFERENCE_SPEC, idx_REF_POS - not used

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

    struct count_pair
    {
        size_t count;
        size_t count_posititve;
    };

    struct coverage_info
    {
        count_pair count_total;
        std::vector <count_pair> counts_matched;
    };

    template <class TLock> void update_run_coverage ( char const* acc,
        size_t alignments_total, size_t alignments_total_positive,
        size_t alignments_matched, size_t alignments_matched_positive,
        TLock* lock_cout, coverage_info* pcoverage_count, size_t index)
    {
        coverage_info& counts = *pcoverage_count;

        if ( counts.count_total.count != (size_t)-1 )
        {
            if ( counts.count_total.count != alignments_total ||
                counts.count_total.count_posititve != alignments_total_positive )
            {
                LOCK_GUARD l(*lock_cout);
                PLOGMSG ( klogWarn,
                    (klogWarn,
                    "Total counts don't match for $(ACC) for query # $(IDXPREV) and $(IDXCUR): "
                    "total: $(TOTALPREV) vs $(TOTALCUR), total positive: $(TOTALPOSPREV) vs $(TOTALPOSCUR)",
                    "ACC=%s,IDXPREV=%zu,IDXCUR=%zu,TOTALPREV=%zu,TOTALCUR=%zu,TOTALPOSPREV=%zu,TOTALPOSCUR=%zu",
                    acc, index, index + 1, alignments_total, counts.count_total.count,
                    alignments_total_positive, counts.count_total.count_posititve));
            }
        }

        counts.count_total.count = alignments_total;
        counts.count_total.count_posititve = alignments_total_positive;

        assert ( index < counts.counts_matched.size() );
        count_pair& matched_count = counts.counts_matched [index];

        matched_count.count = alignments_matched;
        matched_count.count_posititve = alignments_matched_positive;
    }
    

    template <class TLock> void report_run_coverage ( char const* acc,
        coverage_info const* pcoverage_count, TLock* lock_cout) 
    {
        coverage_info const& counts = *pcoverage_count;
        if ( g_Params.calc_coverage )
        {
            LOCK_GUARD l(*lock_cout);

            OUTMSG (( "%s", acc ));

            std::vector <count_pair>::const_iterator cit = counts.counts_matched.begin();
            std::vector <count_pair>::const_iterator cend = counts.counts_matched.end();
            for (; cit != cend; ++cit)
            {
                count_pair const& c = *cit;
                OUTMSG (( "\t%zu", c.count ));
        
                if ( g_Params.count_strand != COUNT_STRAND_NONE )
                    OUTMSG (( ",%zu", c.count_posititve ));
            }

            OUTMSG (( "\t%zu", counts.count_total.count ));
        
            if ( g_Params.count_strand != COUNT_STRAND_NONE )
                OUTMSG (( ",%zu", counts.count_total.count_posititve ));
            
            OUTMSG (("\n"));
        }
        else
        {

            std::vector <count_pair>::const_iterator cit = counts.counts_matched.begin();
            std::vector <count_pair>::const_iterator cend = counts.counts_matched.end();
            for (; cit != cend; ++cit)
            {
                count_pair const& c = *cit;
                if ( c.count > 0 )
                {
                    LOCK_GUARD l(*lock_cout);
                    OUTMSG (( "%s\n", acc ));
                    break;
                }
            }
        }
    }

    template <class TLock> int find_alignment_in_pileup_db ( char const* acc,
                char const* acc_pileup, char const* ref_name,
                KSearch::CVRefVariation const* pobj, size_t ref_pos,
                size_t query_len_on_ref,
                TLock* lock_cout, size_t thread_num,
                coverage_info* pcoverage_count, size_t index )
    {
        if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_MORE_DETAILS )
        {
            LOCK_GUARD l(*lock_cout);
            PLOGMSG ( klogInfo,
                ( klogInfo,
                "[$(THREAD_NUM)] Processing $(ACC)...",
                "THREAD_NUM=%zu,ACC=%s", thread_num, acc_pileup
                ));
        }

        KSearch::CVRefVariation const& obj = *pobj;
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
            if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_MORE_DETAILS )
            {
                LOCK_GUARD l(*lock_cout);
                PLOGMSG ( klogInfo,
                    ( klogInfo,
                    "[$(THREAD_NUM)] $(FOUND)found $(REFNAME) row_id=$(ROWID), id_count=$(IDCOUNT)",
                    "THREAD_NUM=%zu,FOUND=%s,REFNAME=%s,ROWID=%ld,IDCOUNT=%lu",
                    thread_num, found ? "" : " not ", ref_name, ref_id_start, id_count
                    ));
            }
            if ( !found )
            {
                update_run_coverage ( acc, 0, 0, 0, 0, lock_cout, pcoverage_count, index );
                return PILEUP_DEFINITELY_NOT_FOUND;
            }

            int64_t indel_cnt = (int64_t)obj.GetAlleleSize() - (int64_t)obj.GetAlleleLenOnRef();
            //int64_t indel_check_cnt = indel_cnt > 0 ? indel_cnt : -indel_cnt;
            size_t alignments_total = (size_t)((uint32_t)-1);

            for ( int64_t pos = (int64_t)ref_pos; pos < (int64_t)( ref_pos + query_len_on_ref ); ++pos )
            {
                if ( pos + ref_id_start >= id_first + (int64_t)row_count )
                {
                    // TODO: this is not expected normally, now
                    // nothing will be printed to the output
                    // but maybe we also need to report matches and total alignments
                    // here (0 0)

                    if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_MORE_DETAILS )
                    {
                        LOCK_GUARD l(*lock_cout);
                        PLOGMSG ( klogInfo,
                            ( klogWarn,
                            "[$(THREAD_NUM)] OUT OF BOUNDS! filtering out",
                            "THREAD_NUM=%zu", thread_num
                            ));
                    }

                    update_run_coverage ( acc, 0, 0, 0, 0, lock_cout, pcoverage_count, index );
                    return PILEUP_DEFINITELY_NOT_FOUND; // went beyond the end of db, probably, it's a bug in db
                }

                uint32_t depth;
                uint32_t count = cursor.ReadItems ( pos + ref_id_start, PileupColumnIndex[idx_DEPTH], & depth, sizeof depth );
                if ( count == 0)
                    depth = 0;

                if ( depth == 0 )
                {
                    if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_MORE_DETAILS )
                    {
                        LOCK_GUARD l(*lock_cout);
                        PLOGMSG ( klogInfo,
                            ( klogInfo,
                            "[$(THREAD_NUM)] depth=0 at the ref_pos=$(POS) (id=$(ID)) filtering out",
                            "THREAD_NUM=%zu,POS=%ld,ID=%ld", thread_num, pos, pos + ref_id_start
                            ));
                    }
                    update_run_coverage ( acc, 0, 0, 0, 0, lock_cout, pcoverage_count, index );
                    return PILEUP_DEFINITELY_NOT_FOUND;
                }

                // if OPTION_COUNT_STRAND != none - cannot report any non-zero counts,
                // so no further optimization is possible
                if ( g_Params.count_strand != COUNT_STRAND_NONE )
                    continue;

                if ( depth < alignments_total )
                    alignments_total = depth;

                if ( indel_cnt == 0 ) // pure mismatch optimization
                {
                    // 1. for query of length == 1 we can return PILEUP_DEFINITELY_FOUND
                    //    and produce output (for total count can use DEPTH for -c option)

                    uint32_t mismatch[4];
                    count = cursor.ReadItems ( pos + ref_id_start, PileupColumnIndex[idx_MISMATCH_COUNTS], mismatch, sizeof mismatch );
                    assert ( count == 0 || count == 4 );

                    size_t allele_size = obj.GetAlleleSize();
                    char const* allele = obj.GetAllele();
                    assert (count == 0 || pos - ref_pos < allele_size );
                    size_t alignments_matched = count == 0 ? 0 :
                        mismatch [base2na_to_index(allele[pos - ref_pos])];

                    if ( obj.GetAlleleLenOnRef() == 1 && obj.GetAlleleSize() == 1 )
                    {
                        update_run_coverage ( acc, alignments_total, 0, alignments_matched, 0, lock_cout, pcoverage_count, index );
                        return alignments_matched == 0 ?
                            PILEUP_DEFINITELY_NOT_FOUND : PILEUP_DEFINITELY_FOUND;
                    }

                    // 2. if at least one position has zero MISMATCH_COUNT - PILEUP_DEFINITELY_NOT_FOUND
                    //    for the case when no -c option is specified
                    //    otherwise we have to go into SRR to get actual
                    //    alignments_total or alignments_matched

                    else if ( alignments_matched == 0 && ! g_Params.calc_coverage )
                    {
                        update_run_coverage ( acc, alignments_total, 0, alignments_matched, 0, lock_cout, pcoverage_count, index );
                        return PILEUP_DEFINITELY_NOT_FOUND;
                    }
                }
                // TODO: see if INSERTION_COUNTS or DELETION_COUNT can be also used
                // for optimizations (at least for the case of lenght=1 indels).
#if 0
                else if ( indel_cnt > 0 ) // insertion
                {
                    uint32_t counts[4];
                    count = cursor.ReadItems ( pos + ref_id_start, PileupColumnIndex[idx_INSERTION_COUNTS], counts, sizeof counts );
                    assert ( count == 0 || count == 4 );

                    LOCK_GUARD l(*lock_cout);
                    std::cout
                        << "pos=" << pos
                        << ", pileup row_id=" << pos + ref_id_start
                        << ", insertions=";
                    if ( count )
                    {
                        std::cout << "[";
                        for (size_t i = 0; i < count; ++i)
                            std::cout << ( i == 0 ? "" : ", ") << counts[i];
                        std::cout << "]";
                    }
                    else
                        std::cout << "<none>";

                    count = cursor.ReadItems ( pos + ref_id_start, PileupColumnIndex[idx_MISMATCH_COUNTS], counts, sizeof counts );
                    assert ( count == 0 || count == 4 );
                    std::cout << ", mismatches=";
                    if ( count )
                    {
                        std::cout << "[";
                        for (size_t i = 0; i < count; ++i)
                            std::cout << ( i == 0 ? "" : ", ") << counts[i];
                        std::cout << "]";
                    }
                    else
                        std::cout << "<none>";

                    count = cursor.ReadItems ( pos + ref_id_start, PileupColumnIndex[idx_DELETION_COUNT], counts, sizeof counts[0] );
                    assert ( count == 0 || count == 1 );

                    std::cout << ", deletions=";
                    if ( count > 0 ) 
                        std::cout << counts[0];
                    else
                        std::cout << "<none>";

                    std::cout << std::endl;
                }
                else
                {
                    LOCK_GUARD l(*lock_cout);
                    std::cout << "DELETION" << std::endl;
                }
#endif
            }

            if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_SOME_DETAILS )
            {
                char run_name[64];
                uint32_t count = cursor.ReadItems ( id_first, PileupColumnIndex[idx_RUN_NAME], run_name, countof(run_name)-1 );
                assert (count < countof(run_name));
                run_name [count] = '\0';

                if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_SOME_DETAILS )
                {
                    LOCK_GUARD l(*lock_cout);
                    PLOGMSG ( klogInfo,
                        ( klogInfo,
                        "[$(THREAD_NUM)] $(RUNNAME) is suspicious",
                        "THREAD_NUM=%zu,RUNNAME=%s", thread_num, run_name
                        ));
                }
                //char const* p = run_name[0] == '/' ? run_name + 1 : run_name;
            }

            return PILEUP_MAYBE_FOUND;
        }
        catch ( Utils::CErrorMsg const& e )
        {
            if ( e.getRC() == SILENT_RC(rcVFS,rcMgr,rcOpening,rcDirectory,rcNotFound)
                || e.getRC() == SILENT_RC(rcVFS,rcTree,rcResolving,rcPath,rcNotFound))
            {
                if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_MORE_DETAILS )
                {
                    LOCK_GUARD l(*lock_cout);
                    PLOGMSG ( klogInfo,
                        ( klogInfo,
                        "[$(THREAD_NUM)] pileup db NOT FOUND, need to look into run itself",
                        "THREAD_NUM=%zu", thread_num
                        ));
                }
                return PILEUP_MAYBE_FOUND;
            }
            else if ( e.getRC() == SILENT_RC(rcDB,rcMgr,rcOpening,rcDatabase,rcIncorrect))
            {
                if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_MORE_DETAILS )
                {
                    LOCK_GUARD l(*lock_cout);
                    PLOGMSG ( klogWarn,
                        ( klogWarn,
                        "[$(THREAD_NUM)] BAD pileup db, need to look into run itself",
                        "THREAD_NUM=%zu", thread_num
                        ));
                }
                return PILEUP_MAYBE_FOUND;
            }
            else
                throw;
        }
    }

    bool is_primary_mate ( ngs::Fragment const& frag )
    {
        ngs::StringRef id = frag.getFragmentId();

        // inplace strstr for non null-terminating string

        char const pattern[] = { '.', 'F', 'A', '0', '.' };

        if ( id.size() < countof ( pattern ) )
            return false;

        char const* data = id.data();
        size_t stop = id.size() - countof ( pattern );

        for (size_t i = 0; i <= stop; ++i )
        {
            if (   data [ i + 0 ] == pattern [ 0 ]
                && data [ i + 1 ] == pattern [ 1 ]
                && data [ i + 2 ] == pattern [ 2 ]
                && data [ i + 3 ] == pattern [ 3 ]
                && data [ i + 4 ] == pattern [ 4 ]
                )
            {
                return true;
            }
        }

        return false;
    }

    template <class TLock> void find_alignments_in_run_db ( char const* acc,
        char const* path, char const* ref_name, size_t ref_start,
        char const* variation, size_t var_size, size_t slice_size,
        TLock* lock_cout, size_t thread_num,
        coverage_info* pcoverage_count, size_t index)
    {
        if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_MORE_DETAILS )
        {
            LOCK_GUARD l(*lock_cout);
            PLOGMSG ( klogInfo,
                ( klogInfo,
                "[$(THREAD_NUM)] Processing $(ACC)",
                "THREAD_NUM=%zu,ACC=%s", thread_num, acc
                ));
        }

        ncbi::ReadCollection run = ncbi::NGS::openReadCollection ( path && path[0] ? path : acc );

        // TODO: if it works, it should me moved at the upper level, so
        // it would not be calculated for each run. It should be done one time
        // for each variation search
        char const* pattern = g_Params.query_min_rep != 0 ? g_Params.query : NULL;
        size_t pattern_len = g_Params.query_min_rep != 0 ? strlen (g_Params.query) : 0;

        if ( run.hasReference ( ref_name ) )
        {
            if ( slice_size == 0 )
                slice_size = 1; // for a pure insertion we at least a slice of length == 1 ?

            ngs::Reference reference = run.getReference( ref_name );
            //ngs::AlignmentIterator ai = reference.getAlignmentSlice (
            //    ref_start, slice_size + pattern_len, ngs::Alignment::all);

            // TODO: remove C-cast to ngs::Alignment::AlignmentFilter
            // when it's fixed in ngs api
            ngs::AlignmentIterator ai = reference.getFilteredAlignmentSlice (
                ref_start, slice_size + pattern_len, ngs::Alignment::all, (ngs::Alignment::AlignmentFilter)0, 0);

            size_t alignments_total = 0, alignments_total_negative = 0;
            size_t alignments_matched = 0, alignments_matched_negative = 0;
            while ( ai.nextAlignment() )
            {
                uint64_t ref_pos_range = ai.getReferencePositionProjectionRange (ref_start);
                if ( ref_pos_range == (uint64_t)-1 ) // effectively, checking that read doesn't start past ref_start
                    continue;

                int64_t align_pos_first = (int64_t)( ref_pos_range >> 32);
                int64_t align_pos_count = ref_pos_range & 0xFFFFFFFF;

                // checking that read doesn't end before ref slice ends
                if ( (int64_t) ai.getAlignmentLength() - align_pos_first < (int64_t)slice_size )
                    continue;

                ngs::StringRef bases = ai.getAlignedFragmentBases ();

                ++ alignments_total;
                bool is_negative = g_Params.count_strand != COUNT_STRAND_NONE
                    && ai.getIsReversedOrientation();
                if ( g_Params.count_strand == COUNT_STRAND_COUNTERALIGNED && ! is_primary_mate ( ai ) )
                    is_negative = ! is_negative;

                if (is_negative)
                    ++ alignments_total_negative;

                char const* bases_data = bases.data();
                size_t bases_size = bases.size();

                // trying all possible starting positions in
                // the case when single reference position
                // can have multiple alignment projections
                for (int64_t i = 0; i < align_pos_count; ++i)
                {
                    int64_t align_pos = align_pos_first + i;

                    bool match = bases_size + align_pos >= var_size && strncmp (variation, bases_data + align_pos, var_size) == 0;
                    if ( match && pattern != NULL )
                    {
                        char const* align_suffix = bases_data + align_pos + var_size;
                        size_t align_suffix_size = bases_size - (align_pos + var_size);

                        size_t min_size = align_suffix_size < pattern_len ? align_suffix_size : pattern_len;
                        match = strncmp ( pattern, align_suffix, min_size ) != 0;
                    }
                    if ( match )
                    {
                        ++ alignments_matched;
                        if ( ! g_Params.calc_coverage )
                            goto BREAK_ALIGNMENT_ITER; // -c option is for speed-up, so we sacrifice verbose output
                        if (is_negative)
                            ++ alignments_matched_negative;
                    }
                    if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_SOME_DETAILS )
                    {
                        LOCK_GUARD l(*lock_cout);
                        PLOGMSG ( klogInfo,
                            ( klogInfo,
                            "[$(THREAD_NUM)] id=$(ID): $(BASES)$(MATCH)",
                            "THREAD_NUM=%zu,ID=%s,BASES=%s,MATCH=%s",
                            thread_num, ai.getAlignmentId().data(),
                            bases.toString ( align_pos, var_size ).c_str(),
                            match ? " MATCH!" : ""
                            ));
                    }
                    if ( match )
                        break;
                }
            }

BREAK_ALIGNMENT_ITER:
            update_run_coverage ( acc,
                alignments_total, alignments_total - alignments_total_negative,
                alignments_matched, alignments_matched - alignments_matched_negative,
                lock_cout, pcoverage_count, index );
        }
        else
        {
            if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_MORE_DETAILS )
            {
                LOCK_GUARD l(*lock_cout);
                PLOGMSG ( klogInfo,
                    ( klogInfo,
                    "[$(THREAD_NUM)] reference $(REFNAME) NOT FOUND in $(ACC), skipping",
                    "THREAD_NUM=%zu,REFNAME=%s,ACC=%s", thread_num, ref_name, acc
                    ));
            }

            update_run_coverage ( acc, 0, 0, 0, 0, lock_cout, pcoverage_count, index );
        }
    }

    template <class TLock> void find_alignments_in_single_run (char const* acc,
        char const* path, char const* pileup_path, char const* ref_name,
        KSearch::CVRefVariation const* pobj, size_t index,
        TLock* lock_cout, size_t thread_num, coverage_info* pcoverage_count)
    {
        char const* variation;
        size_t query_len_on_ref, var_start, var_size;

        variation = pobj->GetSearchQuery();
        var_size = pobj->GetSearchQuerySize();
        query_len_on_ref = pobj->GetSearchQueryLenOnRef();
        var_start = pobj->GetSearchQueryStartAbsolute();

        int res = find_alignment_in_pileup_db ( acc, pileup_path, ref_name,
            pobj, var_start, query_len_on_ref, lock_cout, thread_num, pcoverage_count, index );

        if ( res == PILEUP_MAYBE_FOUND )
        {
            find_alignments_in_run_db( acc, path, ref_name, var_start,
                variation, var_size, query_len_on_ref,
                lock_cout, thread_num, pcoverage_count, index );
        }
    }

    template <class TLock> void find_alignments_in_single_run ( char const* acc,
        char const* path, char const* pileup_path, char const* ref_name,
        std::vector <KSearch::CVRefVariation> const* pvec_obj,
        TLock* lock_cout, size_t thread_num )
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

        std::vector <KSearch::CVRefVariation> const& vec_obj = *pvec_obj;

        coverage_info coverage_counts = {};
        coverage_counts.count_total.count = (size_t)-1;
        coverage_counts.count_total.count_posititve = (size_t)-1;
        coverage_counts.counts_matched.resize( vec_obj.size() );

        std::vector <KSearch::CVRefVariation>::const_iterator cit = vec_obj.begin();
        std::vector <KSearch::CVRefVariation>::const_iterator cend = vec_obj.end();
        for ( size_t i = 0; cit != cend; ++cit, ++i )
        {
            KSearch::CVRefVariation const& obj = *cit;
            find_alignments_in_single_run ( acc, path, pileup_path,
                ref_name, & obj, i, lock_cout, thread_num, & coverage_counts );
        }

        report_run_coverage ( acc, & coverage_counts, lock_cout );
    }

    template <class TLock> void find_alignments ( char const* ref_name,
        std::vector <KSearch::CVRefVariation> const* pvec_obj,
        TLock* lock_cout, size_t thread_num, CInputRuns const* p_input_runs,
        KApp::CProgressBar* progress_bar )
    {
        try
        {
            for ( ; ; )
            {
                size_t index = p_input_runs -> GetNextIndex();
                CInputRun const& input_run = p_input_runs -> Get(index);

                if ( ! input_run.IsValid() )
                    break;

                char const* acc = input_run.GetRunName().c_str();
                char const* path = input_run.GetRunPath().c_str();
                char const* pileup_path = input_run.GetPileupStatsPath().c_str();

                if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_MORE_DETAILS )
                {
                    LOCK_GUARD l(*lock_cout);
                    PLOGMSG ( klogInfo,
                        ( klogInfo,
                        "[$(THREAD_NUM)] Processing parameter # $(INDEX): $(ACC), path=[$(PATH)], pileup path=[$(PILEUPPATH)]",
                        "THREAD_NUM=%zu,INDEX=%zu,ACC=%s,PATH=%s,PILEUPPATH=%s",
                        thread_num, index,
                        acc, path, pileup_path
                        ));
                }

                try
                {
                    find_alignments_in_single_run ( acc, path, pileup_path,
                        ref_name, pvec_obj, lock_cout, thread_num );
                }
                catch ( ngs::ErrorMsg const& e )
                {
                    if ( strstr (e.what(), "Cannot open accession") == e.what() )
                    {
                        if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_MORE_DETAILS )
                        {
                            LOCK_GUARD l(*lock_cout);
                            PLOGMSG ( klogWarn,
                                ( klogWarn,
                                "[$(THREAD_NUM)] $(WHAT), skipping",
                                "THREAD_NUM=%zu,WHAT=%s", thread_num, e.what()
                                ));
                        }
                    }
                    else
                        throw;
                }

                {
                    LOCK_GUARD l(*lock_cout);
                    progress_bar -> Process( 1, false );
                }

                if ( ::Quitting() )
                {
                    LOCK_GUARD l(*lock_cout);
                    PLOGMSG ( klogWarn,
                        ( klogWarn,
                        "[$(THREAD_NUM)] Interrupted",
                        "THREAD_NUM=%zu", thread_num
                        ));
                    break;
                }
            }
        }
        catch ( ngs::ErrorMsg const& e )
        {
            LOCK_GUARD l(*lock_cout); // reuse cout mutex
            PLOGMSG ( klogErr,
                ( klogErr,
                "[$(THREAD_NUM)] ngs::ErrorMsg: $(WHAT)",
                "THREAD_NUM=%zu,WHAT=%s", thread_num, e.what()
                ));
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
        std::vector <KSearch::CVRefVariation> const* pvec_obj;
        LOCK* lock_cout;
        size_t thread_num;
        CInputRuns const* p_input_runs;
        KApp::CProgressBar* progress_bar;
    };

    void AdapterFindAlignment_Init (AdapterFindAlignment & params,
            size_t param_start, size_t param_count,
            char const* ref_name,
            std::vector <KSearch::CVRefVariation> const* pvec_obj,
            LOCK* lock_cout,
            size_t thread_num,
            CInputRuns const* p_input_runs,
            KApp::CProgressBar* progress_bar
        )
    {
        params.param_start = param_start;
        params.param_count = param_count;
        params.ref_name = ref_name;
        params.pvec_obj = pvec_obj;
        params.lock_cout = lock_cout;
        params.thread_num = thread_num;
        params.p_input_runs = p_input_runs;
        params.progress_bar = progress_bar;
    }

    rc_t AdapterFindAlignmentFunc ( void* data )
    {
        AdapterFindAlignment& p = * (static_cast<AdapterFindAlignment*>(data));
        find_alignments ( p.ref_name, p.pvec_obj, p.lock_cout, p.thread_num,
            p.p_input_runs, p.progress_bar );
        return 0;
    }
#endif

    void finish_find_variation_region ( KApp::CArgs const & args,
        std::vector <KSearch::CVRefVariation> const& vec_obj,
        KApp::CProgressBar& progress_bar )
    {
        // Split further processing into multiple threads if there too many params
        CInputRuns input_runs ( args );

        size_t param_count = input_runs.GetCount();
        progress_bar.Append ( param_count );

        if ( param_count == 0 )
            return;

        if ( param_count >= 1 && param_count < g_Params.thread_count )
            g_Params.thread_count = param_count;

        size_t thread_count = g_Params.thread_count;

        if ( thread_count == 1 )
        {
            CNoMutex mtx;
            find_alignments (g_Params.ref_acc, & vec_obj, & mtx,
                0, & input_runs, & progress_bar);
        }
        else
        {
            // split
            if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_SOME_DETAILS )
            {
                PLOGMSG ( klogInfo,
                    ( klogInfo,
                    "Splitting $(PARAMCOUNT) jobs into $(THREADCOUNT) threads...",
                    "PARAMCOUNT=%zu,THREADCOUNT=%zu", param_count, thread_count
                    ));
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
                    std::thread( find_alignments <LOCK>,
                                    i * param_chunk_size,
                                    current_chunk_size, g_Params.ref_acc, & vec_obj,
                                    & mutex_cout, i + 1, & input_runs, & progress_bar
                               ));
#else
                AdapterFindAlignment & params = vec_threads [ i ];

                AdapterFindAlignment_Init ( params, i * param_chunk_size,
                    current_chunk_size, g_Params.ref_acc, & vec_obj,
                    & mutex_cout, i + 1, & input_runs, & progress_bar );

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

    bool check_ref_slice ( char const* ref, size_t ref_size )
    {
        bool ret = ref_size == 0;
        for ( size_t i = 0; i < ref_size; ++i )
        {
            if ( !(ref [i] == 'N' || ref [i] == 'n' || ref [i] == '.') )
            {
                ret = true; // at least one non-N base is OK
                break;
            }
        }
        return ret;
    }

    char const* get_query (char const* pattern, uint32_t repetitions, std::string& generated_query)
    {
        if ( repetitions == 0 )
            return pattern;
        else
        {
            size_t pattern_len = strlen (pattern);
            generated_query.reserve ( pattern_len * repetitions );
            generated_query.clear();

            while ( repetitions-- > 0 )
                generated_query.append ( pattern );

            return generated_query.c_str();
        }
    }

    void print_variation_specs ( char const* ref_slice, size_t ref_slice_size,
        KSearch::CVRefVariation const& obj, const char* query, size_t query_len )
    {
        if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_SOME_DETAILS )
        {
            size_t ref_start = obj.GetAlleleStartAbsolute();
            size_t ref_len = obj.GetAlleleLenOnRef();
            PLOGMSG ( klogInfo,
                ( klogInfo,
                "Found indel box at pos=$(REFSTART), length=$(REFLEN)",
                "REFSTART=%zu,REFLEN=%zu", ref_start, ref_len
                ));
            print_indel ( "reference", ref_slice, ref_slice_size, obj.GetAlleleStartRelative(), ref_len );

            PLOGMSG ( klogInfo,
                ( klogInfo,
                "var_query=$(VARIATION)", "VARIATION=%s", obj.GetSearchQuery()
                ));
        }

        if ( g_Params.verbosity >= NSRefVariation::VERBOSITY_PRINT_VAR_SPEC )
        {
            PLOGMSG ( klogWarn,
                ( klogWarn,
                "Input variation spec   : $(REFACC):$(REFPOSVAR):$(VARLENONREF):$(QUERY)",
                "REFACC=%s,REFPOSVAR=%ld,VARLENONREF=%lu,QUERY=%.*s",
                g_Params.ref_acc, g_Params.ref_pos_var,
                g_Params.var_len_on_ref, query_len, query
                ));

            size_t allele_size = obj.GetAlleleSize();
            char const* allele = obj.GetAllele();
            PLOGMSG ( klogWarn,
                ( klogWarn,
                "Adjusted variation spec: $(REFACC):$(REFPOSVAR):$(VARLENONREF):$(ALLELE)",
                "REFACC=%s,REFPOSVAR=%ld,VARLENONREF=%lu,ALLELE=%.*s",
                g_Params.ref_acc, obj.GetAlleleStartAbsolute(),
                obj.GetAlleleLenOnRef(), (int)allele_size, allele
                ));
        }
    }

    void get_ref_var_object (KSearch::CVRefVariation& obj,
        char const* query, size_t query_len, ngs::ReferenceSequence const& ref_seq)
    {
        size_t var_len = query_len;

        size_t chunk_size = 5000; // TODO: add the method Reference[Sequence].getChunkSize() to the API
        size_t chunk_no = g_Params.ref_pos_var / chunk_size;
        size_t ref_pos_in_slice = g_Params.ref_pos_var % chunk_size;
        size_t bases_start = chunk_no * chunk_size;
        size_t chunk_no_last = ref_seq.getLength() / chunk_size;

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
            
            cont = Common::find_variation_core_step ( obj, g_Params.alg,
                ref_chunk.data(), ref_chunk.size(), ref_pos_in_slice,
                query, var_len, g_Params.var_len_on_ref,
                chunk_size, chunk_no_last, bases_start, chunk_no_start, chunk_no_end );

            if ( !cont )
            {
                print_variation_specs ( ref_chunk.data(), ref_chunk.size(), obj, query, query_len );
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

                cont = Common::find_variation_core_step ( obj, g_Params.alg,
                    ref_slice.c_str(), ref_slice.size(), ref_pos_in_slice,
                    query, var_len, g_Params.var_len_on_ref,
                    chunk_size, chunk_no_last, bases_start, chunk_no_start, chunk_no_end );
            }
            print_variation_specs ( ref_slice.c_str(), ref_slice.size(), obj, query, query_len );
        }
    }

    void check_var_objects (std::vector <KSearch::CVRefVariation> const& vec_obj)
    {
        assert (vec_obj.size() > 0);

        // checking for invariants
        size_t count = vec_obj.size();

        if (count > 1)
        {
            size_t i_first_var = 0;
            for (; i_first_var < count; ++i_first_var)
            {
                KSearch::CVRefVariation const& cur = vec_obj [i_first_var];
                if ( cur.GetAlleleLenOnRef() != cur.GetAlleleSize() )
                    break;
            }

            KSearch::CVRefVariation const& first = vec_obj [i_first_var];

            for (size_t i = i_first_var + 1; i < count; ++i)
            {
                KSearch::CVRefVariation const& cur = vec_obj[i];

                // exception: if var_len_on_ref == var_size - skip
                // this is probably the exact reference
                if ( cur.GetAlleleLenOnRef() != cur.GetAlleleSize() )
                {
                    if ( first.GetAlleleStartAbsolute() != cur.GetAlleleStartAbsolute()
                        || first.GetAlleleLenOnRef() != cur.GetAlleleLenOnRef() )
                    {
                        size_t allele_size_first = first.GetAlleleSize();
                        char const* allele_first = first.GetAllele();

                        size_t allele_size_cur = cur.GetAlleleSize();
                        char const* allele_cur = cur.GetAllele();

                        PLOGMSG( klogWarn, (klogWarn,
                            "Inconsistent variations found: (start=$(STARTFIRST), len=$(LENFIRST), allele=$(ALLELEFIRST)) vs (start=$(STARTCUR), len=$(LENCUR), allele=$(ALLELECUR))",
                            "STARTFIRST=%zu,LENFIRST=%zu,ALLELEFIRST=%.*s,STARTCUR=%zu,LENCUR=%zu,ALLELECUR=%.*s",
                            first.GetAlleleStartAbsolute(), first.GetAlleleLenOnRef(), (int)allele_size_first, allele_first,
                            cur.GetAlleleStartAbsolute(), cur.GetAlleleLenOnRef(), (int)allele_size_cur, allele_cur
                            ));
                        //throw Utils::CErrorMsg (
                        //    "Inconsistent variations found: (start=%zu, len=%zu, variation=%s) vs (start=%zu, len=%zu, variation=%s)",
                        //    first.GetVarStartAbsolute(), first.GetVarLenOnRef(), first.GetVariation(),
                        //    cur.GetVarStartAbsolute(), cur.GetVarLenOnRef(), cur.GetVariation());
                    }
                }
            }
        }
    }

    void find_reference_variation ( ngs::ReferenceSequence const& ref_seq )
    {
        uint64_t start = g_Params.ref_pos_var;
        uint64_t len = g_Params.var_len_on_ref;

        KSearch::CVRefVariation obj;

        // this doesn't work for len = 0
        ngs::StringRef query = ref_seq.getReferenceChunk ( start, len );
        if ( query.size() == len )
        {
            get_ref_var_object ( obj, query.data(), query.size(), ref_seq );
        }
        else
        {
            ngs::String query_copy = ref_seq.getReferenceBases ( start, len );
            get_ref_var_object ( obj, query_copy.c_str(), query_copy.size(), ref_seq );
        }
    }

    int find_variation_region_impl (KApp::CArgs const& args)
    {
        // Adding 0% mark at the very beginning of the program
        KApp::CProgressBar progress_bar(1);
        progress_bar.Process ( 0, true );

        ngs::ReferenceSequence ref_seq = ncbi::NGS::openReferenceSequence ( g_Params.ref_acc );

        size_t variation_count = g_Params.query_min_rep == 0 ?
            1 : g_Params.query_max_rep - g_Params.query_min_rep + 1;

        std::string generated_query;
        if (g_Params.query_min_rep > 0)
            generated_query.reserve ( strlen(g_Params.query) * g_Params.query_max_rep );

        std::vector <KSearch::CVRefVariation> vec_obj ( variation_count );

        // first, try to search against reference itself
        // find_reference_variation ( ref_seq );

        for (uint32_t i = 0; i < variation_count; ++i)
        {
            char const* query = get_query ( g_Params.query,
                g_Params.query_min_rep + i, generated_query );
            get_ref_var_object ( vec_obj [i], query, strlen(query), ref_seq );
        }

        check_var_objects (vec_obj);
        finish_find_variation_region ( args, vec_obj, progress_bar );

        return 0;
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
        ngs::StringRef align_str = align.getAlignedFragmentBases();

        std::cout
            << "ref: " << ref_str << std::endl
            << "seq: " << align_str << std::endl;

        for ( ; ref_pos < (int64_t)stop; ++ ref_pos )
        {
            ngs::String ref_base = ref.getReferenceBases( ref_pos, 1 );
            uint64_t range = align.getReferencePositionProjectionRange ( ref_pos );
            uint32_t align_pos = range >> 32;
            uint32_t range_len = range & 0xFFFFFFFF;

            ngs::StringRef bases = align.getAlignedFragmentBases ( );

            std::cout
                << ref_pos << " "
                << ref_base
                << " (" << (int32_t)align_pos << ", " << range_len << ") "
                << bases.toString( align_pos, range_len == 0 ? 1 : range_len )
                << std::endl;
        }

        std::cout << "getReferenceProjectionRange test has SUCCEEDED" << std::endl;
    }

    void test_deletion_ambiguity ()
    {
        char const ref[] = "xABCABCy";
        char const var[] = "";
        size_t var_len_on_ref = g_Params.var_len_on_ref;
        size_t pos = g_Params.ref_pos_var;

        KSearch::CVRefVariation obj = KSearch::VRefVariationIUPACMake (
            ref, countof(ref) - 1, pos, var, countof(var) - 1, var_len_on_ref, 0 );

        std::cout
            << "Found indel box at pos=" << obj.GetVarStartRelative()
            << ", length=" << obj.GetVarLenOnRef()
            << std::endl;
        print_indel ( "reference", ref, countof(ref) - 1, obj.GetVarStartRelative(), obj.GetVarLenOnRef() );
    }

    void test_temp ()
    {
        char const ref[] = "ACACACTA";
        char const var[] = "AGCACACA";
        size_t var_len_on_ref = countof(ref) - 1;
        size_t pos = 0;

        KSearch::CVRefVariation obj = KSearch::VRefVariationIUPACMake (
            ref, countof(ref) - 1, pos, var, countof(var) - 1, var_len_on_ref, 0 );

        std::cout
            << "Found indel box at pos=" << obj.GetVarStartRelative()
            << ", length=" << obj.GetVarLenOnRef()
            << std::endl;
        print_indel ( "reference", ref, countof(ref) - 1, obj.GetVarStartRelative(), obj.GetVarLenOnRef() );
        print_variation_specs ( ref, countof(ref), obj, var );
    }
#endif

    // returns pointer to the character that cannot be parsed
    char const* parse_query ( char const* str )
    {
        char const allowed[] = "ACGTNacgtn.-";
        size_t i;
        for ( i = 0; str [i] != '\0'; ++i )
        {
            if ( strchr ( allowed, str[i] ) == NULL )
                break;
        }

        if ( i >= countof (g_Params.query) )
        {
            LOGMSG( klogErr, "The query is too long" );
            return str + i;
        }

        strncpy ( g_Params.query, str, i );

        // TODO: maybe CArgs should allow for empty option value
        if (g_Params.query [0] == '-' && g_Params.query [1] == '\0' )
            g_Params.query[0] = '\0';

        if ( str [i] == '[' )
        {
            // the next substring must contain: "\[[0-9]+\-[0-9]+\]$"
            ++i;
            uint32_t num = 0;
            size_t i_num_start = i;
            for ( ; str[i] >= '0' && str[i] <= '9'; ++i )
            {
                num *= 10;
                num += str[i] - '0';
            }

            if ( num == 0 )
                return str + i_num_start;

            g_Params.query_min_rep = num;

            if ( str[i] != '-' )
                return str + i;

            ++i;
            i_num_start = i;
            for ( num = 0; str[i] >= '0' && str[i] <= '9'; ++i )
            {
                num *= 10;
                num += str[i] - '0';
            }

            if ( num == 0 )
                return str + i_num_start;

            g_Params.query_max_rep = num;

            if ( str[i] != ']' )
                return str + i;

            ++i;
            if ( str[i] != '\0' )
                return str + i;

            return NULL;
        }
        else if ( str [i] != '\0' )
            return str + i;
        else
            return NULL;
    }

    int find_variation_region_impl_safe ( KApp::CArgs const& args)
    {
        int ret = 0;
        try
        {
            ret = find_variation_region_impl ( args );
        }
        catch ( ngs::ErrorMsg const& e )
        {
            PLOGMSG ( klogErr,
                ( klogErr, "ngs::ErrorMsg: $(WHAT)", "WHAT=%s", e.what()
                ));
            ret = 3;
        }
        catch (...)
        {
            Utils::HandleException ();
            ret = 3;
        }

        return ret;
    }

    int find_variation_region (int argc, char** argv)
    {
        int ret = 0;
        try
        {
            KApp::CArgs args;
            args.MakeAndHandle (argc, argv, Options, countof (Options), ::XMLLogger_Args, ::XMLLogger_ArgsQty);
            KApp::CXMLLogger xml_logger ( args );

            // Actually GetOptionCount check is not needed here since
            // CArgs checks that exactly 1 option is required

            if (args.GetOptionCount (OPTION_REFERENCE_ACC) == 1)
                g_Params.ref_acc = args.GetOptionValue ( OPTION_REFERENCE_ACC, 0 );

            if (args.GetOptionCount (OPTION_REF_POS) == 1)
                g_Params.ref_pos_var = args.GetOptionValueInt<int64_t> ( OPTION_REF_POS, 0 );

            if (args.GetOptionCount (OPTION_QUERY) == 1)
            {
                char const* query = args.GetOptionValue ( OPTION_QUERY, 0 );
                char const* pInvalid = parse_query ( query );
                if ( pInvalid != NULL )
                {
                    PLOGMSG ( klogErr,
                        ( klogErr,
                        "Error: the given query ($(QUERY)) contains an invalid character ($(CHAR))",
                        "QUERY=%s,CHAR=%c", g_Params.query, *pInvalid
                        ));
                    return 3;
                }
                else if ( g_Params.query_min_rep != 0
                    && g_Params.query_min_rep >= g_Params.query_max_rep )
                {
                    PLOGMSG ( klogErr,
                        ( klogErr,
                        "Error: the given query ($(QUERY)) contains an invalid repetition range [$(MIN)-$(MAX)] (min must be less than max and more than zero)",
                        "QUERY=%s,MIN=%u,MAX=%u",
                        g_Params.query, g_Params.query_min_rep, g_Params.query_max_rep
                        ));
                    return 3;
                }
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
                    PLOGMSG ( klogErr,
                        ( klogErr,
                        "$(PROGNAME) $(OPTIONNAME) option must not be provided along with parameters,"
                        " use either command line parameters or input file but not both",
                        "PROGNAME=%s,OPTIONNAME=%s", argv [0], OPTION_INPUT_FILE
                        ));
                    return 3;
                }
            }

            if (args.GetOptionCount (OPTION_COUNT_STRAND) == 1)
            {
                char const* val = args.GetOptionValue ( OPTION_COUNT_STRAND, 0 );
                if ( strcmp (val, COUNT_STRAND_NONE_STR) == 0 )
                    g_Params.count_strand = COUNT_STRAND_NONE;
                else if ( strcmp (val, COUNT_STRAND_COUNTERALIGNED_STR) == 0 )
                    g_Params.count_strand = COUNT_STRAND_COUNTERALIGNED;
                else if ( strcmp (val, COUNT_STRAND_COALIGNED_STR) == 0 )
                    g_Params.count_strand = COUNT_STRAND_COALIGNED;
                else
                {
                    PLOGMSG ( klogErr,
                        ( klogErr,
                        "Error: unrecognized $(OPTIONNAME) option value: \"$(VALUE)\"",
                        "OPTIONNAME=%s,VALUE=%s", OPTION_COUNT_STRAND, val
                        ));
                    return 3;
                }

                if ( args.GetOptionCount (OPTION_COVERAGE) == 0 )
                {
                    PLOGMSG ( klogWarn,
                        ( klogWarn,
                        "Warning: $(COUNTSTRAND) option has no effect if $(COVERAGE) is not specified",
                        "COUNTSTRAND=%s,COVERAGE=%s", OPTION_COUNT_STRAND, OPTION_COVERAGE
                        ));
                    g_Params.count_strand = COUNT_STRAND_NONE;
                }
            }

            if (args.GetOptionCount (OPTION_ALG) == 1)
            {
                char const* alg = args.GetOptionValue ( OPTION_ALG, 0 );
                if (!strcmp(alg, PARAM_ALG_SW))
                    g_Params.alg = ::refvarAlgSW;
                else if (!strcmp(alg, PARAM_ALG_RA))
                    g_Params.alg = ::refvarAlgRA;
                else
                {
                    PLOGMSG ( klogErr, ( klogErr,
                        "Error: Unknown algorithm specified: \"$(ALG)\"", "ALG=%s", alg ));
                    return 3;
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
                case 5:
                    test_deletion_ambiguity ();
                    break;
                case 6:
                    test_temp ();
                    break;
                default:
                    std::cout
                        << "specify value for this option:" << std::endl
                        << "1 - run correctness test" << std::endl
                        << "2 - run ngs performance test" << std::endl
                        << "3 - run vdb performance test" << std::endl
                        << "4 - run getReferencePositionProjectionRange test" << std::endl
                        << "5 - run deletion ambiguity test" << std::endl
                        << "6 - run temporary test" << std::endl;
                }
            }
            else
#endif
            {
                ret = find_variation_region_impl_safe (args);
            }

        }
        catch (...) // here we handle only exceptions in CArgs or CXMLLogger
        {
            ret = 3;
        }
        
        return ret;
    }

/////////////////////////////
    // class CInputRuns

    CInputRuns::CInputRuns (KApp::CArgs const& args)
    {
        m_param_index.counter = 0;

        Init ( args );
    }

    CInputRuns::CInputRuns ()
    {
        m_param_index.counter = 0;
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
        size_t current_index = atomic_read_and_add( & m_param_index, 1 );
        return current_index < m_input_runs.size() ? m_input_runs[current_index] : CInputRun("");
    }
    size_t CInputRuns::GetNextIndex () const
    {
        return atomic_read_and_add( & m_param_index, 1 );
    }
    CInputRun CInputRuns::Get(size_t index) const
    {
        return index < m_input_runs.size() ? m_input_runs[index] : CInputRun("");
    }

}

extern "C"
{
    const char UsageDefaultName[] = "ref-variation";

    rc_t CC UsageSummary (const char * progname)
    {
        OUTMSG ((
        "Usage example:\n"
        "  %s -r <reference accession> -p <position on reference> -q <query to look for> -l 0 [<parameters>]\n"
        "\n"
        "Summary:\n"
        "  Find a possible indel window\n"
        "\n", progname));
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


        OUTMSG (("\nParameters: optional space-separated list of run accessions in which the query will be looked for\n\n"));

        OUTMSG (("\nOptions:\n"));

        HelpOptionLine (NSRefVariation::ALIAS_REFERENCE_ACC, NSRefVariation::OPTION_REFERENCE_ACC, "acc", NSRefVariation::USAGE_REFERENCE_ACC);
        HelpOptionLine (NSRefVariation::ALIAS_REF_POS, NSRefVariation::OPTION_REF_POS, "value", NSRefVariation::USAGE_REF_POS);
        HelpOptionLine (NULL, NSRefVariation::OPTION_QUERY, "string", NSRefVariation::USAGE_QUERY);
        HelpOptionLine (NSRefVariation::ALIAS_VAR_LEN_ON_REF, NSRefVariation::OPTION_VAR_LEN_ON_REF, "value", NSRefVariation::USAGE_VAR_LEN_ON_REF);
        HelpOptionLine (NSRefVariation::ALIAS_THREADS, NSRefVariation::OPTION_THREADS, "value", NSRefVariation::USAGE_THREADS);
        HelpOptionLine (NSRefVariation::ALIAS_COVERAGE, NSRefVariation::OPTION_COVERAGE, "", NSRefVariation::USAGE_COVERAGE);
        HelpOptionLine (NSRefVariation::ALIAS_INPUT_FILE, NSRefVariation::OPTION_INPUT_FILE, "string", NSRefVariation::USAGE_INPUT_FILE);
        HelpOptionLine (NULL, NSRefVariation::OPTION_COUNT_STRAND, "value", NSRefVariation::USAGE_COUNT_STRAND);
        HelpOptionLine (NULL, NSRefVariation::OPTION_ALG, "value", NSRefVariation::USAGE_ALG);
        //HelpOptionLine (NSRefVariation::ALIAS_VERBOSITY, NSRefVariation::OPTION_VERBOSITY, "", NSRefVariation::USAGE_VERBOSITY);
#if SECRET_OPTION != 0
        HelpOptionLine (NULL, NSRefVariation::OPTION_SECRET, NULL, NSRefVariation::USAGE_SECRET);
#endif
        XMLLogger_Usage();

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
          NEW: -r NC_000013.10 -p 100635036 --query ACC -l 0 SRR793062 SRR795251

       windows example: -r NC_000002.11 -p 73613067 --query "-" -l 3 ..\..\..\tools\ref-variation\SRR618508.pileup

       -r NC_000002.11 -p 73613071 --query "C" -l 1
       -vv -t 16 -r NC_000007.13 -p 117292900 --query "-" -l 4          

       -vv -c -t 16 -r NC_000002.11 -p 73613067 --query "-" -l 3 /netmnt/traces04/sra33/SRZ/000867/SRR867061/SRR867061.pileup /netmnt/traces04/sra33/SRZ/000867/SRR867131/SRR867131.pileup
       -vv -c -t 16 -r NC_000002.11 -p 73613067 --query "-" -l 3 ..\..\..\tools\ref-variation\SRR867061.pileup ..\..\..\tools\ref-variation\SRR867131.pileup

       -vvv -c --count-strand counteraligned -t 16 -r NC_000002.11 -p 73613067 --query "-" -l 3 SRR867061 SRR867131

       old problem cases (didn't stop) - now OK:
       -v -c -r CM000671.1 -p 136131022 --query "T" -l 1 SRR1601768
       -v -c -r NC_000001.11 -p 136131022 --query "T" -l 1 SRR1601768

       NEW problebm - FIXED:
       -t 16 -v -c -r CM000671.1 -p 136131021 --query "T" -l 1 SRR1596639

       NEW problem:
       -vvv -r NC_000002.11 -p 73613030 --query "AT[1-3]" -l 3
       Inconsistent variations found

       NEW problem - FIXED (not completely): 
       -c -r CM000664.1 -p 234668879  -l 14 --query "ATATATATATATAT" SRR1597895 ok, non zero 30/33
       -c -r CM000664.1 -p 234668879  -l 14 --query "AT[1-8]" SRR1597895 - all counts 0 - FIXED
       was different total count because of SRR1597895.PA.26088404
       had wrong projected pos - fixed in ncbi-vdb
       UPDATE: now the sum of counts for AT[1-8] < for "ATATATATATATAT"

       NEW problem - FIXED
       -r NC_000001.10 -p 1064999 -l 1 --query A -v - hangs up

       NEW problem - FIXED
       -r NC_000020.10 -p 137534 -l 2 --query - -v

       NEW problem - FIXED
       -c -r CM000663.1 -p 123452 -l 7 --query "GGGAGAAAT" -vv -L info
       expanded variation has smaller window (6) but the variation returned is still of length 7

       */

        return NSRefVariation::find_variation_region ( argc, argv );
    }
}
