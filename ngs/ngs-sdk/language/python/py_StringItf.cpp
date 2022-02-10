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

#include "py_StringItf.h"
#include "py_ErrorMsg.hpp"

#include <ngs/itf/StringItf.hpp>

PY_RES_TYPE PY_NGS_StringGetSubstring ( void* pRef, size_t offset, size_t size, void** pRet, void** ppNGSStrError )
{
    PY_RES_TYPE ret = PY_RES_ERROR; // TODO: use xt_* codes
    try
    {
        ngs::StringItf* res = CheckedCast< ngs::StringItf* >(pRef) -> substr ( offset, size );
        assert (pRet != NULL);
        *pRet = (void*) res;
        ret = PY_RES_OK;
    }
    catch ( ngs::ErrorMsg & x )
    {
        ret = ExceptionHandler ( x, ppNGSStrError );
    }
    catch ( std::exception & x )
    {
        ret = ExceptionHandler ( x, ppNGSStrError );
    }
    catch ( ... )
    {
        ret = ExceptionHandler ( ppNGSStrError );
    }

    return ret;
}


PY_RES_TYPE PY_NGS_StringGetData ( void* pRef, char const** pRet )
{
    assert ( pRef );
    assert ( pRet );
    *pRet = ((ngs::StringItf const*)pRef) -> data ();
    return PY_RES_OK;
}

PY_RES_TYPE PY_NGS_StringGetSize ( void* pRef, size_t* pRet )
{
    assert ( pRef );
    assert ( pRet );
    *pRet = ((ngs::StringItf const*)pRef) -> size ();
    return PY_RES_OK;
}
