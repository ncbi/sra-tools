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

#include <klib/rc.h>
#include <klib/text.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <vfs/manager.h>
#include <vfs/resolver.h>
#include <vfs/path.h>

#include <kns/manager.h>
#include <kns/http.h>
#include <kns/kns-mgr-priv.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>


/*******************************************************************************
 *  Here we will try to resolve path
 */

rc_t
kar_resolve_path (
                    const char * AccessionOrPath,
                    char * RetBuf,
                    size_t BufSize,
                    bool * IsLocal
)
{
    rc_t RCt;
    struct VFSManager * Manager;
    struct VResolver * Resolver;
    struct VPath * Query;
    const struct VPath * Remote;
    const struct VPath * Local;
    const struct VPath * Path;
    const String * Str;

    RCt = 0;
    Manager = NULL;
    Resolver = NULL;
    Query = NULL;
    Remote = NULL;
    Local = NULL;
    Path = NULL;
    Str = NULL;

    if ( IsLocal != NULL ) {
        * IsLocal = true;
    }

    if ( AccessionOrPath == NULL ) {
        return RC ( rcExe, rcPath, rcSearching, rcParam, rcNull );
    }

    if ( RetBuf == NULL || BufSize == 0 ) {
        return RC ( rcExe, rcPath, rcSearching, rcParam, rcInvalid );
    }

    RCt = VFSManagerMake ( & Manager );
    if ( RCt == 0 ) {
        RCt = VFSManagerGetResolver ( Manager, & Resolver );
        if ( RCt == 0 ) {
            RCt = VFSManagerMakePath(
                                    Manager,
                                    & Query,
                                    "ncbi-acc:%s",
                                    AccessionOrPath
                                    );
            if ( RCt == 0 ) {
/*  JOJOBA: Do we really need to do it here ? Prolly we will need
            additional argument to programm
                VResolverRemoteEnable( Resolver, vrAlwaysEnable );
                VResolverLocalEnable ( Resolver, vrAlwaysEnable );
*/
                RCt = VResolverQuery (
                                    Resolver,
                                    0, 
                                    Query,
                                    & Local,
                                    & Remote,
                                    NULL 
                                    );
                if ( RCt == 0 ) {
                        /*  I believe that local path has priority
                         *  and will be used at first.
                         */
                    Path = Local == NULL ? Remote : Local;
                    if ( Path == NULL ) {
                        RCt = RC ( rcExe, rcPath, rcSearching, rcName, rcNotFound );
                    }
                    else {
                        * IsLocal = Local != NULL;
                        RCt = VPathMakeString( Path, & Str );
                        if ( RCt == 0 ) {
                            if ( Str -> size + 1 >= BufSize ) {
                                RCt = RC ( rcExe, rcPath, rcSearching, rcSize, rcInsufficient );
                            }
                            else {
                                string_copy (
                                            RetBuf,
                                            BufSize,
                                            Str -> addr,
                                            Str -> size
                                            );
                                RetBuf [ Str -> size ] = 0;
                            }

                                /*  Lol, whack is 'free'
                                 */
                            StringWhack ( Str );
                        }
                    }
                }

                if ( Remote != NULL ) {
                    VPathRelease ( Remote );
                }

                if ( Local != NULL ) {
                    VPathRelease ( Local );
                }

                VPathRelease ( Query );
            }

            VResolverRelease ( Resolver );
        }

        VFSManagerRelease ( Manager );
    }

    return RCt;
}   /* kar_resolve_path () */

rc_t
kar_open_file_read (
                struct KDirectory * Dir,
                const struct KFile ** File,
                const char * PathOrAccession

)
{
    rc_t RCt;
    struct KNSManager * Manager;
    char ResolvedPath [ 2048 ];
    struct KDirectory * NatDir;
    bool IsLocal;
    int PathType;
    const char * PathToOpen;

    RCt = 0;
    Manager = NULL;
    NatDir = NULL;
    IsLocal = false;
    PathType = 0;
    PathToOpen = NULL;

    if ( File != NULL ) {
        * File = NULL;
    }

    if ( PathOrAccession == NULL ) {
        return RC ( rcExe, rcFile, rcOpening, rcParam, rcNull );
    }

    if ( File == NULL ) {
        return RC ( rcExe, rcFile, rcOpening, rcParam, rcInvalid );
    }
        /*  First we will try to find/open it as a local file
         *  and after that we will go and try to resolve as accession
         */
    NatDir = Dir;
    if ( NatDir == NULL ) {
        RCt = KDirectoryNativeDir ( & NatDir );
    }
    if ( RCt == 0 ) {
        PathType = KDirectoryPathType ( NatDir, PathOrAccession );
        if ( ( PathType & ~kptAlias ) == kptFile ) {
            PathToOpen = PathOrAccession;
            IsLocal = true;
        }
        else {
            RCt = kar_resolve_path (
                                    PathOrAccession,
                                    ResolvedPath,
                                    sizeof ( ResolvedPath ),
                                    & IsLocal
                                    );
            PathToOpen = ResolvedPath;
        }

        if ( RCt == 0 ) {
            if ( IsLocal ) {
                RCt = KDirectoryOpenFileRead (
                                                NatDir,
                                                File,
                                                "%s",
                                                PathToOpen
                                                );

            }
            else {
                RCt = KNSManagerMake ( & Manager );
                if ( RCt == 0 ) {
                    RCt = KNSManagerMakeReliableHttpFile (
                                                        Manager,
                                                        File,
                                                        NULL,
                                                        0x01010000,
                                                        true,
                                                        false,
                                                        false,
                                                        "%s",
                                                        PathToOpen
                                                        );
/*
                    RCt = KNSManagerMakeHttpFile (
                                                    Manager,
                                                    File,
                                                    NULL,
                                                    0x01010000,
                                                    "%s",
                                                    PathToOpen
                                                    );
*/
                }
                KNSManagerRelease ( Manager );
            }

            if ( Dir == NULL ) {
                KDirectoryRelease ( NatDir );
            }
        }
    }

    return RCt;
}   /* kar_open_file_read () */
