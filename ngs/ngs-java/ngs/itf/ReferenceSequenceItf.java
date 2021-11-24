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
import ngs.ReferenceSequence;


/*==========================================================================
 * ReferenceSequenceItf
 *  represents a reference sequence
 */
public class ReferenceSequenceItf
    extends Refcount
    implements ReferenceSequence
{

    /***********************
     * ReferenceSequence Interface *
     ***********************/

    /* getCanonicalName
     *  returns the accessioned name of reference, e.g. "NC_000001.11"
     */
    public String getCanonicalName ()
        throws ErrorMsg
    {
        return this . GetCanonicalName ( self );
    }


    /* getIsCircular
     *  returns true if reference is circular
     */
    public boolean getIsCircular ()
        throws ErrorMsg
    {
        return this . GetIsCircular ( self );
    }


    /* getLength
     *  returns the length of the reference sequence
     */
    public long getLength ()
        throws ErrorMsg
    {
        return this . GetLength ( self );
    }


    /* getReferenceBases
     *  return sub-sequence bases for Reference
     *  "offset" is zero-based
     *  "size" must be >= 0
     */
    public String getReferenceBases ( long offset )
        throws ErrorMsg
    {
        return this . getReferenceBases ( offset, -1 );
    }

    public String getReferenceBases ( long offset, long length )
        throws ErrorMsg
    {
        return this . GetReferenceBases ( self, offset, length );
    }

    /* getReferenceChunk
     *  return largest contiguous chunk available of
     *  sub-sequence bases for Reference
     *  "offset" is zero-based
     *  "size" must be >= 0
     *
     * NB - actual returned sequence may be shorter
     *  than requested. to obtain all bases available
     *  in chunk, use a negative "size" value
     */
    public String getReferenceChunk ( long offset )
        throws ErrorMsg
    {
        return this . getReferenceChunk ( offset, -1 );
    }

    public String getReferenceChunk ( long offset, long length )
        throws ErrorMsg
    {
        return this . GetReferenceChunk ( self, offset, length );
    }


    /*******************************
     * ReferenceSequenceItf Implementation *
     *******************************/

    // constructors
    public ReferenceSequenceItf ( long ref )
    {
        super ( ref );
    }

    ReferenceSequenceItf ( ReferenceSequence obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            ReferenceSequenceItf ref = ( ReferenceSequenceItf ) obj;
            this . self = ref . duplicate ();
        }
        catch ( Exception x )
        {
            throw new ErrorMsg ( x . toString () );
        }
    }

    // native interface
    private native String GetCanonicalName ( long self )
        throws ErrorMsg;
    private native boolean GetIsCircular ( long self )
        throws ErrorMsg;
    private native long GetLength ( long self )
        throws ErrorMsg;
    private native String GetReferenceBases ( long self, long offset, long length )
        throws ErrorMsg;
    private native String GetReferenceChunk ( long self, long offset, long length )
        throws ErrorMsg;
}
