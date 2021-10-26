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
import ngs.Pileup;
import ngs.PileupIterator;


/*==========================================================================
 * PileupIteratorItf
 *  represents a slice through a stack of Alignments
 *  at a given position on the Reference
 */
class PileupIteratorItf
    extends PileupItf
    implements PileupIterator
{

    /****************************
     * PileupIterator Interface *
     ****************************/

    /* nextPileup
     *  advance to first Pileup on initial invocation
     *  advance to next Pileup subsequently
     *  returns false if no more Pileups are available.
     *  throws exception if more Pileups should be available,
     *  but could not be accessed.
     */
    public boolean nextPileup ()
        throws ErrorMsg
    {
        return this . NextPileup ( self );
    }


    /************************************
     * PileupIteratorItf Implementation *
     ************************************/


    // constructors
    PileupIteratorItf ( long ref )
    {
        super ( ref );
    }

    PileupIteratorItf ( PileupIterator obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            PileupIteratorItf ref = ( PileupIteratorItf ) obj;
            this . self = ref . duplicate ();
        }
        catch ( Exception x )
        {
            throw new ErrorMsg ( x . toString () );
        }
    }

    // native interface
    private native boolean NextPileup ( long self )
        throws ErrorMsg;
}
