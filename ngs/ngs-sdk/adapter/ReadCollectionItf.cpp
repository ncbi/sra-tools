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

#include <ngs/adapter/ReadCollectionItf.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ReadGroupItf.hpp>
#include <ngs/adapter/ReferenceItf.hpp>
#include <ngs/adapter/AlignmentItf.hpp>
#include <ngs/adapter/ReadItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * ReadCollectionItf
     */

    ReadCollectionItf :: ReadCollectionItf ()
        : Refcount < ReadCollectionItf, NGS_ReadCollection_v1 > ( & ivt . dad )
    {
    }


    NGS_String_v1 * CC ReadCollectionItf :: get_name ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReadCollectionItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getName ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_ReadGroup_v1 * CC ReadCollectionItf :: get_read_groups ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReadCollectionItf * self = Self ( iself );
        try
        {
            ReadGroupItf * val = self -> getReadGroups ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_ReadGroup_v1 * CC ReadCollectionItf :: get_read_group ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err,
            const char * spec )
    {
        const ReadCollectionItf * self = Self ( iself );
        try
        {
            ReadGroupItf * val = self -> getReadGroup ( spec );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Reference_v1 * CC ReadCollectionItf :: get_references ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReadCollectionItf * self = Self ( iself );
        try
        {
            ReferenceItf * val = self -> getReferences ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Reference_v1 * CC ReadCollectionItf :: get_reference ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err,
            const char * spec )
    {
        const ReadCollectionItf * self = Self ( iself );
        try
        {
            ReferenceItf * val = self -> getReference ( spec );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Alignment_v1 * CC ReadCollectionItf :: get_alignment ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err,
            const char * alignmentId )
    {
        const ReadCollectionItf * self = Self ( iself );
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

    NGS_Alignment_v1 * CC ReadCollectionItf :: get_alignments ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err,
            bool wants_primary, bool wants_secondary )
    {
        const ReadCollectionItf * self = Self ( iself );
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

    uint64_t CC ReadCollectionItf :: get_align_count ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err,
            bool wants_primary, bool wants_secondary )
    {
        const ReadCollectionItf * self = Self ( iself );
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

    NGS_Alignment_v1 * CC ReadCollectionItf :: get_align_range ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err,
            uint64_t first, uint64_t count, bool wants_primary, bool wants_secondary )
    {
        const ReadCollectionItf * self = Self ( iself );
        try
        {
            AlignmentItf * val = self -> getAlignmentRange ( first, count, wants_primary, wants_secondary );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Read_v1 * CC ReadCollectionItf :: get_read ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err,
            const char * readId )
    {
        const ReadCollectionItf * self = Self ( iself );
        try
        {
            ReadItf * val = self -> getRead ( readId );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Read_v1 * CC ReadCollectionItf :: get_reads ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err,
            bool wants_full, bool wants_partial, bool wants_unaligned )
    {
        const ReadCollectionItf * self = Self ( iself );
        try
        {
            ReadItf * val = self -> getReads ( wants_full, wants_partial, wants_unaligned );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint64_t CC ReadCollectionItf :: get_read_count ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err,
            bool wants_full, bool wants_partial, bool wants_unaligned )
    {
        const ReadCollectionItf * self = Self ( iself );
        try
        {
            return self -> getReadCount ( wants_full, wants_partial, wants_unaligned );
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Read_v1 * CC ReadCollectionItf :: get_read_range ( const NGS_ReadCollection_v1 * iself, NGS_ErrBlock_v1 * err,
            uint64_t first, uint64_t count, bool wants_full, bool wants_partial, bool wants_unaligned )
    {
        const ReadCollectionItf * self = Self ( iself );
        try
        {
            ReadItf * val = self -> getReadRange ( first, count, wants_full, wants_partial, wants_unaligned );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_ReadCollection_v1_vt ReadCollectionItf :: ivt =
    {
        {
            "ngs_adapt::ReadCollectionItf",
            "NGS_ReadCollection_v1",
            0,
            & OpaqueRefcount :: ivt . dad
        },

        get_name,
        get_read_groups,
        get_read_group,
        get_references,
        get_reference,
        get_alignment,
        get_alignments,
        get_align_count,
        get_align_range,
        get_read,
        get_reads,
        get_read_count,
        get_read_range
    };

} // namespace ngs_adapt
