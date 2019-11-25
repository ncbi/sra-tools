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

#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <ncbi/secure/string.hpp>

#include "cmdline.hpp"
#include "support2.hpp"

namespace sratools2
{

// just a demo-struct
struct OriginalParams
{
    std :: vector <ncbi::String> params;
    ncbi::U32 opt1, opt1_count;
};

int impersonate_fastq_dump( const Args &args )
{
    int res = 0;

    // Cmdline is a class defined in cmdline.hpp
    ncbi::Cmdline cmdline( args._argc, args._argv );
    CmnOptAndAccessions cmn;

    cmn.add( cmdline );
    /*
    OriginalParams org;
    org . opt1 = 0;

    cmdline . addParam ( org . params, 0, 256, "token(s)", "list of tokens to process" );
    cmdline . addOption ( org . opt1, &( org . opt1_count ), "o", "option", "a value", "opt1 help" );
    cmdline . addOption ( org . opt1, &( org . opt1_count ), "lo", "long-option", "a value", "opt1 help" );
    */
    
    try
    {
        cmdline . parse ( true );

        cmdline . parse ();

        std::cout << cmn.as_string() << std::endl;
        //for ( auto const& value : cmn . accessions )
        //    std::cout << "acc = " << value << std::endl;
        //std::cout << "option1 = " << org . opt1 << std::endl;

    }
    catch ( ncbi::InvalidArgument const &e )
    {
        std::cerr << "An error occured: " << e.what() << std::endl;
    }

    return res;
}

} // namespace