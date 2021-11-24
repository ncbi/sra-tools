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

#ifndef _hpp_ngs_adapt_refcount_
#define _hpp_ngs_adapt_refcount_

#ifndef _h_ngs_itf_refcount_
#include <ngs/itf/Refcount.h>
#endif

#ifndef _h_ngs_adapt_defs_
#include <ngs/adapter/defs.h>
#endif

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * OpaqueRefcount
     */
    class OpaqueRefcount
    {
    public:

        // interface
        virtual void Release ();
        virtual void * Duplicate () const;

    public:

        // C++ support
        virtual ~ OpaqueRefcount ();

    protected:

        // not directly instantiable
        OpaqueRefcount ( const OpaqueRefcount & obj );

    protected:

        // support for C vtable
        OpaqueRefcount ( const NGS_VTable * vt );
        static NGS_Refcount_v1_vt ivt;

        inline void * offset_this ()
        { return ( void* ) & cobj; }

        inline const void * offset_this () const
        { return ( const void* ) & cobj; }

        static inline void * offset_cobj ( void * obj )
        {
            char * cobj = ( char* ) obj;
            size_t offset = ( size_t ) & ( ( OpaqueRefcount* ) 8 ) -> cobj - 8;
            return ( void * ) ( cobj - offset );
        }
        static inline const void * offset_cobj ( const void * obj )
        {
            const char * cobj = ( const char* ) obj;
            size_t offset = ( size_t ) & ( ( OpaqueRefcount* ) 8 ) -> cobj - 8;
            return ( const void * ) ( cobj - offset );
        }

    private:

        static void CC release ( NGS_Refcount_v1 * self, NGS_ErrBlock_v1 * err );
        static void * CC duplicate ( const NGS_Refcount_v1 * self, NGS_ErrBlock_v1 * err );
        NGS_Refcount_v1 cobj;


    private:

        // member variables
        mutable atomic32_t refcount;
    };

    /*----------------------------------------------------------------------
     * Refcount
     */
    template < class T, class C >
    class Refcount : protected OpaqueRefcount
    {
    protected:

        Refcount ( const NGS_VTable * vt )
            : OpaqueRefcount ( vt )
        {
        }

    public:

        inline C * Cast ()
        { return static_cast < C* > ( OpaqueRefcount :: offset_this () ); }

        inline const C * Cast () const
        { return static_cast < const C* > ( OpaqueRefcount :: offset_this () ); }

        // assistance for C objects
        static inline T * Self ( C * obj )
        { return static_cast < T* > ( OpaqueRefcount :: offset_cobj ( obj ) ); }

        static inline const T * Self ( const C * obj )
        { return static_cast < const T* > ( OpaqueRefcount :: offset_cobj ( obj ) ); }
    };

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_refcount_
