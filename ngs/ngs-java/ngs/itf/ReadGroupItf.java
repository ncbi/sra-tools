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
import ngs.ReadGroup;
import ngs.Read;
import ngs.ReadIterator;
import ngs.Statistics;


/*==========================================================================
 * ReadGroupItf
 *  represents an NGS-capable object with a group of Reads
 */
class ReadGroupItf
    extends Refcount
    implements ReadGroup
{

    /***********************
     * ReadGroup Interface *
     ***********************/

    /* getName
     *  returns the simple name of the read group
     */
    public String getName ()
        throws ErrorMsg
    {
        return this . GetName ( self );
    }


    /*------------------------------------------------------------------
     * READS
     *  [ support for reads has been removed ]
     */

    /* getRead
     *  returns an individual Read
     *  throws ErrorMsg if Read does not exist
     *  or is not part of this ReadGroup
     */
/*
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
*/

    /* ReadCategory
     *  see Read for category definitions
     */

    /* getReads
     *  returns an iterator of all contained machine Reads
     */
/*
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
*/


    public Statistics getStatistics ()
        throws ErrorMsg
    {
        long ref = this . GetStatistics ( self );
        try
        {
            return new StatisticsItf ( ref );
        }
        catch ( Exception x )
        {
            this . release ( ref );
            throw new ErrorMsg ( x . toString () );
        }
    }

    /************************************
     * ReadGroupItf Implementation *
     ************************************/

    // constructors
    ReadGroupItf ( long ref )
    {
        super ( ref );
    }

    ReadGroupItf ( ReadGroup obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            ReadGroupItf ref = ( ReadGroupItf ) obj;
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
/*
    private native long GetRead ( long self, String readId )
        throws ErrorMsg;
    private native long GetReads ( long self, int categories )
        throws ErrorMsg;
*/
    private native long GetStatistics ( long self )
        throws ErrorMsg;
}
