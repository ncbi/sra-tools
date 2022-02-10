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

#ifndef _hpp_ngs_adapt_referenceitf_
#define _hpp_ngs_adapt_referenceitf_

#ifndef _hpp_ngs_adapt_refcount_
#include <ngs/adapter/Refcount.hpp>
#endif

#ifndef _h_ngs_itf_referenceitf_
#include <ngs/itf/ReferenceItf.h>
#endif

namespace ngs_adapt
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

        virtual StringItf * getCommonName () const = 0;
        virtual StringItf * getCanonicalName () const = 0;
        virtual bool getIsCircular () const = 0;
        virtual bool getIsLocal () const = 0;
        virtual uint64_t getLength () const = 0;
        virtual StringItf * getReferenceBases ( uint64_t offset, uint64_t length ) const = 0;
        virtual StringItf * getReferenceChunk ( uint64_t offset, uint64_t length ) const = 0;
        virtual uint64_t getAlignmentCount ( bool wants_primary, bool wants_secondary ) const = 0;
        virtual AlignmentItf * getAlignment ( const char * alignmentId ) const = 0;
        virtual AlignmentItf * getAlignments ( bool wants_primary, bool wants_secondary ) const = 0;
        virtual AlignmentItf * getAlignmentSlice ( int64_t start, uint64_t length, bool wants_primary, bool wants_secondary ) const = 0;
        virtual AlignmentItf * getFilteredAlignmentSlice ( int64_t start, uint64_t length, uint32_t flags, int32_t map_qual ) const = 0;
        virtual PileupItf * getPileups ( bool wants_primary, bool wants_secondary ) const = 0;
        virtual PileupItf * getFilteredPileups ( uint32_t flags, int32_t map_qual ) const = 0;
        virtual PileupItf * getPileupSlice ( int64_t start, uint64_t length, bool wants_primary, bool wants_secondary ) const = 0;
        virtual PileupItf * getFilteredPileupSlice ( int64_t start, uint64_t length, uint32_t flags, int32_t map_qual ) const = 0;
        virtual bool nextReference () = 0;

    protected:

        ReferenceItf ();
        static NGS_Reference_v1_vt ivt;

    private:

        static NGS_String_v1 * CC get_cmn_name ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_canon_name ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC is_circular ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC is_local ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );
        static uint64_t CC get_length ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_ref_bases ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length );
        static NGS_String_v1 * CC get_ref_chunk ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length );
        static uint64_t CC get_alignment_count ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            bool wants_primary, bool wants_secondary );
        static NGS_Alignment_v1 * CC get_alignment ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            const char * alignmentId );
        static NGS_Alignment_v1 * CC get_alignments ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            bool wants_primary, bool wants_secondary );
        static NGS_Alignment_v1 * CC get_align_slice ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            int64_t start, uint64_t length, bool wants_primary, bool wants_secondary );
        static NGS_Alignment_v1 * CC get_filtered_align_slice ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            int64_t start, uint64_t length, uint32_t flags, int32_t map_qual );
        static NGS_Pileup_v1 * CC get_pileups ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            bool wants_primary, bool wants_secondary );
        static NGS_Pileup_v1 * CC get_filtered_pileups ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            uint32_t flags, int32_t map_qual );
        static NGS_Pileup_v1 * CC get_pileup_slice ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            int64_t start, uint64_t length, bool wants_primary, bool wants_secondary );
        static NGS_Pileup_v1 * CC get_filtered_pileup_slice ( const NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err,
            int64_t start, uint64_t length, uint32_t flags, int32_t map_qual );
        static bool CC next ( NGS_Reference_v1 * self, NGS_ErrBlock_v1 * err );

    };

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_referenceitf_
