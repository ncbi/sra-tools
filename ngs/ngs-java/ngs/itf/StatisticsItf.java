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
import ngs.Statistics;


/*==========================================================================
 * StatisticsItf
 */
class StatisticsItf
    extends Refcount
    implements Statistics
{
    
    public int getValueType ( String path )
    {
        return this . GetValueType ( self, path );
    }
    
    public String getAsString ( String path )
        throws ErrorMsg
    {
        return this . GetAsString ( self, path );
    }

    public long getAsI64 ( String path )
        throws ErrorMsg
    {
        return this . GetAsI64 ( self, path );
    }
            
    public long getAsU64 ( String path )
        throws ErrorMsg
    {
        return this . GetAsU64 ( self, path );
    }
        
    public double getAsDouble ( String path )
        throws ErrorMsg
    {
        return this . GetAsDouble ( self, path );
    }
            
    public String nextPath ( String path )
    {
        return this . NextPath ( self, path );
    }


    /***************************
     * StatisticsItf Implementation *
     ***************************/

    // constructors
    StatisticsItf ( long ref )
    {
        super ( ref );
    }

    StatisticsItf ( Statistics obj )
        throws ErrorMsg
    {
        super ( 0 );
        try
        {
            StatisticsItf ref = ( StatisticsItf ) obj;
            this . self = ref . duplicate ();
        }
        catch ( Exception x )
        {
            throw new ErrorMsg ( x . toString () );
        }
    }

    // native interface
    private native int GetValueType ( long self, String path );
    private native String GetAsString ( long self, String path )
        throws ErrorMsg;
    private native long GetAsI64 ( long self, String path )
        throws ErrorMsg;
    private native long GetAsU64 ( long self, String path )
        throws ErrorMsg;
    private native double GetAsDouble ( long self, String path )
        throws ErrorMsg;
    private native String NextPath ( long self, String path );
}
