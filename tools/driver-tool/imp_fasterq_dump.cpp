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

#if WINDOWS
    #define TOOL_NAME "fasterq-dump-orig"
#else
    #define TOOL_NAME "fasterq-dump"
#endif

namespace sratools2
{

struct FasterqParams final : CmnOptAndAccessions
{
    ncbi::String outfile;
    ncbi::String outdir;
    ncbi::String bufsize;
    ncbi::String curcache;
    ncbi::String mem;
    ncbi::String temp;
    ncbi::String table;
    ncbi::String bases;
    ncbi::U32 ThreadsCount;
    ncbi::U32 Threads;
    bool progress;
    bool details;
    bool split_spot;
    bool split_files;
    bool split_3;
    bool concatenate_reads;
    bool to_stdout;
    bool force;
    bool rowid_as_name;
    bool skip_tech;
    bool include_tech;
    bool print_read_nr;
    ncbi::U32 MinReadLenCount;
    ncbi::U32 MinReadLen;
    bool strict;
    bool append;

    explicit FasterqParams(WhatImposter const &what)
    : CmnOptAndAccessions(what)
    , ThreadsCount( 0 )
    , Threads( 0 )
    , progress( false )
    , details( false )
    , split_spot( false )
    , split_files( false )
    , split_3( false )
    , concatenate_reads( false )
    , to_stdout( false )
    , force( false )
    , rowid_as_name( false )
    , skip_tech( false )
    , include_tech( false )
    , print_read_nr( false )
    , MinReadLenCount( 0 )
    , MinReadLen( 0 )
    , strict( false )
    , append( false )
    {
    }

    void add( ncbi::Cmdline &cmdline ) override
    {
        cmdline . addOption ( outfile, nullptr, "o", "outfile", "<path>",
            "full path of outputfile (overrides usage of current directory and given accession)" );
        cmdline . addOption ( outdir, nullptr, "O", "outdir", "<path>",
            "path for outputfile (overrides usage of current directory, but uses given accession)" );
        cmdline . addOption ( bufsize, nullptr, "b", "bufsize", "<size>",
            "size of file-buffer (dflt=1MB, takes number or number and unit)" );
        cmdline . addOption ( curcache, nullptr, "c", "curcache", "<size>",
            "size of cursor-cache (dflt=10MB, takes number or number and unit)" );
        cmdline . addOption ( mem, nullptr, "m", "mem", "<size>",
            "memory limit for sorting (dflt=100MB, takes number or number and unit)" );
        cmdline . addOption ( temp, nullptr, "t", "temp", "<path>",
            "path to directory for temp. files (dflt=current dir.)" );

        cmdline . addOption ( Threads, &ThreadsCount, "e", "threads", "<count>",
            "how many threads to use (dflt=6)" );

        cmdline . addOption ( progress, "p", "progres", "show progress (not possible if stdout used)" );
        cmdline . addOption ( details, "x", "details", "print details of all options selected" );
        cmdline . addOption ( split_spot, "s", "split-spot", "split spots into reads" );
        cmdline . addOption ( split_files, "S", "split-files", "write reads into different files" );
        cmdline . addOption ( split_3, "3", "split-3", "writes single reads into special file" );
        cmdline . addOption ( concatenate_reads, "", "concatenate-reads", "writes whole spots into one file" );
        cmdline . addOption ( to_stdout, "Z", "stdout", "print output to stdout" );
        cmdline . addOption ( force, "f", "force", "force overwrite of existing file(s)" );
        cmdline . addOption ( rowid_as_name, "N", "rowid-as-name", "use rowid as name (avoids using the name column)" );
        cmdline . addOption ( skip_tech, "", "skip-technical", "skip technical reads" );
        cmdline . addOption ( include_tech, "", "include-technical", "explicitly include technical reads" );
        cmdline . addOption ( print_read_nr, "P", "print-read-nr", "include read-number in defline" );

        cmdline . addOption ( MinReadLen, &MinReadLenCount, "M", "min-read-len", "<count>",
            "filter by sequence-lenght" );

        cmdline . addOption ( table, nullptr, "", "table", "<name>", "which seq-table to use in case of pacbio" );
        cmdline . addOption ( strict, "", "strict", "terminate on invalid read" );

        cmdline . addOption ( bases, nullptr, "B", "bases", "<bases>", "filter output by matching against given bases" );
        cmdline . addOption ( append, "A", "append", "append to output-file, instead of overwriting it" );

        CmnOptAndAccessions::add(cmdline);
    }

    std::ostream &show(std::ostream &ss) const override
    {
        if ( !outfile.isEmpty() ) ss << "outfile : " << outfile << std::endl;
        if ( !outdir.isEmpty() ) ss << "outdir : " << outdir << std::endl;
        if ( !bufsize.isEmpty() ) ss << "bufsize : " << bufsize << std::endl;
        if ( !curcache.isEmpty() ) ss << "curcache : " << curcache << std::endl;
        if ( !mem.isEmpty() ) ss << "mem : " << mem << std::endl;
        if ( !temp.isEmpty() ) ss << "temp : " << temp << std::endl;
        if ( ThreadsCount > 0  ) ss << "threads : " << Threads << std::endl;
        if ( progress ) ss << "progress" << std::endl;
        if ( details ) ss << "details" << std::endl;
        if ( split_spot ) ss << "split-spot" << std::endl;
        if ( split_files ) ss << "split-files" << std::endl;
        if ( split_3 ) ss << "split-3" << std::endl;
        if ( concatenate_reads ) ss << "concatenate-reads" << std::endl;
        if ( to_stdout ) ss << "stdout" << std::endl;
        if ( force ) ss << "force" << std::endl;
        if ( rowid_as_name ) ss << "rowid-as-name" << std::endl;
        if ( skip_tech ) ss << "skip-technical" << std::endl;
        if ( include_tech ) ss << "include-technical" << std::endl;
        if ( print_read_nr ) ss << "print-read-nr" << std::endl;
        if ( MinReadLenCount > 0  )  ss << "min-read-len : " << MinReadLen << std::endl;
        if ( !table.isEmpty() )  ss << "table : " << table << std::endl;
        if ( strict ) ss << "strict" << std::endl;
        if ( !bases.isEmpty() )  ss << "bases : " << bases << std::endl;
        if ( append ) ss << "append" << std::endl;
        return CmnOptAndAccessions::show(ss);
    }

    void populate_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions ) const override
    {
        CmnOptAndAccessions::populate_argv_builder(builder, acc_index, accessions);

        if ( !outfile.isEmpty() ) {
            if (accessions.size() > 1) {
                if (acc_index == 0) {
                    print_unsafe_output_file_message("fasterq-dump", ".fastq", accessions);
                }
                builder . add_option( "-o", accessions[acc_index] + ".fastq" );
            }
            else
                builder . add_option( "-o", outfile );
        }
        if ( !outdir.isEmpty() ) builder . add_option( "-O", outdir );
        if ( !bufsize.isEmpty() ) builder . add_option( "-b", bufsize );
        if ( !curcache.isEmpty() ) builder . add_option( "-c", curcache );
        if ( !mem.isEmpty() ) builder . add_option( "-m", mem );
        if ( !temp.isEmpty() ) builder . add_option( "-t", temp );
        if ( ThreadsCount > 0 ) builder . add_option( "-e", Threads );
        if ( progress ) builder . add_option( "-p" );
        if ( details ) builder . add_option( "-x" );
        if ( split_spot ) builder . add_option( "-s" );
        if ( split_files ) builder . add_option( "-S" );
        if ( split_3 ) builder . add_option( "-3" );
        if ( concatenate_reads) builder . add_option( "concatenate-reads" );
        if ( to_stdout ) builder . add_option( "-Z" );
        if ( force ) builder . add_option( "-f" );
        if ( rowid_as_name ) builder . add_option( "-N" );
        if ( skip_tech ) builder . add_option( "--skip-technical" );
        if ( include_tech ) builder . add_option( "--include-technical" );
        if ( print_read_nr ) builder . add_option( "--print-read-nr" );
        if ( MinReadLenCount > 0 ) builder . add_option( "-M", MinReadLen );
        if ( !table.isEmpty() ) builder . add_option( "--table", table );
        if ( strict ) builder . add_option( "--strict" );
        if ( !bases.isEmpty() ) builder . add_option( "-B", bases );
        if ( append ) builder . add_option( "-A" );
    }

    bool check() const override
    {
        int problems = 0;

        return CmnOptAndAccessions::check() && ( problems == 0 );
    }

    int run() const override {
        auto const theirArgv0 = what.toolpath.getPathFor(TOOL_NAME).fullpath();
        {
            auto const realpath = what.toolpath.getPathFor(TOOL_NAME);
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

int impersonate_fasterq_dump(Args const &args, WhatImposter const &what)
{
#if DEBUG || _DEBUGGING
    FasterqParams temp(what);
    auto &params = *randomized(&temp, what);
#else
    FasterqParams params(what);
#endif
    return Impersonator::run(args, params);
}

}
