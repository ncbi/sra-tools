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

#ifndef _hpp_ngs_pileup_event_iterator_
#define _hpp_ngs_pileup_event_iterator_

#ifndef _hpp_ngs_pileup_event_
#include <ngs/PileupEvent.hpp>
#endif

namespace ngs
{
    /*----------------------------------------------------------------------
     * PileupEventIterator
     *  iterates across a list of PileupEvents
     */
    class PileupEventIterator : public PileupEvent
    {
    public:

        /* nextPileupEvent
         *  advance to first PileupEvent on initial invocation
         *  advance to next PileupEvent subsequently
         *  returns false if no more PileupEvents are available.
         *  throws exception if more PileupEvents should be available,
         *  but could not be accessed.
         */
        bool nextPileupEvent ()
            NGS_THROWS ( ErrorMsg );


        /* resetPileupEvent
         *  resets to initial iterator state
         *  the next call to "nextPileupEvent" will advance to first event
         */
        void resetPileupEvent ()
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support

        PileupEventIterator ( PileupEventRef ref )
            NGS_NOTHROW ();

        PileupEventIterator & operator = ( const PileupEventIterator & obj )
            NGS_THROWS ( ErrorMsg );
        PileupEventIterator ( const PileupEventIterator & obj )
            NGS_THROWS ( ErrorMsg );

        ~ PileupEventIterator ()
            NGS_NOTHROW ();

    private:

        PileupEvent & operator = ( const PileupEvent & obj )
            NGS_THROWS ( ErrorMsg );
        PileupEventIterator & operator = ( PileupEventRef ref )
            NGS_NOTHROW ();
    };

} // namespace ngs


#ifndef _inl_ngs_pileup_event_iterator_
#include <ngs/inl/PileupEventIterator.hpp>
#endif

#endif // _hpp_ngs_pileup_event_iterator_
