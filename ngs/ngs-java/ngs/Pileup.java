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

package ngs;


/**
 * Represents a slice through a stack of Alignments at a given position on the Reference
 */
public interface Pileup
    extends PileupEventIterator
{

    /*----------------------------------------------------------------------
     * Reference
     */

    /**
     * getReferenceSpec
     * @return name of the Reference
     * @throws ErrorMsg upon an error accessing data
     */
    String getReferenceSpec ()
        throws ErrorMsg;

    /**
     * getReferencePosition
     * @return current position on the Reference
     * @throws ErrorMsg upon an error accessing data	 
     */
    long getReferencePosition ()
        throws ErrorMsg;

    /**
     * @return base at current Reference position
     * @throws ErrorMsg upon an error accessing data
     */
    char getReferenceBase ()
        throws ErrorMsg;


    /*----------------------------------------------------------------------
     * details of this pileup row
     */

    /**
     * getPileupDepth
     * @return the coverage depth at the current reference position
     * @throws ErrorMsg upon an error accessing data
     */
    int getPileupDepth ()
        throws ErrorMsg;
}
