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

#ifndef _inl_ngs_pileup_event_
#define _inl_ngs_pileup_event_

#ifndef _hpp_ngs_pileup_event_
#include <ngs/PileupEvent.hpp>
#endif

#ifndef _hpp_ngs_itf_pileup_eventitf_
#include <ngs/itf/PileupEventItf.hpp>
#endif

namespace ngs
{

    /*----------------------------------------------------------------------
     * PileupEvent
     */

    inline
    int PileupEvent :: getMappingQuality () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getMappingQuality (); }

    inline
    StringRef PileupEvent :: getAlignmentId () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getAlignmentId () ); }

    inline
    int64_t PileupEvent :: getAlignmentPosition () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getAlignmentPosition (); }

    inline
    int64_t PileupEvent :: getFirstAlignmentPosition () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getFirstAlignmentPosition (); }

    inline
    int64_t PileupEvent :: getLastAlignmentPosition () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getLastAlignmentPosition (); }

    inline
    PileupEvent :: PileupEventType PileupEvent :: getEventType () const
        NGS_THROWS ( ErrorMsg )
    { return ( PileupEvent :: PileupEventType ) self -> getEventType (); }

    inline
    char PileupEvent :: getAlignmentBase () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getAlignmentBase (); }

    inline
    char PileupEvent :: getAlignmentQuality () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getAlignmentQuality (); }

    inline
    StringRef PileupEvent :: getInsertionBases () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getInsertionBases () ); }

    inline
    StringRef PileupEvent :: getInsertionQualities () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getInsertionQualities () ); }

    inline
    uint32_t PileupEvent :: getEventRepeatCount () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getEventRepeatCount (); }

    inline
    PileupEvent :: EventIndelType PileupEvent :: getEventIndelType () const
        NGS_THROWS ( ErrorMsg )
    { return ( PileupEvent :: EventIndelType ) self -> getEventIndelType (); }


} // namespace ngs

#endif // _inl_ngs_pileup_event_
