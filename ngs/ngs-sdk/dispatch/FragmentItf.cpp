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

#include <ngs/itf/FragmentItf.hpp>
#include <ngs/itf/StringItf.hpp>
#include <ngs/itf/ErrBlock.hpp>
#include <ngs/itf/VTable.hpp>

#include <ngs/itf/FragmentItf.h>

namespace ngs
{
    /*----------------------------------------------------------------------
     * metadata
     */
    extern ItfTok NGS_Refcount_v1_tok;
    ItfTok NGS_Fragment_v1_tok ( "NGS_Fragment_v1", NGS_Refcount_v1_tok );


    /*----------------------------------------------------------------------
     * access vtable
     */
    static inline
    const NGS_Fragment_v1_vt * Access ( const NGS_VTable * vt )
    {
        const NGS_Fragment_v1_vt * out = static_cast < const NGS_Fragment_v1_vt* >
            ( Cast ( vt, NGS_Fragment_v1_tok ) );
        if ( out == 0 )
            throw ErrorMsg ( "object is not of type NGS_Fragment_v1" );
        return out;
    }

    /*----------------------------------------------------------------------
     * FragmentItf
     */

    StringItf * FragmentItf :: getFragmentId () const
            NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Fragment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Fragment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_id != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_id ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * FragmentItf :: getFragmentBases () const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getFragmentBases ( 0, -1 );
    }

    StringItf * FragmentItf :: getFragmentBases ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getFragmentBases ( offset, -1 );
    }

    StringItf * FragmentItf :: getFragmentBases ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Fragment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Fragment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_bases != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_bases ) ( self, & err, offset, length );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * FragmentItf :: getFragmentQualities () const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getFragmentQualities ( 0, -1 );
    }

    StringItf * FragmentItf :: getFragmentQualities ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getFragmentQualities ( offset, -1 );
    }

    StringItf * FragmentItf :: getFragmentQualities ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Fragment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Fragment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_quals != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_quals ) ( self, & err, offset, length );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    bool FragmentItf :: nextFragment ()
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        NGS_Fragment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Fragment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> next != 0 );
        bool ret  = ( * vt -> next ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    bool FragmentItf :: isPaired () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Fragment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Fragment_v1_vt * vt = Access ( self -> vt );

        // test for v1.1
        if ( vt -> dad . minor_version < 1 )
            throw ErrorMsg ( "the Fragment interface provided by this NGS engine is too old to support this message" );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> is_paired != 0 );
        bool ret = ( * vt -> is_paired ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    bool FragmentItf :: isAligned () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Fragment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Fragment_v1_vt * vt = Access ( self -> vt );

        // test for v1.1
        if ( vt -> dad . minor_version < 1 )
            throw ErrorMsg ( "the Fragment interface provided by this NGS engine is too old to support this message" );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> is_aligned != 0 );
        bool ret = ( * vt -> is_aligned ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

} // namespace ngs
