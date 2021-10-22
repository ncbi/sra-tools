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

#include <ngs/adapter/PileupItf.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/PileupEventItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * PileupItf
     */

    PileupItf :: PileupItf ()
        : PileupEventItf ( & ivt . dad )
    {
    }

    NGS_String_v1 * CC PileupItf :: get_ref_spec ( const NGS_Pileup_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupItf * self = Self ( iself );
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

    int64_t CC PileupItf :: get_ref_pos ( const NGS_Pileup_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupItf * self = Self ( iself );
        try
        {
            return self -> getReferencePosition ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    char CC PileupItf :: get_ref_base ( const NGS_Pileup_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupItf * self = Self ( iself );
        try
        {
            return self -> getReferenceBase ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint32_t CC PileupItf :: get_pileup_depth ( const NGS_Pileup_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const PileupItf * self = Self ( iself );
        try
        {
            return self -> getPileupDepth ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool PileupItf :: next ( NGS_Pileup_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        PileupItf * self = Self ( iself );
        try
        {
            return self -> nextPileup ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    NGS_Pileup_v1_vt PileupItf :: ivt =
    {
        {
            "ngs_adapt::PileupItf",
            "NGS_Pileup_v1",
            0,
            & PileupEventItf :: ivt . dad
        },

        get_ref_spec,
        get_ref_pos,
        get_ref_base,
        get_pileup_depth,
        next
    };

} // namespace ngs_adapt
