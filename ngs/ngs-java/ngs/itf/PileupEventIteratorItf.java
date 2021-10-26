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

package ngs.itf;

import ngs.ErrorMsg;
import ngs.PileupEvent;
import ngs.PileupEventIterator;


/*==========================================================================
 * PileupEventIteratorItf
 *  represents a single cell of a sparse 2D matrix
 *  with Reference coordinates on one axis
 *  and stacked Alignments on the other axis
 */
class PileupEventIteratorItf
    extends PileupEventItf
    implements PileupEventIterator
{

    /*********************************
     * PileupEventIterator Interface *
     *********************************/

    /* nextPileup
     *  advance to first Pileup on initial invocation
     *  advance to next Pileup subsequently
     *  returns false if no more Pileups are available.
     *  throws exception if more Pileups should be available,
     *  but could not be accessed.
     */
    public boolean nextPileupEvent ()
        throws ErrorMsg
    {
        return this . NextPileupEvent ( self );
    }

    /* resetPileupEvent
     *  resets to initial iterator state
     *  the next call to "nextPileupEvent" will advance to first event
     */
    public void resetPileupEvent ()
        throws ErrorMsg
    {
        this . ResetPileupEvent ( self );
    }


    /*****************************************
     * PileupEventIteratorItf Implementation *
     *****************************************/


    // constructors
    PileupEventIteratorItf ( long ref )
    {
        super ( ref );
    }

    PileupEventIteratorItf ( PileupEventIterator obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            PileupEventIteratorItf ref = ( PileupEventIteratorItf ) obj;
            this . self = ref . duplicate ();
        }
        catch ( Exception x )
        {
            throw new ErrorMsg ( x . toString () );
        }
    }

    // native interface
    private native boolean NextPileupEvent ( long self )
        throws ErrorMsg;
    private native void ResetPileupEvent ( long self )
        throws ErrorMsg;
}
