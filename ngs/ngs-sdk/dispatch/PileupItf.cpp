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

#include <ngs/itf/PileupItf.hpp>
#include <ngs/itf/PileupEventItf.hpp>
#include <ngs/itf/StringItf.hpp>
#include <ngs/itf/ErrBlock.hpp>
#include <ngs/itf/VTable.hpp>

#include <ngs/itf/PileupItf.h>

namespace ngs
{
    /*----------------------------------------------------------------------
     * metadata
     */
    extern ItfTok NGS_PileupEvent_v1_tok;
    ItfTok NGS_Pileup_v1_tok ( "NGS_Pileup_v1", NGS_PileupEvent_v1_tok );


    /*----------------------------------------------------------------------
     * access vtable
     */
    static inline
    const NGS_Pileup_v1_vt * Access ( const NGS_VTable * vt )
    {
        const NGS_Pileup_v1_vt * out = static_cast < const NGS_Pileup_v1_vt* >
            ( Cast ( vt, NGS_Pileup_v1_tok ) );
        if ( out == 0 )
            throw ErrorMsg ( "object is not of type NGS_Pileup_v1" );
        return out;
    }


    /*----------------------------------------------------------------------
     * PileupItf
     */

    StringItf * PileupItf :: getReferenceSpec () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Pileup_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Pileup_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ref_spec != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_ref_spec ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    int64_t PileupItf :: getReferencePosition () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Pileup_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Pileup_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ref_pos != 0 );
        int64_t ret  = ( * vt -> get_ref_pos ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    char PileupItf :: getReferenceBase () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Pileup_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Pileup_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ref_base != 0 );
        char ret  = ( * vt -> get_ref_base ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    uint32_t PileupItf :: getPileupDepth () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Pileup_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Pileup_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_pileup_depth != 0 );
        uint32_t ret  = ( * vt -> get_pileup_depth ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    bool PileupItf :: nextPileup ()
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        NGS_Pileup_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Pileup_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> next != 0 );
        bool ret  = ( * vt -> next ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }
}

