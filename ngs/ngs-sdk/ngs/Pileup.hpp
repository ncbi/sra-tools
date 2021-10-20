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

#ifndef _hpp_ngs_pileup_
#define _hpp_ngs_pileup_

#ifndef _hpp_ngs_pileup_event_iterator_
#include <ngs/PileupEventIterator.hpp>
#endif

namespace ngs
{

    /*----------------------------------------------------------------------
     * forwards and typedefs
     */
    typedef PileupEventRef PileupRef;


    /*======================================================================
     * Pileup
     *  represents a slice through a stack of Alignments
     *  at a given position on the Reference
     */
    class Pileup : public PileupEventIterator
    {
    public:

        /*------------------------------------------------------------------
         * Reference
         */

        /* getReferenceSpec
         */
        String getReferenceSpec () const
            NGS_THROWS ( ErrorMsg );

        /* getReferencePosition
         */
        int64_t getReferencePosition () const
            NGS_THROWS ( ErrorMsg );

        /* getReferenceBase
         *  retrieves base at current Reference position
         */
        char getReferenceBase () const
            NGS_THROWS ( ErrorMsg );


        /*------------------------------------------------------------------
         * details of this pileup row
         */

        /* getPileupDepth
         *  returns the coverage depth
         *  at the current reference position
         */
        uint32_t getPileupDepth () const
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support

        Pileup & operator = ( PileupRef ref )
            NGS_NOTHROW ();
        Pileup ( PileupRef ref )
            NGS_NOTHROW ();

        Pileup & operator = ( const Pileup & obj )
            NGS_THROWS ( ErrorMsg );
        Pileup ( const Pileup & obj )
            NGS_THROWS ( ErrorMsg );

        ~ Pileup ()
            NGS_NOTHROW ();
    };

} // namespace ngs


#ifndef _inl_ngs_pileup_
#include <ngs/inl/Pileup.hpp>
#endif

#endif // _hpp_ngs_pileup_
