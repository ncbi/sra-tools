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

#ifndef _inl_ngs_fragment_
#define _inl_ngs_fragment_

#ifndef _hpp_ngs_fragment_
#include <ngs/Fragment.hpp>
#endif

#ifndef _hpp_ngs_itf_fragmentitf_
#include <ngs/itf/FragmentItf.hpp>
#endif


namespace ngs
{

    /*----------------------------------------------------------------------
     * Fragment
     */

    inline
    StringRef Fragment :: getFragmentId () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getFragmentId () ); }

    inline
    StringRef Fragment :: getFragmentBases () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getFragmentBases () ); }

    inline
    StringRef Fragment :: getFragmentBases ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getFragmentBases ( offset ) ); }

    inline
    StringRef Fragment :: getFragmentBases ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getFragmentBases ( offset, length ) ); }

    inline
    StringRef Fragment :: getFragmentQualities () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getFragmentQualities () ); }

    inline
    StringRef Fragment :: getFragmentQualities ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getFragmentQualities ( offset ) ); }

    inline
    StringRef Fragment :: getFragmentQualities ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getFragmentQualities ( offset, length ) ); }

    inline
    bool Fragment :: isPaired () const
        NGS_THROWS ( ErrorMsg )
    { return self -> isPaired (); }

    inline
    bool Fragment :: isAligned () const
        NGS_THROWS ( ErrorMsg )
    { return self -> isAligned (); }


} // namespace ngs

#endif // _inl_ngs_fragment_
