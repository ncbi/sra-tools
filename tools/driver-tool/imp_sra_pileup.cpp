/* ===========================================================================
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

#include "cmdline.hpp"
#include "support2.hpp"

namespace sratools2
{

struct SraPileupParams : OptionBase
{
    std::vector < ncbi::String > regions;
    ncbi::String outfile;
    ncbi::String table;
    bool bzip, gzip, timing, divide_by_spotgroups, depth_per_spotgroup, seqname, noqual;
    ncbi::U32 min_mapq_count;
    ncbi::U32 min_mapq_value;
    ncbi::U32 duplicates_count;
    ncbi::U32 duplicates_value;
    ncbi::U32 min_mismatch_count;
    ncbi::U32 min_mismatch_value;
    ncbi::U32 merge_dist_count;
    ncbi::U32 merge_dist_value;
    ncbi::String function;

    SraPileupParams()
        : bzip( false )
        , gzip( false )
        , timing( false )
        , divide_by_spotgroups( false )
        , depth_per_spotgroup( false )
        , seqname( false )
        , noqual( false )
        , min_mapq_count( 0 ), min_mapq_value( 0 )
        , duplicates_count( 0 ), duplicates_value( 0 )
        , min_mismatch_count( 0 ), min_mismatch_value( 0 )
        , merge_dist_count( 0 ), merge_dist_value( 0 )
    {
    }

    void add( ncbi::Cmdline &cmdline )
    {
        cmdline . addListOption( regions, ',', 255, "r", "aligned-region", "<name[:from-to]>",
            "Filter by position on genome. Name can either be file specific name (ex: \"chr1\" or "
            "\"1\"). \"from\" and \"to\" (inclusive) are 1-based coordinates" );

        cmdline . addOption ( outfile, nullptr, "o", "outfile", "<output-filen>",
            "Output will be written to this file instead of std-out" );
        cmdline . addOption ( table, nullptr, "t", "table", "<shortcut>",
            "Which alignment table(s) to use (p|s|e): primary, secondary, evidence-interval (default p)" );

        cmdline . addOption ( bzip, "", "bzip2", "Compress output using bzip2" );
        cmdline . addOption ( gzip, "", "gzip", "Compress output using gzip" );
        cmdline . addOption ( timing, "", "timing", "write timing log-file" );

        cmdline . addOption ( min_mapq_value, &min_mapq_count, "q", "minmapq", "<value>",
            "Minimum mapq-value, alignments with lower mapq will be ignored (default=0)" );
        cmdline . addOption ( duplicates_value, &duplicates_count, "d", "duplicates", "<0|1>",
            "process duplicates 0..off/1..on" );

        cmdline . addOption ( divide_by_spotgroups, "p", "spotgroups", "divide by spotgroups" );
        cmdline . addOption ( depth_per_spotgroup, "", "depth-per-spotgroup", "print depth per spotgroup" );
        cmdline . addOption ( seqname, "e", "seqname", "use original seq-name" );

        cmdline . addOption ( min_mismatch_value, &min_mismatch_count, "", "minmismatch", "<value>",
            "min percent of mismatches used in function mismatch, default is 5%" );

        cmdline . addOption ( merge_dist_value, &merge_dist_count, "", "merge-dist", "<value>",
            "If adjacent slices are closer than this, they are merged and skiplist is created. "
            "a value of zero disables the feature, default is 10000" );

        cmdline . addOption ( noqual, "n", "noqual", "omit qualities (faster)" );

        cmdline . addOption ( function, nullptr, "", "function", "<selector>",
            "ref = list references, ref-ex = list references with coverage, count = sort pileup with counters "
            "stat = strand/tlen statistic, mismatch = only lines with mismatches, index = list deletions counts "
            "varcount = variation counters ( columns: ref-name, ref-pos, ref-base, coverage, mismatch A "
            "mismatch C, mismatch G, mismatch T, deletes, inserts, ins after A, ins after C ins after G "
            "ins after T ) deletes = list deletions greater than 20, indels = list only inserts/deletions" );
    }

    std::string as_string()
    {
        std::stringstream ss;
        print_vec( ss, regions, "aligned-regions: " );
        if ( !outfile.isEmpty() ) ss << "outfile: " << outfile << std::endl;
        if ( !table.isEmpty() ) ss << "table: " << table << std::endl;
        if ( bzip ) ss << "bzip2" << std::endl;
        if ( gzip ) ss << "gzip" << std::endl;
        if ( timing ) ss << "timing" << std::endl;
        if ( min_mapq_count > 0 ) ss << "min-map-q: " << min_mapq_value << std::endl;
        if ( duplicates_count > 0 ) ss << "duplicates: " << duplicates_value << std::endl;
        if ( divide_by_spotgroups ) ss << "div by spotgroups" << std::endl;
        if ( depth_per_spotgroup ) ss << "depth per spotgroup" << std::endl;
        if ( seqname ) ss << "orig. seqname" << std::endl;
        if ( min_mismatch_count > 0 ) ss << "min. mismatch: " << min_mismatch_value << std::endl;
        if ( merge_dist_count > 0 ) ss << "merge-dist: " << merge_dist_value << std::endl;
        if ( noqual ) ss << "no qualities" << std::endl;
        if ( !function.isEmpty() ) ss << "function: " << function << std::endl;
        return ss.str();
    }

    void populate_argv_builder( ArgvBuilder & builder )
    {
        builder . add_option( "-r", regions );
        if ( !outfile.isEmpty() ) builder . add_option( "-o", outfile );
        if ( !table.isEmpty() ) builder . add_option( "-t", table );
        if ( bzip ) builder . add_option( "--bzip2" );
        if ( gzip ) builder . add_option( "--gzip" );
        if ( timing ) builder . add_option( "--timing" );
        if ( min_mapq_count > 0 ) builder . add_option( "-q", min_mapq_value );
        if ( duplicates_count > 0 ) builder . add_option( "-d", duplicates_value );
        if ( divide_by_spotgroups ) builder . add_option( "-p" );
        if ( depth_per_spotgroup ) builder . add_option( "--depth-per-spotgroup" );
        if ( seqname ) builder . add_option( "-e" );
        if ( min_mismatch_count > 0 ) builder . add_option( "--minmismatch", min_mismatch_value );
        if ( merge_dist_count > 0 ) builder . add_option( "--merge-dist", merge_dist_value );
        if ( noqual ) builder . add_option( "-n" );
        if ( !function.isEmpty() ) builder . add_option( "--function", function );
    }
};

int impersonate_sra_pileup( const Args &args )
{
    int res = 0;

    // SraPileupParams is a derived class of ToolOptions, defined in support2.hpp
    SraPileupParams params;

    Impersonator imp( args, "sra-pileup", params );
    res = imp . run();

    return res;
}

}