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

#ifndef _hpp_ngs_adapt_pileup_eventitf_
#define _hpp_ngs_adapt_pileup_eventitf_

#ifndef _hpp_ngs_adapt_refcount_
#include <ngs/adapter/Refcount.hpp>
#endif

#ifndef _hpp_ngs_itf_pileup_eventitf_
#include <ngs/itf/PileupEventItf.h>
#endif

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;

    /*----------------------------------------------------------------------
     * PileupEventItf
     */
    class PileupEventItf : public Refcount < PileupEventItf, NGS_PileupEvent_v1 >
    {
    public:

        virtual int32_t getMappingQuality () const = 0;
        virtual StringItf * getAlignmentId () const = 0;
        virtual int64_t getAlignmentPosition () const = 0;
        virtual int64_t getFirstAlignmentPosition () const = 0;
        virtual int64_t getLastAlignmentPosition () const = 0;
        virtual uint32_t getEventType () const = 0;
        virtual char getAlignmentBase () const = 0;
        virtual char getAlignmentQuality () const = 0;
        virtual StringItf * getInsertionBases () const = 0;
        virtual StringItf * getInsertionQualities () const = 0;
        virtual uint32_t getEventRepeatCount () const = 0;
        virtual uint32_t getEventIndelType () const = 0;
        virtual bool nextPileupEvent () = 0;
        virtual void resetPileupEvent () = 0;

    protected:

        PileupEventItf ( const NGS_VTable * vt );
        static struct NGS_PileupEvent_v1_vt ivt;

    private:

        static int32_t CC get_map_qual ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_align_id ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static int64_t CC get_align_pos ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static int64_t CC get_first_align_pos ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static int64_t CC get_last_align_pos ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static uint32_t CC get_event_type ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static char CC get_align_base ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static char CC get_align_qual ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_ins_bases ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_ins_quals ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static uint32_t CC get_rpt_count ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static uint32_t CC get_indel_type ( const NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC next ( NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
        static void CC reset ( NGS_PileupEvent_v1 * self, NGS_ErrBlock_v1 * err );
    };

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_pileup_eventitf_
