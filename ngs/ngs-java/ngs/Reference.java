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
 * Represents a reference sequence
 */
public interface Reference
{

    /**
     * getCommonName
     * @return the common name of reference, e.g. "chr1"
	 * @throws ErrorMsg if no common name found
     */
    String getCommonName ()
        throws ErrorMsg;

    /**
     * getCanonicalName
     * @return the accessioned name of reference, e.g. "NC_000001.11"
	 * @throws ErrorMsg if no cannonical name found
     */
    String getCanonicalName ()
        throws ErrorMsg;


    /**
     * getIsCircular
     * @return true if reference is circular
	 * @throws ErrorMsg if cannot detect if reference is circular
     */
    boolean getIsCircular ()
        throws ErrorMsg;


    /**
     * getIsLocal
     * @return true if Reference is stored locally within the ReadCollection.
     *          Unique Reference sequences are stored locally, while  Publicly available Reference sequences are stored externally.
     *          An exception to this rule is when a public Reference is small,  in which case the sequence will be available both locally and externally
	 * @throws ErrorMsg if cannot detect if reference is local
     */
    boolean getIsLocal ()
        throws ErrorMsg;


    /**
     * getLength
     * @return the length of the reference sequence
	 * @throws ErrorMsg if length cannot be detected
     */
    long getLength ()
        throws ErrorMsg;


    /**
     * getReferenceBases
     * @param offset is zero-based and non-negative
     * @return sub-sequence bases for Reference
	 * @throws ErrorMsg if no reference-bases found at offset
     */
    String getReferenceBases ( long offset )
        throws ErrorMsg;

    /**
     * getReferenceBases
     * @param offset is zero-based and non-negative
     * @param length must be &ge; 0
     * @return sub-sequence bases for Reference
	 * @throws ErrorMsg if no reference-bases found at offset or lenght invalid
     */
    String getReferenceBases ( long offset, long length )
        throws ErrorMsg;

    /**
     * getReferenceChunk
     * @param offset is zero-based and non-negative
     * @return largest contiguous chunk available of sub-sequence bases for Reference
     * <p>
     *  NB - actual returned sequence may be shorter
     *  than requested. to obtain all bases available
     *  in chunk, use a negative "size" value
     * </p>
	 * @throws ErrorMsg if no ReferenceChunk found
     */
    String getReferenceChunk ( long offset )
        throws ErrorMsg;

    /**
     * getReferenceChunk
     * @param offset is zero-based and non-negative
     * @param length must be &gt; 0
     * @return largest contiguous chunk available of sub-sequence bases for Reference
     * <p>
     *  NB - actual returned sequence may be shorter
     *  than requested. to obtain all bases available
     *  in chunk, use a negative "size" value
     * </p>
	 * @throws ErrorMsg if no ReferenceChunk found
     */
    String getReferenceChunk ( long offset, long length )
        throws ErrorMsg;

    /*----------------------------------------------------------------------
     * ALIGNMENTS
     */

    /**
     * Count all Alignments within the Reference
     * @return 0 if there are no aligned Reads, &gt; 0 otherwise
     * @throws ErrorMsg upon an error accessing data
     */
    long getAlignmentCount ()
        throws ErrorMsg;
/**
     * Count selected Alignments within the Reference
     * @param categories is a bitfield of AlignmentCategory
     * @return count of all alignments
     * @see ngs.Alignment Alignment interface for definitions of AlignmentCategory
	 * @throws ErrorMsg if no AlignmentCount can be found
     */
    long getAlignmentCount ( int categories )
        throws ErrorMsg;

    /**
     * getAlignment
     * @return an individual Alignment
	 * @param alignmentId the ID of the Alignment to be returned
     * @throws ErrorMsg if Alignment does not exist or is not part of this Reference
     */
    Alignment getAlignment ( String alignmentId )
        throws ErrorMsg;

    /* AlignmentCategory
     * see Alignment for categories
     */

    /**
     * getAlignments
     * @return an iterator of contained alignments
	 * @param categories the or'ed list of categories
     * @throws ErrorMsg if no Iterator can be created
     */
    AlignmentIterator getAlignments ( int categories )
        throws ErrorMsg;

    /**
     * getAlignmentSlice
     * @param start is a signed 0-based offset from the start of the Reference
     * @param length is the length of the slice.
     * @return an iterator across a range of Alignments
     * @throws ErrorMsg if no Iterator can be created
     */
    AlignmentIterator getAlignmentSlice ( long start, long length )
        throws ErrorMsg;

    /**
     * getAlignmentSlice
     * @param start is a signed 0-based offset from the start of the Reference
     * @param length is the length of the slice.
     * @param categories provides a means of filtering by AlignmentCategory
     * @return an iterator across a range of Alignments
     * @throws ErrorMsg if no Iterator can be created
     */
    AlignmentIterator getAlignmentSlice ( long start, long length, int categories )
        throws ErrorMsg;

    /**
     * getFilteredAlignmentSlice
     * Behaves like "getAlignmentSlice" except that supported filters are applied to selection
     * @param start is a signed 0-based offset from the start of the Reference
     * @param length is the length of the slice.
     * @param categories provides a means of filtering by AlignmentCategory
     * @param filters is a set of filter bits defined in Alignment
     * @param mappingQuality is a cutoff to be used according to bits in "filter"
     * @return an iterator across a range of Alignments
     * @throws ErrorMsg if no Iterator can be created
     *
     */
    AlignmentIterator getFilteredAlignmentSlice ( long start, long length, int categories, int filters, int mappingQuality )
        throws ErrorMsg;


    /*----------------------------------------------------------------------
     * PILEUP
     */

    /**
     * getPileups
     * @return an iterator of contained Pileups
     * @param categories is a bitfield of AlignmentCategory
     * @throws ErrorMsg if no Iterator can be created
     */
    PileupIterator getPileups ( int categories )
        throws ErrorMsg;

    /**
     * getFilteredPileups
     * Filtered according to criteria in parameters
     * @param categories is a bitfield of AlignmentCategory
     * @param filters is a set of filter bits defined in Alignment
     * @param mappingQuality is a cutoff to be used according to bits in "filter"
     * @return an iterator of contained Pileups
     * @throws ErrorMsg if no Iterator can be created
     */
    PileupIterator getFilteredPileups ( int categories, int filters, int mappingQuality )
        throws ErrorMsg;

    /**
     * getPileupSlice
     * Creates a PileupIterator on a slice (window) of reference
     * @return an iterator of contained Pileups
     * @param start is the signed starting position on reference
     * @param length is the unsigned number of bases in the window
     * @throws ErrorMsg if no Iterator can be created
     */
    PileupIterator getPileupSlice ( long start, long length )
        throws ErrorMsg;

    /**
     * getPileupSlice
     * Creates a PileupIterator on a slice (window) of reference
     * @param start is the signed starting position on reference
     * @param length is the unsigned number of bases in the window
     * @param categories provides a means of filtering by AlignmentCategory
     * @return an iterator of contained Pileups
     * @throws ErrorMsg if no Iterator can be created
     */
    PileupIterator getPileupSlice ( long start, long length, int categories )
        throws ErrorMsg;

    /**
     * getFilteredPileupSlice
     * Creates a PileupIterator on a slice (window) of reference
     * filtered according to criteria in parameters
     * @param start is the signed starting position on reference
     * @param length is the unsigned number of bases in the window
     * @param categories provides a means of filtering by AlignmentCategory
     * @param filters is a set of filter bits defined in Alignment
     * @param mappingQuality is a cutoff to be used according to bits in "filter"
     * @return an iterator of contained Pileups
     * @throws ErrorMsg if no Iterator can be created
     */
    PileupIterator getFilteredPileupSlice ( long start, long length,
            int categories, int filters, int mappingQuality )
        throws ErrorMsg;
}
