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

#include <iostream>

#include <stdlib.h>
#include <string.h>

#include "support2.hpp"

namespace sratools2
{
    std::string runpath_from_argv0( int argc, char *argv[] )
    {
        auto const impersonate = getenv( "SRATOOLS_IMPERSONATE" );
        auto const argv0 = (impersonate && impersonate[0]) ? impersonate : argv[0];
        WhatImposter what( argv0 );
        return what._runpath;
    }

    static auto const error_continues_message = std::string( "If this continues to happen, please contact the SRA Toolkit at https://trace.ncbi.nlm.nih.gov/Traces/sra/" );
    
    // the 'new' main-function of sratools
    int main2( int argc, char *argv[] )
    {
        int res = 0;
        // encapsulate the original args into a class, eventually inserting argv[0] from env-variable
        // Args is a class defined in support2.hpp
        Args args( argc, argv, getenv( "SRATOOLS_IMPERSONATE" ) );

        // just to see what we got
        //args.print();

        // detect what imposter we are asked to impersonate
        // WhatImposter is a class defined in support2.hpp
        WhatImposter what( args . argv[ 0 ] );
        
        // just to see what was detected...
        //std::cout << "what = " << what.as_string() << std::endl;

        if ( what.invalid() )
        {
            std::cerr << "An error occured: invalid tool requested" << std::endl << error_continues_message << std::endl;
            res = 3;
        }
        else if ( what.invalid_version() )
        {
            std::cerr << "An error occured: invalid version requested" << std::endl << error_continues_message << std::endl;
            res = 3;
        }
        else
        {
            switch( what._imposter )
            {
                case Imposter::SRAPATH      : return impersonate_srapath( args, what ); break;
                case Imposter::PREFETCH     : return impersonate_prefetch( args, what ); break;
                case Imposter::FASTQ_DUMP   : return impersonate_fastq_dump( args, what ); break;
                case Imposter::FASTERQ_DUMP : return impersonate_fasterq_dump( args, what ); break;
                case Imposter::SRA_PILEUP   : return impersonate_sra_pileup( args, what ); break;
                case Imposter::SAM_DUMP     : return impersonate_sam_dump( args, what ); break;
                case Imposter::VDB_DUMP     : return impersonate_vdb_dump( args, what ); break;
                default : 
                    std::cerr << "An error occured: invalid tool requested" << std::endl << error_continues_message << std::endl;
                    res = 3;
                    break;
            }
        }
        return res;
    }
    
}
