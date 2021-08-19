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
 *<p>
 * Represents an NGS-capable object with a collection of
 * <strong>Reads</strong>, <strong>References</strong>
 * and <strong>Alignments</strong>.
 *</p>
 *<p>
 * Each of the basic content types may be accessed by <strong>id</strong>
 * as either a standalone object, or more commonly through
 * an <strong>Iterator</strong> over a selected collection of objects.
 *</p>
 *<p>
 * <em>Reads</em> are grouped by <strong>ReadGroup</strong>. When
 * not specifically assigned, <em>Reads</em> will be placed into the
 * <strong>default</strong> <em>ReadGroup</em>.
 *</p>
 */
public interface ReadCollection
{

    /**
     * <p>
     *  Access the simple name of the ReadCollection.
     *  This name is generally extracted from the "spec"
     *  used to create the object, but may also be mapped
     *  to a canonical name if one may be determined and
     *  differs from that given in the spec.
     * </p>
     * <p>
     *  if the name is extracted from "spec" and contains
     *  well-known file extensions that do not form part of
     *  a canonical name (e.g. ".sra"), they will be removed.
     * </p>
     *
     * @return the simple name of the ReadCollection
     * @throws ErrorMsg if the name cannot be retrieved
     * @see gov.nih.nlm.ncbi.ngs.NGS NGS class for "spec" definition
     */
    String getName ()
        throws ErrorMsg;


    /*----------------------------------------------------------------------
     * READ GROUPS
     */

    /**
     * Access all non-empty ReadGroups.
     * Iterator will contain at least one ReadGroup
     * unless the ReadCollection itself is empty.
     *
     * @return an unordered Iterator of ReadGroups
     * @throws ErrorMsg only upon an error accessing data
     */
    ReadGroupIterator getReadGroups ()
        throws ErrorMsg;

    /**
     * @param spec the name of a contained read group    
     * @return true if a call to "getReadGroup()" should succeed
     */
    boolean hasReadGroup ( String spec );

    /**
     * Access a single ReadGroup by name.
     *
     * @param spec the name of a contained read group
     * @return an instance of the designated ReadGroup
     * @throws ErrorMsg if specified ReadGroup is not found
     * @throws ErrorMsg upon an error accessing data
     */
    ReadGroup getReadGroup ( String spec )
        throws ErrorMsg;


    /*----------------------------------------------------------------------
     * REFERENCES
     */

    /**
     * Access all References having aligned Reads.
     * Iterator will contain at least one ReadGroup
     * unless no Reads are aligned.
     *
     * @return an unordered Iterator of References
     * @throws ErrorMsg upon an error accessing data
     */
    ReferenceIterator getReferences ()
        throws ErrorMsg;

    /**
     * @param spec the name of a contained Reference
     * @return true if a call to "getReference()" should succeed
     */
    boolean hasReference ( String spec );

    /**
     * Access a single Reference by name spec.
     *
     * @param spec a common or canonical name of a contained reference
     * @return an instance of the designated Reference
     * @throws ErrorMsg if the Reference is not found
     * @throws ErrorMsg upon an error accessing data
     */
    Reference getReference ( String spec )
        throws ErrorMsg;


    /*----------------------------------------------------------------------
     * ALIGNMENTS
     */

    /**
     * Access a single Alignment by id.
     * The object id is unique within any given ReadCollection,
     * and may designate an Alignment of any category.
     *
     * Note Excessive usage may create pressure on JVM and System memory.
     *
     * @param alignmentId the ID of the alignment to return
     * @return an individual Alignment instance
     * @throws ErrorMsg if Alignment is not found
     * @throws ErrorMsg upon an error accessing data
     */
    Alignment getAlignment ( String alignmentId )
        throws ErrorMsg;

    /**
     * Select Alignments by AlignmentCategory.
     * @param categories is a bitfield of AlignmentCategory	 
     * @return an iterator of all Alignments from specified categories
     * @throws ErrorMsg upon an error accessing data
     */
    AlignmentIterator getAlignments ( int categories )
        throws ErrorMsg;

    /** 
     * Count all Alignments within the ReadCollection
     * @return 0 if there are no aligned Reads, &gt; 0 otherwise
     * @throws ErrorMsg upon an error accessing data
     */
    long getAlignmentCount ()
        throws ErrorMsg;
    
    /** 
     * Count selected Alignments within the ReadCollection
     * @param categories is a bitfield of AlignmentCategory
     * @return count of all alignments
     * @see ngs.Alignment Alignment interface for definitions of AlignmentCategory
     * @throws ErrorMsg upon an error accessing data	 
     */
    long getAlignmentCount ( int categories )
        throws ErrorMsg;

    /** 
     * getAlignmentRange
     * @param first is an unsigned ordinal into set
     * @param count the number of alignments
     * @return an iterator across a range of Alignments
     * @throws ErrorMsg upon an error accessing data	 
     */
    AlignmentIterator getAlignmentRange ( long first, long count )
        throws ErrorMsg;
    
    /** 
     * getAlignmentRange
     * @param first is an unsigned ordinal into set
     * @param count number of alignments
     * @param categories provides a means of filtering by AlignmentCategory
     * @return an iterator across a range of Alignments
     * @throws ErrorMsg upon an error accessing data	 
     */
  AlignmentIterator getAlignmentRange ( long first, long count, int categories )
        throws ErrorMsg;


    /*----------------------------------------------------------------------
     * READS
     */

    /** 
     * getRead
     * @param readId the ID of the Read to return	 
     * @return an individual Read
     * @throws ErrorMsg if Read does not exist
     */
    Read getRead ( String readId )
        throws ErrorMsg;

    /* ReadCategory
     *  see Read for categories
     */

    /**
     * getReads
     * @param categories provides a means of filtering by ReadCategory
     * @return an iterator of all contained machine Reads
     * @throws ErrorMsg upon an error accessing data	 	 
     */
    ReadIterator getReads ( int categories )
        throws ErrorMsg;

    /**
     * getReadCount
     * @return the number of reads in the collection
     * @throws ErrorMsg upon an error accessing data	 	 
     */
    long getReadCount ()
        throws ErrorMsg;

    /**
     * getReadCount
     * @param categories provides an optional means of filtering by ReadCategory
     * @return the number of reads in the collection
     * @throws ErrorMsg upon an error accessing data	 	 
     */
    long getReadCount ( int categories )
        throws ErrorMsg;

    /** 
     * getReadRange
     * @param first is an unsigned ordinal into set
     * @param count the number of reads
     * @return an iterator across a range of Reads
     * @throws ErrorMsg upon an error accessing data	 	 
     */
    ReadIterator getReadRange ( long first, long count )
        throws ErrorMsg;
    /** 
     * getReadRange
     * @param first is an unsigned ordinal into set
     * @param count the number of reads
     * @param categories provides an optional means of filtering by ReadCategory
     * @return an iterator across a range of Reads
     * @throws ErrorMsg upon an error accessing data	 	 
     */
    ReadIterator getReadRange ( long first, long count, int categories )
        throws ErrorMsg;
}
