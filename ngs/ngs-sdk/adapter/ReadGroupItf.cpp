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

#include <ngs/adapter/ReadGroupItf.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/StatisticsItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * ReadGroupItf
     */

    ReadGroupItf :: ReadGroupItf ()
        : Refcount < ReadGroupItf, NGS_ReadGroup_v1 > ( & ivt . dad )
    {
    }

    NGS_String_v1 * CC ReadGroupItf :: get_name ( const NGS_ReadGroup_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReadGroupItf * self = Self ( iself );
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

    NGS_Statistics_v1 * CC ReadGroupItf :: get_stats ( const NGS_ReadGroup_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReadGroupItf * self = Self ( iself );
        try
        {
            StatisticsItf * val = self -> getStatistics ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool CC ReadGroupItf :: next ( NGS_ReadGroup_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        ReadGroupItf * self = Self ( iself );
        try
        {
            return self -> nextReadGroup ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    NGS_ReadGroup_v1_vt ReadGroupItf :: ivt =
    {
        {
            "ngs_adapt::ReadGroupItf",
            "NGS_ReadGroup_v1",
            0,
            & OpaqueRefcount :: ivt . dad
        },

        get_name,
        get_stats,
        next
    };

} // namespace ngs_adapt
