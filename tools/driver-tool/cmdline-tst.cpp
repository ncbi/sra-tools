/*==============================================================================
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
 *  Author: Kurt Rodarmer
 *
 * ===========================================================================
 *
 */

#pragma once

#include <ncbi/secure/except.hpp>

#include "cmdline.hpp"

#include <iostream>

using namespace ncbi;

int main ( int argc, char * argv [] )
{
    int status = 0;

    try
    {
        Cmdline args ( argc, argv );

        uint32_t count = 0;
        args . addParam ( count, "count", "set the counter" );

        uint32_t verbosity = 0;
        args . addOption ( verbosity, "v", "verbose", "increase verbosity" );
        args . parse ();

        std :: cout
            << "count is now "
            << count
            << ", and verbosity is "
            << verbosity
            << '\n'
            ;
        args . help ();
    }
    catch ( Exception & x )
    {
        std :: cerr
            << "ERROR: "
            << x . msg
            << '\n'
            ;
        status = x . status;
    }
    catch ( ... )
    {
        std :: cerr
            << "UNKNOWN ERROR\n"
            ;
        status = rc_runtime_err;
    }

    return status;
}
