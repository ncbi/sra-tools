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

#ifndef _hpp_ngs_pileup_iterator_
#define _hpp_ngs_pileup_iterator_

#ifndef _hpp_ngs_pileup_
#include <ngs/Pileup.hpp>
#endif

namespace ngs
{
    /*----------------------------------------------------------------------
     * PileupIterator
     *  iterates across a list of Pileups
     */
    class PileupIterator : public Pileup
    {
    public:

        /* nextPileup
         *  advance to first Pileup on initial invocation
         *  advance to next Pileup subsequently
         *  returns false if no more Pileups are available.
         *  always resets PileupEventIterator
         *  throws exception if more Pileups should be available,
         *  but could not be accessed.
         */
        bool nextPileup ()
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support

        PileupIterator ( PileupRef ref )
            NGS_NOTHROW ();

        PileupIterator & operator = ( const PileupIterator & obj )
            NGS_THROWS ( ErrorMsg );
        PileupIterator ( const PileupIterator & obj )
            NGS_THROWS ( ErrorMsg );

        ~ PileupIterator ()
            NGS_NOTHROW ();

    private:

        Pileup & operator = ( const Pileup & obj )
            NGS_THROWS ( ErrorMsg );
        PileupIterator & operator = ( PileupRef ref )
            NGS_NOTHROW ();
    };

} // namespace ngs


#ifndef _inl_ngs_pileup_iterator_
#include <ngs/inl/PileupIterator.hpp>
#endif

#endif // _hpp_ngs_pileup_iterator_
