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

#ifndef _hpp_ngs_itf_alignment_
#define _hpp_ngs_itf_alignment_

#ifndef _hpp_ngs_itf_refcount
#include <ngs/itf/Refcount.hpp>
#endif

struct NGS_Alignment_v1;

namespace ngs
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;

    /*----------------------------------------------------------------------
     * AlignmentItf
     */
    class   AlignmentItf : public Refcount < AlignmentItf, NGS_Alignment_v1 >
    {
    public:

        StringItf * getAlignmentId () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReferenceSpec () const
            NGS_THROWS ( ErrorMsg );
        int32_t getMappingQuality () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReferenceBases () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReadGroup () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReadId () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getClippedFragmentBases () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getClippedFragmentQualities () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getAlignedFragmentBases () const
            NGS_THROWS ( ErrorMsg );
        uint32_t getAlignmentCategory () const
            NGS_THROWS ( ErrorMsg );
        int64_t getAlignmentPosition () const
            NGS_THROWS ( ErrorMsg );
        uint64_t getReferencePositionProjectionRange ( int64_t ref_pos ) const
            NGS_THROWS ( ErrorMsg );
        uint64_t getAlignmentLength () const
            NGS_THROWS ( ErrorMsg );
        bool getIsReversedOrientation () const
            NGS_THROWS ( ErrorMsg );
        int32_t getSoftClip ( uint32_t edge ) const
            NGS_THROWS ( ErrorMsg );
        uint64_t getTemplateLength () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getShortCigar ( bool clipped ) const
            NGS_THROWS ( ErrorMsg );
        StringItf * getLongCigar ( bool clipped ) const
            NGS_THROWS ( ErrorMsg );
        char getRNAOrientation () const
            NGS_THROWS ( ErrorMsg );
        bool hasMate () const
            NGS_NOTHROW ();
        StringItf * getMateAlignmentId () const
            NGS_THROWS ( ErrorMsg );
        AlignmentItf * getMateAlignment () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getMateReferenceSpec () const
            NGS_THROWS ( ErrorMsg );
        bool getMateIsReversedOrientation () const
            NGS_THROWS ( ErrorMsg );
        bool nextAlignment ()
            NGS_THROWS ( ErrorMsg );
    };

} // namespace ngs

#endif // _hpp_ngs_itf_alignment_
