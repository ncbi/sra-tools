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
import ngs.ReadCollection;
import ngs.ReadGroup;
import ngs.ReadGroupIterator;
import ngs.Read;
import ngs.ReadIterator;
import ngs.Reference;
import ngs.ReferenceIterator;
import ngs.Alignment;
import ngs.AlignmentIterator;


/*==========================================================================
 * ReadIteratorItf
 *  represents an NGS machine read
 *  having some number of biological Fragments
 */
public class ReadCollectionItf
    extends Refcount
    implements ReadCollection
{

    /****************************
     * ReadCollection Interface *
     ****************************/

    /* getName
     *  returns the simple name of the read collection
     *  this name is generally extracted from the "spec"
     *  used to create the object, but may also be mapped
     *  to a canonical name if one may be determined and
     *  differs from that given in the spec.
     *
     *  if the name is extracted from "spec" and contains
     *  well-known file extensions that do not form part of
     *  a canonical name (e.g. ".sra"), they will be removed.
     */
    public String getName ()
        throws ErrorMsg
    {
        return this . GetName ( self );
    }


    /*----------------------------------------------------------------------
     * READ GROUPS
     */

    /* getReadGroups
     *  returns an iterator of all ReadGroups used
     */
    public ReadGroupIterator getReadGroups ()
        throws ErrorMsg
    {
        long ref = this . GetReadGroups ( self );
        try
        {
            return new ReadGroupIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    /* hasReadGroup
     *  returns true if a call to "getReadGroup()" should succeed
     */
    public boolean hasReadGroup ( String spec )
    {
        return this . HasReadGroup ( self, spec );
    }

    /* getReadGroup
     */
    public ReadGroup getReadGroup ( String spec )
        throws ErrorMsg
    {
        long ref = this . GetReadGroup ( self, spec );
        try
        {
            return new ReadGroupItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }


    /*----------------------------------------------------------------------
     * REFERENCES
     */

    /* getReferences
     *  returns an iterator of all References used
     */
    public ReferenceIterator getReferences ()
        throws ErrorMsg
    {
        long ref = this . GetReferences ( self );
        try
        {
            return new ReferenceIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    /* hasReference
     *  returns true if a call to "getReference()" should succeed
     */
    public boolean hasReference ( String spec )
    {
        return this . HasReference ( self, spec );
    }

    /* getReference
     */
    public Reference getReference ( String spec )
        throws ErrorMsg
    {
        long ref = this . GetReference ( self, spec );
        try
        {
            return new ReferenceItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }


    /*----------------------------------------------------------------------
     * ALIGNMENTS
     */

    /* getAlignment
     *  returns an individual Alignment
     *  throws ErrorMsg if Alignment does not exist
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
     *  returns an iterator of all Alignments from specified categories
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


    /* getAlignmentCount
     *  returns count of all alignments
     *  "categories" provides a means of filtering by AlignmentCategory
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

    /* getAlignmentRange
     *  returns an iterator across a range of Alignments
     *  "first" is an unsigned ordinal into set
     *  "categories" provides a means of filtering by AlignmentCategory
     */
    public AlignmentIterator getAlignmentRange ( long first, long count )
        throws ErrorMsg
    {
        return this . getAlignmentRange ( first, count, Alignment . all );
    }

    public AlignmentIterator getAlignmentRange ( long first, long count, int categories )
        throws ErrorMsg
    {
        long ref = this . GetAlignmentRange ( self, first, count, categories );
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
     * READS
     */

    /* getRead
     *  returns an individual Read
     *  throws ErrorMsg if Read does not exist
     */
    public Read getRead ( String readId )
        throws ErrorMsg
    {
        long ref = this . GetRead ( self, readId );
        try
        {
            return new ReadItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    /* ReadCategory
     *  see Read for categories
     */

    /* getReads
     *  returns an iterator of all contained machine Reads
     */
    public ReadIterator getReads ( int categories )
        throws ErrorMsg
    {
        long ref = this . GetReads ( self, categories );
        try
        {
            return new ReadIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    /* getReadCount
     *  returns the number of reads in the collection
     *  of all combined categories
     */
    public long getReadCount ()
        throws ErrorMsg
    {
        return this . GetReadCount ( self, Read . all );
    }

    public long getReadCount ( int categories )
        throws ErrorMsg
    {
        return this . GetReadCount ( self, categories );
    }

    /* getReadRange
     *  returns an iterator across a range of Reads
     */
    public ReadIterator getReadRange ( long first, long count )
        throws ErrorMsg
    {
        return getReadRange ( first, count, Read . all );
    }

    public ReadIterator getReadRange ( long first, long count, int categories )
        throws ErrorMsg
    {
        long ref = this . GetReadRange ( self, first, count, categories );
        try
        {
            return new ReadIteratorItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }


    /************************************
     * ReadCollectionItf Implementation *
     ************************************/

    // constructors
    public ReadCollectionItf ( long ref )
    {
        super ( ref );
    }

    ReadCollectionItf ( ReadCollection obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            ReadCollectionItf ref = ( ReadCollectionItf ) obj;
            this . self = ref . duplicate ();
        }
        catch ( Exception x )
        {
            throw new ErrorMsg ( x . toString () );
        }
    }

    // native interface
    private native String GetName ( long self )
        throws ErrorMsg;
    private native long GetReadGroups ( long self )
        throws ErrorMsg;
    private native boolean HasReadGroup ( long self, String spec );
    private native long GetReadGroup ( long self, String spec )
        throws ErrorMsg;
    private native long GetReferences ( long self )
        throws ErrorMsg;
    private native boolean HasReference ( long self, String spec );
    private native long GetReference ( long self, String spec )
        throws ErrorMsg;
    private native long GetAlignment ( long self, String alignmentId )
        throws ErrorMsg;
    private native long GetAlignments ( long self, int categories )
        throws ErrorMsg;
    private native long GetAlignmentCount ( long self, int categories )
        throws ErrorMsg;
    private native long GetAlignmentRange ( long self, long first, long count, int categories )
        throws ErrorMsg;
    private native long GetRead ( long self, String readId )
        throws ErrorMsg;
    private native long GetReads ( long self, int categories )
        throws ErrorMsg;
    private native long GetReadCount ( long self, int categories )
        throws ErrorMsg;
    private native long GetReadRange ( long self, long first, long count, int categories )
        throws ErrorMsg;
}
