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
#include <kapp/args.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/rc.h>
#include <klib/container.h>
#include <klib/refcount.h>
#include <kns/manager.h>
#include <kns/http.h>
#include <kns/stream.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/cacheteefile.h>
#include <kproc/lock.h>
#include <vfs/path.h>
#include <vfs/manager.h>
#include <kapp/main.h>

#include <os-native.h>

#include "remote-cache.h"

#include <stdlib.h>
#include <string.h>
#include <sysalloc.h>

#include "log.h"

/*)))
 /// Some unusual macroses
(((*/

/*) There are methods which are using VPath to get schema and host
 (*/

typedef rc_t ( CC * _PathReader ) (
                                const VPath * self,
                                char * Buffer,
                                size_t BufferSize,
                                size_t * NumRead
                                );
static
rc_t CC
_ReadSomething (
            const char * Path,
            char * Buffer,
            size_t BufferSize,
            _PathReader Reader
)
{
    rc_t RCt;
    VFSManager * Manager;
    VPath * ThePath;
    size_t NumRead;

    RCt = 0;
    NumRead = 0;
    Manager = NULL;
    ThePath = NULL;

    if ( Path == NULL || Buffer == NULL || BufferSize <= 0 || Reader == NULL ) {
        return RC ( rcExe, rcPath, rcReading, rcParam, rcNull );
    }

    * Buffer = 0;

    RCt = VFSManagerMake ( & Manager );
    if ( RCt == 0 ) {
        RCt = VFSManagerMakePath ( Manager, & ThePath, Path );
        if ( RCt == 0 ) {
            RCt = Reader ( ThePath, Buffer, BufferSize, & NumRead );

            ReleaseComplain ( VPathRelease, ThePath );
        }

        ReleaseComplain ( VFSManagerRelease, Manager );
    }

    return RCt;
}   /* _ReadSomething () */

static
rc_t CC
_ReadSchema ( const char * Path, char * Buffer, size_t BufferSize )
{
    return _ReadSomething ( Path, Buffer, BufferSize, VPathReadScheme );
}   /* _ReadSchema () */

#ifdef IDDQD
static
rc_t CC
_ReadHost ( const char * Path, char * Buffer, size_t BufferSize )
{
    return _ReadSomething ( Path, Buffer, BufferSize, VPathReadHost );
}   /* _ReadHost () */
#endif /* IDDQD */

bool CC
_MatchSchemas ( const char * Schema1, const char * Schema2 )
{
    size_t S1Size, S2Size;

    if ( Schema1 == NULL || Schema2 == NULL ) { 
        return false;
    }

    S1Size = string_size ( Schema1 );
    S2Size = string_size ( Schema2 );

    if ( S1Size == S2Size ) {
        if ( string_cmp (
                        Schema1,
                        S1Size,
                        Schema2,
                        S1Size,
                        S1Size
                        ) == 0 ) {
            return true;
        }
    }

    return false;
}   /* _MatchSchemas () */

bool CC
IsRemotePath ( const char * Path )
{
    char Schema [ 256 ];
    bool IsRemote;


    IsRemote = false;

    if ( Path == NULL ) {
        return false;
    }

    if ( _ReadSchema ( Path, Schema, sizeof ( Schema ) ) != 0 ) {
        return false;
    }

    IsRemote = _MatchSchemas ( Schema, "http" );

/* Just for case, he-he ... YAGNI - S!CKS
    if ( IsRemote == false ) {
        IsRemote = _MatchSchemas ( Schema, "https" );
    }
*/
    return IsRemote;
}   /* IsRemotePath () */

bool CC
IsLocalPath ( const char * Path )
{
    KDirectory * NativeDir;
    uint32_t PathType;

    NativeDir = NULL;
    PathType = kptNotFound;

        /*  Just checking if Path do exists, and it is a file
         */
    if ( Path != NULL ) {
        if ( KDirectoryNativeDir ( & NativeDir ) == 0 ) {
            PathType = KDirectoryPathType ( NativeDir, Path );

            KDirectoryRelease ( NativeDir );
        }
    }

    return PathType == kptFile;
}   /* IsLocalPath () */

/*)))
 ///  Cache ... hmmm
(((*/
static KNSManager * _ManagerOfKNS = NULL;
static BSTree _Cache;
    /* That lock will be used for adding/searching cache entries */
static KLock * _CacheLock = NULL;

const char * _CacheEntryClassName = "RCacheEntry_class";
const char * _CacheDirName = ".cache";
static char _CacheRoot [ 4096 ];
static char * _PCacheRoot = NULL;
static size_t _CacheEntryNo = 0;
static uint32_t _HttpBlockSize = 0;
static bool _DisklessMode = false;

struct RCacheEntry {
    BSTNode AsIs;

    KRefcount refcount;
    KLock * mutabor;

    char * Name;
    char * Url;

    const KFile * File;
};

/*))
 //  Some extremely useful methods
((*/
rc_t CC
_EGetCachePath (
            const char * CacheRoot,
            char * Buffer,
            size_t BufferSize
)
{
    size_t NumWrit;

    if ( CacheRoot == NULL || Buffer == NULL || BufferSize == 0 ) {
        return RC ( rcExe, rcString, rcCopying, rcParam, rcNull );
    }

    return string_printf (
                        Buffer,
                        BufferSize,
                        & NumWrit,
                        "%s/%s",
                        CacheRoot,
                        _CacheDirName
                        );
}   /* _EGetCachePath () */

rc_t CC
_GetCachePath ( char * Buffer, size_t BufferSize )
{
    return _EGetCachePath ( RemoteCachePath (), Buffer, BufferSize );
}   /* _GetCachePath () */

rc_t CC
_EGetCachePathOld (
                const char * CacheRoot,
                char * Buffer,
                size_t BufferSize
)
{
    size_t NumWrit;

    if ( CacheRoot == NULL || Buffer == NULL || BufferSize == 0 ) {
        return RC ( rcExe, rcString, rcCopying, rcParam, rcNull );
    }

    return string_printf (
                        Buffer,
                        BufferSize,
                        & NumWrit,
                        "%s/%s.old",
                        CacheRoot,
                        _CacheDirName
                        );
}   /* _EGetCachePathOld () */

rc_t CC
_GetCachePathOld ( char * Buffer, size_t BufferSize )
{
    return _EGetCachePathOld ( RemoteCachePath (), Buffer, BufferSize );
}   /* _GetCachePathOld () */

rc_t CC
_CheckCreateDirectory ( const char * Path )
{
    rc_t RCt;
    KDirectory * NativeDir;
    uint32_t PathType;

    RCt = 0;
    NativeDir = NULL;
    PathType = kptNotFound;

    if ( Path == NULL ) {
        return RC ( rcExe, rcDirectory, rcCreating, rcParam, rcNull );
    }

    RCt = KDirectoryNativeDir ( & NativeDir );
    if ( RCt == 0 ) {
        PathType = KDirectoryPathType ( NativeDir, Path );
        switch ( PathType ) {
            case kptNotFound :
                        RmOutMsg ( "Creating directory '%s'\n", Path );
                        RCt = KDirectoryCreateDir (
                                                NativeDir,
                                                0777,
                                                kcmCreate,
                                                Path
                                                );
                        break;
            case kptDir :
                        break;
            default :
                            /* Somtheing is wrong */
                        RCt = RC ( rcExe, rcDirectory, rcCreating, rcError, rcInvalid );
                        break;
        }

        ReleaseComplain ( KDirectoryRelease, NativeDir );
    }

    return RCt;
}   /* _CheckCreateDirectory () */

rc_t CC
_CheckRemoveDirectory ( const char * Path )
{
    rc_t RCt;
    KDirectory * NativeDir;
    uint32_t PathType;

    RCt = 0;
    NativeDir = NULL;
    PathType = kptNotFound;

    if ( Path == NULL ) {
        return RC ( rcExe, rcDirectory, rcRemoving, rcParam, rcNull );
    }

    RCt = KDirectoryNativeDir ( & NativeDir );
    if ( RCt == 0 ) {
        PathType = KDirectoryPathType ( NativeDir, Path );
        switch ( PathType ) {
            case kptNotFound:
                    /* Everything is good */
                    break;
            default:
                RmOutMsg ( "Removing directory '%s'\n", Path );
                RCt = KDirectoryRemove ( NativeDir, true, Path );
                break;
        }

        ReleaseComplain ( KDirectoryRelease, NativeDir );
    }

    return RCt;
}   /* _CheckRemoveDirectory () */

rc_t CC
_CheckRemoveOldCacheDirectory ( const char * CacheRoot )
{
    rc_t RCt;
    KDirectory * NativeDir;
    char CacheDir [ 4096 ];
    char OldCacheDir [ 4096 ];

    RCt = 0;
    NativeDir = NULL;
    * CacheDir = 0;
    * OldCacheDir = 0;

    if ( CacheRoot == NULL ) {
        return RC ( rcExe, rcDirectory, rcRemoving, rcParam, rcNull );
    }

    if ( _EGetCachePath ( CacheRoot, CacheDir, sizeof ( CacheDir ) ) != 0
        || _EGetCachePathOld ( CacheRoot, OldCacheDir, sizeof ( OldCacheDir ) ) )
    {
        return RC ( rcExe, rcDirectory, rcRemoving, rcParam, rcInvalid );
    }

    RCt = KDirectoryNativeDir ( & NativeDir );
    if ( RCt == 0 ) {
            /*) Just because I do not know wahat to do in the case if
            (*) it is impossible to remove directory I am doing that
            (*) hookup
            (*/
        RCt = _CheckRemoveDirectory ( OldCacheDir );
        if ( RCt == 0 ) {
            RCt = KDirectoryRename (
                                NativeDir,
                                true,
                                CacheDir,
                                OldCacheDir
                                );
            if ( RCt == 0 ) {
                RCt = _CheckRemoveDirectory ( OldCacheDir );
            }
        }

        ReleaseComplain ( KDirectoryRelease, NativeDir );
    }

    return RCt;
}   /* _CheckRemoveOldCacheDirectory () */

#ifdef LOPPPATA
######
rc_t CC
_EGetCachePathToFile (
                const char * CacheRoot,
                const char * PathToFile,
                char * Buffer,
                size_t BufferSize
)
{
    size_t NumWrit;

    if ( Buffer == NULL
        || BufferSize == 0
        || PathToFile == NULL
        || CacheRoot == NULL
        )
    {
        return RC ( rcExe, rcString, rcCopying, rcParam, rcNull );
    }

    return string_printf (
                        Buffer,
                        BufferSize,
                        & NumWrit,
                        "%s/%s/%s",
                        CacheRoot,
                        CacheDirName,
                        PathToFile
                        );
}   /* _EGetCachePathToFile () */

rc_t CC
_GetCachePathToFile (
                const char * PathToFile,
                char * Buffer,
                size_t BufferSize
)
{
    return _EGetCachePathToFile (
                            RemoteCachePath (),
                            PathToFile,
                            Buffer,
                            BufferSize
                            );
}   /* _GetCachePathToFile () */
#endif /* LOPPPATA */

/*
 *  Lyrics: This method will set buffer size for HTTP transport
 *  and return previous value.
 *  That method is thread unsafe, and it is better to set it once
 *  on the time of cache initialization
 */
uint32_t CC
RemoteCacheSetHttpBlockSize ( uint32_t HttpBlockSize )
{
    uint32_t RetVal = _HttpBlockSize;

    _HttpBlockSize = HttpBlockSize;

    return RetVal;
}   /* RemoteCacheSetHttpBlockSize () */

/*
 * Lyrics: Cache initialising: keeping in memory cache path 
 *         cache path could be a NULL, and in that case no cacheing
 *         performed
 */
rc_t CC
RemoteCacheInitialize ( const char * Path )
{
    rc_t RCt;
    char Buffer [ 4096 ];
    struct KDirectory * Directory;

    RCt = 0;
    * _CacheRoot = 0;
    * Buffer = 0;

    if ( RemoteCacheIsDisklessMode () ) {
        LOGMSG ( klogErr, "[RemoteCache] Already initialized in diskless mode" );

        return RC ( rcExe, rcPath, rcInitializing, rcParam, rcNull );
    }

        /*))
         //  If path is NULL - diskless mode ... we introduced
        //   variable for that ... to avoid confusion
       ((*/
    if ( Path == NULL ) {
        LOGMSG ( klogErr, "[RemoteCache] initializing [diskless]" );

        _DisklessMode = true;

        return 0;
    }

    PLOGMSG ( klogErr, ( klogErr, "[RemoteCache] initializing [$(f)]", PLOG_S(f), Path ) );

        /* we are checking that cache directory already initialized */
    if ( _PCacheRoot != NULL ) {
        return RC ( rcExe, rcPath, rcInitializing, rcSelf, rcExists );
    }

    RCt = KDirectoryNativeDir ( & Directory );
    if ( RCt == 0 ) {
        RCt = KDirectoryResolvePath (
                                Directory,
                                true,
                                _CacheRoot,
                                sizeof ( _CacheRoot ),
                                Path
                                );
    }

    if ( RCt == 0 ) {
        _PCacheRoot = _CacheRoot;
    }

    return RCt;
}   /*  RemoteCacheInitialize () */

static
rc_t CC
_InitKNSManager ()
{
    rc_t RCt;
    KNSManager * Manager;
    ver_t Ver;

    RCt = 0;
    Manager = NULL;
    Ver = KAppVersion ();

    if ( _ManagerOfKNS != NULL ) {
        return RC ( rcExe, rcNS, rcInitializing, rcSelf, rcInvalid );
    }

    RCt = KNSManagerMake ( & Manager );
    if ( RCt == 0 ) {
        RCt = KNSManagerSetUserAgent (
                                    Manager,
                                    "sra-toolkit remote-fuser.%V",
                                    Ver
                                    );
        if ( RCt == 0 ) {
            _ManagerOfKNS = Manager;
        }
        else {
            ReleaseComplain ( KNSManagerRelease, Manager );
        }
    }

    return RCt;
}   /* _InitKNSManager () */

static
rc_t CC
_DisposeKNSManager ()
{
    KNSManager * Manager = _ManagerOfKNS;

    _ManagerOfKNS = NULL;

    if ( Manager == NULL ) {
        return 0;
    }

    ReleaseComplain ( KNSManagerRelease, Manager );

    return 0;
}   /* _DisposeKNSManager () */

/*
 * Lyrics: Cache make 
 * Cache initialisation consists from two steps :
 *    Creating directory if it does not exist
 *    Removing content of directory if it is something here
 */
rc_t CC
RemoteCacheCreate ()
{
    rc_t RCt;
    char Buffer [ 4096 ];

    if ( RemoteCacheIsDisklessMode () ) {
        LOGMSG( klogInfo, "[RemoteCache] entering diskless mode\n" );
        return 0;
    }

    LOGMSG( klogInfo, "[RemoteCache] creating\n" );

    RCt = 0;
    * Buffer = 0;

        /* standard c=ecks */
        /* we are checking that cache directory already initialized */
    if ( _PCacheRoot == NULL ) {
        return RC ( rcExe, rcPath, rcInitializing, rcSelf, rcNull );
    }

        /* Checking if CacheRoot directory exists and creating if not */
    RCt = _CheckCreateDirectory ( _PCacheRoot );
    if ( RCt == 0 ) {
            /* Here we are moving old cache path
             */
        RCt = _CheckRemoveOldCacheDirectory ( _PCacheRoot );
        if ( RCt ) {
            RCt = _EGetCachePath (
                                _PCacheRoot,
                                Buffer,
                                sizeof ( Buffer )
                                );
            if ( RCt == 0 ) {

                RCt = _CheckCreateDirectory ( Buffer );
            }
        }
    }

    if ( RCt == 0 ) {

        RCt = _InitKNSManager ();
        if ( RCt == 0 ) {

                /* Initializing BSTree */
            BSTreeInit ( & _Cache );
                /* Initializing _CacheLock */
            RCt = KLockMake ( & _CacheLock );
        }
    }

    if ( RCt != 0 ) {
            /* Endangered specie TODO!!! */
        RemoteCacheDispose ();
    }

    return RCt;
}   /* RemoteCacheCreate () */

rc_t CC _RCacheEntryDestroy ( struct RCacheEntry * Entry );

void CC
_RcAcHeEnTrYwHaCk ( BSTNode * Node, void * UnusedParam )
{
    if ( Node != NULL ) {
        _RCacheEntryDestroy ( ( struct RCacheEntry * ) Node );
    }
}   /* _RcAcHeEnTrYwHaCk () */

/*
 * Lyrics: Cache finalization 
 * Cache finalization consists from one step(s) :
 *    Removing content of cache directory if it exists and
 *    something is here
 */
rc_t CC
RemoteCacheDispose ()
{
    rc_t RCt;

    if ( RemoteCacheIsDisklessMode () ) {
        LOGMSG( klogInfo, "[RemoteCache] leaving diskless mode\n" );
    }

    LOGMSG( klogInfo, "[RemoteCache] disposing\n" );

    RCt = 0;

    if ( RemoteCacheIsDisklessMode () ) {
        _DisklessMode = false;

        return 0;
    }

        /*) Cache was not initialized
         (*/
    if ( _PCacheRoot == NULL ) {
        return 0;
    }

    RCt = _CheckRemoveOldCacheDirectory ( _CacheRoot );
    if ( RCt == 0 ) {
        * _CacheRoot = 0;
        _PCacheRoot = NULL;
    }

        /* Releasing Lock */
    if ( _CacheLock != NULL ) {
        ReleaseComplain ( KLockRelease, _CacheLock );
        _CacheLock = NULL;
    }

    _DisposeKNSManager ();

    BSTreeWhack ( & _Cache, _RcAcHeEnTrYwHaCk, NULL );

    _CacheEntryNo = 0;

    * _CacheRoot = 0;
    _PCacheRoot = NULL;

    return RCt;
}   /* RemoteCacheDispose () */

/*))
 //  Generates effective name for a file
((*/
rc_t CC
_RCacheEntryGenerateName ( char * Buffer, size_t Size )
{
    size_t NumWritten;

    NumWritten = 0;

    if ( Buffer == NULL || Size <= 2 ) {
        return RC ( rcExe, rcFile, rcInitializing, rcParam, rcNull );
    }

    return string_printf (
                        Buffer,
                        Size,
                        & NumWritten,
                        "etwas.%d",
                        _CacheEntryNo + 1
                        );
}   /* _RCacheEntryGenerateName () */

/*))
 //  This method will destroy CacheEntry and free all resources
((*/
rc_t CC
_RCacheEntryDestroy ( struct RCacheEntry * self )
{
    if ( self != NULL ) {
 /*
 RmOutMsg ( "++++++DL DESTROY [0x%p] entry\n", self );
 */
        /*)) Reverse order. I suppose it will be destoryed only
         //  in particualr cases, so no locking :|
        ((*/
            /*) File
             (*/
        if ( self -> File != NULL ) {
            ReleaseComplain ( KFileRelease, self -> File );
            self -> File = 0;
        }
            /*) Url
             (*/
        if ( self -> Url != NULL ) {
            free ( self -> Url );
            self -> Url = NULL;
        }
            /*) Name
             (*/
        if ( self -> Name != NULL ) {
            free ( self -> Name );
            self -> Name = NULL;
        }
            /*) mutabor
             (*/
        if ( self -> mutabor != NULL ) {
            ReleaseComplain ( KLockRelease, self -> mutabor );
            self -> mutabor = NULL;
        }
            /*) refcount 
             (*/
        KRefcountWhack ( & ( self -> refcount ), _CacheEntryClassName ); 

        free ( self );
    }

    return 0;
}   /* _RCacheEntryDestroy () */

/*))
 //  This method will create new CacheEntry
((*/
rc_t CC
_RCacheEntryMake (
            const char * Url,
            struct RCacheEntry ** RetEntry
)
{
    rc_t RCt;
    struct RCacheEntry * Entry;
    char Buffer [ 4096 ];

    RCt = 0;

    if ( Url == NULL || RetEntry == NULL ) {
        return RC ( rcExe, rcFile, rcInitializing, rcParam, rcNull );
    }
    * RetEntry = NULL;

    if ( ! RemoteCacheIsDisklessMode () ) {
            /*)  It is better do it here, before any allocation
             (*/
        RCt = _RCacheEntryGenerateName ( Buffer, sizeof ( Buffer ) );
        if ( RCt != 0 ) {
            return RCt;
        }
    }

    Entry = ( struct RCacheEntry * ) calloc (
                                        1,
                                        sizeof ( struct RCacheEntry )
                                        );
    if ( Entry == NULL ) {
        return RC ( rcExe, rcFile, rcInitializing, rcMemory, rcExhausted );
    }

        /*) refcount
         (*/
    KRefcountInit (
                & ( Entry -> refcount ),
                0,
                _CacheEntryClassName,
                "_RCacheEntryMake()",
                Buffer
                );
        /*) mutabor
         (*/
    RCt = KLockMake ( & ( Entry -> mutabor ) );

    if ( RCt == 0 ) {
        if ( ! RemoteCacheIsDisklessMode () ) {
                /*) Name
                 (*/
            Entry -> Name = string_dup_measure ( Buffer, NULL );
            if ( Entry -> Name == NULL ) {
                RCt = RC ( rcExe, rcFile, rcInitializing, rcMemory, rcExhausted );
            }
        }

        if ( RCt == 0 ) {
                /*) Url
                 (*/
            Entry -> Url = string_dup_measure ( Url, NULL );
            if ( Entry -> Url == NULL ) {
                RCt = RC ( rcExe, rcFile, rcInitializing, rcMemory, rcExhausted );
            }
            if ( RCt == 0 ) {
                    /*) File, nothing about file, it will be opened by
                     /  on demand
                    (*/
                    /*) Increasing count and assigning value
                     (*/
                _CacheEntryNo ++;
                * RetEntry = Entry;
            }
        }
    }

    if ( RCt != 0 && Entry != NULL ) {
        _RCacheEntryDestroy ( Entry );
    }

    return RCt;
}   /*  _RCacheEntryMake () */

/*))
 //  Comparator: common case
((*/
int CC
_RcUrLcMp ( const char * Url1, const char * Url2 )
{
    if ( Url1 == NULL || Url2 == NULL ) {
        if ( Url1 != NULL ) {
            return 4096;
        }
        if ( Url2 != NULL ) {
            return 4096;
        }
        return 0;
    }

    return strcmp ( Url1, Url2 );
}   /* _RcUrLcMp () */

/*))
 //  Comparator: we suppose that 'item' is an Url
((*/
int CC
_RcEnTrYcMp ( const void * item, const BSTNode * node )
{
    return _RcUrLcMp (
                    ( const char * ) item,
                    node == NULL
                        ? NULL
                        : ( ( struct RCacheEntry * ) node ) -> Url
                    );
}   /* _RcEnTrYcMp () */

/*))
 //  Sorter for BSTreeInsert
((*/
int CC
_RcNoDeCmP ( const BSTNode * node1, const BSTNode * node2 )
{
    return _RcUrLcMp (
                    node1 == NULL
                        ? NULL
                        : ( ( struct RCacheEntry * ) node1 ) -> Url,
                    node2 == NULL
                        ? NULL
                        : ( ( struct RCacheEntry * ) node2 ) -> Url
                    );
}   /* _RcNoDeCmP () */

/*))
 //  This method suppeosed to find or create Entry
((*/
rc_t CC
RemoteCacheFindOrCreateEntry (
                            const char * Url,
                            struct RCacheEntry ** Entry
)
{
    rc_t RCt;
    struct RCacheEntry * RetEntry;

    RCt = 0;
    RetEntry = NULL;

    if ( Url == NULL || Entry == NULL ) {
        return RC ( rcExe, rcPath, rcInitializing, rcParam, rcNull );
    }
    * Entry = NULL;

        /*)  Diskless mode
         (*/
    if ( RemoteCacheIsDisklessMode () ) {
        RCt = _RCacheEntryMake ( Url, & RetEntry );
        if ( RCt == 0 ) {
 /*
 RmOutMsg ( "++++++DL CREATE [0x%p][%s] entry\n", RetEntry, Url );
 */
            * Entry = RetEntry;
        }

        return RCt;
    }
        /*)  Here we are locking
         (*/
    RCt = KLockAcquire ( _CacheLock );
    if ( RCt == 0 ) {
            /*)  Here we are 'looking for' and 'fooking lor'
             (*/
        RetEntry = ( struct RCacheEntry * ) BSTreeFind (
                                                    & _Cache,
                                                    Url,
                                                    _RcEnTrYcMp
                                                    );
/*
 RmOutMsg ( "++++++ %s entry\n", RetEntry == NULL ? "Creating" : "Loading" );
*/
        if ( RetEntry == NULL ) {
            RCt = _RCacheEntryMake ( Url, & RetEntry );
            if ( RCt == 0 ) {
                RCt = BSTreeInsert (
                                & _Cache,
                                ( BSTNode * ) RetEntry,
                                _RcNoDeCmP
                                );
            }
        }

        if ( RCt == 0 ) {
            * Entry = RetEntry;
        }

            /*)  First we are trying to find appropriate entry
             (*/
        ReleaseComplain ( KLockUnlock, _CacheLock );
    }

    return RCt;
}   /* RemoteCacheFindOrCreateEntry () */

const char * CC
RemoteCachePath ()
{
    return RemoteCacheIsDisklessMode() ? NULL : _PCacheRoot;
}   /* RemoteCachePath () */

bool CC
RemoteCacheIsDisklessMode ()
{
    return _DisklessMode;
}   /* RemoteCacheIsDisklessMode () */

/*)))
 ///  Top of the crop methods
(((*/
rc_t CC
RCacheEntryAddRef ( struct RCacheEntry * self )
{
    rc_t RCt;

    RCt = 0;

    if ( self != NULL ) {
        RCt = KLockAcquire ( self -> mutabor );

        if ( RCt == 0 ) {
 /*
 RmOutMsg ( "++++++DL ADDREF [0x%p] entry\n", self );
 */
            switch ( KRefcountAdd (
                            & ( self -> refcount ),
                            _CacheEntryClassName
                            ) ) {
                case krefLimit:
                    RCt = RC ( rcExe, rcFile, rcAttaching, rcRange, rcExcessive );
                case krefNegative:
                    RCt = RC ( rcExe, rcFile, rcAttaching, rcSelf, rcInvalid );
                default:
                    break;
            }

            KLockUnlock ( self -> mutabor );
        }
    }

    return RCt;
}   /* RCacheEntryAddRef () */

rc_t CC
_RCacheEntryReleaseWithoutLock ( struct RCacheEntry * self )
{
    /*)) This method called from special place, so no NULL checks
     ((*/

    if ( self -> File != NULL ) {
/*
RmOutMsg ( "|||<-- Releasing [%s][%s]\n", self -> Name, self -> Url );
*/
        ReleaseComplain ( KFileRelease, self -> File );
        self -> File = NULL;
    }

    return 0;
}   /*  _RCacheEntryReleaseWithoutLock () */

rc_t CC
RCacheEntryRelease ( struct RCacheEntry * self )
{
    rc_t RCt;

    RCt = 0;

    if ( self != NULL ) {
        RCt = KLockAcquire ( self -> mutabor );

        if ( RCt == 0 ) {
            switch ( KRefcountDrop (
                            & ( self -> refcount ),
                            _CacheEntryClassName
                            ) ) {
                case krefWhack:
                    _RCacheEntryReleaseWithoutLock ( self );
                    if ( RemoteCacheIsDisklessMode () ) {
 /*
 RmOutMsg ( "++++++DL RELEASE [0x%p] entry\n", self );
 */
                        _RCacheEntryDestroy ( self );
                    }
                    break;
                case krefNegative:
                    RCt = RC ( rcExe, rcFile, rcReleasing, rcRange, rcExcessive );
                default:
                    break;
            }

            KLockUnlock ( self -> mutabor );
        }
    }

    return RCt;
}   /* RCacheEntryRelease () */

rc_t CC
_RCacheEntryOpenFileReadRemote (
                            struct RCacheEntry * self,
                            const char * Path
)
{
    rc_t RCt;
    struct KDirectory * Directory;
    const struct KFile * HttpFile, * TeeFile;

    RCt = 0;
    Directory = NULL;
    HttpFile = TeeFile = NULL;

    if ( self == NULL ) {
        return RC ( rcExe, rcFile, rcOpening, rcParam, rcNull );
    }

    if ( ( ! RemoteCacheIsDisklessMode () ) && Path == NULL ) {
        return RC ( rcExe, rcFile, rcOpening, rcParam, rcNull );
    }

/*
RmOutMsg ( "|||<-- Opening [R] [%s][%s]\n", self -> Name, self -> Url );
RmOutMsg ( "  |<-- Cache Entry [%s]\n", Path );
*/

    RCt = KNSManagerMakeHttpFile (
                                _ManagerOfKNS,
                                & HttpFile,
                                NULL, /* no open connections */
                                0x01010000,
                                self -> Url
                                );
    if ( RCt == 0 ) {
        if ( RemoteCacheIsDisklessMode () ) {
            self -> File = ( KFile * ) HttpFile;
        }
        else {
            RCt = KDirectoryNativeDir ( & Directory );
            if ( RCt == 0 ) {
                RCt = KDirectoryMakeCacheTee (
                                    Directory,
                                    & TeeFile,
                                    HttpFile,
                                    NULL,   /* logger */
                                    _HttpBlockSize, /* blocksize */
                                    1,      /* cluster */
                                    false,  /* report */
                                    Path
                                    );
                if ( RCt == 0 ) {
                    self -> File = ( KFile * ) TeeFile;
                }

                ReleaseComplain ( KDirectoryRelease, Directory );
            }
            ReleaseComplain ( KFileRelease, HttpFile );
        }
    }

    return RCt;
}   /* _RCacneEntryOpenFileReadRemote () */

rc_t CC
_RCacheEntryOpenFileReadLocal (
                            struct RCacheEntry * self,
                            const char * Path
)
{
    rc_t RCt;
    const struct KFile * File;
    struct KDirectory * Directory;

    RCt = 0;
    File = NULL;
    Directory = NULL;

    if ( self == NULL || Path == NULL ) {
        return RC ( rcExe, rcFile, rcOpening, rcParam, rcNull );
    }

/*
RmOutMsg ( "|||<-- Opening [L] [%s][%s]\n", self -> Name, self -> Url );
RmOutMsg ( "  |<-- Cache Entry [%s]\n", Path );
*/

    RCt = KDirectoryNativeDir ( & Directory );
    if ( RCt == 0 ) {
        RCt = KDirectoryOpenFileRead ( Directory, & File, Path );
        if ( RCt == 0 ) {
            self -> File = File;
        }

        ReleaseComplain ( KDirectoryRelease, Directory );
    }

    return RCt;
}   /* _RCacneEntryOpenFileReadLocal () */

/*((
   \\  The only way to check that cache file completed, is to check if
    \\  it exists
     ((*/
bool CC
_RCacheCheckCompleted ( const char * Path )
{
    uint32_t PathType;
    KDirectory * Directory;

    PathType = kptNotFound;
    Directory = NULL;

    if ( KDirectoryNativeDir ( & Directory ) == 0 ) {
        PathType = KDirectoryPathType ( Directory, Path );
        ReleaseComplain ( KDirectoryRelease, Directory );
    }

    return PathType == kptFile;
}   /* _RCacheCheckCompleted () */

rc_t CC
_RCacheEntryOpenFileRead ( struct RCacheEntry * self)
{
    rc_t RCt;
    char Buffer [ 4096 ], ThePath [ 4096 ];
    size_t TheSize;

    RCt = 0;
    TheSize = 0;
    * Buffer = 0;
    * ThePath = 0;

    if ( self == NULL ) {
        return RC ( rcExe, rcFile, rcOpening, rcParam, rcNull );
    }

    if ( self -> File != NULL ) {
        return 0;
    }

        /*)  Do that before messing with disk
         (*/
    if ( RemoteCacheIsDisklessMode () ) {
        RCt = _RCacheEntryOpenFileReadRemote ( self, NULL );
    }
    else {

            /*)  First we should to make path to real cache file
             (*/
        RCt = _GetCachePath ( Buffer, sizeof ( Buffer ) );
        if ( RCt == 0 ) {
            RCt = string_printf (
                                ThePath,
                                sizeof ( ThePath ),
                                & TheSize,
                                "%s/%s",
                                Buffer,
                                self -> Name
                                );
        }

        if ( RCt != 0 ) {
            return RCt;
        }

        if ( _RCacheCheckCompleted ( ThePath ) ) {
            RCt = _RCacheEntryOpenFileReadLocal ( self, ThePath );
        }
        else {
            RCt = _RCacheEntryOpenFileReadRemote ( self, ThePath );
        }

    }

    return RCt;
}   /* _RCacheEntryOpenFileRead () */

rc_t CC
RCacheEntryRead (
            struct RCacheEntry * self,
            char * Buffer,
            size_t SizeToRead,
            uint64_t Offset,
            size_t * NumReaded
)
{
    rc_t RCt;
    int llp;
    const int NumAttempts = 3;

    RCt = 0;
    llp = 0;

    if ( self == NULL ) { 
        return RC ( rcExe, rcFile, rcReading, rcParam, rcNull );
    }

        /*)  Here we are locking
         (*/
    RCt = KLockAcquire ( self -> mutabor );

    if ( RCt == 0 ) {

        for ( llp = 0; llp < NumAttempts; llp ++ ) {
                /*) There could be non zero value from previous pass
                 (*/
            if ( RCt != 0 ) {
PLOGMSG ( klogErr, ( klogErr, "|||<- Trying to read file $(n)$(u) at attempt $(l)", PLOG_3(PLOG_S(n),PLOG_S(u),PLOG_I64(l)), self -> Name, self -> Url, llp + 1 ) );
                RCt = 0;
            }

                /*) If error happen on previous pass file is released
                 (*/
            if ( self -> File == NULL ) {
                /*)  We are opening file for read here
                 (*/
                RCt = _RCacheEntryOpenFileRead ( self );
/*
RmOutMsg ( "|||<-- Opening file [%s][%s] [A=%d]\n", self -> Name, self -> Url, RCt );
*/
            }

            if ( RCt == 0 ) {
                RCt = KFileRead (
                                self -> File,
                                Offset,
                                Buffer,
                                SizeToRead,
                                NumReaded
                                );
/*
RmOutMsg ( "|||<-- Reading [%s][%s] [O=%d][S=%d][R=%d][A=%d]\n", self -> Name, self -> Url, Offset, SizeToRead, * NumReaded, RCt );
*/
                if ( RCt == 0 ) {
                    break;
                }
            }
/*
RmOutMsg ( "|||<- Failed to read file [%s][%s] at attempt [%d]\n", self -> Name, self -> Url, llp + 1 );
*/
            _RCacheEntryReleaseWithoutLock ( self );

        }

        if ( RCt != 0 ) {
PLOGMSG ( klogErr, ( klogErr, "|||<- Failed to read file $(n)$(u) after $(l) attempts", PLOG_3(PLOG_S(n),PLOG_S(u),PLOG_I64(l)), self -> Name, self -> Url, llp + 1 ) );
        }

        KLockUnlock ( self -> mutabor );
    }

    return RCt;
}   /* RCacheEntryRead () */

/*)))
 ///  That method will read KFile into a memory.
 \\\  He-he, it will allocate +1 byte array and set 0 to last one.
 ///  It suppose to read text files, so, 0 terminated string
(((*/
rc_t CC
_ReadKFileToMemory (
                const KFile * File,
                char ** RetBuffer,
                uint64_t * RetSize
)
{
    rc_t RCt;
    char * Buf;
    size_t BufSize;

    RCt = 0;
    Buf = NULL;
    BufSize = 0;

    * RetBuffer = NULL;
    * RetSize = 0;

    if ( File == NULL || RetBuffer == NULL || RetSize == NULL ) {
        return RC ( rcExe, rcFile, rcReading, rcParam, rcNull );
    }

    RCt = KFileSize ( File, & BufSize );
    if ( RCt == 0 ) {

        if ( BufSize != 0 ) {

            Buf = ( char * ) calloc ( BufSize + 1, sizeof ( char ) );
            if ( Buf == NULL ) {
                RCt = RC ( rcExe, rcFile, rcAllocating, rcParam, rcNull );
            }
            else {
                RCt = KFileReadExactly ( File, 0, Buf, BufSize );
                if ( RCt == 0 ) {
                    * ( Buf + BufSize ) = 0;
                    * RetBuffer = Buf;
                    * RetSize = BufSize;
                }
                else {
                    free ( Buf );
                    RCt = RC ( rcExe, rcFile, rcReading, rcParam, rcFailed );
                    Buf = NULL;
                    BufSize = 0;
                }
            }
        }
        else {
            RCt = RC ( rcExe, rcFile, rcReading, rcParam, rcEmpty );
        }
    }

    return RCt;
}   /* _ReadKFileToMemory () */

/*)))
 ///  We need that method because KXML does not read document properly
 \\\  from KFile over HTTP
 ///
(((*/
rc_t CC
ReadHttpFileToMemory (
                    const char * Url,
                    char ** RetBuffer,
                    uint64_t * RetSize
)
{
    rc_t RCt;
    const KFile * File;

    RCt = 0;
    File = NULL;

    if ( Url == NULL || RetBuffer == NULL || RetSize == NULL ) {
        return RC ( rcExe, rcFile, rcReading, rcParam, rcNull );
    }

    RCt = KNSManagerMakeHttpFile (
                                _ManagerOfKNS,
                                & File,
                                NULL,   /* no open connections */
                                0x01010000,
                                Url
                                );
    if ( RCt == 0 ) {
        RCt = _ReadKFileToMemory ( File, RetBuffer, RetSize );

        ReleaseComplain ( KFileRelease, File );
    }

    return RCt;
}   /* ReadHttpFileToMemory () */

/*)))
 ///  We need that method because KXML does not read document properly
 \\\  from KFile over HTTP
 ///
(((*/
rc_t CC
ReadLocalFileToMemory (
                    const char * FileName,
                    char ** RetBuffer,
                    uint64_t * RetSize
)
{
    rc_t RCt;
    KDirectory * NativeDir;
    const KFile * File;

    RCt = 0;
    NativeDir = NULL;
    File = NULL;

    RCt = KDirectoryNativeDir ( & NativeDir );
    if ( RCt == 0 ) {

        RCt = KDirectoryOpenFileRead ( NativeDir, & File, FileName );
        if ( RCt == 0 ) {

            RCt = _ReadKFileToMemory ( File, RetBuffer, RetSize );

            KFileRelease ( File );
        }

        KDirectoryRelease ( NativeDir );
    }

    return RCt;
}   /* ReadLocalFileToMemory () */

rc_t CC
ExecuteCGI ( const char * CGICommand )
{
    rc_t RCt;
    const KFile * File;
    uint64_t FileSize, ToRead, Pos;
    char Buffer [ 4096 ];
    size_t NumRead, BSize;

    RCt = 0;
    File = NULL;
    FileSize = ToRead = Pos = 0;
    NumRead = 0;
    BSize = sizeof ( Buffer );

    if ( CGICommand == NULL ) {
        return RC ( rcExe, rcFile, rcReading, rcParam, rcNull );
    }

    RCt = KNSManagerMakeHttpFile (
                                _ManagerOfKNS,
                                & File,
                                NULL,   /* no open connections */
                                0x01010000,
                                CGICommand
                                );
    if ( RCt == 0 ) {
        RCt = KFileSize ( File, & FileSize );
        if ( RCt == 0 ) {
            Pos = 0;
            NumRead = 0;

            while ( 0 <= FileSize ) {
                ToRead = FileSize < BSize ? FileSize : BSize ;
                RCt = KFileRead (
                                File,
                                Pos,
                                Buffer,
                                BSize,
                                & NumRead
                                );
                if ( RCt != 0 ) {
                    break;
                }

                Pos += NumRead;
                FileSize -= NumRead;
            }
        }

        ReleaseComplain ( KFileRelease, File );
    }

    return RCt;
}   /* ExecuteCGI () */
