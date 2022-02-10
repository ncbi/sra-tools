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
 *  Represents an alignment between a Fragment and Reference sub-sequence
 *  provides a path to Read and mate Alignment
 */
public interface Alignment
    extends Fragment
{

    /** 
     * Retrieve an identifying String that can be used for later access.
     * The id will be unique within ReadCollection.
     * @return alignment id
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getAlignmentId ()
        throws ErrorMsg;


    /*------------------------------------------------------------------
     * Reference
     */

    /** 
     * getReferenceSpec
     * @return the name of the reference	 
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getReferenceSpec ()
        throws ErrorMsg;

    /**
     * getMappingQuality 
     * @return mapping quality 
     * @throws ErrorMsg if the property cannot be retrieved
     */
    int getMappingQuality ()
        throws ErrorMsg;

    /** 
     * getReferenceBases
     * @return reference bases
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getReferenceBases ()
        throws ErrorMsg;


    /*------------------------------------------------------------------
     * Fragment
     */

    /**
     *  getReadGroup
     * @return the name of the read-group
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getReadGroup ()
        throws ErrorMsg;

    /** 
     * getReadId
     * @return the unique name of the read
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getReadId ()
        throws ErrorMsg;

    /** 
     * getClippedFragmentBases
     * @return clipped fragment bases
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getClippedFragmentBases ()
        throws ErrorMsg;

    /** 
     * getClippedFragmentQualities
     * @return clipped fragment phred quality values using ASCII offset of 33
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getClippedFragmentQualities ()
        throws ErrorMsg;

    /** 
     * getAlignedFragmentBases
     * @return fragment bases in their aligned orientation
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getAlignedFragmentBases ()
        throws ErrorMsg;

    /*------------------------------------------------------------------
     * details of this alignment
     */

    /* AlignmentFilter
     *  values should be or'd together to produce filter bits
     */
    static int passFailed = 1;         // reads rejected due to platform/vendor quality criteria
    static int passDuplicates = 2;     // either a PCR or optical duplicate
    static int minMapQuality = 4;      // pass alignments with mappingQuality >= param
    static int maxMapQuality = 8;      // pass alignments with mappingQuality <= param
    static int noWraparound = 16;      // do not include leading wrapped around alignments to circular references
    static int startWithinSlice = 32;  // change slice intersection criteria so that start pos is within slice

    /* AlignmentCategory
     */
    static int primaryAlignment   = 1;
    static int secondaryAlignment = 2;
    static int all                = primaryAlignment | secondaryAlignment;

    /** 
     * Alignments are categorized as primary or secondary (alternate).
     * @return either Alignment.primaryAlignment or Alignment.secondaryAlignment
     * @throws ErrorMsg if the property cannot be retrieved
     */
    int getAlignmentCategory ()
        throws ErrorMsg;

    /** 
     * Retrieve the Alignment's starting position on the Reference
     * @return unsigned 0-based offset from start of Reference
     * @throws ErrorMsg if the property cannot be retrieved
     */
    long getAlignmentPosition ()
        throws ErrorMsg;

    /**
     * Retrieve the projected length of an Alignment projected upon Reference.
     * @return unsigned length of projection
     * @throws ErrorMsg if the property cannot be retrieved
     */
    long getAlignmentLength ()
        throws ErrorMsg;

    /**
     * Test if orientation is reversed with respect to the Reference sequence.
     * @return true if reversed
     * @throws ErrorMsg if the property cannot be retrieved
     */
    boolean getIsReversedOrientation ()
        throws ErrorMsg;

    /* ClipEdge
     */
    static int clipLeft  = 0;
    static int clipRight = 1;

    /** 
     * getSoftClip
     * @return the position of the clipping
     * @param edge which edge
     * @throws ErrorMsg if the property cannot be retrieved
     */
    int getSoftClip ( int edge )
        throws ErrorMsg;

    /** 
     * getTemplateLength
     * @return the lenght of the template
     * @throws ErrorMsg if the property cannot be retrieved
     */
    long getTemplateLength ()
        throws ErrorMsg;

    /** 
     * getShortCigar
     * @param clipped selects if clipping has to be applied
     * @return a text string describing alignment details
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getShortCigar ( boolean clipped )
        throws ErrorMsg;

    /** 
     * getLongCigar
     * @param clipped selects if clipping has to be applied	 
     * @return a text string describing alignment details
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getLongCigar ( boolean clipped )
        throws ErrorMsg;

    /**
     * getRNAOrientation
     * @return '+' if positive strand is transcribed
     * @return '-' if negative strand is transcribed
     * @return '?' if unknown
     * @throws ErrorMsg if the property cannot be retrieved	 
     */
    char getRNAOrientation ()
        throws ErrorMsg;


    /*------------------------------------------------------------------
     * details of mate alignment
     */

    /** 
     * hasMate
     * @return if the alignment has a mate
     */
    boolean hasMate ();
        
    /** 
     * getMateAlignmentId
     * @return unique ID of th alignment
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getMateAlignmentId ()
        throws ErrorMsg;

    /** 
     * getMateAlignment
     * @return the mate as alignment
     * @throws ErrorMsg if the property cannot be retrieved
     */
    Alignment getMateAlignment ()
        throws ErrorMsg;

    /** 
     * getMateReferenceSpec
     * @return the name of the reference the mate is aligned at
     * @throws ErrorMsg if the property cannot be retrieved
     */
    String getMateReferenceSpec ()
        throws ErrorMsg;

    /**
     * getMateIsReversedOrientation
     * @return the orientation of the mate
     * @throws ErrorMsg if the property cannot be retrieved
     */
    boolean getMateIsReversedOrientation ()
        throws ErrorMsg;
}
