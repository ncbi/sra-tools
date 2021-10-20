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
 *  Represents a single cell of a sparse 2D matrix with Reference coordinates on one axis
 *  and stacked Alignments on the other axis
 */
public interface PileupEvent
{

    /*----------------------------------------------------------------------
     * Reference
     */

    /** 
     * getMappingQuality
     * @return the mapping quality
     * @throws ErrorMsg upon an error accessing data
     */
    int getMappingQuality ()
        throws ErrorMsg;


    /*----------------------------------------------------------------------
     * Alignment
     */

    /**
     * getAlignmentId
     * @return unique AlignmentId within ReadCollection
     * @throws ErrorMsg upon an error accessing data
     */
    String getAlignmentId ()
        throws ErrorMsg;

    /** 
     * getAlignmentPosition
     * @return Position of the alignment in Reference coordinates
     * @throws ErrorMsg upon an error accessing data
     */
    long getAlignmentPosition ()
        throws ErrorMsg;

    /**
     * getFirstAlignmentPosition
     * @return the position of this Alignment's first event in Reference coordinates
     * @throws ErrorMsg upon an error accessing data
     */
    long getFirstAlignmentPosition ()
        throws ErrorMsg;

    /**
     * getLastAlignmentPosition
     * @return the position of this Alignment's last event in INCLUSIVE Reference coordinates
     * @throws ErrorMsg upon an error accessing data
     */
    long getLastAlignmentPosition ()
        throws ErrorMsg;


    /*----------------------------------------------------------------------
     * event details
     */

    /**
     *  EventType
     */
    
    // event types representable in reference coordinate space
    static int match                     = 0;
    static int mismatch                  = 1;
    static int deletion                  = 2;

    // an insertion cannot be represented in reference coordinate
    // space ( so no insertion event can be directly represented ),
    // but it can occur before a match or mismatch event.
    // insertion is represented as a bit
    static int insertion                 = 0x08;

    // insertions into the reference
    static int insertion_before_match    = insertion | match;
    static int insertion_before_mismatch = insertion | mismatch;

    // simultaneous insertion and deletion,
    // a.k.a. a replacement
    static int insertion_before_deletion = insertion | deletion;
    static int replacement               = insertion_before_deletion;

    // additional modifier bits - may be added to any event above
    static int alignment_start           = 0x80;
    static int alignment_stop            = 0x40;
    static int alignment_minus_strand    = 0x20;

    /**
     * getEventType
     * @return the type of event being represented
     * @throws ErrorMsg upon an error accessing data
     */
    int getEventType ()
        throws ErrorMsg;

    /**
     * getAlignmentBase
     * @return retrieves base aligned at current Reference position
     * @throws ErrorMsg if event is an insertion or deletion
     */
    char getAlignmentBase ()
        throws ErrorMsg;

    /** 
     * getAlignmentQuality
     * @return retrieves base aligned at current Reference position
     * @throws ErrorMsg if event is an insertion or deletion
     */
    char getAlignmentQuality ()
        throws ErrorMsg;


    /**
     * getInsertionBases
     * @return bases corresponding to insertion event
     * @throws ErrorMsg upon an error accessing data
     */
    String getInsertionBases ()
        throws ErrorMsg;

    /** 
     * getInsertionQualities
     * @return qualities corresponding to insertion event
     * @throws ErrorMsg upon an error accessing data
     */
    String getInsertionQualities ()
        throws ErrorMsg;

    /**
     * getEventRepeatCount
     * @return the number of times this event repeats, i.e. the distance to the first reference position yielding a different event type for this alignment
     * @throws ErrorMsg upon an error accessing data
     */
    int getEventRepeatCount ()
        throws ErrorMsg;

    /**
     * EventIndelType
     */

    static int normal_indel              = 0;

    // introns behave like deletions
    // (i.e. can retrieve deletion count),
    // "_plus" and "_minus" signify direction
    // of transcription if known
    static int intron_plus               = 1;
    static int intron_minus              = 2;
    static int intron_unknown            = 3;

    // overlap is reported as an insertion,
    // but is actually an overlap in the read
    // inherent in technology like Complete Genomics
    static int read_overlap              = 4;

    // gap is reported as a deletion,
    // but is actually a gap in the read
    // inherent in technology like Complete Genomics
    static int read_gap                  = 5;

    /**
     * getEventIndelType
     * @return detail about the type of indel when event type is an insertion or deletion
     * @throws ErrorMsg upon an error accessing data
     */
    int getEventIndelType ()
        throws ErrorMsg;
}
