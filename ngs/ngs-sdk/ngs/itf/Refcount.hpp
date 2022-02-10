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

#ifndef _hpp_ngs_itf_refcount_
#define _hpp_ngs_itf_refcount_

#pragma clang diagnostic ignored "-Wtautological-undefined-compare"

#ifndef _hpp_ngs_itf_error_msg_
#include <ngs/itf/ErrorMsg.hpp>
#endif

#ifndef _h_ngs_itf_defs_
#include <ngs/itf/defs.h>
#endif

struct NGS_Refcount_v1;

namespace ngs
{

    /*----------------------------------------------------------------------
     * OpaqueRefcount
     */
    class   OpaqueRefcount
    {
    public:

        void Release ()
            NGS_NOTHROW ();

        void * Duplicate () const
            NGS_THROWS ( ErrorMsg );

    private:
        OpaqueRefcount ();

        NGS_Refcount_v1 * Self ();
        const NGS_Refcount_v1 * Self () const;
    };


    /*----------------------------------------------------------------------
     * Refcount
     */
    template < class T, class C >
    class   Refcount : protected OpaqueRefcount
    {
    public:

        inline
        void Release ()
            NGS_NOTHROW ()
        {
            OpaqueRefcount :: Release ();
        }

        inline
        T * Duplicate () const
            NGS_THROWS ( ErrorMsg )
        {
            return static_cast < T* > ( OpaqueRefcount :: Duplicate () );
        }

    protected:

        // type-punning casts from C++ to C object
        inline C * Self ()
        { return reinterpret_cast < C* > ( this ); }

        inline const C * Self () const
        { return reinterpret_cast < const C* > ( this ); }

        // test "this" pointer for NULL
        inline C * Test ()
        {
            if ( this == 0 )
                throw ErrorMsg ( "message sent to NULL object" );
            return Self ();
        }
        inline const C * Test () const
        {
            if ( this == 0 )
                throw ErrorMsg ( "message sent to NULL object" );
            return Self ();
        }

    public:

        // type-punning cast from C object to C++
        static inline T * Cast ( C * cobj )
        { return reinterpret_cast < T* > ( cobj ); }

        static inline const T * Cast ( const C * cobj )
        { return reinterpret_cast < const T* > ( cobj ); }

    };

} // namespace ngs

#endif // _hpp_ngs_itf_refcount_
