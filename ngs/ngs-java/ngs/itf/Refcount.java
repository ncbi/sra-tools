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

/*==========================================================================
 * Refcount
 *  manages reference in C heap
 */
class Refcount
{

    // constructors
    Refcount ( long ref )
    {
        this . self = ref;
    }

    // implementation details
    long duplicate ()
        throws ErrorMsg
    {
        return this . Duplicate ( self );
    }

    synchronized void invalidate ()
    {
        this . Release ( self );
        self = 0;
    }

    protected void finalize ()
    {
        if ( self != 0 )
        {
            this . Release ( self );
            self = 0;
        }
    }

    protected static void release ( long ref )
    {
        ReleaseRef ( ref );
    }

    // native interface
    private native long Duplicate ( long self )
        throws ErrorMsg;
    private native void Release ( long self );
    private native static void ReleaseRef ( long ref );

    protected long self;
}
