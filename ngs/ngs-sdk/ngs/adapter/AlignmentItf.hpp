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

#ifndef _hpp_ngs_adapt_alignment_
#define _hpp_ngs_adapt_alignment_

#ifndef _hpp_ngs_adapt_fragmentitf
#include <ngs/adapter/FragmentItf.hpp>
#endif

#ifndef _h_ngs_itf_alignment_
#include <ngs/itf/AlignmentItf.h>
#endif

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;

    /*----------------------------------------------------------------------
     * AlignmentItf
     */
    class AlignmentItf : public FragmentItf
    {
    public:

        virtual StringItf * getAlignmentId () const = 0;
        virtual StringItf * getReferenceSpec () const = 0;
        virtual int32_t getMappingQuality () const = 0;
        virtual StringItf * getReferenceBases () const = 0;
        virtual StringItf * getReadGroup () const = 0;
        virtual StringItf * getReadId () const = 0;
        virtual StringItf * getClippedFragmentBases () const = 0;
        virtual StringItf * getClippedFragmentQualities () const = 0;
        virtual StringItf * getAlignedFragmentBases () const = 0;
        virtual bool isPrimary () const = 0;
        virtual int64_t getAlignmentPosition () const = 0;
        virtual uint64_t getReferencePositionProjectionRange ( int64_t ref_pos ) const = 0;
        virtual uint64_t getAlignmentLength () const = 0;
        virtual bool getIsReversedOrientation () const = 0;
        virtual int32_t getSoftClip ( uint32_t edge ) const = 0;
        virtual uint64_t getTemplateLength () const = 0;
        virtual StringItf * getShortCigar ( bool clipped ) const = 0;
        virtual StringItf * getLongCigar ( bool clipped ) const = 0;
        virtual char getRNAOrientation () const = 0;
        virtual bool hasMate () const = 0;
        virtual StringItf * getMateAlignmentId () const = 0;
        virtual AlignmentItf * getMateAlignment () const = 0;
        virtual StringItf * getMateReferenceSpec () const = 0;
        virtual bool getMateIsReversedOrientation () const = 0;
        virtual bool nextAlignment () = 0;

        inline NGS_Alignment_v1 * Cast ()
        { return static_cast < NGS_Alignment_v1* > ( OpaqueRefcount :: offset_this () ); }

        inline const NGS_Alignment_v1 * Cast () const
        { return static_cast < const NGS_Alignment_v1* > ( OpaqueRefcount :: offset_this () ); }

        // assistance for C objects
        static inline AlignmentItf * Self ( NGS_Alignment_v1 * obj )
        { return static_cast < AlignmentItf* > ( OpaqueRefcount :: offset_cobj ( obj ) ); }

        static inline const AlignmentItf * Self ( const NGS_Alignment_v1 * obj )
        { return static_cast < const AlignmentItf* > ( OpaqueRefcount :: offset_cobj ( obj ) ); }

    protected:

        AlignmentItf ();
        static NGS_Alignment_v1_vt ivt;

    private:

        // throws ErrorMsg because it's not applicable here
        virtual bool nextFragment ();

        static NGS_String_v1 * CC get_id ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_ref_spec ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static int32_t CC get_map_qual ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_ref_bases ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_read_group ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_read_id ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_clipped_frag_bases ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_clipped_frag_quals ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_aligned_frag_bases ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC is_primary ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static int64_t CC get_align_pos ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static uint64_t CC get_ref_pos_projection_range ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, int64_t ref_pos );
        static uint64_t CC get_align_length ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC get_is_reversed ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static int32_t CC get_soft_clip ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, uint32_t edge );
        static uint64_t CC get_template_len ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_short_cigar ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, bool clipped );
        static NGS_String_v1 * CC get_long_cigar ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err, bool clipped );
        static char CC get_rna_orientation ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC has_mate ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_mate_id ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_Alignment_v1 * CC get_mate_alignment ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_mate_ref_spec ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC get_mate_is_reversed ( const NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC next ( NGS_Alignment_v1 * self, NGS_ErrBlock_v1 * err );

    };

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_alignment_
