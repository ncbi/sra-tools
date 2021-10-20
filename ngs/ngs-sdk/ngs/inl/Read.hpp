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

#ifndef _inl_ngs_read_
#define _inl_ngs_read_

#ifndef _hpp_ngs_read_
#include <ngs/Read.hpp>
#endif

#include <ngs/itf/ReadItf.hpp>

namespace ngs
{

    // the "self" member is typed as FragmentRef
    // but is used here as an ReadRef
#define self reinterpret_cast < const ReadItf * > ( self )

    /*----------------------------------------------------------------------
     * Read
     *  represents an NGS machine read
     *  having some number of biological Fragments
     */

    inline
    StringRef Read :: getReadId () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReadId () ); }

    inline
    uint32_t Read :: getNumFragments () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getNumFragments (); }

    inline
    bool Read :: fragmentIsAligned ( uint32_t fragIdx ) const
        NGS_THROWS ( ErrorMsg )
    { return self -> fragmentIsAligned ( fragIdx ); }

    inline
    Read :: ReadCategory Read :: getReadCategory () const
        NGS_THROWS ( ErrorMsg )
    { return ( Read :: ReadCategory ) self -> getReadCategory (); }

    inline
    String Read :: getReadGroup () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReadGroup () ) . toString (); }

    inline
    StringRef Read :: getReadName () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReadName () ); }

    inline
    StringRef Read :: getReadBases () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReadBases () ); }

    inline
    StringRef Read :: getReadBases ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReadBases ( offset ) ); }

    inline
    StringRef Read :: getReadBases ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReadBases ( offset, length ) ); }

    inline
    StringRef Read :: getReadQualities () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReadQualities () ); }

    inline
    StringRef Read :: getReadQualities ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReadQualities ( offset ) ); }

    inline
    StringRef Read :: getReadQualities ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReadQualities ( offset, length ) ); }

#undef self

} // namespace ngs

#endif // _inl_ngs_read_
