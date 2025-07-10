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

#pragma once

#include <kapp/extern.h>

#include <klib/defs.h>
#include <kapp/vdbapp.h>

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * KMain
 *  invoked by platform specific "main" entrypoint
 */

/* KMain - EXTERN
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
rc_t CC KMain ( int argc, char *argv [] );

/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
ver_t CC KAppVersion ( void );

#ifdef __cplusplus
}
#endif

// use VDB_INITIALIZE/VDB_TERMINATE to capture/convert/free argv
#define VDB_INITIALIZE(argc, argv, rc) do { if ( VdbInitialize( argc, argv, KAppVersion() ) ) return rc; } while (0)
#define VDB_TERMINATE(rc) VdbTerminate( rc )

// BSD is defined when compiling on Mac
// Use the MAC case below, not this one
#if BSD && !MAC
    #define MAIN_DECL(argc, argv) int main(int argc, char *argv[], char *envp[])
#endif

#if MAC
    #define MAIN_DECL(argc, argv) int main(int argc, char *argv[], char *envp[], char *apple[])
#endif

#if LINUX
#define MAIN_DECL(argc, argv) int main(int argc, char *argv[], char *envp[])
#endif

#if WINDOWS
    #if USE_WIDE_API

        #ifndef __cplusplus
            #define MAIN_DECL(argc, argv) int wmain(int argc, wchar_t *wargv[], wchar_t *envp[])
            // use this version of VDB_INITIALIZE/VDB_TERMINATE to capture/convert/free argv
            #undef  VDB_INITIALIZE
            #define VDB_INITIALIZE(argc, argv, rc) char ** argv = NULL; if ( wVdbInitialize( argc, wargv, &argv ) ) return rc;
            #undef VDB_TERMINATE 
            #define VDB_TERMINATE(rc) (free(argv), VdbTerminate( rc ))
        #else
            #define MAIN_DECL(argc, argv) int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
            // use VDB::Application to capture/convert/free argv
        #endif

    #else
        #define MAIN_DECL(argc, argv) int main(int argc, char *argv[], char *envp[])
    #endif
#endif
