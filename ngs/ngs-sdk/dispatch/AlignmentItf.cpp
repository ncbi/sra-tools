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

#include <ngs/itf/AlignmentItf.hpp>
#include <ngs/itf/StringItf.hpp>
#include <ngs/itf/ErrBlock.hpp>
#include <ngs/itf/VTable.hpp>

#include <ngs/itf/AlignmentItf.h>

#include <ngs/Alignment.hpp>

namespace ngs
{
    /*----------------------------------------------------------------------
     * metadata
     */
    extern ItfTok NGS_Fragment_v1_tok;
    ItfTok NGS_Alignment_v1_tok ( "NGS_Alignment_v1", NGS_Fragment_v1_tok );


    /*----------------------------------------------------------------------
     * access vtable
     */
    static inline
    const NGS_Alignment_v1_vt * Access ( const NGS_VTable * vt )
    {
        const NGS_Alignment_v1_vt * out = static_cast < const NGS_Alignment_v1_vt* >
            ( Cast ( vt, NGS_Alignment_v1_tok ) );
        if ( out == 0 )
            throw ErrorMsg ( "object is not of type NGS_Alignment_v1" );
        return out;
    }


    /*----------------------------------------------------------------------
     * AlignmentItf
     */

    StringItf * AlignmentItf :: getAlignmentId () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_id != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_id ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * AlignmentItf :: getReferenceSpec () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ref_spec != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_ref_spec ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    int32_t AlignmentItf :: getMappingQuality () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_map_qual != 0 );
        int32_t ret  = ( * vt -> get_map_qual ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    StringItf * AlignmentItf :: getReferenceBases () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ref_bases != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_ref_bases ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * AlignmentItf :: getReadGroup () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_read_group != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_read_group ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * AlignmentItf :: getReadId () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_read_id != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_read_id ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * AlignmentItf :: getClippedFragmentBases () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_clipped_frag_bases != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_clipped_frag_bases ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * AlignmentItf :: getClippedFragmentQualities () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_clipped_frag_quals != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_clipped_frag_quals ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * AlignmentItf :: getAlignedFragmentBases () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_aligned_frag_bases != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_aligned_frag_bases ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    uint32_t AlignmentItf :: getAlignmentCategory () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> is_primary != 0 );
        bool ret  = ( * vt -> is_primary ) ( self, & err );

        // check for errors
        err . Check ();

        return ret ? Alignment :: primaryAlignment : Alignment :: secondaryAlignment;
    }

    int64_t AlignmentItf :: getAlignmentPosition () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_align_pos != 0 );
        int64_t ret  = ( * vt -> get_align_pos ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    uint64_t AlignmentItf :: getReferencePositionProjectionRange (int64_t ref_pos) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // test for v1.2
        if ( vt -> dad . minor_version < 2 )
            throw ErrorMsg ( "the Alignment interface provided by this NGS engine is too old to support this message" );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ref_pos_projection_range != 0 );
        uint64_t ret  = ( * vt -> get_ref_pos_projection_range ) ( self, & err, ref_pos );

        // check for errors
        err . Check ();

        return ret;
    }

    uint64_t AlignmentItf :: getAlignmentLength () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_align_length != 0 );
        uint64_t ret  = ( * vt -> get_align_length ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    bool AlignmentItf :: getIsReversedOrientation () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_is_reversed != 0 );
        bool ret  = ( * vt -> get_is_reversed ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    int32_t AlignmentItf :: getSoftClip ( uint32_t edge ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_soft_clip != 0 );
        int32_t ret  = ( * vt -> get_soft_clip ) ( self, & err, edge );

        // check for errors
        err . Check ();

        return ret;
    }

    uint64_t AlignmentItf :: getTemplateLength () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_template_len != 0 );
        uint64_t ret  = ( * vt -> get_template_len ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    StringItf * AlignmentItf :: getShortCigar ( bool clipped ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_short_cigar != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_short_cigar ) ( self, & err, clipped );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * AlignmentItf :: getLongCigar ( bool clipped ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_long_cigar != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_long_cigar ) ( self, & err, clipped );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    char AlignmentItf :: getRNAOrientation () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // test for v1.1
        if ( vt -> dad . minor_version < 1 )
            throw ErrorMsg ( "the Alignment interface provided by this NGS engine is too old to support this message" );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_rna_orientation != 0 );
        char orientation  = ( * vt -> get_rna_orientation ) ( self, & err );

        // check for errors
        err . Check ();

        return orientation;
    }

    bool AlignmentItf :: hasMate () const
        NGS_NOTHROW ()
    {
        try
        {
            // the object is really from C
            const NGS_Alignment_v1 * self = Test ();

            // cast vtable to our level
            const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

            // call through C vtable
            ErrBlock err;
            assert ( vt -> has_mate != 0 );
            bool ret  = ( * vt -> has_mate ) ( self, & err );

            // check for errors
            err . Check ();

            return ret;
        }
        catch ( ErrorMsg & x )
        {
            // warn
        }
        catch ( ... )
        {
            // warn
        }

        return false;
    }

    StringItf * AlignmentItf :: getMateAlignmentId () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_mate_id != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_mate_id ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    AlignmentItf * AlignmentItf :: getMateAlignment () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_mate_alignment != 0 );
        NGS_Alignment_v1 * ret  = ( * vt -> get_mate_alignment ) ( self, & err );

        // check for errors
        err . Check ();

        return AlignmentItf :: Cast ( ret );
    }

    StringItf * AlignmentItf :: getMateReferenceSpec () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_mate_ref_spec != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_mate_ref_spec ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    bool AlignmentItf :: getMateIsReversedOrientation () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_mate_is_reversed != 0 );
        bool ret  = ( * vt -> get_mate_is_reversed ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    bool AlignmentItf :: nextAlignment ()
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        NGS_Alignment_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Alignment_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> next != 0 );
        bool ret  = ( * vt -> next ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

}

