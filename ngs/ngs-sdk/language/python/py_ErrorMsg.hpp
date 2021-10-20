/*===========================================================================
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

#include <string.h>
#include <ngs/itf/ErrorMsg.hpp>

namespace
{
    template < typename T >
    T CheckedCast ( void* pRef )
    {
        if ( !pRef )
            throw ngs::ErrorMsg ( "NULL pRef parameter" );

        return ( T ) pRef;
    }

    template < typename E >
    PY_RES_TYPE ExceptionHandler (E& x, void** ppNGSStrError)
    {
        assert(ppNGSStrError);

        char const* error_descr = x.what();
        size_t len = strlen ( error_descr );
        char* error_copy = new char [ len + 1 ];
        ::memmove ( error_copy, error_descr, len + 1 );
        *((char**)ppNGSStrError) = error_copy;

        return PY_RES_ERROR;
    }

    PY_RES_TYPE ExceptionHandler ( void** ppNGSStrError )
    {
        char const error_text_constant[] = "INTERNAL ERROR";
        char* error_copy = new char [ sizeof error_text_constant ];
        ::memmove ( error_copy, error_text_constant, sizeof error_text_constant );
        *((char**)ppNGSStrError) = error_copy;

        return PY_RES_ERROR;
    }
}
