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

#include <ngs/itf/ErrorMsg.hpp>

namespace ngs
{
    /*----------------------------------------------------------------------
     * ErrorMsg
     *  a generic NGS error class
     *  holds a message describing what happened
     */

    /* what ( for C++ )
     *  what went wrong
     */
    const char * ErrorMsg :: what () const
        NGS_NOTHROW ()
    {
        return msg . c_str ();
    }

    /* toMessage ( for Java )
     *  returns the detailed message
     */
    const :: std :: string & ErrorMsg :: toMessage () const
        NGS_NOTHROW ()
    {
        return msg;
    }

    /* toString ( for Java )
     *  returns a short description
     */
    const :: std :: string & ErrorMsg :: toString () const
        NGS_NOTHROW ()
    {
        return msg;
    }

    /* constructors
     *  various means of constructing
     */
    ErrorMsg :: ErrorMsg ()
        NGS_NOTHROW ()
    {
    }

    ErrorMsg :: ErrorMsg ( const :: std :: string & message )
            NGS_NOTHROW ()
        : msg ( message )
    {
    }

    ErrorMsg :: ErrorMsg ( const ErrorMsg & obj )
            NGS_NOTHROW ()
        : msg ( obj . msg )
    {
    }

    ErrorMsg & ErrorMsg :: operator = ( const ErrorMsg & obj )
        NGS_NOTHROW ()
    {
        this -> msg = obj . msg;
        return * this;
    }

    ErrorMsg :: ~ ErrorMsg ()
        NGS_NOTHROW ()
    {
    }

}
