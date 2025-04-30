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

#include <kapp/main.h>

#include <kapp/vdbapp.h>

#include <klib/log.h>
#include <klib/rc.h>

/* KMane
 *  executable entrypoint "main" is implemented by
 *  an OS-specific wrapper that takes care of establishing
 *  signal handlers, logging, etc.
 *
 *  in turn, OS-specific "main" will invoke "KMain" as
 *  platform independent main entrypoint.
 *
 *  "argc" [ IN ] - the number of textual parameters in "argv"
 *  should never be < 0, but has been left as a signed int
 *  for reasons of tradition.
 *
 *  "argv" [ IN ] - array of NUL terminated strings expected
 *  to be in the shell-native character set: ASCII or UTF-8
 *  element 0 is expected to be executable identity or path.
 */

rc_t KMane ( int argc, char *argv [] )
{
    rc_t rc = VdbInitialize( argc, argv, GetKAppVersion () );
    if ( rc != 0 )
    {
        return rc;
    }

    rc = KMain ( argc, argv );
    if ( rc != 0 )
    {
#if _DEBUGGING
        rc_t rc2;
        uint32_t lineno;
        const char *filename, *function;
        while ( GetUnreadRCInfo ( & rc2, & filename, & function, & lineno ) )
        {
            pLogErr ( klogWarn, rc2, "$(filename):$(lineno) within $(function)"
                        , "filename=%s,lineno=%u,function=%s"
                        , filename
                        , lineno
                        , function
                );
        }
#endif
    }

    VdbTerminate( rc );

    return rc;
}
