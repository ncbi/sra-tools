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

#include <ngs/adapter/FragmentItf.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * FragmentItf
     */

    FragmentItf :: FragmentItf ( const NGS_VTable * vt )
        : Refcount < FragmentItf, NGS_Fragment_v1 > ( vt )
    {
    }

    NGS_String_v1 * CC FragmentItf :: get_id ( const NGS_Fragment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const FragmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getFragmentId ();
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC FragmentItf :: get_bases ( const NGS_Fragment_v1 * iself, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length )
    {
        const FragmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getFragmentBases ( offset, length );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC FragmentItf :: get_quals ( const NGS_Fragment_v1 * iself, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length )
    {
        const FragmentItf * self = Self ( iself );
        try
        {
            StringItf * val = self -> getFragmentQualities ( offset, length );
            return val -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    bool FragmentItf :: next ( NGS_Fragment_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        FragmentItf * self = Self ( iself );
        try
        {
            return self -> nextFragment ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return false;
    }

    NGS_Fragment_v1_vt FragmentItf :: ivt =
    {
        {
            "ngs_adapt::FragmentItf",
            "NGS_Fragment_v1",
            0,
            & OpaqueRefcount :: ivt . dad
        },

        get_id,
        get_bases,
        get_quals,
        next
    };

} // namespace ngs_adapt
