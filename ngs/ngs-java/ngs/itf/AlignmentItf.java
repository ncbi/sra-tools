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
import ngs.Fragment;
import ngs.Alignment;


/*==========================================================================
 * AlignmentItf
 *  represents an alignment between a Fragment and Reference sub-sequence
 *  provides a path to Read and mate Alignment
 */
class AlignmentItf
    extends Refcount
    implements Alignment
{

    /**********************
     * Fragment Interface *
     **********************/

    /* getFragmentId
     */
    public String getFragmentId ()
        throws ErrorMsg
    {
        return this . GetFragmentId ( self );
    }

    /* getFragmentBases
     *  return sequence bases
     *  "offset" is zero-based
     *  "length" must be >= 0
     */
    public String getFragmentBases ()
        throws ErrorMsg
    {
        return this . GetFragmentBases ( self, 0, -1 );
    }

    public String getFragmentBases ( long offset )
        throws ErrorMsg, IndexOutOfBoundsException
    {
        if ( offset < 0 )
            throw new IndexOutOfBoundsException ( "offset " + offset + " is negative" );

        return this . GetFragmentBases ( self, offset, -1 );
    }

    public String getFragmentBases ( long offset, long length )
        throws ErrorMsg, IndexOutOfBoundsException
    {
        if ( offset < 0 )
            throw new IndexOutOfBoundsException ( "offset " + offset + " is negative" );
        if ( length < 0 )
            throw new IndexOutOfBoundsException ( "length " + length + " is negative");

        return this . GetFragmentBases ( self, offset, length );
    }


    /* getFragmentQualities
     *  return phred quality values
     *  using ASCII offset of 33
     *  "offset" is zero-based
     *  "length" must be >= 0
     */
    public String getFragmentQualities ()
        throws ErrorMsg
    {
        return this . GetFragmentQualities ( self, 0, -1 );
    }

    public String getFragmentQualities ( long offset )
        throws ErrorMsg, IndexOutOfBoundsException
    {
        if ( offset < 0 )
            throw new IndexOutOfBoundsException ( "offset " + offset + " is negative" );

        return this . getFragmentQualities ( offset, -1 );
    }

    public String getFragmentQualities ( long offset, long length )
        throws ErrorMsg, IndexOutOfBoundsException
    {
        if ( offset < 0 )
            throw new IndexOutOfBoundsException ( "offset " + offset + " is negative" );
        if ( length < 0 )
            throw new IndexOutOfBoundsException ( "length " + length + " is negative");

        return this . GetFragmentQualities ( self, offset, length );
    }

    public boolean isPaired ()
        throws ErrorMsg
    {
        return this . IsPaired ( self );
    }
    
    public boolean isAligned ()
        throws ErrorMsg
    {
        return true;
    }

    /***********************
     * Alignment Interface *
     ***********************/

    /* getAlignmentId
     *  unique within ReadCollection
     */
    public String getAlignmentId ()
        throws ErrorMsg
    {
        return this . GetAlignmentId ( self );
    }


    /*------------------------------------------------------------------
     * Reference
     */

    /* getReferenceSpec
     */
    public String getReferenceSpec ()
        throws ErrorMsg
    {
        return this . GetReferenceSpec ( self );
    }

    /* getMappingQuality 
     */
    public int getMappingQuality ()
        throws ErrorMsg
    {
        return this . GetMappingQuality ( self );
    }

    /* getReferenceBases
     *  return reference bases
     */
    public String getReferenceBases ()
        throws ErrorMsg
    {
        return this . GetReferenceBases ( self );
    }


    /*------------------------------------------------------------------
     * Fragment
     */

    /* getReadGroup
     */
    public String getReadGroup ()
        throws ErrorMsg
    {
        return this . GetReadGroup ( self );
    }

    /* getReadId
     */
    public String getReadId ()
        throws ErrorMsg
    {
        return this . GetReadId ( self );
    }

    /* getClippedFragmentBases
     *  return fragment bases
     */
    public String getClippedFragmentBases ()
        throws ErrorMsg
    {
        return this . GetClippedFragmentBases ( self );
    }

    /* getClippedFragmentQualities
     *  return fragment phred quality values
     *  using ASCII offset of 33
     */
    public String getClippedFragmentQualities ()
        throws ErrorMsg
    {
        return this . GetClippedFragmentQualities ( self );
    }

    /* getAlignedFragmentBases
     *  return fragment bases in their aligned orientation
     */
    public String getAlignedFragmentBases ()
        throws ErrorMsg
    {
        return this . GetAlignedFragmentBases ( self );
    }
        
    /*------------------------------------------------------------------
     * details of this alignment
     */


    /* getAlignmentCategory
     */
    public int getAlignmentCategory ()
        throws ErrorMsg
    {
        return this . GetAlignmentCategory ( self );
    }

    /* getAlignmentPosition
     *  returns the alignment's starting position on reference
     */
    public long getAlignmentPosition ()
        throws ErrorMsg
    {
        return this . GetAlignmentPosition ( self );
    }

    /* getAligmentLength
     *  returns the alignment's projected length upon reference
     */
    public long getAlignmentLength ()
        throws ErrorMsg
    {
        return this . GetAlignmentLength ( self );
    }

    /* getIsReversedOrientation
     *  returns true if orientation is reversed
     *  with respect to the reference sequence
     */
    public boolean getIsReversedOrientation ()
        throws ErrorMsg
    {
        return this . GetIsReversedOrientation ( self );
    }

    /* getSoftClip 
     */
    public int getSoftClip ( int edge )
        throws ErrorMsg
    {
        return this . GetSoftClip ( self, edge );
    }

    /* getTemplateLength
     */
    public long getTemplateLength ()
        throws ErrorMsg
    {
        return this . GetTemplateLength ( self );
    }

    /* getShortCigar
     *  returns a text string describing alignment details
     */
    public String getShortCigar ( boolean clipped )
        throws ErrorMsg
    {
        return this . GetShortCigar ( self, clipped );
    }

    /* getLongCigar
     *  returns a text string describing alignment details
     */
    public String getLongCigar ( boolean clipped )
        throws ErrorMsg
    {
        return this . GetLongCigar ( self, clipped );
    }


    /* getRNAOrientation
     */
    public char getRNAOrientation ()
        throws ErrorMsg
    {
        return this . GetRNAOrientation ( self );
    }



    /*------------------------------------------------------------------
     * details of mate alignment
     */

    /* hasMate
     */
    public boolean hasMate ()
    {
        return this . HasMate ( self );
    }
    
    /* getMateAlignmentId
     */
    public String getMateAlignmentId ()
        throws ErrorMsg
    {
        return this . GetMateAlignmentId ( self );
    }

    /* getMateAlignment
     */
    public Alignment getMateAlignment ()
        throws ErrorMsg
    {
        long ref = this . GetMateAlignment ( self );
        try
        {
            return new AlignmentItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    /* getMateReferenceSpec
     */
    public String getMateReferenceSpec ()
        throws ErrorMsg
    {
        return this . GetMateReferenceSpec ( self );
    }

    /* getMateIsReversedOrientation
     */
    public boolean getMateIsReversedOrientation ()
        throws ErrorMsg
    {
        return this . GetMateIsReversedOrientation ( self );
    }


    /*******************************
     * AlignmentItf Implementation *
     *******************************/


    // constructors
    AlignmentItf ( long ref )
    {
        super ( ref );
    }

    AlignmentItf ( Alignment obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            AlignmentItf ref = ( AlignmentItf ) obj;
            this . self = ref . duplicate ();
        }
        catch ( Exception x )
        {
            throw new ErrorMsg ( x . toString () );
        }
    }

    // native interface
    private native String GetFragmentId ( long self )
        throws ErrorMsg;
    private native String GetFragmentBases ( long self, long offset, long length )
        throws ErrorMsg;
    private native String GetFragmentQualities ( long self, long offset, long length )
        throws ErrorMsg;
    private native boolean IsPaired ( long self )
        throws ErrorMsg;
    private native String GetAlignmentId ( long self )
        throws ErrorMsg;
    private native String GetReferenceSpec ( long self )
        throws ErrorMsg;
    private native int GetMappingQuality ( long self )
        throws ErrorMsg;
    private native String GetReferenceBases ( long self )
        throws ErrorMsg;
    private native String GetReadGroup ( long self )
        throws ErrorMsg;
    private native String GetReadId ( long self )
        throws ErrorMsg;
    private native String GetClippedFragmentBases ( long self )
        throws ErrorMsg;
    private native String GetClippedFragmentQualities ( long self )
        throws ErrorMsg;
    private native String GetAlignedFragmentBases ( long self )
        throws ErrorMsg;
    private native int GetAlignmentCategory ( long self )
        throws ErrorMsg;
    private native long GetAlignmentPosition ( long self )
        throws ErrorMsg;
    private native long GetAlignmentLength ( long self )
        throws ErrorMsg;
    private native boolean GetIsReversedOrientation ( long self )
        throws ErrorMsg;
    private native int GetSoftClip ( long self, int edge )
        throws ErrorMsg;
    private native long GetTemplateLength ( long self )
        throws ErrorMsg;
    private native String GetShortCigar ( long self, boolean clipped )
        throws ErrorMsg;
    private native String GetLongCigar ( long self, boolean clipped )
        throws ErrorMsg;
    private native char GetRNAOrientation ( long self )
        throws ErrorMsg;
    private native boolean HasMate ( long self );
    private native String GetMateAlignmentId ( long self )
        throws ErrorMsg;
    private native long GetMateAlignment ( long self )
        throws ErrorMsg;
    private native String GetMateReferenceSpec ( long self )
        throws ErrorMsg;
    private native boolean GetMateIsReversedOrientation ( long self )
        throws ErrorMsg;
}
