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

#ifndef _inl_ngs_reference_
#define _inl_ngs_reference_

#ifndef _hpp_ngs_reference_
#include <ngs/Reference.hpp>
#endif

#ifndef _hpp_ngs_itf_referenceitf_
#include <ngs/itf/ReferenceItf.hpp>
#endif

namespace ngs
{

    /*----------------------------------------------------------------------
     * Reference
     */

    inline
    String Reference :: getCommonName () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getCommonName () ) . toString (); }

    inline
    String Reference :: getCanonicalName () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getCanonicalName () ) . toString (); }

    inline
    bool Reference :: getIsCircular () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getIsCircular (); }

    inline
    bool Reference :: getIsLocal () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getIsLocal (); }

    inline
    uint64_t Reference :: getLength () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getLength (); }

    inline
    String Reference :: getReferenceBases ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReferenceBases ( offset ) ) . toString (); }

    inline
    String Reference :: getReferenceBases ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReferenceBases ( offset, length ) ) . toString (); }

    inline
    StringRef Reference :: getReferenceChunk ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReferenceChunk ( offset ) ); }

    inline
    StringRef Reference :: getReferenceChunk ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getReferenceChunk ( offset, length ) ); }

    inline
    uint64_t Reference :: getAlignmentCount () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getAlignmentCount (); }

    inline
    uint64_t Reference :: getAlignmentCount ( Alignment :: AlignmentCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return self -> getAlignmentCount ( ( uint32_t ) categories ); }

    inline
    Alignment Reference :: getAlignment ( const String & alignmentId ) const
        NGS_THROWS ( ErrorMsg )
    { return Alignment ( ( AlignmentRef ) self -> getAlignment ( alignmentId . c_str () ) ); }

    inline
    AlignmentIterator Reference :: getAlignments ( Alignment :: AlignmentCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return AlignmentIterator ( ( AlignmentRef ) self -> getAlignments ( ( uint32_t ) categories ) ); }

    inline
    AlignmentIterator Reference :: getAlignmentSlice ( int64_t start, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    { return AlignmentIterator ( ( AlignmentRef ) self -> getAlignmentSlice ( start, length ) ); }

    inline
    AlignmentIterator Reference :: getAlignmentSlice ( int64_t start, uint64_t length, Alignment :: AlignmentCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return AlignmentIterator ( ( AlignmentRef ) self -> getAlignmentSlice ( start, length, ( uint32_t ) categories ) ); }

    inline
    AlignmentIterator Reference :: getFilteredAlignmentSlice ( int64_t start, uint64_t length, Alignment :: AlignmentCategory categories, Alignment :: AlignmentFilter filters, int32_t mappingQuality ) const
        NGS_THROWS ( ErrorMsg )
    { return AlignmentIterator ( ( AlignmentRef ) self -> getFilteredAlignmentSlice ( start, length, ( uint32_t ) categories, ( uint32_t ) filters, mappingQuality ) ); }

    inline
    PileupIterator Reference :: getPileups ( Alignment :: AlignmentCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return PileupIterator ( ( PileupRef ) self -> getPileups ( ( uint32_t ) categories ) ); }

    inline
    PileupIterator Reference :: getFilteredPileups ( Alignment :: AlignmentCategory categories, Alignment :: AlignmentFilter filters, int32_t mappingQuality ) const
        NGS_THROWS ( ErrorMsg )
    { return PileupIterator ( ( PileupRef ) self -> getFilteredPileups ( ( uint32_t ) categories, ( uint32_t ) filters, mappingQuality ) ); }

    inline
    PileupIterator Reference :: getPileupSlice ( int64_t start, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    { return PileupIterator ( ( PileupRef ) self -> getPileupSlice ( start, length ) ); }

    inline
    PileupIterator Reference :: getPileupSlice ( int64_t start, uint64_t length, Alignment :: AlignmentCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return PileupIterator ( ( PileupRef ) self -> getPileupSlice ( start, length, ( uint32_t ) categories ) ); }

    inline
    PileupIterator Reference :: getFilteredPileupSlice ( int64_t start, uint64_t length, Alignment :: AlignmentCategory categories, Alignment :: AlignmentFilter filters, int32_t mappingQuality ) const
        NGS_THROWS ( ErrorMsg )
    { return PileupIterator ( ( PileupRef ) self -> getFilteredPileupSlice ( start, length, ( uint32_t ) categories, ( uint32_t ) filters, mappingQuality ) ); }

} // namespace ngs

#endif // _inl_ngs_reference_
