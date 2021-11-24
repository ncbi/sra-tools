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

#include <ngs/adapter/AlignmentItf.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * AlignmentItf
     */

    AlignmentItf :: AlignmentItf ()
        : FragmentItf ( & ivt . dad )
    {
    }

    bool AlignmentItf :: nextFragment ()
    {
        throw ErrorMsg ( "INTERNAL ERROR: nextFragment message on Alignment" );
    }

    NGS_String_v1 * CC AlignmentItf :: get_id ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getAlignmentId ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC AlignmentItf :: get_ref_spec ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getReferenceSpec ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    int32_t CC AlignmentItf :: get_map_qual ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> getMappingQuality ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC AlignmentItf :: get_ref_bases ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getReferenceBases ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC AlignmentItf :: get_read_group ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getReadGroup ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC AlignmentItf :: get_read_id ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getReadId ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC AlignmentItf :: get_clipped_frag_bases ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getClippedFragmentBases ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC AlignmentItf :: get_clipped_frag_quals ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getClippedFragmentQualities ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC AlignmentItf :: get_aligned_frag_bases ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getAlignedFragmentBases ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool CC AlignmentItf :: is_primary ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> isPrimary ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    int64_t CC AlignmentItf :: get_align_pos ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> getAlignmentPosition ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint64_t CC AlignmentItf :: get_ref_pos_projection_range ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err, int64_t ref_pos )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> getReferencePositionProjectionRange ( ref_pos );
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint64_t CC AlignmentItf :: get_align_length ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> getAlignmentLength ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool CC AlignmentItf :: get_is_reversed ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> getIsReversedOrientation ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    int32_t CC AlignmentItf :: get_soft_clip ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err, uint32_t edge )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> getSoftClip ( edge );
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint64_t CC AlignmentItf :: get_template_len ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> getTemplateLength ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC AlignmentItf :: get_short_cigar ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err, bool clipped )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getShortCigar ( clipped );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC AlignmentItf :: get_long_cigar ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err, bool clipped )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getLongCigar ( clipped );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    char CC AlignmentItf :: get_rna_orientation ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> getRNAOrientation ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    bool CC AlignmentItf :: has_mate ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> hasMate ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    NGS_String_v1 * CC AlignmentItf :: get_mate_id ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getMateAlignmentId ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Alignment_v1 * CC AlignmentItf :: get_mate_alignment ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            AlignmentItf * val = self -> getMateAlignment ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC AlignmentItf :: get_mate_ref_spec ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getMateReferenceSpec ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool CC AlignmentItf :: get_mate_is_reversed ( const NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const AlignmentItf * self = Self ( iself );
        try
        {
            return self -> getMateIsReversedOrientation ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    bool CC AlignmentItf :: next ( NGS_Alignment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        AlignmentItf * self = Self ( iself );
        try
        {
            return self -> nextAlignment ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    NGS_Alignment_v1_vt AlignmentItf :: ivt =
    {
        {
            "ngs_adapt::AlignmentItf",
            "NGS_Alignment_v1",
            1,
            & FragmentItf :: ivt . dad
        },

        // v1.0
        get_id,
        get_ref_spec,
        get_map_qual,
        get_ref_bases,
        get_read_group,
        get_read_id,
        get_clipped_frag_bases,
        get_clipped_frag_quals,
        get_aligned_frag_bases,
        is_primary,
        get_align_pos,
        get_align_length,
        get_is_reversed,
        get_soft_clip,
        get_template_len,
        get_short_cigar,
        get_long_cigar,
        has_mate,
        get_mate_id,
        get_mate_alignment,
        get_mate_ref_spec,
        get_mate_is_reversed,
        next,

        // v1.1
        get_rna_orientation,

        // v1.2
        get_ref_pos_projection_range
    };

} // namespace ngs_adapt
