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

#include <sstream>

#include "cmdline.hpp"
#include "support2.hpp"

#define TOOL_NAME "fastq-dump"

namespace sratools2
{

struct FastqParams final : CmnOptAndAccessions
{
    ncbi::String accession_replacement;
    ncbi::String table_name;
    ncbi::String read_filter;
    ncbi::String aligned_region;
    ncbi::String matepair_dist;
    ncbi::String outdir;
    ncbi::String dumpcs;
    ncbi::String defline_seq;
    ncbi::String defline_qual;
    ncbi::String fasta;
    std::vector < ncbi::String > spot_groups;
    bool split_spot, clip, qual_filter, qual_filter1;
    bool aligned, unaligned, skip_tech, to_stdout, gzip, bzip;
    bool split_files, split3, spot_group, group_in_dirs;
    bool keep_empty_files, dumpbase, no_q_for_cskey;
    bool origfmt, readids, helicos;
    ncbi::U64 minSpotId;
    ncbi::U32 minSpotIdCount;
    ncbi::U64 maxSpotId;
    ncbi::U32 maxSpotIdCount;
    ncbi::U32 minReadLen;
    ncbi::U32 minReadLenCount;
    ncbi::U32 ReadFilterCount;
    ncbi::U32 DumpcsCount;
    ncbi::U32 QOffsetCount;
    ncbi::U32 QOffset;

    explicit FastqParams(WhatImposter const &what)
    : CmnOptAndAccessions(what)
    , accession_replacement( "" )
    , split_spot( false )
    , clip( false )
    , qual_filter( false )
    , qual_filter1( false )
    , aligned( false )
    , unaligned( false )
    , skip_tech( false )
    , to_stdout( false )
    , gzip( false )
    , bzip( false )
    , split_files( false )
    , split3( false )
    , spot_group( false )
    , group_in_dirs( false )
    , keep_empty_files( false )
    , dumpbase( false )
    , no_q_for_cskey( false )
    , origfmt( false )
    , readids( false )
    , helicos( false )
    , minSpotId( 0 )
    , minSpotIdCount( 0 )
    , maxSpotId( 0 )
    , maxSpotIdCount( 0 )
    , minReadLen( 0 )
    , minReadLenCount( 0 )
    , ReadFilterCount( 0 )
    , DumpcsCount( 0 )
    , QOffsetCount( 0 )
    , QOffset( 0 )
    {}

    void add( ncbi::Cmdline &cmdline ) override
    {
        cmdline . addOption ( accession_replacement, nullptr, "A", "accession", "<accession>",
            "Replaces accession derived from <path> in filename(s) and deflines (only for single table dump)" );

        cmdline . addOption ( table_name, nullptr, "", "table", "<table-name>",
            "Table name within cSRA object, default is \"SEQUENCE\"" );

        cmdline . addOption ( split_spot, "", "split-spot", "Split spots into individual reads" );

        cmdline . addOption ( minSpotId, &minSpotIdCount, "N", "minSpotId", "<rowid>", "Minimum spot id" );
        cmdline . addOption ( maxSpotId, &maxSpotIdCount, "X", "maxSpotId", "<rowid>", "Maximum spot id" );

        cmdline . addListOption( spot_groups, ',', 255, "", "spot-groups", "<[list]>",
            "Filter by SPOT_GROUP (member): name[,...]" );

        cmdline . addOption ( clip, "W", "clip", "Remove adapter sequences from reads" );

        cmdline . addOption ( minReadLen, &minReadLenCount, "M", "minReadLen", "<len>",
            "Filter by sequence length >= <len>" );

        cmdline . addOption ( read_filter, &ReadFilterCount, "R", "read-filter", "<filter>",
            "Split into files by READ_FILTER value [split], optionally filter by value: [pass|reject|criteria|redacted]" );

        cmdline . addOption ( qual_filter, "E", "qual-filter",
            "Filter used in early 1000 Genomes data: no sequences starting or ending with >= 10N" );

        cmdline . addOption ( qual_filter1, "", "qual-filter-1",
            "Filter used in current 1000 Genomes data" );

        cmdline . addOption ( aligned, "", "aligned", "Dump only aligned sequences" );
        cmdline . addOption ( unaligned, "", "unaligned", "Dump only unaligned sequences" );

        cmdline . addOption ( aligned_region, nullptr, "", "aligned-region", "<name[:from-to]>",
            "Filter by position on genome. Name can eiter by accession.version (ex: NC_000001.10) "
            "or file specific name (ex: \"chr1\" or \"1\". \"from\" and \"to\" are 1-based coordinates" );

        cmdline . addOption ( matepair_dist, nullptr, "", "matepair_distance", "<from-to|unknown>",
            "Filter by distance between matepairs. Use \"unknown\" to find matepairs split between "
            "the references. Use from-to to limit matepair distance on the same reference" );

        cmdline . addOption ( skip_tech, "", "skip-technical", "Dump only biological reads" );

        cmdline . addOption ( outdir, nullptr, "O", "outdir", "<path>",
            "Output directory, default is working directory '.'" );

        cmdline . addOption ( to_stdout, "Z", "stdout", "Output to stdout, "
            "all split data become joined into single stream" );

        cmdline . addOption ( gzip, "", "gzip", "Compress output using gzip: deprecated, not recommended" );
        cmdline . addOption ( bzip, "", "bzip2", "Compress output using bzip2: deprecated, not recommended" );

        cmdline . addOption ( split_files, "", "split-files",
            "Write reads into separate files. Read number will be suffixed to the file name. "
            "NOTE! The `--split-3` option is recommended. In cases where not all spots have the same "
            "number of reads, this option will produce files that WILL CAUSE ERRORS in most programs "
            "which process split pair fastq files." );

        cmdline . addOption ( split3, "", "split-e",
            "3-way splitting for mate-pairs. For each spot, if there are two biological reads "
            "satisfying filter conditions, the first is placed in the `*_1.fastq` file, and the "
            "second is placed in the `*_2.fastq` file. If there is only one biological read "
            "satisfying the filter conditions, it is placed in the `*.fastq` file.All other "
            "reads in the spot are ignored." );

        cmdline . addOption ( spot_group, "G", "spot-group", "Split into files by SPOT_GROUP (member name)" );
        cmdline . addOption ( group_in_dirs, "T", "group-in-dirs", "Split into subdirectories instead of files" );
        cmdline . addOption ( keep_empty_files, "K", "keep-empty-files", "Do not delete empty files" );

        cmdline . addOption ( dumpcs, &DumpcsCount, "C", "dumpcs", "<cskey>",
            "Formats sequence using color space (default for SOLiD), \"cskey\" may be specified for translation"
            " or else specify \"dflt\" to use the default value" );

        cmdline . addOption ( dumpbase, "B", "dumpbase",
            "Formats sequence using base space (default for other than SOLiD)." );

        cmdline . addOption ( QOffset, &QOffsetCount, "Q", "offset", "<integer",
            "Offset to use for quality conversion, default is 33" );

        // !!! double duty option: mandatory value introduced, "default" ---> "--fasta"
        // other values ---> "--fasta value"
        cmdline . addOption ( fasta, nullptr, "", "fasta", "<line-width>",
            "FASTA only, no qualities, with can be \"default\" or \"0\" for no wrapping" );

        cmdline . addOption ( no_q_for_cskey, "", "suppress-qual-for-cskey", "suppress quality-value for cskey" );
        cmdline . addOption ( origfmt, "F", "origfmt", "Defline contains only original sequence name" );
        cmdline . addOption ( readids, "I",
            "readids", "Append read id after spot id as 'accession.spot.readid' on defline" );
        cmdline . addOption ( helicos, "", "helicos", "Helicos style defline" );

        cmdline . addOption ( defline_seq, nullptr, "", "defline-seq", "<fmt>",
            "Defline format specification for sequence." );

        cmdline . addOption ( defline_qual, nullptr, "", "defline-qual", "<fmt>",
            "Defline format specification for quality. <fmt> is string of characters and/or "
            "variables. The variables can be one of: $ac - accession, $si spot id, $sn spot "
            "name, $sg spot group (barcode), $sl spot length in bases, $ri read number, $rn "
            "read name, $rl read length in bases. '[]' could be used for an optional output: if "
            "all vars in [] yield empty values whole group is not printed. Empty value is empty "
            "string or for numeric variables. Ex: @$sn[_$rn]/$ri '_$rn' is omitted if name is empty" );

        CmnOptAndAccessions::add(cmdline);
    }

    std::ostream &show(std::ostream &ss) const override
    {
        if ( !accession_replacement.isEmpty() )  ss << "acc-replace : " << accession_replacement << std::endl;
        if ( !table_name.isEmpty() )  ss << "table-name : " << table_name << std::endl;
        if ( split_spot ) ss << "split-spot" << std::endl;
        if ( minSpotIdCount > 0 ) ss << "minSpotId : " << minSpotId << std::endl;
        if ( maxSpotIdCount > 0 ) ss << "maxSpotId : " << maxSpotId << std::endl;
        print_vec( ss, spot_groups, "spot-groups : " );
        if ( clip ) ss << "clip" << std::endl;
        if ( minReadLenCount > 0 ) ss << "minReadLen : " << minReadLen << std::endl;
        if ( ReadFilterCount > 0 )  ss << "read-filter : '" << read_filter << "'" << std::endl;
        if ( qual_filter ) ss << "qual-filter" << std::endl;
        if ( qual_filter1 ) ss << "qual-filter-1" << std::endl;
        if ( aligned ) ss << "aligned" << std::endl;
        if ( unaligned ) ss << "unaligned" << std::endl;
        if ( !aligned_region.isEmpty() )  ss << "aligned-region : " << aligned_region << std::endl;
        if ( !matepair_dist.isEmpty() )  ss << "matepair-dist : " << matepair_dist << std::endl;
        if ( skip_tech ) ss << "skip-tech" << std::endl;
        if ( !outdir.isEmpty() )  ss << "outdir : " << outdir << std::endl;
        if ( to_stdout ) ss << "stdout" << std::endl;
        if ( gzip ) ss << "gzip" << std::endl;
        if ( bzip ) ss << "bzip2" << std::endl;
        if ( split_files ) ss << "split-files" << std::endl;
        if ( split3 ) ss << "split-3" << std::endl;
        if ( spot_group ) ss << "splot-gropu" << std::endl;
        if ( group_in_dirs ) ss << "group-in-dirs" << std::endl;
        if ( keep_empty_files ) ss << "keep-empty-files" << std::endl;
        if ( DumpcsCount > 0 )  ss << "dumpcs : '" << dumpcs << "'" << std::endl;
        if ( dumpbase ) ss << "dumpbase" << std::endl;
        if ( QOffsetCount > 0 ) ss << "offset : " << QOffset << std::endl;
        if ( !fasta.isEmpty() ) ss << "fasta : " << fasta << std::endl;
        if ( no_q_for_cskey ) ss << "suppress-qual-for-cskey" << std::endl;
        if ( origfmt ) ss << "origfmt" << std::endl;
        if ( readids ) ss << "readids" << std::endl;
        if ( helicos ) ss << "helicos" << std::endl;
        if ( !defline_seq.isEmpty() ) ss << "defline-seq: '" << defline_seq << "'" << std::endl;
        if ( !defline_qual.isEmpty() ) ss << "defline-qual: '" << defline_qual << "'" << std::endl;
        return CmnOptAndAccessions::show(ss);
    }

    void populate_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions ) const override
    {
        CmnOptAndAccessions::populate_argv_builder(builder, acc_index, accessions);

        if ( !accession_replacement.isEmpty() ) builder . add_option( "-A", accession_replacement );
        if ( !table_name.isEmpty() ) builder . add_option( "--table", table_name );
        if ( split_spot ) builder . add_option( "--split-spot" );
        if ( minSpotIdCount > 0 ) builder . add_option( "-N", minSpotId );
        if ( maxSpotIdCount > 0 ) builder . add_option( "-X", maxSpotId );
        if ( spot_groups.size() > 0 ) {
            auto list = spot_groups[0].toSTLString();
            int i = 0;
            for (auto & value : spot_groups) {
                if (i++ > 0) {
                    list += ',' + value.toSTLString();
                }
            }
            builder.add_option("--spot-groups", list);
        }
        if ( clip ) builder . add_option( "-W" );
        if ( minReadLenCount > 0 ) builder . add_option( "-M", minReadLen );

        // problem-child: !!! read-filter has dual form: with and without value !!!
        if ( ReadFilterCount > 0 )
        {
            if ( read_filter == "split" )
                builder . add_option( "-R" );
            else
                builder . add_option( "-R", read_filter );
        }

        if ( qual_filter ) builder . add_option( "-E" );
        if ( qual_filter1 ) builder . add_option( "--qual-filter-1" );
        if ( aligned ) builder . add_option( "--aligned" );
        if ( unaligned ) builder . add_option( "--unaligned" );
        if ( !aligned_region.isEmpty() ) builder . add_option( "--aligned-region", aligned_region );
        if ( !matepair_dist.isEmpty() ) builder . add_option( "--matepair-distance", matepair_dist );
        if ( skip_tech ) builder . add_option( "--skip-technical" );
        if ( !outdir.isEmpty() ) builder . add_option( "-O", outdir );
        if ( to_stdout ) builder . add_option( "-Z" );
        if ( gzip ) builder . add_option( "--gzip" );
        if ( bzip ) builder . add_option( "--bizp2" );
        if ( split_files ) builder . add_option( "--split-files" );
        if ( split3 ) builder . add_option( "--split-3" );
        if ( spot_group ) builder . add_option( "--spot-group" );
        if ( group_in_dirs ) builder . add_option( "-T" );
        if ( keep_empty_files ) builder . add_option( "-K" );

        // problem-child: !!! dumpcs has dual form: with and without value !!!
        if ( DumpcsCount > 0 )
        {
            if ( dumpcs == "default" )
                builder . add_option( "-C" );
            else
                builder . add_option( "-C", dumpcs );
        }

        if ( dumpbase ) builder . add_option( "-B" );
        if ( QOffsetCount > 0 ) builder . add_option( "-Q", QOffset );

        // problem-child: !!! fasta has dual form: with and without value !!!
        // we have ommited the possibility to specify the line-width optionally
        if ( !fasta.isEmpty() )
        {
            if ( fasta.equal( "default" ) )
                builder . add_option( "--fasta" );
            else
                builder . add_option( "--fasta", fasta );
        }

        if ( no_q_for_cskey ) builder . add_option( "--suppress-qual-for-cskey" );
        if ( origfmt ) builder . add_option( "-F" );
        if ( readids ) builder . add_option( "-I" );
        if ( helicos ) builder . add_option( "--helicos" );
        if ( !defline_seq.isEmpty() ) builder . add_option( "--defline-seq", defline_seq );
        if ( !defline_qual.isEmpty() ) builder . add_option( "--defline-qual", defline_qual );
    }

    bool check() const override
    {
        int problems = 0;
        if ( bzip && gzip )
        {
            std::cerr << "bzip2 and gzip cannot both be used at the same time" << std::endl;
            problems++;
        }
        if ( !read_filter.isEmpty() )
        {
            if ( !is_one_of( read_filter, 5, "split", "pass", "reject", "criterial", "redacted" ) )
            {
                std::cerr << "invalid read-filter-value: " << read_filter << std::endl;
                problems++;
            }

        }

        return CmnOptAndAccessions::check() && ( problems == 0 );
    }

    int run() const override {
        auto const theirArgv0 = what.toolpath.getPathFor(TOOL_NAME).fullpath();
        {
            auto const realpath = what.toolpath.getPathFor(TOOL_NAME "-orig");
            if (realpath.executable())
                return ToolExec::run(TOOL_NAME, realpath.fullpath(), theirArgv0, *this, accessions);
        }
#if DEBUG || _DEBUGGING
        {
            auto const realpath = what.toolpath.getPathFor(TOOL_NAME);
            if (realpath.executable())
                return ToolExec::run(TOOL_NAME, realpath.fullpath(), theirArgv0, *this, accessions);
        }
#endif
        throw std::runtime_error(TOOL_NAME " was not found or is not executable.");
    }

};

int impersonate_fastq_dump( const Args &args, WhatImposter const &what )
{
#if DEBUG || _DEBUGGING
    FastqParams temp(what);
    auto &params = *randomized(&temp, what);
#else
    FastqParams params(what);
#endif
    return Impersonator::run(args, params);
}

} // namespace
