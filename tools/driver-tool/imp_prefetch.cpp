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

struct PrefetchParams : OptionBase
{
    ncbi::String file_type;
    ncbi::String transport;
    ncbi::U32 min_size_count;
    ncbi::U32 min_size_value;
    ncbi::U32 max_size_count;
    ncbi::U32 max_size_value;
    ncbi::String force;
    ncbi::U32 progress_count;
    ncbi::U32 progress_value;
    bool eliminate_quals;
    bool check_all;
    ncbi::String ascp_path;
    ncbi::String ascp_options;
    ncbi::String output_file;
    ncbi::String output_dir;
    
    PrefetchParams()
        : min_size_count( 0 ), min_size_value( 0 )
        , max_size_count( 0 ), max_size_value( 0 )
        , progress_count( 0 ), progress_value( 0 )
        , eliminate_quals( false )
        , check_all( false )
    {
    }
    
    void add( ncbi::Cmdline &cmdline )
    {
        cmdline . addOption ( file_type, nullptr, "T", "type", "<file-type>",
            "Specify file type to download. Default: sra" );
        cmdline . addOption ( transport, nullptr, "t", "transport", "<value>",
            "Transport: one of: fasp; http; both. (fasp only; http only; first try fasp (ascp), use "
            "http if cannot download using fasp). Default: both" );

        cmdline . addOption ( min_size_value, &min_size_count, "N", "min_size", "<size>",
            "Minimum file size to download in KB (inclusive)." );
        cmdline . addOption ( max_size_value, &max_size_count, "X", "max_size", "<size>",
            "Maximum file size to download in KB (exclusive). Default: 20G" );

        cmdline . addOption ( force, nullptr, "f", "force", "<value>",
            "Force object download one of: no, yes, all. no [default]: skip download if the "
            "object if found and complete; yes: download it even if it is found and is complete; all: "
            "ignore lock files (stale locks or it is being downloaded by another process: "
            "use at your own risk!)" );

        cmdline . addOption ( progress_value, &progress_count, "p", "progress", "<value>",
            "Time period in minutes to display download progress (0: no progress), default: 1" );

        cmdline . addOption ( eliminate_quals, "", "eliminate-quals", "Don't download QUALITY column" );
        cmdline . addOption ( check_all, "c", "check-all", "Double-check all refseqs" );

        cmdline . addOption ( ascp_path, nullptr, "a", "ascp-path", "<ascp-binary|private-key-file>",
            "Path to ascp program and private key file (asperaweb_id_dsa.putty)" );
        cmdline . addOption ( ascp_options, nullptr, "", "ascp-options", "<value>",
            "Arbitrary options to pass to ascp command line" );

        cmdline . addOption ( output_file, nullptr, "o", "output-file", "<value>",
            "Write file to FILE when downloading single file" );
        cmdline . addOption ( output_dir, nullptr, "O", "output-directory", "<path>",
            "Save files to path/" );

    }

    std::string as_string()
    {
        std::stringstream ss;
        if ( !file_type.isEmpty() ) ss << "file-type: " << file_type << std::endl;
        if ( !transport.isEmpty() ) ss << "transport: " << transport << std::endl;
        if ( min_size_count > 0 ) ss << "min-size: " << min_size_value << std::endl;
        if ( max_size_count > 0 ) ss << "max-size: " << max_size_value << std::endl;
        if ( !force.isEmpty() ) ss << "force: " << force << std::endl;
        if ( progress_count > 0 ) ss << "progress: " << progress_value << std::endl;
        if ( eliminate_quals ) ss << "eliminate-quals" << std::endl;
        if ( check_all ) ss << "check-all" << std::endl;
        if ( !ascp_path.isEmpty() ) ss << "ascp-path: " << ascp_path << std::endl;
        if ( !ascp_options.isEmpty() ) ss << "ascp-options: " << ascp_options << std::endl;
        if ( !output_file.isEmpty() ) ss << "output-file: " << output_file << std::endl;
        if ( !output_dir.isEmpty() ) ss << "output-dir: " << output_dir << std::endl;
        return ss.str();
    }

    void populate_argv_builder( ArgvBuilder & builder )
    {
        if ( !file_type.isEmpty() ) builder . add_option( "-T", file_type );
        if ( !transport.isEmpty() ) builder . add_option( "-t", transport );
        if ( min_size_count > 0 ) builder . add_option( "-N", min_size_value );
        if ( max_size_count > 0 ) builder . add_option( "-X", max_size_value );
        if ( !force.isEmpty() ) builder . add_option( "-f", force );
        if ( progress_count > 0 ) builder . add_option( "-p", progress_value );
        if ( eliminate_quals ) builder . add_option( "--eliminate-quals" );
        if ( check_all ) builder . add_option( "-c" );
        if ( !ascp_path.isEmpty() ) builder . add_option( "-a", ascp_path );
        if ( !ascp_options.isEmpty() ) builder . add_option( "--ascp_options", ascp_options );
        if ( !output_file.isEmpty() ) builder . add_option( "-o", output_file );
        if ( !output_dir.isEmpty() ) builder . add_option( "-O", output_dir );
    }

    bool check()
    {
        int problems = 0;

        return ( problems == 0 );
    }

    int run( ArgvBuilder &builder, CmnOptAndAccessions &cmn )
    {
        int res = 0;

        // instead of looping over the accessions, expand them and loop over the 
        // expanded url's
        for ( auto const &value : cmn . accessions )
        {
            if ( res == 0 )
            {
                int argc;
                char ** argv = builder . generate_argv( argc, value );
                if ( argv != nullptr )
                {
                    // instead of this run the tool...
                    for ( int i = 0; i < argc; ++i )
                        std::cout << "argv[" << i << "] = '" << argv[ i ] << "'" << std::endl;

                    builder . free_argv( argc, argv );
                }
            }
        }
        return res;
    }

};

int impersonate_prefetch( const Args &args )
{
    int res = 0;

    // PrefetchParams is a derived class of ToolOptions, defined in support2.hpp
    PrefetchParams params;
    
    Impersonator imp( args, "prefetch", params );
    res = imp . run();

    return res;
}

}
