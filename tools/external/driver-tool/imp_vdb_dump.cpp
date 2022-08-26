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

#define TOOL_NAME "vdb-dump"
#define TOOL_ORIGINAL_NAME TOOL_NAME "-orig"

namespace sratools2
{

struct VdbDumpParams final : CmnOptAndAccessions
{
    bool
          row_id_on
        , colname_off
        , in_hex
        , schema_dump
        , table_enum
        , column_enum
        , column_enum_short
        , id_range
        , without_sra
        , obj_vers
        , obj_timestamp
        , obj_type
        , numelem
        , numelemsum
        , blobbing
        , phys
        , readable
        , idx_report
        , gzip
        , bzip
        , info
        , spotgroups
        , merge_ranges
        , spread
        , append;
    ncbi::U32
          LineFeedCount
        , LineFeedValue
        , MaxLenCount
        , MaxLenValue
        , IndentCount
        , IndentValue;
    ncbi::String table_name;
    ncbi::String rows;
    ncbi::String columns;
    ncbi::String schema;
    ncbi::String dna_bases;
    ncbi::String format;
    ncbi::String excluded;
    ncbi::String bool_print;
    ncbi::String idx_range;
    ncbi::String cur_cache;
    ncbi::String output_file;
    ncbi::String output_path;
    ncbi::String output_buffer_size;
    
    explicit VdbDumpParams( WhatImposter const &what )
    : CmnOptAndAccessions( what )
    , row_id_on(false)
    , colname_off(false)
    , in_hex(false)
    , schema_dump(false)
    , table_enum(false)
    , column_enum(false)
    , column_enum_short(false)
    , id_range(false)
    , without_sra(false)
    , obj_vers(false)
    , obj_timestamp(false)
    , obj_type(false)
    , numelem(false)
    , numelemsum(false)
    , blobbing(false)
    , phys(false)
    , readable(false)
    , idx_report(false)
    , gzip(false)
    , bzip(false)
    , info(false)
    , spotgroups(false)
    , merge_ranges(false)
    , spread(false)
    , append(false)
    , LineFeedCount(0)
    , LineFeedValue(0)
    , MaxLenCount(0)
    , MaxLenValue(0)
    , IndentCount(0)
    , IndentValue(0)
    {
    }

    void add( ncbi::Cmdline &cmdline ) override
    {
        cmdline . addOption ( row_id_on, "I", "row_id_on", "print row id" );
        cmdline . addOption ( colname_off, "N", "colname_off", "do not print column-names" );
        cmdline . addOption ( in_hex, "X", "in_hex", "print numbers in hex" );
        cmdline . addOption ( schema_dump, "A", "schema_dump", "prints the schema" );
        cmdline . addOption ( table_enum, "E", "table_enum", "enumerate tables" );
        cmdline . addOption ( column_enum, "O", "column_enum", "enumerate columns in extended form" );
        cmdline . addOption ( column_enum_short, "o", "column_enum_short", "enumerate columns in short form" );
        cmdline . addOption ( id_range, "r", "id_range", "prints id-range" );
        cmdline . addOption ( without_sra, "n", "without_sra", "without sra-translation" );
        cmdline . addOption ( obj_vers, "j", "obj_version", "request vdb-version" );
        cmdline . addOption ( obj_timestamp, "", "obj_timestamp", "request object modification date" );
        cmdline . addOption ( obj_type, "y", "obj_type", "report type of object" );
        cmdline . addOption ( numelem, "u", "numelem", "print only element-count" );
        cmdline . addOption ( numelemsum, "U", "numelemsum", "sum element-count" );
        cmdline . addOption ( blobbing, "", "blobbing", "show blobbing" );
        cmdline . addOption ( phys, "", "phys", "enumeate physical columns" );
        cmdline . addOption ( readable, "", "readable", "enumeate readable columns" );
        cmdline . addOption ( idx_report, "", "idx-report", "enumeate all available index" );
        cmdline . addOption ( gzip, "", "gzip", "compress output using gzip" );
        cmdline . addOption ( bzip, "", "bzip2", "compress output using bzip2" );
        cmdline . addOption ( info, "", "info", "print info about accession" );
        cmdline . addOption ( spotgroups, "", "spotgroups", "show spotgroups" );
        cmdline . addOption ( merge_ranges, "", "merge-ranges", "merge and sort row-ranges" );
        cmdline . addOption ( spread, "", "spread", "show spread of integer values" );
        cmdline . addOption ( append, "a", "append", "append to output-file, if output-file is used" );

        cmdline . addOption ( LineFeedValue, &LineFeedCount, "l", "line_feed", "<count>",
            "number of line-feed's inbetween rows" );
        cmdline . addOption ( MaxLenValue, &MaxLenCount, "M", "max_length", "<length>",
            "limits line length" );
        cmdline . addOption ( IndentValue, &IndentCount, "i", "indent_with", "<width>",
            "indents the line" );

        cmdline . addOption ( table_name, nullptr, "T", "table", "<name>", "name of table to use" );
        cmdline . addOption ( rows, nullptr, "R", "rows", "<rows>", "rows (default:all)" );
        cmdline . addOption ( columns, nullptr, "C", "columns", "<columns>", "columns (default:all)" );
        cmdline . addOption ( schema, nullptr, "S", "schema", "<path>", "path to schema-file" );
        cmdline . addOption ( dna_bases, nullptr, "D", "dna_bases", "<bases>", "print dna-bases" );
        cmdline . addOption ( format, nullptr, "f", "format", "<format>",
            "output-format: csv ... comma-separated values on one line, xml ... xml-style without complete "
            "xml-frame, json ... json-style, piped ... 1 line per cell ( row-id, column-name: value ), "
            "tab ... 1 line per row ( tab-separated values only ), fastq ... FASTQ( 4 lines ) for each row, "
            "fastq1 ... FASTQ( 4 lines ) for each fragment, fasta ... FASTA ( 2 lines ) for each fragment "
            "if possible, fasta1 ... one FASTA-record for the whole accession (REFSEQ), fasta2 ... one "
            "FASTA-record for each REFERENCE in cSRA, qual ... QUAL( 2 lines ) for each row, qual1 ... QUAL "
            "( 2 lines ) for each fragment if possible"
            );
        cmdline . addOption ( excluded, nullptr, "x", "exclude", "<columns>", "exclude these columns" );
        cmdline . addOption ( bool_print, nullptr, "b", "boolean", "<mode>",
            "defines how boolean's are printed, valid value: [1|T]" );
        cmdline . addOption ( idx_range, nullptr, "", "idx-range", "<idx-name>",
            "enumerate values and row-ranges of one index" );
        cmdline . addOption ( cur_cache, nullptr, "", "cur-cache", "<size>", "size of cursor cache" );
        cmdline . addOption ( output_file, nullptr, "", "output-file", "<filename>",
            "write output to this file" );
        cmdline . addOption ( output_path, nullptr, "", "output-path", "<path>",
            "write output to this directory" );
        cmdline . addOption ( output_buffer_size, nullptr, "", "output-buffer-size", "<size>",
            "size of output-buffer, 0...none" );

        CmnOptAndAccessions::add( cmdline );
    }

    std::ostream &show( std::ostream &ss ) const override
    {
        if ( row_id_on ) ss << "row-id-on" << std::endl;
        if ( colname_off ) ss << "colname-off" << std::endl;
        if ( in_hex ) ss << "in-hex" << std::endl;
        if ( schema_dump ) ss << "schema-dump" << std::endl;
        if ( table_enum ) ss << "table-enum" << std::endl;
        if ( column_enum ) ss << "column-enum" << std::endl;
        if ( column_enum_short ) ss << "column-enum-short" << std::endl;
        if ( id_range ) ss << "id-range" << std::endl;
        if ( without_sra ) ss << "without-sra" << std::endl;
        if ( obj_vers ) ss << "print vdb-vers" << std::endl;
        if ( obj_timestamp ) ss << "print object timestamp" << std::endl;
        if ( obj_type ) ss << "print object type" << std::endl;
        if ( numelem ) ss << "numelem" << std::endl;
        if ( numelemsum ) ss << "numelemsum" << std::endl;
        if ( blobbing ) ss << "blobbing" << std::endl;
        if ( phys ) ss << "phys" << std::endl;
        if ( readable ) ss << "readable" << std::endl;
        if ( idx_report ) ss << "idx-report" << std::endl;
        if ( gzip ) ss << "gzip" << std::endl;
        if ( bzip ) ss << "bzip" << std::endl;
        if ( info ) ss << "info" << std::endl;
        if ( spotgroups ) ss << "spotgroups" << std::endl;
        if ( merge_ranges ) ss << "merge-ranges" << std::endl;
        if ( spread ) ss << "spread" << std::endl;
        if ( append ) ss << "append" << std::endl;
        
        if ( LineFeedCount > 0  )  ss << "line-feed : " << LineFeedValue << std::endl;
        if ( MaxLenCount > 0  )  ss << "max-length : " << MaxLenValue << std::endl;
        if ( IndentCount > 0  )  ss << "indent-width : " << IndentValue << std::endl;
        
        if ( !table_name.isEmpty() )  ss << "table : " << table_name << std::endl;
        if ( !rows.isEmpty() )  ss << "rows : " << rows << std::endl;
        if ( !columns.isEmpty() )  ss << "columns : " << columns << std::endl;
        if ( !schema.isEmpty() )  ss << "schema : " << schema << std::endl;
        if ( !dna_bases.isEmpty() ) ss << "dna-bases : " << dna_bases << std::endl;
        if ( !format.isEmpty() ) ss << "format " << format << std::endl;
        if ( !excluded.isEmpty() ) ss << "excluded: " << format << std::endl;
        if ( !bool_print.isEmpty() ) ss << "boolean printed as: " << bool_print << std::endl;
        if ( !idx_range.isEmpty() ) ss << "idx-range: " << idx_range << std::endl;
        if ( !cur_cache.isEmpty() ) ss << "cur-cache: " << cur_cache << std::endl;
        if ( !output_file.isEmpty() ) ss << "out-file: " << output_file << std::endl;
        if ( !output_path.isEmpty() ) ss << "out-path: " << output_path << std::endl;
        if ( !output_buffer_size.isEmpty() ) ss << "out-buffer-size: " << output_buffer_size << std::endl;
        
        return CmnOptAndAccessions::show( ss );
    }

    void populate_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions ) const override
    {
        populate_common_argv_builder( builder, acc_index, accessions );

        if ( row_id_on ) builder . add_option( "-I" );
        if ( colname_off ) builder . add_option( "-N" );
        if ( in_hex ) builder . add_option( "-X" );
        if ( schema_dump ) builder . add_option( "-A" );
        if ( table_enum ) builder . add_option( "-E" );
        if ( column_enum ) builder . add_option( "-O" );
        if ( column_enum_short ) builder . add_option( "-o" );
        if ( id_range ) builder . add_option( "-r" );
        if ( without_sra ) builder . add_option( "-n" );
        if ( obj_vers ) builder . add_option( "-j" );
        if ( obj_timestamp ) builder . add_option( "--obj_timestamp" );
        if ( obj_type ) builder . add_option( "-y" );
        if ( numelem ) builder . add_option( "-u" );
        if ( numelemsum ) builder . add_option( "-U" );
        if ( blobbing ) builder . add_option( "--blobbing" );
        if ( phys ) builder . add_option( "--phys" );
        if ( readable ) builder . add_option( "--readable" );
        if ( idx_report ) builder . add_option( "--idx_report" );
        if ( gzip ) builder . add_option( "--gzip" );
        if ( bzip ) builder . add_option( "--bzip2" );
        if ( info ) builder . add_option( "--info" );
        if ( spotgroups ) builder . add_option( "--spotgroups" );
        if ( merge_ranges ) builder . add_option( "--merge-ranges" );
        if ( spread ) builder . add_option( "--spread" );
        if ( append ) builder . add_option( "--append" );

        if ( LineFeedCount > 0  )  builder . add_option( "-l", LineFeedValue );
        if ( MaxLenCount > 0  )  builder . add_option( "-M", MaxLenValue );
        if ( IndentCount > 0  )  builder . add_option( "-i", IndentValue );
        
        if ( !table_name.isEmpty() )  builder . add_option( "-T", table_name );
        if ( !rows.isEmpty() )  builder . add_option( "-R", rows );
        if ( !columns.isEmpty() )  builder . add_option( "-C", columns );
        if ( !schema.isEmpty() )  builder . add_option( "-S", schema );
        if ( !dna_bases.isEmpty() )  builder . add_option( "-D", dna_bases );
        if ( !format.isEmpty() )  builder . add_option( "-f", format );
        if ( !excluded.isEmpty() )  builder . add_option( "-x", excluded );
        if ( !bool_print.isEmpty() )  builder . add_option( "-b", bool_print );
        if ( !idx_range.isEmpty() )  builder . add_option( "--idx-range", idx_range );
        if ( !cur_cache.isEmpty() )  builder . add_option( "--cur-cache", cur_cache );
        if ( !output_file.isEmpty() )  builder . add_option( "--output-file", output_file );
        if ( !output_path.isEmpty() )  builder . add_option( "--output-path", output_path );
        if ( !output_buffer_size.isEmpty() )  builder . add_option( "--output-buffer-size", output_buffer_size );
    }

    bool check() const override
    {
        int problems = 0;

        if ( !format.isEmpty() )
        {
            if ( !is_one_of( format, 12,
                             "csv", "xml", "json", "piped", "tab", "fastq", "fastq1",
                             "fasta", "fasta1", "fasta2", "qual", "qual1" ) )
            {
                std::cerr << "invalid format: " << format << std::endl;
                problems++;
            }
        }
        if ( !bool_print.isEmpty() )
        {
            if ( !is_one_of( bool_print, 2, "1", "T" ) )
            {
                std::cerr << "invalid boolean: " << bool_print << std::endl;
                problems++;
            }
        }
        if ( bzip && gzip )
        {
            std::cerr << "bzip2 and gzip cannot both be used at the same time" << std::endl;
            problems++;
        }
        return CmnOptAndAccessions::check() && ( problems == 0 );
    }

    int run() const override
    {
        auto const theirArgv0 = what.toolpath.getPathFor(TOOL_NAME).fullpath();
        {
            auto const realpath = what.toolpath.getPathFor(TOOL_ORIGINAL_NAME);
            if (realpath.executable())
                return ToolExec::run(TOOL_NAME, realpath.fullpath(), theirArgv0, *this, accessions);
        }
        throw std::runtime_error(TOOL_NAME " was not found or is not executable.");
    }

};

int impersonate_vdb_dump( Args const &args, WhatImposter const &what )
{
#if DEBUG || _DEBUGGING
    VdbDumpParams temp(what);
    auto &params = *randomized(&temp, what);
#else
    VdbDumpParams params(what);
#endif
    return Impersonator::run( args, params );
}

}
