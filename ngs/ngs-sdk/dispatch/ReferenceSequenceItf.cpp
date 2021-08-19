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

#include <ngs/itf/ReferenceSequenceItf.hpp>
#include <ngs/itf/StringItf.hpp>
#include <ngs/itf/ErrBlock.hpp>
#include <ngs/itf/VTable.hpp>

#include <ngs/itf/ReferenceSequenceItf.h>

#include <ngs/Alignment.hpp>

namespace ngs
{
    /*----------------------------------------------------------------------
     * metadata
     */
    extern ItfTok NGS_Refcount_v1_tok;
    ItfTok NGS_ReferenceSequence_v1_tok ( "NGS_ReferenceSequence_v1", NGS_Refcount_v1_tok );


    /*----------------------------------------------------------------------
     * access vtable
     */
    static inline
    const NGS_ReferenceSequence_v1_vt * Access ( const NGS_VTable * vt )
    {
        const NGS_ReferenceSequence_v1_vt * out = static_cast < const NGS_ReferenceSequence_v1_vt* >
            ( Cast ( vt, NGS_ReferenceSequence_v1_tok ) );
        if ( out == 0 )
            throw ErrorMsg ( "object is not of type NGS_ReferenceSequence_v1" );
        return out;
    }


    /*----------------------------------------------------------------------
     * ReferenceSequenceItf
     */

    StringItf * ReferenceSequenceItf :: getCanonicalName () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReferenceSequence_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReferenceSequence_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_canon_name != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_canon_name ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    bool ReferenceSequenceItf :: getIsCircular () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReferenceSequence_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReferenceSequence_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> is_circular != 0 );
        bool ret  = ( * vt -> is_circular ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    uint64_t ReferenceSequenceItf :: getLength () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReferenceSequence_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReferenceSequence_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_length != 0 );
        uint64_t ret  = ( * vt -> get_length ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    StringItf * ReferenceSequenceItf :: getReferenceBases ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getReferenceBases ( offset, -1 );
    }

    StringItf * ReferenceSequenceItf :: getReferenceBases ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReferenceSequence_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReferenceSequence_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ref_bases != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_ref_bases ) ( self, & err, offset, length );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * ReferenceSequenceItf :: getReferenceChunk ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getReferenceChunk ( offset, -1 );
    }

    StringItf * ReferenceSequenceItf :: getReferenceChunk ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReferenceSequence_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReferenceSequence_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ref_chunk != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_ref_chunk ) ( self, & err, offset, length );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }
}

