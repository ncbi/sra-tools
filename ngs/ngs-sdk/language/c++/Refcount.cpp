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

#include <ngs/itf/Refcount.hpp>
#include <ngs/itf/ErrBlock.hpp>
#include <ngs/itf/VTable.hpp>
#include <ngs/itf/Refcount.h>

namespace ngs
{
    /*----------------------------------------------------------------------
     * Cast
     *  cast vtable to proper type
     */
    ItfTok NGS_Refcount_v1_tok ( "NGS_Refcount_v1" );

    inline
    const NGS_Refcount_v1_vt * Cast ( const NGS_VTable * in )
    {
        const NGS_Refcount_v1_vt * out = static_cast < const NGS_Refcount_v1_vt* >
            ( Cast ( in, NGS_Refcount_v1_tok ) );
        if ( out == 0 )
            throw ErrorMsg ( "object is not of type Refcount_v1" );
        return out;
    }

    /*----------------------------------------------------------------------
     * OpaqueRefcount
     */

    inline
    NGS_Refcount_v1 * OpaqueRefcount :: Self ()
    {
        return reinterpret_cast < NGS_Refcount_v1* > ( this );
    }

    inline
    const NGS_Refcount_v1 * OpaqueRefcount :: Self () const
    {
        return reinterpret_cast < const NGS_Refcount_v1* > ( this );
    }

    void OpaqueRefcount :: Release ()
        NGS_NOTHROW ()
    {
        if ( this != 0 )
        {
            // cast to C object
            NGS_Refcount_v1 * self = Self ();

            try
            {
                // extract VTable
                const NGS_Refcount_v1_vt * vt = Cast ( self -> vt );

                // release object
                ErrBlock err;
                assert ( vt -> release != 0 );
                ( * vt -> release ) ( self, & err );

                // check for errors
                err . Check ();
            }
            catch ( ErrorMsg & x )
            {
                // good place for a message to console or error log
            }
            catch ( ... )
            {
            }
        }
    }

    void * OpaqueRefcount :: Duplicate () const
        NGS_THROWS ( ErrorMsg )
    {
        if ( this != 0 )
        {
            // cast to C object
            const NGS_Refcount_v1 * self = Self ();

            // extract VTable
            const NGS_Refcount_v1_vt * vt = Cast ( self -> vt );

            // duplicate object reference
            ErrBlock err;
            assert ( vt -> duplicate != 0 );
            void * dup = ( * vt -> duplicate ) ( self, & err );

            // check for errors
            err. Check ();

            // return duplicated reference
            assert ( dup != 0 );
            return dup;
        }

        return 0;
    }
}
