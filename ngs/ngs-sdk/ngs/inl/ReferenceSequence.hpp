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

#ifndef _inl_ngs_reference_sequence_
#define _inl_ngs_reference_sequence_

#ifndef _hpp_ngs_reference_sequence_
#include <ngs/ReferenceSequence.hpp>
#endif

#ifndef _hpp_ngs_itf_reference_sequence_itf_
#include <ngs/itf/ReferenceSequenceItf.hpp>
#endif

namespace ngs
{

    /*----------------------------------------------------------------------
     * Reference
     */

    inline
    String ReferenceSequence :: getCanonicalName () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getCanonicalName () ) . toString (); }

    inline
    bool ReferenceSequence :: getIsCircular () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getIsCircular (); }

    inline
    uint64_t ReferenceSequence :: getLength () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getLength (); }

    inline
    String ReferenceSequence :: getReferenceBases ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReferenceBases ( offset ) ) . toString (); }

    inline
    String ReferenceSequence :: getReferenceBases ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReferenceBases ( offset, length ) ) . toString (); }

    inline
    StringRef ReferenceSequence :: getReferenceChunk ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReferenceChunk ( offset ) ); }

    inline
    StringRef ReferenceSequence :: getReferenceChunk ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReferenceChunk ( offset, length ) ); }

} // namespace ngs

#endif // _inl_ngs_reference_sequence_
