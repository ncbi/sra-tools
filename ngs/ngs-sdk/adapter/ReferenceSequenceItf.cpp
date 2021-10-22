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

#include <ngs/adapter/ReferenceSequenceItf.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * ReferenceSequenceItf
     */

    ReferenceSequenceItf :: ReferenceSequenceItf ()
        : Refcount < ReferenceSequenceItf, NGS_ReferenceSequence_v1 > ( & ivt . dad )
    {
    }

    NGS_String_v1 * CC ReferenceSequenceItf :: get_canon_name ( const NGS_ReferenceSequence_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReferenceSequenceItf * self = Self ( iself );
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

    bool CC ReferenceSequenceItf :: is_circular ( const NGS_ReferenceSequence_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReferenceSequenceItf * self = Self ( iself );
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

    uint64_t CC ReferenceSequenceItf :: get_length ( const NGS_ReferenceSequence_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReferenceSequenceItf * self = Self ( iself );
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

    NGS_String_v1 * CC ReferenceSequenceItf :: get_ref_bases ( const NGS_ReferenceSequence_v1 * iself, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length )
    {
        const ReferenceSequenceItf * self = Self ( iself );
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

    NGS_String_v1 * CC ReferenceSequenceItf :: get_ref_chunk ( const NGS_ReferenceSequence_v1 * iself, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length )
    {
        const ReferenceSequenceItf * self = Self ( iself );
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

    NGS_ReferenceSequence_v1_vt ReferenceSequenceItf :: ivt =
    {
        {
            "ngs_adapt::ReferenceSequenceItf",
            "NGS_ReferenceSequence_v1",
            1,
            & OpaqueRefcount :: ivt . dad
        },

        // 1.0
        get_canon_name,
        is_circular,
        get_length,
        get_ref_bases,
        get_ref_chunk,
    };

} // namespace ngs_adapt
