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

#include <ngs/adapter/ReadItf.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * ReadItf
     */

    ReadItf :: ReadItf ()
        : FragmentItf ( & ivt . dad )
    {
    }

    NGS_String_v1 * CC ReadItf :: get_id ( const NGS_Read_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReadItf * self = Self ( iself );
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

    uint32_t CC ReadItf :: get_num_frags ( const NGS_Read_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReadItf * self = Self ( iself );
        try
        {
            return self -> getNumFragments ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    uint32_t CC ReadItf :: get_category ( const NGS_Read_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReadItf * self = Self ( iself );
        try
        {
            return self -> getReadCategory ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC ReadItf :: get_read_group ( const NGS_Read_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReadItf * self = Self ( iself );
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

    NGS_String_v1 * CC ReadItf :: get_name ( const NGS_Read_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const ReadItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getReadName ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC ReadItf :: get_bases ( const NGS_Read_v1 * iself, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length )
    {
        const ReadItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getReadBases ( offset, length );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC ReadItf :: get_quals ( const NGS_Read_v1 * iself, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length )
    {
        const ReadItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getReadQualities ( offset, length );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool CC ReadItf :: next ( NGS_Read_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        ReadItf * self = Self ( iself );
        try
        {
            return self -> nextRead ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    NGS_Read_v1_vt ReadItf :: ivt =
    {
        {
            "ngs_adapt::ReadItf",
            "NGS_Read_v1",
            0,
            & FragmentItf :: ivt . dad
        },

        get_id,
        get_num_frags,
        get_category,
        get_read_group,
        get_name,
        get_bases,
        get_quals,
        next
    };

} // namespace ngs_adapt
