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

#include <ngs/itf/PileupEventItf.hpp>
#include <ngs/itf/AlignmentItf.hpp>
#include <ngs/itf/StringItf.hpp>
#include <ngs/itf/ErrBlock.hpp>
#include <ngs/itf/VTable.hpp>

#include <ngs/itf/PileupEventItf.h>

namespace ngs
{
    /*----------------------------------------------------------------------
     * metadata
     */
    extern ItfTok NGS_Refcount_v1_tok;
    ItfTok NGS_PileupEvent_v1_tok ( "NGS_PileupEvent_v1", NGS_Refcount_v1_tok );


    /*----------------------------------------------------------------------
     * access vtable
     */
    static inline
    const NGS_PileupEvent_v1_vt * Access ( const NGS_VTable * vt )
    {
        const NGS_PileupEvent_v1_vt * out = static_cast < const NGS_PileupEvent_v1_vt* >
            ( Cast ( vt, NGS_PileupEvent_v1_tok ) );
        if ( out == 0 )
            throw ErrorMsg ( "object is not of type NGS_PileupEvent_v1" );
        return out;
    }


    /*----------------------------------------------------------------------
     * PileupEventItf
     */

    int32_t PileupEventItf :: getMappingQuality () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_map_qual != 0 );
        int32_t ret  = ( * vt -> get_map_qual ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    StringItf * PileupEventItf :: getAlignmentId () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_align_id != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_align_id ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    int64_t PileupEventItf :: getAlignmentPosition () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_align_pos != 0 );
        int64_t ret  = ( * vt -> get_align_pos ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    int64_t PileupEventItf :: getFirstAlignmentPosition () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_first_align_pos != 0 );
        int64_t ret  = ( * vt -> get_first_align_pos ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    int64_t PileupEventItf :: getLastAlignmentPosition () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_last_align_pos != 0 );
        int64_t ret  = ( * vt -> get_last_align_pos ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    uint32_t PileupEventItf :: getEventType () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_event_type != 0 );
        uint32_t ret  = ( * vt -> get_event_type ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    char PileupEventItf :: getAlignmentBase () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_align_base != 0 );
        char ret  = ( * vt -> get_align_base ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    char PileupEventItf :: getAlignmentQuality () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_align_qual != 0 );
        char ret  = ( * vt -> get_align_qual ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    StringItf * PileupEventItf :: getInsertionBases () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ins_bases != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_ins_bases ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * PileupEventItf :: getInsertionQualities () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ins_quals != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_ins_quals ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    uint32_t PileupEventItf :: getEventRepeatCount () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_rpt_count != 0 );
        uint32_t ret  = ( * vt -> get_rpt_count ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    uint32_t PileupEventItf :: getEventIndelType () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_indel_type != 0 );
        uint32_t ret  = ( * vt -> get_indel_type ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    bool PileupEventItf :: nextPileupEvent ()
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> next != 0 );
        bool ret  = ( * vt -> next ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    void PileupEventItf :: resetPileupEvent ()
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        NGS_PileupEvent_v1 * self = Test ();

        // cast vtable to our level
        const NGS_PileupEvent_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> reset != 0 );
        ( * vt -> reset ) ( self, & err );

        // check for errors
        err . Check ();
    }
}

