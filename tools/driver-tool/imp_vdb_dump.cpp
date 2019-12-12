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
#include "which.hpp"

#define TOOL_NAME "vdb-dump"

namespace sratools2
{

struct VdbDumpParams final : CmnOptAndAccessions
{
    VdbDumpParams( WhatImposter const &what )
    : CmnOptAndAccessions( what )
    {
    }

    void add( ncbi::Cmdline &cmdline ) override
    {

        CmnOptAndAccessions::add( cmdline );
    }

    std::ostream &show( std::ostream &ss ) const override
    {
        return CmnOptAndAccessions::show( ss );
    }

    void populate_argv_builder( ArgvBuilder & builder, int acc_index, std::vector<ncbi::String> const &accessions ) const override
    {
        CmnOptAndAccessions::populate_argv_builder( builder, acc_index, accessions );
    }

    bool check() const override
    {
        int problems = 0;

        return CmnOptAndAccessions::check() && ( problems == 0 );
    }

    int run() const override
    {
        return ToolExec::run( what.toolpath().c_str(), what._basename.c_str(), *this, accessions );
    }

};

int impersonate_vdb_dump( Args const &args, WhatImposter const &what )
{
    VdbDumpParams params( what );
    return Impersonator::run( args, params );
}

}