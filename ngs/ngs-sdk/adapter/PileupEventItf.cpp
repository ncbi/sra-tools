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

#include <ngs/adapter/PileupEventItf.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/AlignmentItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * PileupEventItf
     */

    PileupEventItf :: PileupEventItf ( const NGS_VTable * vt )
        : Refcount < PileupEventItf, NGS_PileupEvent_v1 > ( vt )
    {
    }

    int32_t CC PileupEventItf :: get_map_qual ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
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

    NGS_String_v1 * CC PileupEventItf :: get_align_id ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
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

    int64_t CC PileupEventItf :: get_align_pos ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
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

    int64_t CC PileupEventItf :: get_first_align_pos ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
        try
        {
            return self -> getFirstAlignmentPosition ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    int64_t CC PileupEventItf :: get_last_align_pos ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
        try
        {
            return self -> getLastAlignmentPosition ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint32_t CC PileupEventItf :: get_event_type ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
        try
        {
            return self -> getEventType ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    char CC PileupEventItf :: get_align_base ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
        try
        {
            return self -> getAlignmentBase ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    char CC PileupEventItf :: get_align_qual ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
        try
        {
            return self -> getAlignmentQuality ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC PileupEventItf :: get_ins_bases ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getInsertionBases ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC PileupEventItf :: get_ins_quals ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getInsertionQualities ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint32_t CC PileupEventItf :: get_rpt_count ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
        try
        {
            return self -> getEventRepeatCount ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint32_t CC PileupEventItf :: get_indel_type ( const NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupEventItf * self = Self ( iself );
        try
        {
            return self -> getEventIndelType ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool PileupEventItf :: next ( NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        PileupEventItf * self = Self ( iself );
        try
        {
            return self -> nextPileupEvent ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    void PileupEventItf :: reset ( NGS_PileupEvent_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        PileupEventItf * self = Self ( iself );
        try
        {
            self -> resetPileupEvent ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }
    }

    NGS_PileupEvent_v1_vt PileupEventItf :: ivt =
    {
        {
            "ngs_adapt::PileupEventItf",
            "NGS_PileupEvent_v1",
            0,
            & OpaqueRefcount :: ivt . dad
        },

        get_map_qual,
        get_align_id,
        get_align_pos,
        get_first_align_pos,
        get_last_align_pos,
        get_event_type,
        get_align_base,
        get_align_qual,
        get_ins_bases,
        get_ins_quals,
        get_rpt_count,
        get_indel_type,
        next,
        reset
    };

} // namespace ngs_adapt
