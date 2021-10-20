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

#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ErrorMsg.hpp>

#include "ErrBlock.hpp"

namespace ngs_adapt
{
    /*----------------------------------------------------------------------
     * StringItf
     *  a simple reference to textual data
     *  main purpose is to avoid copying
     *  provides a cast operator to create a language-specific String
     */

    /* data
     *  return character string
     *  NOT necessarily NUL-terminated
     */
    const char * StringItf :: data () const
    {
        return str;
    }

    /* size
     *   return size of string in bytes
     */
    size_t StringItf :: size () const
    {
        return sz;
    }

    /* substr
     *  create a substring of the original
     *  almost certainly NOT NUL-terminated
     *  "offset" is zero-based
     */
    StringItf * StringItf :: substr ( size_t offset, size_t size ) const
    {
		return 0;
    }

    // C++ support
    StringItf :: StringItf ( const char * data, size_t size )
        : Refcount < StringItf, NGS_String_v1 > ( & ivt . dad )
        , str ( data )
        , sz ( size )
    {
        assert ( sz == 0 || str != 0 );
    }

    StringItf :: ~ StringItf ()
    {
        str = "<zombie-string>";
        sz = 0;
    }

    const char * CC StringItf :: data_dispatch ( const NGS_String_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const StringItf * self = Self ( iself );
        try
        {
            return self -> data ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return "";
    }

    size_t CC StringItf :: size_dispatch ( const NGS_String_v1 * iself, NGS_ErrBlock_v1 * err )
    {
        const StringItf * self = Self ( iself );
        try
        {
            return self -> size ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }

    NGS_String_v1 * CC StringItf :: substr_dispatch ( const NGS_String_v1 * iself, NGS_ErrBlock_v1 * err,
            size_t offset, size_t size )
    {
        const StringItf * self = Self ( iself );
        try
        {
            StringItf * sub = self -> substr ( offset, size );
            return sub -> Cast ();
        }
        catch ( ... )
        {
            ErrBlockHandleException ( err );
        }

        return 0;
    }


    NGS_String_v1_vt StringItf :: ivt =
    {
        {
            "ngs_adapt::StringItf",
            "NGS_String_v1",
            0,
            & OpaqueRefcount :: ivt . dad
        },

        data_dispatch,
        size_dispatch,
        substr_dispatch
    };


} // namespace ngs_adapt
