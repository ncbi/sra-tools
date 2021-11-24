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
import ngs.ReadGroupIterator;


/*==========================================================================
 * ReadGroupItf
 *  represents an NGS-capable object with a group of Reads
 */
class ReadGroupIteratorItf
    extends ReadGroupItf
    implements ReadGroupIterator
{

    /*******************************
     * ReadGroupIterator Interface *
     *******************************/

    /* nextReadGroup
     *  advance to first ReadGroup on initial invocation
     *  advance to next ReadGroup subsequently
     *  returns false if no more ReadGroups are available.
     *  throws exception if more ReadGroups should be available,
     *  but could not be accessed.
     */
    public boolean nextReadGroup ()
        throws ErrorMsg
    {
        return this . NextReadGroup ( self );
    }


    /***************************************
     * ReadGroupIteratorItf Implementation *
     ***************************************/

    // constructors
    ReadGroupIteratorItf ( long ref )
    {
        super ( ref );
    }

    ReadGroupIteratorItf ( ReadGroupIterator obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            ReadGroupIteratorItf ref = ( ReadGroupIteratorItf ) obj;
            this . self = ref . duplicate ();
        }
        catch ( Exception x )
        {
            throw new ErrorMsg ( x . toString () );
        }
    }

    // native interface
    private native boolean NextReadGroup ( long self )
        throws ErrorMsg;
}
