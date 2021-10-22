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
 * Represents a reference sequence standalone object
 */
public interface ReferenceSequence
{
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
}
