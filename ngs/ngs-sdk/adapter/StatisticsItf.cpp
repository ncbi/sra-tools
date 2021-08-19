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

#include <ngs/adapter/StatisticsItf.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * StatisticsItf
     */

    StatisticsItf :: StatisticsItf ()
        : Refcount < StatisticsItf, NGS_Statistics_v1 > ( & ivt . dad )
    {
    }

    uint32_t CC StatisticsItf :: get_type ( const NGS_Statistics_v1 * iself, NGS_ErrBlock_v1 * err, const char * path )
    {
        const StatisticsItf * self = Self ( iself );
        try
        {
            return self -> getValueType ( path );
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC StatisticsItf :: as_string ( const NGS_Statistics_v1 * iself, NGS_ErrBlock_v1 * err, const char * path )
    {
        const StatisticsItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getAsString ( path );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    int64_t CC StatisticsItf :: as_I64 ( const NGS_Statistics_v1 * iself, NGS_ErrBlock_v1 * err, const char * path )
    {
        const StatisticsItf * self = Self ( iself );
        try
        {
            return self -> getAsI64 ( path );
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint64_t CC StatisticsItf :: as_U64 ( const NGS_Statistics_v1 * iself, NGS_ErrBlock_v1 * err, const char * path )
    {
        const StatisticsItf * self = Self ( iself );
        try
        {
            return self -> getAsU64 ( path );
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    double CC StatisticsItf :: as_F64 ( const NGS_Statistics_v1 * iself, NGS_ErrBlock_v1 * err, const char * path )
    {
        const StatisticsItf * self = Self ( iself );
        try
        {
            return self -> getAsDouble ( path );
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0.0;
    }

    NGS_String_v1 * CC StatisticsItf :: next_path ( const NGS_Statistics_v1 * iself, NGS_ErrBlock_v1 * err, const char * path )
    {
        const StatisticsItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> nextPath ( path );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_Statistics_v1_vt StatisticsItf :: ivt =
    {
        {
            "ngs_adapt::StatisticsItf",
            "NGS_Statistics_v1",
            0,
            & OpaqueRefcount :: ivt . dad
        },

        get_type,
        as_string,
        as_I64,
        as_U64,
        as_F64,
        next_path
    };

} // namespace ngs_adapt
