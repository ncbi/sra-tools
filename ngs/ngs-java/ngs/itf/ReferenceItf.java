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
import ngs.Reference;
import ngs.Alignment;
import ngs.AlignmentIterator;
import ngs.PileupIterator;


/*==========================================================================
 * ReferenceItf
 *  represents a reference sequence
 */
class ReferenceItf
    extends Refcount
    implements Reference
{

    /***********************
     * Reference Interface *
     ***********************/

    /* getCommonName
     *  returns the common name of reference, e.g. "chr1"
     */
    public String getCommonName ()
        throws ErrorMsg
    {
        return this . GetCommonName ( self );
    }

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


    /* getIsLocal
     *  returns true if reference is local
     */
    public boolean getIsLocal ()
        throws ErrorMsg
    {
        return this . GetIsLocal ( self );
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


    /*----------------------------------------------------------------------
     * ALIGNMENTS
     */

    /* getAlignmentCount
     */
    public long getAlignmentCount ()
        throws ErrorMsg
    {
        return this . GetAlignmentCount ( self, Alignment . all );
    }

    public long getAlignmentCount ( int categories )
        throws ErrorMsg
    {
        return this . GetAlignmentCount ( self, categories );
    }

    /* getAlignment
     *  returns an individual Alignment
     *  throws ErrorMsg if Alignment does not exist
     *  or is not part of this Reference
     */
    public Alignment getAlignment ( String alignmentId )
        throws ErrorMsg
    {
        long ref = this . GetAlignment ( self, alignmentId );
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

    /* AlignmentCategory
     *  see Alignment for categories
     */

    /* getAlignments
     *  returns an iterator of contained alignments
     */
    public AlignmentIterator getAlignments ( int categories )
        throws ErrorMsg
    {
        long ref = this . GetAlignments ( self, categories );
        try
        {
            return new AlignmentIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    /* getAlignmentSlice
     *  returns an iterator across a range of Alignments
     *  "first" is an unsigned ordinal into set
     *  "categories" provides a means of filtering by AlignmentCategory
     */
    public AlignmentIterator getAlignmentSlice ( long offset, long length )
        throws ErrorMsg
    {
        return this . getAlignmentSlice ( offset, length, Alignment . all );
    }

    public AlignmentIterator getAlignmentSlice ( long offset, long length, int categories )
        throws ErrorMsg
    {
        long ref = this . GetAlignmentSlice ( self, offset, length, categories );
        try
        {
            return new AlignmentIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    /* getFilteredAlignmentSlice
     *  returns a filtered iterator across a slice of the Reference
     *  behaves like "getAlignmentSlice" except that supported filters are applied to selection
     *  "filters" is a set of filter bits defined in Alignment
     *  "mappingQuality" is a cutoff to be used according to bits in "filters"
     */
    public AlignmentIterator getFilteredAlignmentSlice ( long offset, long length, int categories, int filters, int mappingQuality )
        throws ErrorMsg
    {
        long ref = this . GetFilteredAlignmentSlice ( self, offset, length, categories, filters, mappingQuality );
        try
        {
            return new AlignmentIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }


    /*----------------------------------------------------------------------
     * PILEUP
     */

    /* getPileups
     *  returns an iterator of contained Pileups
     */
    public PileupIterator getPileups ( int categories )
        throws ErrorMsg
    {
        long ref = this . GetPileups ( self, categories );
        try
        {
            return new PileupIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    public PileupIterator getFilteredPileups ( int categories, int filters, int mappingQuality )
        throws ErrorMsg
    {
        long ref = this . GetFilteredPileups ( self, categories, filters, mappingQuality );
        try
        {
            return new PileupIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    /* getPileupSlice
     *  creates a PileupIterator on a slice (window) of reference
     *  "start" is the signed starting position on reference
     *  "length" is the unsigned number of bases in the window
     *  "categories" provides a means of filtering by AlignmentCategory
     */
    public PileupIterator getPileupSlice ( long offset, long length )
        throws ErrorMsg
    {
        return this . getPileupSlice ( offset, length, Alignment . all );
    }

    public PileupIterator getPileupSlice ( long offset, long length, int categories )
        throws ErrorMsg
    {
        long ref = this . GetPileupSlice ( self, offset, length, categories );
        try
        {
            return new PileupIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    public PileupIterator getFilteredPileupSlice ( long offset, long length, int categories, int filters, int mappingQuality )
        throws ErrorMsg
    {
        long ref = this . GetFilteredPileupSlice ( self, offset, length, categories, filters, mappingQuality );
        try
        {
            return new PileupIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    /*******************************
     * ReferenceItf Implementation *
     *******************************/

    // constructors
    ReferenceItf ( long ref )
    {
        super ( ref );
    }

    ReferenceItf ( Reference obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            ReferenceItf ref = ( ReferenceItf ) obj;
            this . self = ref . duplicate ();
        }
        catch ( Exception x )
        {
            throw new ErrorMsg ( x . toString () );
        }
    }

    // native interface
    private native String GetCommonName ( long self )
        throws ErrorMsg;
    private native String GetCanonicalName ( long self )
        throws ErrorMsg;
        private native boolean GetIsCircular ( long self )
        throws ErrorMsg;
    private native boolean GetIsLocal ( long self )
        throws ErrorMsg;
    private native long GetLength ( long self )
        throws ErrorMsg;
    private native String GetReferenceBases ( long self, long offset, long length )
        throws ErrorMsg;
    private native String GetReferenceChunk ( long self, long offset, long length )
        throws ErrorMsg;
    private native long GetAlignmentCount ( long self, int categories )
        throws ErrorMsg;
    private native long GetAlignment ( long self, String alignmentId )
        throws ErrorMsg;
    private native long GetAlignments ( long self, int categories )
        throws ErrorMsg;
    private native long GetAlignmentSlice ( long self, long offset, long length, int categories )
        throws ErrorMsg;
    private native long GetFilteredAlignmentSlice ( long self, long offset, long length, int categories, int filters, int mappingQuality )
        throws ErrorMsg;
    private native long GetPileups ( long self, int categories )
        throws ErrorMsg;
    private native long GetFilteredPileups ( long self, int categories, int filters, int mappingQuality )
        throws ErrorMsg;
    private native long GetPileupSlice ( long self, long offset, long count, int categories )
        throws ErrorMsg;
    private native long GetFilteredPileupSlice ( long self, long offset, long count, int categories, int filters, int mappingQuality )
        throws ErrorMsg;
}
