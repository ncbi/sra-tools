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

#ifndef _hpp_ngs_itf_referenceitf_
#define _hpp_ngs_itf_referenceitf_

#ifndef _hpp_ngs_itf_refcount_
#include <ngs/itf/Refcount.hpp>
#endif

struct NGS_Reference_v1;

namespace ngs
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;
    class PileupItf;
    class AlignmentItf;

    /*----------------------------------------------------------------------
     * Reference
     */
    class ReferenceItf : public Refcount < ReferenceItf, NGS_Reference_v1 >
    {
    public:

        StringItf * getCommonName () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getCanonicalName () const
            NGS_THROWS ( ErrorMsg );
        bool getIsCircular () const
            NGS_THROWS ( ErrorMsg );
        bool getIsLocal () const
            NGS_THROWS ( ErrorMsg );
        uint64_t getLength () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReferenceBases ( uint64_t offset ) const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReferenceBases ( uint64_t offset, uint64_t length ) const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReferenceChunk ( uint64_t offset ) const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReferenceChunk ( uint64_t offset, uint64_t length ) const
            NGS_THROWS ( ErrorMsg );
        uint64_t getAlignmentCount () const
            NGS_THROWS ( ErrorMsg );
        uint64_t getAlignmentCount ( uint32_t categories ) const
            NGS_THROWS ( ErrorMsg );
        AlignmentItf * getAlignment ( const char * alignmentId ) const
            NGS_THROWS ( ErrorMsg );
        AlignmentItf * getAlignments ( uint32_t categories ) const
            NGS_THROWS ( ErrorMsg );
        AlignmentItf * getAlignmentSlice ( int64_t start, uint64_t length ) const
            NGS_THROWS ( ErrorMsg );
        AlignmentItf * getAlignmentSlice ( int64_t start, uint64_t length, uint32_t categories ) const
            NGS_THROWS ( ErrorMsg );
        AlignmentItf * getFilteredAlignmentSlice ( int64_t start, uint64_t length, uint32_t categories, uint32_t filters, int32_t mappingQuality ) const
            NGS_THROWS ( ErrorMsg );
        PileupItf * getPileups ( uint32_t categories ) const
            NGS_THROWS ( ErrorMsg );
        PileupItf * getFilteredPileups ( uint32_t categories, uint32_t filters, int32_t mappingQuality ) const
            NGS_THROWS ( ErrorMsg );
        PileupItf * getPileupSlice ( int64_t start, uint64_t length ) const
            NGS_THROWS ( ErrorMsg );
        PileupItf * getPileupSlice ( int64_t start, uint64_t length, uint32_t categories ) const
            NGS_THROWS ( ErrorMsg );
        PileupItf * getFilteredPileupSlice ( int64_t start, uint64_t length, uint32_t categories, uint32_t filters, int32_t mappingQuality ) const
            NGS_THROWS ( ErrorMsg );
        bool nextReference ()
            NGS_THROWS ( ErrorMsg );
    };

} // namespace ngs

#endif // _hpp_ngs_itf_referenceitf_
