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

#define TOOL_NAME "srapath"

namespace sratools2
{

struct SrapathParams final : CmnOptAndAccessions
{
    ncbi::String function;
    ncbi::U32 timeout_count;
    ncbi::U32 timeout_value;
    ncbi::String protocol;
    ncbi::String version;
    ncbi::String url;
    ncbi::String param;
    ncbi::String project;
    bool print_raw, print_json, resolve_cache, print_path;


    SrapathParams(WhatImposter const &what)
    : CmnOptAndAccessions(what)
    , timeout_count( 0 ), timeout_value( 0 )
    , print_raw( false )
    , print_json( false )
    , resolve_cache( false )
    , print_path( false )
    {
    }
    
    void add( ncbi::Cmdline &cmdline ) override
    {
        cmdline . addOption ( function, nullptr, "f", "function", "<function>",
            "function to perform (resolve, names, search) default=resolve "
            "or names if protocol is specified" );
        cmdline . addOption ( timeout_value, &timeout_count, "t", "timeout", "<value>",
            "timeout-value for request" );
        cmdline . addOption ( protocol, nullptr, "a", "protocol", "<protocol>",
            "protocol (fasp; http; https; fasp,http; ..) default=https" );
        cmdline . addOption ( version, nullptr, "e", "vers", "<version>", "version-string for cgi-calls" );
        cmdline . addOption ( url, nullptr, "u", "url", "<url>", "url to be used for cgi-calls" );
        cmdline . addOption ( param, nullptr, "p", "param", "<parameter>", 
            "param to be added to cgi-call (tic=XXXXX): raw-only" );
            
        cmdline . addOption ( print_raw, "r", "raw", "print the raw reply (instead of parsing it)" );
        cmdline . addOption ( print_json, "j", "json", "print the reply in JSON" );

        cmdline . addOption ( project, nullptr, "d", "project>", "<project-id>", 
            "use numeric [dbGaP] project-id in names-cgi-call" );

        cmdline . addOption ( resolve_cache, "c", "cache",
            "resolve cache location along with remote when performing names function" );

        cmdline . addOption ( print_path, "P", "path", "print path of object: names function-only" );

        CmnOptAndAccessions::add(cmdline);
    }

    std::ostream &show(std::ostream &ss) const override
    {
        if ( !function.isEmpty() ) ss << "function: " << function << std::endl;
        if ( timeout_count > 0 ) ss << "timeout: " << timeout_value << std::endl;
        if ( !protocol.isEmpty() ) ss << "protocol: " << protocol << std::endl;
        if ( !version.isEmpty() ) ss << "version: " << version << std::endl;
        if ( !url.isEmpty() ) ss << "url: " << url << std::endl;
        if ( !param.isEmpty() ) ss << "param: : "<< param << std::endl;
        if ( print_raw ) ss << "print raw" << std::endl;
        if ( print_json ) ss << "print json" << std::endl;
        if ( !project.isEmpty() ) ss << "project: " << project << std::endl;
        if ( resolve_cache ) ss << "resolve cache-file" << std::endl;
        if ( print_path ) ss << "print path" << std::endl;
        return CmnOptAndAccessions::show(ss);
    }

    void populate_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions ) const override
    {
        (void)(acc_index); (void)(accessions);

        CmnOptAndAccessions::populate_argv_builder(builder, acc_index, accessions);

        if ( !function.isEmpty() ) builder . add_option( "-f", function );
        if ( timeout_count > 0 ) builder . add_option( "-t", timeout_value );
        if ( !protocol.isEmpty() ) builder . add_option( "-a", protocol );
        if ( !version.isEmpty() ) builder . add_option( "-e", version );
        if ( !url.isEmpty() ) builder . add_option( "-u", url );
        if ( !param.isEmpty() ) builder . add_option( "-p", param );
        if ( print_raw ) builder . add_option( "-r" );
        if ( print_json ) builder . add_option( "-j" );
        if ( !project.isEmpty() ) builder . add_option( "-d", project );
        if ( resolve_cache ) builder . add_option( "-c" );
        if ( print_path ) builder . add_option( "-P" );

        // srapath get perm and location
        if (!perm_file.isEmpty()) builder.add_option("--perm", perm_file);
        if (!location.isEmpty()) builder.add_option("--location", perm_file);
    }

    bool check() const override
    {
        int problems = 0;

        return CmnOptAndAccessions::check() && ( problems == 0 );
    }

    int run() const override {
        return ToolExecNoSDL::run(what.toolpath().c_str(), what._basename.c_str(), *this, accessions);
    }
};

int impersonate_srapath( const Args &args, WhatImposter const &what )
{
    SrapathParams params(what);
    return Impersonator::run(args, params);
}

}
