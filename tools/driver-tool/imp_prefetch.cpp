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

#define TOOL_NAME "prefetch"
#define TOOL_ORIGINAL_NAME TOOL_NAME "-orig"

namespace sratools2
{

struct PrefetchParams final : CmnOptAndAccessions
{
    ncbi::String file_type;
    ncbi::String transport;
    ncbi::String min_size;
    ncbi::String max_size;
    ncbi::String force;
    ncbi::String resume;
    ncbi::String validate;
    bool progress;
    bool eliminate_quals;
    bool check_all;
    //ncbi::String ascp_path;
    //ncbi::String ascp_options;
    ncbi::String output_file;
    ncbi::String output_dir;
    bool dryrun;

    explicit PrefetchParams(WhatImposter const &what)
    : CmnOptAndAccessions(what)
    , progress( false )
    , eliminate_quals( false )
    , check_all( false )
    , dryrun( false )
    {
    }
    
    void add( ncbi::Cmdline &cmdline ) override
    {
        cmdline . addOption ( file_type, nullptr, "T", "type", "<file-type>",
            "Specify file type to download. Default: sra" );
        cmdline . addOption ( min_size, nullptr, "N", "min-size", "<size>",
            "Minimum file size to download in KB (inclusive)." );
        cmdline . addOption ( max_size, nullptr, "X", "max-size", "<size>",
            "Maximum file size to download in KB (exclusive). Default: 20G" );

        cmdline . addOption ( force, nullptr, "f", "force", "<no|yes|all|ALL>",
            "Force object download - one of: no, yes, all, ALL. "
            "no [default]: skip download if the object if found and complete; "
            "yes: download it even if it is found and is complete; "
            "all: ignore lock files (stale locks or "
            "it is being downloaded by another process - "
            "use at your own risk!); "
            "ALL: ignore lock files, restart download from beginning");

        cmdline . addOption ( progress, "p", "progress", "Show progress" );
        cmdline . addOption ( resume, nullptr, "r", "resume", "<yes|no>",
            "Resume partial downloads - one of: no, yes [default]" );

        cmdline . addOption ( validate, nullptr, "C", "verify", "<yes|no>",
            "Verify after download - one of: no, yes [default]" );

        cmdline . addOption ( check_all, "c", "check-all", "Double-check all refseqs" );

        /*
        cmdline . addOption ( ascp_path, nullptr, "a", "ascp-path", "<ascp-binary|private-key-file>",
            "Path to ascp program and private key file (asperaweb_id_dsa.putty)" );
        cmdline . addOption ( ascp_options, nullptr, "", "ascp-options", "<value>",
            "Arbitrary options to pass to ascp command line" );
        */
        
        cmdline . addOption ( output_file, nullptr, "o", "output-file",
            "<file>", "Write file to <file> when downloading single file" );
        cmdline . addOption ( output_dir, nullptr, "O", "output-directory",
            "<directory>", "Save files to <directory>/" );

        CmnOptAndAccessions::add(cmdline);

        // add a silent option...
        cmdline . startSilentOptions();
        cmdline . addOption ( dryrun, "", "dryrun", "-" );
        /* switched to silent instead of removing it */
        cmdline . addOption ( eliminate_quals, "", "eliminate-quals", "Don't download QUALITY column" );
        cmdline . addOption ( transport, nullptr, "t", "transport", "<value>",
            "Transport: one of: fasp; http; both [default]. "
            "(fasp only; http only; first try fasp (ascp), use "
            "http if cannot download using fasp)" );

    }

    std::ostream &show(std::ostream &ss) const override
    {
        if ( !file_type.isEmpty() ) ss << "file-type: " << file_type << std::endl;
        // if ( !transport.isEmpty() ) ss << "transport: " << transport << std::endl;
        if ( !min_size.isEmpty() ) ss << "min-size: " << min_size << std::endl;
        if ( !max_size.isEmpty() ) ss << "max-size: " << max_size << std::endl;
        if ( !force.isEmpty() ) ss << "force: " << force << std::endl;
        if ( progress ) ss << "progress" << std::endl;
        if ( !resume.isEmpty() ) ss << "resume: " << resume << std::endl;
        if ( !validate.isEmpty() ) ss << "validate: " << validate << std::endl;
        if ( eliminate_quals ) ss << "eliminate-quals" << std::endl;
        if ( check_all ) ss << "check-all" << std::endl;
        //if ( !ascp_path.isEmpty() ) ss << "ascp-path: " << ascp_path << std::endl;
        //if ( !ascp_options.isEmpty() ) ss << "ascp-options: " << ascp_options << std::endl;
        if ( !output_file.isEmpty() ) ss << "output-file: " << output_file << std::endl;
        if ( !output_dir.isEmpty() ) ss << "output-dir: " << output_dir << std::endl;
        if ( dryrun ) ss << "dryrun" << std::endl;
        return CmnOptAndAccessions::show(ss);
    }

    void populate_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions ) const override
    {
        (void)(acc_index); (void)(accessions);

        populate_common_argv_builder(builder, acc_index, accessions);

        if ( !file_type.isEmpty() ) builder . add_option( "-T", file_type );
        //if ( !transport.isEmpty() ) builder . add_option( "-t", transport );
        if ( !perm_file.isEmpty() ) builder . add_option( "--perm", perm_file );
        if ( !cart_file.isEmpty() ) builder . add_option( "--cart", cart_file );
        if ( !min_size.isEmpty() ) builder . add_option( "-N", min_size );
        if ( !max_size.isEmpty() ) builder . add_option( "-X", max_size );
        if ( !force.isEmpty() ) builder . add_option( "-f", force );
        if ( progress ) builder . add_option( "-p" );
        if ( !resume.isEmpty() ) builder . add_option( "-r", resume );
        if ( !validate.isEmpty() ) builder . add_option( "-C", validate);
        if ( eliminate_quals ) builder . add_option( "--eliminate-quals" );
        if ( check_all ) builder . add_option( "-c" );
        //if ( !ascp_path.isEmpty() ) builder . add_option( "-a", ascp_path );
        //if ( !ascp_options.isEmpty() ) builder . add_option( "--ascp_options", ascp_options );
        if ( !output_file.isEmpty() ) builder . add_option( "-o", output_file );
        if ( !output_dir.isEmpty() ) builder . add_option( "-O", output_dir );
        if ( dryrun ) builder . add_option( "--dryrun" );

        // permanently pin the transport option to 'http'
        builder . add_option( "-t", "http" );

        // prefetch gets location
        if (!location.isEmpty()) builder.add_option("--location", location);
    }

    bool check() const override
    {
        int problems = 0;
        if ( eliminate_quals )
        {
            std::cerr << "The option 'eliminate-quals' has been temporary disabled." << std::endl;
            problems++;
        }

        if ( !transport.isEmpty() )
        {
            bool http = ( ( transport.compare( "http" ) == 0 ) ||
                          ( transport.compare( "https" ) == 0 ) );
            if ( !http )
            {
                std::cerr << "The option 'transport' has been temporary disabled due to the unavailability of Aspera servers. "
                    "The data will be downloaded using https." << std::endl;
            }
        }

        return CmnOptAndAccessions::check() && ( problems == 0 );
    }

    int run() const override {
        auto const theirArgv0 = what.toolpath.getPathFor(TOOL_NAME).fullpath();
        {
            auto const realpath = what.toolpath.getPathFor(TOOL_ORIGINAL_NAME);
            if (realpath.executable())
                return ToolExecNoSDL::run(TOOL_NAME, realpath.fullpath(), theirArgv0, *this, accessions);
        }
#if DEBUG || _DEBUGGING
		{	// look for the "official" name not the -orig; TODO: remove when Make creates symlinks
			auto const realpath = what.toolpath.getPathFor(TOOL_NAME);
            if (realpath.executable())
                return ToolExecNoSDL::run(TOOL_NAME, realpath.fullpath(), theirArgv0, *this, accessions);
        }
#endif
        throw std::runtime_error(TOOL_NAME " was not found or is not executable.");
    }
};

int impersonate_prefetch( const Args &args, WhatImposter const &what )
{
#if DEBUG || _DEBUGGING
    PrefetchParams temp(what);
    auto &params = *randomized(&temp, what);
#else
    PrefetchParams params(what);
#endif
    return Impersonator::run(args, params);
}

}
