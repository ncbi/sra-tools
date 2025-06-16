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

#define UNICODE 1
#define _UNICODE 1

#include "../main-priv.h"
#include <kapp/win/main-priv-win.h>
#include <klib/rc.h>

#include <WINDOWS.H>

/*--------------------------------------------------------------------------
 * Main
 */

static bool convert_args_paths = true;
#if 0
int __cdecl wmain ( int argc, wchar_t *wargv [], wchar_t *envp [] )
{
    char **argv = NULL;
    int status = 0;

    /* must initialize COM... must initialize COM... */
    /* CoInitializeEx ( NULL, COINIT_MULTITHREADED ); */
    CoInitialize(NULL);

    status = ConvertWArgsToUtf8(argc, wargv, &argv, convert_args_paths);

    if ( status == 0 )
    {
        /* perform normal main operations on UTF-8 with POSIX-style paths */
        rc_t rc = KMane(argc, argv);
        status = (rc == 0) ? 0 : IF_EXITCODE(rc, 3);

        /* tear down argv */
        int i = argc;
        while ( -- i >= 0 )
            free ( argv [ i ] );
        free ( argv );
    }

    /* balance the COM initialization */
    CoUninitialize ();

    return status;
}
#endif

extern int wmainCRTStartup();

void  __declspec(dllexport) __stdcall wmainCRTStartupNoPathConversion()
{
    convert_args_paths = false;
    wmainCRTStartup();
}
