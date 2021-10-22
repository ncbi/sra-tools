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

#include <ngs/adapter/ReferenceItf.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/StatisticsItf.hpp>
#include <ngs/adapter/AlignmentItf.hpp>
#include <ngs/adapter/PileupItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * ReferenceItf
     */

    ReferenceItf :: ReferenceItf ()
        : Refcount < ReferenceItf, NGS_Reference_v1 > ( & ivt . dad )
    {
    }

    NGS_String_v1 * CC ReferenceItf :: get_cmn_name ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getCommonName ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC ReferenceItf :: get_canon_name ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getCanonicalName ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool CC ReferenceItf :: is_circular ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            return self -> getIsCircular ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool CC ReferenceItf :: is_local ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            return self -> getIsLocal ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint64_t CC ReferenceItf :: get_length ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            return self -> getLength ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC ReferenceItf :: get_ref_bases ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getReferenceBases ( offset, length );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC ReferenceItf :: get_ref_chunk ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getReferenceChunk ( offset, length );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint64_t CC ReferenceItf :: get_alignment_count ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
            bool wants_primary, bool wants_secondary )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            return self -> getAlignmentCount ( wants_primary, wants_secondary );
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Alignment_v1 * CC ReferenceItf :: get_alignment ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
            const char * alignmentId )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            AlignmentItf * val = self -> getAlignment ( alignmentId );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Alignment_v1 * CC ReferenceItf :: get_alignments ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
                                           bool wants_primary, bool wants_secondary )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            AlignmentItf * val = self -> getAlignments ( wants_primary, wants_secondary );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Alignment_v1 * CC ReferenceItf :: get_align_slice ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
                                            int64_t start, uint64_t length, bool wants_primary, bool wants_secondary )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            AlignmentItf * val = self -> getAlignmentSlice ( start, length, wants_primary, wants_secondary );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Alignment_v1 * CC ReferenceItf :: get_filtered_align_slice ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
                                            int64_t start, uint64_t length, uint32_t flags, int32_t map_qual )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            AlignmentItf * val = self -> getFilteredAlignmentSlice ( start, length, flags, map_qual );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Pileup_v1 * CC ReferenceItf :: get_pileups ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
                                     bool wants_primary, bool wants_secondary )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            PileupItf * val = self -> getPileups ( wants_primary, wants_secondary );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Pileup_v1 * CC ReferenceItf :: get_filtered_pileups ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
        uint32_t flags, int32_t map_qual )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            PileupItf * val = self -> getFilteredPileups ( flags, map_qual );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Pileup_v1 * CC ReferenceItf :: get_pileup_slice ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
                                          int64_t start, uint64_t length, bool wants_primary, bool wants_secondary )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            PileupItf * val = self -> getPileupSlice ( start, length, wants_primary, wants_secondary );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Pileup_v1 * CC ReferenceItf :: get_filtered_pileup_slice ( const NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err,
        int64_t start, uint64_t length, uint32_t flags, int32_t map_qual )
    {
        const ReferenceItf * self = Self ( iself );
        try
        {
            PileupItf * val = self -> getFilteredPileupSlice ( start, length, flags, map_qual );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool CC ReferenceItf :: next ( NGS_Reference_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        ReferenceItf * self = Self ( iself );
        try
        {
            return self -> nextReference ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    NGS_Reference_v1_vt ReferenceItf :: ivt =
    {
        {
            "ngs_adapt::ReferenceItf",
            "NGS_Reference_v1",
            4,
            & OpaqueRefcount :: ivt . dad
        },

        // 1.0
        get_cmn_name,
        get_canon_name,
        is_circular,
        get_length,
        get_ref_bases,
        get_ref_chunk,
        get_alignment,
        get_alignments,
        get_align_slice,
        get_pileups,
        get_pileup_slice,
        next,

        // 1.1
        get_filtered_pileups,
        get_filtered_pileup_slice,

        // 1.2
        get_alignment_count,

        // 1.3 interface
        NULL,// get_filtered_alignments (not exposed via ngs::Reference)
        get_filtered_align_slice,

        // 1.4
        is_local
    };

} // namespace ngs_adapt
