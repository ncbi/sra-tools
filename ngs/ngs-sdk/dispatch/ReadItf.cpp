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

#include <ngs/itf/ReadItf.hpp>
#include <ngs/itf/StringItf.hpp>
#include <ngs/itf/ErrBlock.hpp>
#include <ngs/itf/VTable.hpp>

#include <ngs/itf/ReadItf.h>

namespace ngs
{
    /*----------------------------------------------------------------------
     * metadata
     */
    extern ItfTok NGS_Fragment_v1_tok;
    ItfTok NGS_Read_v1_tok ( "NGS_Read_v1", NGS_Fragment_v1_tok );


    /*----------------------------------------------------------------------
     * access vtable
     */
    static inline
    const NGS_Read_v1_vt * Access ( const NGS_VTable * vt )
    {
        const NGS_Read_v1_vt * out = static_cast < const NGS_Read_v1_vt* >
            ( Cast ( vt, NGS_Read_v1_tok ) );
        if ( out == 0 )
            throw ErrorMsg ( "object is not of type NGS_Read_v1" );
        return out;
    }

    /*----------------------------------------------------------------------
     * ReadItf
     */

    StringItf * ReadItf :: getReadId () const
            NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Read_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Read_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_id != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_id ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    uint32_t ReadItf :: getNumFragments () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Read_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Read_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_num_frags != 0 );
        uint32_t ret  = ( * vt -> get_num_frags ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    bool ReadItf :: fragmentIsAligned ( uint32_t fragIdx ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Read_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Read_v1_vt * vt = Access ( self -> vt );

        // test for v1.1
        if ( vt -> dad . minor_version < 1 )
            throw ErrorMsg ( "the Read interface provided by this NGS engine is too old to support this message" );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> frag_is_aligned != 0 );
        bool ret  = ( * vt -> frag_is_aligned ) ( self, & err, fragIdx );

        // check for errors
        err . Check ();

        return ret;
    }

    uint32_t ReadItf :: getReadCategory () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Read_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Read_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_category != 0 );
        uint32_t ret  = ( * vt -> get_category ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    StringItf * ReadItf :: getReadGroup () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Read_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Read_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_read_group != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_read_group ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * ReadItf :: getReadName () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Read_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Read_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_name != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_name ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * ReadItf :: getReadBases () const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getReadBases ( 0, -1 );
    }

    StringItf * ReadItf :: getReadBases ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getReadBases ( offset, -1 );
    }

    StringItf * ReadItf :: getReadBases ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Read_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Read_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_bases != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_bases ) ( self, & err, offset, length );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * ReadItf :: getReadQualities () const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getReadQualities ( 0, -1 );
    }

    StringItf * ReadItf :: getReadQualities ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getReadQualities ( offset, -1 );
    }

    StringItf * ReadItf :: getReadQualities ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Read_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Read_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_quals != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_quals ) ( self, & err, offset, length );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    bool ReadItf :: nextRead ()
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        NGS_Read_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Read_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> next != 0 );
        bool ret  = ( * vt -> next ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

} // namespace ngs
