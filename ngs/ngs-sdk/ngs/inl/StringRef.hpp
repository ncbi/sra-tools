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

#ifndef _inl_ngs_stringref_
#define _inl_ngs_stringref_

#ifndef _hpp_ngs_stringref_
#include <ngs/StringRef.hpp>
#endif

#ifndef _hpp_ngs_itf_stringitf_
#include <ngs/itf/StringItf.hpp>
#endif

namespace ngs
{
    /*----------------------------------------------------------------------
     * StringRef
     *  inline dispatch
     */

    inline
    const char * StringRef :: data () const
        NGS_NOTHROW ()
    { return self -> data (); }

    inline
    size_t StringRef :: size () const
        NGS_NOTHROW ()
    { return self -> size (); }

    inline
    StringRef StringRef :: substr ( size_t offset ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> substr ( offset ) ); }

    inline
    StringRef StringRef :: substr ( size_t offset, size_t size ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> substr ( offset, size ) ); }


} // namespace ngs

#endif // _inl_ngs_stringref_
