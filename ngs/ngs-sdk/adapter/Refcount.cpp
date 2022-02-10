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

#include <ngs/adapter/Refcount.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

#include <limits.h>

#include "atomic32.h"

namespace ngs_adapt
{
    /*----------------------------------------------------------------------
     * OpaqueRefcount
     */

    OpaqueRefcount :: ~OpaqueRefcount ()
    {
	}
  
    void OpaqueRefcount :: Release ()
    {
        int result = atomic32_read_and_add ( & refcount, -1 );
        switch ( result )
        {
        case 1:
            // the object should be collected
            delete this;
            break;

        case 0:
            // this is an exceptional case
            atomic32_set ( & refcount, 0 );
            throw ErrorMsg ( "releasing a zombie object" );

        default:
            // should be positive
            assert ( result > 0 ); 
        }
    }

    void * OpaqueRefcount :: Duplicate () const
    {
        int prior = atomic32_read_and_add_gt ( & refcount, 1, 0 );
        if ( prior <= 0 )
            throw ErrorMsg ( "attempt to duplicate a zombie object" );

        if ( prior == INT_MAX )
        {
            atomic32_dec ( & refcount );
            throw ErrorMsg ( "too many references to object" );
        }

        return ( void* ) this;
    }

    OpaqueRefcount :: OpaqueRefcount ( const OpaqueRefcount & obj )
    {
        cobj = obj . cobj;
        // TBD - should we be copying the refcount?
        atomic32_set ( & refcount, 1 );
    }

    OpaqueRefcount :: OpaqueRefcount ( const NGS_VTable * vt )
    {
        cobj . vt = vt;
        atomic32_set ( & refcount, 1 );
    }

    void CC OpaqueRefcount :: release ( NGS_Refcount_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        OpaqueRefcount * self = static_cast < OpaqueRefcount* > ( offset_cobj ( ( void* ) iself ) );
        try
        {
            self -> Release ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }
    }

    void * CC OpaqueRefcount :: duplicate ( const NGS_Refcount_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const OpaqueRefcount * self = static_cast < const OpaqueRefcount* > ( offset_cobj ( ( const void* ) iself ) );
        try
        {
            OpaqueRefcount * obj = ( OpaqueRefcount* ) self -> Duplicate ();
            return & obj -> cobj;
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }
		return 0;
    }

    NGS_Refcount_v1_vt OpaqueRefcount :: ivt =
    {
        {
            "ngs_adapt::OpaqueRefcount",
            "NGS_Refcount_v1"
        },

        release,
        duplicate
    };

} // namespace ngs_adapt
