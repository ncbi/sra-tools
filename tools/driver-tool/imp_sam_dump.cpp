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

#define TOOL_NAME "sam-dump"

namespace sratools2
{

struct SamDumpParams final : CmnOptAndAccessions
{
    bool unaligned, primary, cigar_long, cigar_cg, header;
    ncbi::String header_file;
    bool no_header, seq_id, hide_identical, gzip, bzip, spot_group;
    bool fastq, fasta, reverse, cigar_cg_merge, xi;
    bool cg_evidence, cg_ev_dnb, cg_mappings, cg_sam, report;
    std::vector < ncbi::String > header_comments;
    std::vector < ncbi::String > aligned_regions;
    ncbi::String matepair_dist;
    ncbi::String prefix;
    ncbi::String qual_quant;
    ncbi::String output_file;
    ncbi::U32 out_buf_size_count;
    ncbi::U32 out_buf_size;
    bool cache_report, unaligned_only, cg_names;
    ncbi::U32 cursor_cache_count;
    ncbi::U32 cursor_cache_size;
    ncbi::U32 min_mapq_count;
    ncbi::U32 min_mapq;
    bool no_mate_cache, rna_splicing;
    ncbi::U32 rna_splice_level_count;
    ncbi::U32 rna_splice_level;
    ncbi::String rna_splice_log;
    bool md_flag;
    
    SamDumpParams(WhatImposter const &what)
    : CmnOptAndAccessions(what)
    , unaligned( false )
    , primary( false )
    , cigar_long( false )
    , cigar_cg( false )
    , header( false )
    , no_header( false )
    , seq_id( false )
    , hide_identical( false )
    , gzip( false )
    , bzip( false )
    , spot_group( false )
    , fastq( false )
    , fasta( false )
    , reverse( false )
    , cigar_cg_merge( false )
    , cg_evidence( false )
    , cg_ev_dnb( false )
    , cg_mappings( false )
    , cg_sam( false )
    , report( false )
    , out_buf_size_count( 0 )
    , out_buf_size( 0 )
    , cache_report( false )
    , unaligned_only( false )
    , cg_names( false )
    , cursor_cache_count( 0 )
    , cursor_cache_size( 0 )
    , min_mapq_count( 0 )
    , min_mapq( 0 )
    , no_mate_cache( false )
    , rna_splicing( false )
    , rna_splice_level_count( 0 )
    , rna_splice_level( 0 )
    , md_flag( false )
    {
    }

    void add( ncbi::Cmdline &cmdline ) override
    {
        cmdline . addOption ( unaligned, "u", "unaligned", "output unaligned reads along with aligned reads" );
        cmdline . addOption ( primary, "1", "primary", "output only primary alignments" );
        cmdline . addOption ( cigar_long, "c", "cigar-long", "output long version of CIGAR" );
        cmdline . addOption ( cigar_cg, "", "cigar-CG", "output CG version of CIGAR" );
        cmdline . addOption ( header, "r", "header", "always reconstruct header" );

        cmdline . addOption ( header_file, nullptr, "", "header-file", "<filename>", "take all headers from this file" );
        cmdline . addOption ( no_header, "n", "no-header", "do not output headers" );

        cmdline . addListOption( header_comments, ',', 255, "", "header-comment", "<text>",
            "add comment to header. Use multiple times for several lines. Use quotes" );

        cmdline . addListOption( aligned_regions, ',', 255, "", "aligned-region", "<name[:from-to]>",
            "Filter by position on genome. Name can either be file specific name (ex: \"chr1\" or "
            "\"1\"). \"from\" and \"to\" (inclusive) are 1-based coordinates" );

        cmdline . addOption ( matepair_dist, nullptr, "", "matepair-distance", "<from-to|'unknown'>",
            "Filter by distance between matepairs. Use \"unknown\" to find matepairs split between "
            "the references. Use from-to (inclusive) to limit matepair distance on the same reference" );

        cmdline . addOption ( seq_id, "s", "seqid", "Print reference SEQ_ID in RNAME instead of NAME" );
        cmdline . addOption ( hide_identical, "=", "hide-identical", "Output '=' if base is identical to reference" );
        cmdline . addOption ( gzip, "", "gzip", "Compress output using gzip" );
        cmdline . addOption ( bzip, "", "bzip2", "Compress output using bzip2" );
        cmdline . addOption ( spot_group, "g", "spot-group", "Add .SPOT_GROUP to QNAME" );
        cmdline . addOption ( fastq, "", "fastq", "Produce FastQ formatted output" );
        cmdline . addOption ( fasta, "", "fasta", "Produce Fasta formatted output" );

        cmdline . addOption ( prefix, nullptr, "p", "prefix", "<prefix>", "Prefix QNAME: prefix.QNAME" );

        cmdline . addOption ( reverse, "", "reverse", "Reverse unaligned reads according to read type" );
        cmdline . addOption ( cigar_cg_merge, "", "cigar-cg-merge",
            "Apply CG fixups to CIGAR/SEQ/QUAL and outputs CG-specific columns" );
        cmdline . addOption ( xi, "", "XI", "Output cSRA alignment id in XI column" );

        cmdline . addOption ( qual_quant, nullptr, "Q", "qual-quant", "<quantization string>",
            "Quality scores quantization level string like '1:10,10:20,20:30,30:-'" );

        cmdline . addOption ( cg_evidence, "", "CG-evidence", "Output CG evidence aligned to reference" );
        cmdline . addOption ( cg_ev_dnb, "", "CG_ev-dnb", "Output CG evidence DNB's aligned to evidence" );
        cmdline . addOption ( cg_mappings, "", "CG-mappings", "Output CG sequences aligned to reference" );
        cmdline . addOption ( cg_sam, "", "CG-SAM", "Output CG evidence DNB's aligned to reference" );
        cmdline . addOption ( report, "", "report", "report options instead of executing" );

        cmdline . addOption ( output_file, nullptr, "", "output-file", "<filename>",
            "print output into this file (instead of STDOUT)" );

        cmdline . addOption ( out_buf_size, &out_buf_size_count, "", "output-buffer-size", "<size>",
            "size of output-buffer (dflt:32k, 0...off)" );

        cmdline . addOption ( cache_report, "", "cachereport", "print report about mate-pair-cache" );
        cmdline . addOption ( unaligned_only, "", "unaligned-spots-only ", "output reads for spots with no aligned reads" );
        cmdline . addOption ( cg_names, "", "CG-names", "prints cg-style spotgroup.spotid formed names" );

        cmdline . addOption ( cursor_cache_size, &cursor_cache_count, "", "cursor-cache", "<size>",
            "open cached cursor with this size" );
        cmdline . addOption ( min_mapq, &min_mapq_count, "", "min-mapq", "<mapq>",
            "min. mapq an alignment has to have, to be printed" );

        cmdline . addOption ( no_mate_cache, "", "no-mate-cache",
            "do not use mate-cache, slower but less memory usage" );

        cmdline . addOption ( rna_splicing, "", "rna-splicing",
            "modify cigar-string (replace .D. with .N.) and add output flags (XS:A:+/-) when "
            "rna-splicing is detected by match to spliceosome recognition sites" );
        cmdline . addOption ( rna_splice_level, &rna_splice_level_count, "", "rna-splice-level", "<level>",
            "level of rna-splicing detection (0,1,2) when testing for spliceosome recognition sites "
            "0=perfect match, 1=one mismatch, 2=two mismatches, one on each site" );
        cmdline . addOption ( rna_splice_log, nullptr, "", "rna-splice-log", "<filename>",
            "file, into which rna-splice events are written" );

        cmdline . addOption ( md_flag, "", "with-md-flag", "print MD-flag" );

        CmnOptAndAccessions::add(cmdline);
    }

    std::ostream &show(std::ostream &ss) const override
    {
        if ( unaligned ) ss << "unaligned" << std::endl;
        if ( primary ) ss << "primary" << std::endl;
        if ( cigar_long ) ss << "cigar-long" << std::endl;
        if ( cigar_cg ) ss << "cigar-cg" << std::endl;
        if ( header ) ss << "header" << std::endl;
        if ( !header_file.isEmpty() ) ss << "header-file: " << header_file << std::endl;
        if ( no_header ) ss << "no-header" << std::endl;
        if ( seq_id ) ss << "seq-id" << std::endl;
        if ( hide_identical ) ss << "hide-identical" << std::endl;
        if ( gzip ) ss << "gzip" << std::endl;
        if ( bzip ) ss << "bzip2" << std::endl;
        if ( spot_group ) ss << "spot-group" << std::endl;
        if ( fastq ) ss << "fastq" << std::endl;
        if ( fasta ) ss << "fasta" << std::endl;
        if ( reverse ) ss << "reverse" << std::endl;
        if ( cigar_cg_merge ) ss << "cigar-cg-merge" << std::endl;
        if ( xi ) ss << "XI" << std::endl;
        if ( cg_evidence ) ss << "CG-evidence" << std::endl;
        if ( cg_ev_dnb ) ss << "CG-EV-dnb" << std::endl;
        if ( cg_mappings ) ss << "CG-mappings" << std::endl;
        if ( cg_sam ) ss << "CG-sam" << std::endl;
        if ( report ) ss << "report" << std::endl;
        print_vec( ss, header_comments, "header-comments" );
        print_vec( ss, aligned_regions, "aligned-regions" );
        if ( !matepair_dist.isEmpty() ) ss << "matepair-dist: " << matepair_dist << std::endl;
        if ( !prefix.isEmpty() ) ss << "prefix: " << prefix << std::endl;
        if ( !qual_quant.isEmpty() ) ss << "qual-quant: " << qual_quant << std::endl;
        if ( !output_file.isEmpty() ) ss << "output-file: " << output_file << std::endl;
        if ( out_buf_size_count > 0 ) ss << "out-buf-size: " << out_buf_size << std::endl;
        if ( cache_report ) ss << "cache-report" << std::endl;
        if ( unaligned_only ) ss << "unaligned_only" << std::endl;
        if ( cg_names ) ss << "cg-names" << std::endl;
        if ( cursor_cache_count > 0 ) ss << "cursor-cache: " << cursor_cache_size << std::endl;
        if ( min_mapq_count > 0 ) ss << "min-mapq: " << min_mapq << std::endl;
        if ( no_mate_cache ) ss << "no-mate-cache" << std::endl;
        if ( rna_splicing ) ss << "rna-splicing" << std::endl;
        if ( rna_splice_level_count > 0 ) ss << "rna-splice-level: " << rna_splice_level << std::endl;
        if ( !rna_splice_log.isEmpty() ) ss << "rna-splice-log: " << rna_splice_log << std::endl;
        if ( md_flag ) ss << "md-flag" << std::endl;
        return CmnOptAndAccessions::show(ss);
    }

    void populate_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions ) const override
    {
        CmnOptAndAccessions::populate_argv_builder(builder, acc_index, accessions);

        if ( unaligned ) builder . add_option( "-u" );
        if ( primary ) builder . add_option( "-1" );
        if ( cigar_long ) builder . add_option( "-c" );
        if ( cigar_cg ) builder . add_option( "--cigar-CG" );
        if ( header ) builder . add_option( "-r" );
        if ( !header_file.isEmpty() ) builder . add_option( "--header_file", header_file );
        if ( no_header ) builder . add_option( "-n" );
        if ( seq_id ) builder . add_option( "-s" );
        if ( hide_identical ) builder . add_option( "-=" );
        if ( gzip ) builder . add_option( "--gzip" );
        if ( bzip ) builder . add_option( "--bzip2" );
        if ( spot_group ) builder . add_option( "-g" );
        if ( fastq ) builder . add_option( "--fastq" );
        if ( fasta ) builder . add_option( "--fasta" );
        if ( reverse ) builder . add_option( "--reverse" );
        if ( cigar_cg_merge ) builder . add_option( "--cigar-CG-merge" );
        if ( xi ) builder . add_option( "-XI" );
        if ( cg_evidence ) builder . add_option( "--CG-evidence" );
        if ( cg_ev_dnb ) builder . add_option( "--CG-ev-dnb" );
        if ( cg_mappings ) builder . add_option( "--CG-mappings" );
        if ( cg_sam ) builder . add_option( "--CG-SAM" );
        if ( report ) builder . add_option( "--report" );
        builder . add_option( "--header-comment", header_comments );
        builder . add_option( "--aligned-region", aligned_regions );
        if ( !matepair_dist.isEmpty() ) builder . add_option( "--matepair-distance", matepair_dist );
        if ( !prefix.isEmpty() ) builder . add_option( "--prefix", prefix );
        if ( !qual_quant.isEmpty() ) builder . add_option( "-Q", qual_quant );
        if ( !output_file.isEmpty() ) {
            if (accessions.size() > 1 && !(fasta || fastq)) {
                if (acc_index == 0)
                    print_unsafe_output_file_message("sam-dump", ".sam", accessions);

                builder . add_option( "--output-file", accessions[acc_index] + ".sam" );
            }
            else
                builder . add_option( "--output-file", output_file );
        }
        if ( out_buf_size_count > 0 ) builder . add_option( "--output-buffer-size", out_buf_size );
        if ( cache_report ) builder . add_option( "--cachereport" );
        if ( unaligned_only ) builder . add_option( "--unaligned-spots-only" );
        if ( cg_names ) builder . add_option( "--CG-names" );
        if ( cursor_cache_count > 0 ) builder . add_option( "--cursor-cache", cursor_cache_size );
        if ( min_mapq_count > 0 ) builder . add_option( "--min-mapq", min_mapq );
        if ( no_mate_cache ) builder . add_option( "--no-mate-cache" );
        if ( rna_splicing ) builder . add_option( "--rna-splicing" );
        if ( rna_splice_level_count > 0 ) builder . add_option( "--rna-splice-level", rna_splice_level );
        if ( !rna_splice_log.isEmpty() ) builder . add_option( "--rna-splice-log", rna_splice_log );
        if ( md_flag ) builder . add_option( "--with-md-flag" );
    }

    bool check() const override
    {
        int problems = 0;
        if ( bzip && gzip )
        {
            std::cerr << "bzip2 and gzip cannot both be used at the same time" << std::endl;
            problems++;
        }
        if ( rna_splice_level_count > 0 && rna_splice_level > 2 )
        {
            std::cerr << "invalid ran-splice-level: " << rna_splice_level << std::endl;
            problems++;
        }
        if (!cart_file.isEmpty()) {
            std::cerr << "unimplemented parameter: --cart is not yet implemented for " << what._basename << std::endl;
            problems++;
        }
        if (fasta && fastq)
        {
            std::cerr << "fasta and fastq cannot both be used at the same time" << std::endl;
            problems++;
        }

        return CmnOptAndAccessions::check() && ( problems == 0 );
    }

    int run() const override {
        return ToolExec::run(what.toolpath().c_str(), what._basename.c_str(), *this, accessions);
    }
};

int impersonate_sam_dump( const Args &args, WhatImposter const &what )
{
    SamDumpParams params(what);
    return Impersonator::run(args, params);
}

}
