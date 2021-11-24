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
import ngs.Read;
import ngs.ReadIterator;


/*==========================================================================
 * ReadIteratorItf
 *  represents an NGS machine read
 *  having some number of biological Fragments
 */
class ReadIteratorItf
    extends ReadItf
    implements ReadIterator
{

    /**************************
     * ReadIterator Interface *
     **************************/

    /* nextRead
     *  advance to first Read on initial invocation
     *  advance to next Read subsequently
     *  returns false if no more Reads are available.
     *  throws exception if more Reads should be available,
     *  but could not be accessed.
     */
    public boolean nextRead ()
        throws ErrorMsg
    {
        return this . NextRead ( self );
    }


    /**********************************
     * ReadIteratorItf Implementation *
     **********************************/


    // constructors
    ReadIteratorItf ( long ref )
    {
        super ( ref );
    }

    ReadIteratorItf ( ReadIterator obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            ReadIteratorItf ref = ( ReadIteratorItf ) obj;
            this . self = ref . duplicate ();
        }
        catch ( Exception x )
        {
            throw new ErrorMsg ( x . toString () );
        }
    }

    // native interface
    private native boolean NextRead ( long self )
        throws ErrorMsg;
}
