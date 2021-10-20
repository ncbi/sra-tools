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
import ngs.Fragment;
import ngs.FragmentIterator;


/*==========================================================================
 * FragmentItf
 *  represents an NGS biological fragment
 */
class FragmentItf
    extends Refcount
    implements Fragment
{

    /**********************
     * Fragment Interface *
     **********************/

    /* getFragmentId
     */
    public String getFragmentId ()
        throws ErrorMsg
    {
        return this . GetFragmentId ( self );
    }


    /*----------------------------------------------------------------------
     * fragment details
     */

    /* getFragmentBases
     *  return sequence bases
     *  "offset" is zero-based
     *  "length" must be >= 0
     */
    public String getFragmentBases ()
        throws ErrorMsg
    {
        return this . GetFragmentBases ( self, 0, -1 );
    }

    public String getFragmentBases ( long offset )
        throws ErrorMsg, IndexOutOfBoundsException
    {
        if ( offset < 0 )
            throw new IndexOutOfBoundsException ( "offset " + offset + " is negative" );

        return this . GetFragmentBases ( self, offset, -1 );
    }

    public String getFragmentBases ( long offset, long length )
        throws ErrorMsg, IndexOutOfBoundsException
    {
        if ( offset < 0 )
            throw new IndexOutOfBoundsException ( "offset " + offset + " is negative" );
        if ( length < 0 )
            throw new IndexOutOfBoundsException ( "length " + length + " is negative");

        return this . GetFragmentBases ( self, offset, length );
    }


    /* getFragmentQualities
     *  return phred quality values
     *  using ASCII offset of 33
     *  "offset" is zero-based
     *  "length" must be >= 0
     */
    public String getFragmentQualities ()
        throws ErrorMsg
    {
        return this . GetFragmentQualities ( self, 0, -1 );
    }

    public String getFragmentQualities ( long offset )
        throws ErrorMsg, IndexOutOfBoundsException
    {
        if ( offset < 0 )
            throw new IndexOutOfBoundsException ( "offset " + offset + " is negative" );

        return this . getFragmentQualities ( offset, -1 );
    }

    public String getFragmentQualities ( long offset, long length )
        throws ErrorMsg, IndexOutOfBoundsException
    {
        if ( offset < 0 )
            throw new IndexOutOfBoundsException ( "offset " + offset + " is negative" );
        if ( length < 0 )
            throw new IndexOutOfBoundsException ( "length " + length + " is negative");

        return this . GetFragmentQualities ( self, offset, length );
    }

    public boolean isPaired()
        throws ErrorMsg
    {
        return this . IsPaired ( self );
    }
    
    public boolean isAligned ()
        throws ErrorMsg
    {
        return this . IsAligned ( self );
    }


    /******************************
     * FragmentItf Implementation *
     ******************************/


    // constructors
    FragmentItf ( long ref )
    {
        super ( ref );
    }

    FragmentItf ( Fragment obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            FragmentItf ref = ( FragmentItf ) obj;
            this . self = ref . duplicate ();
        }
        catch ( Exception x )
        {
            throw new ErrorMsg ( x . toString () );
        }
    }

    // native interface
    private native String GetFragmentId ( long self )
        throws ErrorMsg;
    private native String GetFragmentBases ( long self, long offset, long length )
        throws ErrorMsg;
    private native String GetFragmentQualities ( long self, long offset, long length )
        throws ErrorMsg;
    private native boolean IsPaired ( long self )
        throws ErrorMsg;
    private native boolean IsAligned ( long self )
        throws ErrorMsg;
}
