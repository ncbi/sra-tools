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

typedef struct DirPair StdDirPair;
#define DIRPAIR_IMPL StdDirPair

#include "dir-pair.h"
#include "db-pair.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/mmap.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/rc.h>

#include <string.h>


FILE_ENTRY ( dir-pair );


static
void StdDirPairWhack ( StdDirPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    DirPairDestroy ( self, ctx );
    MemFree ( ctx, self, sizeof * self );
}

static DirPair_vt StdDirPair_vt =
{
    StdDirPairWhack
};


/* Make
 *  make a standard directory pair from existing KDirectory objects
 */
DirPair *DirPairMake ( const ctx_t *ctx,
    const KDirectory *src, KDirectory *dst,
    const char *name, const char *opt_full_spec )
{
    FUNC_ENTRY ( ctx );

    StdDirPair *dir;

    TRY ( dir = MemAlloc ( ctx, sizeof * dir, false ) )
    {
        TRY ( DirPairInit ( dir, ctx, & StdDirPair_vt, src, dst, name, opt_full_spec ) )
        {
            return dir;
        }

        MemFree ( ctx, dir, sizeof * dir );
    }

    return NULL;
}


DirPair *DbPairMakeStdDirPair ( DbPair *self, const ctx_t *ctx,
    const KDirectory *src, KDirectory *dst, const char *name )
{
    FUNC_ENTRY ( ctx );
    return DirPairMake ( ctx, src, dst, name, self -> full_spec );
}


/* Release
 *  called by db at end of copy
 */
void DirPairRelease ( DirPair *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "DirPair" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            ( * self -> vt -> whack ) ( ( void* ) self, ctx );
            break;
        case krefZero:
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcDirectory, rcDestroying, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to release DirPair" );
        }
    }
}

/* Duplicate
 */
DirPair *DirPairDuplicate ( DirPair *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "DirPair" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcDirectory, rcAttaching, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to duplicate DirPair" );
            return NULL;
        }
    }

    return ( DirPair* ) self;
}


/* Copy
 *  copy from source to destination directory
 */
typedef struct DirPairCopyParams DirPairCopyParams;
struct DirPairCopyParams
{
    const DirPair *self;
    const ctx_t *ctx;
    char *relpath;
    size_t psize;
    const KDirectory *src;
    KDirectory *dst;
};

static
rc_t DirPairCopyDir ( const DirPair *self, const ctx_t *ctx,
    const KDirectory *src, KDirectory *dst, char *relpath, size_t psize );

static
rc_t DirPairCopyFile ( const DirPair *self, const ctx_t *ctx,
    const KDirectory *src, KDirectory *dst, char *relpath )
{
    FUNC_ENTRY ( ctx );

    /* get source access mode */
    uint32_t access;
    rc_t rc = KDirectoryAccess ( src, & access, "%s", relpath );
    if ( rc != 0 )
    {
        ERROR ( rc, "failed to determine access mode of file 'dst.%.*s%s'",
                self -> owner_spec_size, self -> full_spec, relpath );
    }
    else
    {
        KFile *d;
        rc = KDirectoryCreateFile ( dst, & d, false, access, kcmInit | kcmParents, "%s", relpath );
        if ( rc != 0 )
        {
            ERROR ( rc, "failed to create file 'dst.%.*s%s'",
                    self -> owner_spec_size, self -> full_spec, relpath );
        }
        else
        {
            const KFile *s;
            /* open source */
            rc = KDirectoryOpenFileRead ( src, & s, "%s", relpath );
            if ( rc != 0 )
            {
                ERROR ( rc, "failed to open file 'src.%.*s%s'",
                        self -> owner_spec_size, self -> full_spec, relpath );
            }
            else
            {
                const KMMap *mm;
                rc = KMMapMakeMaxRead ( & mm, s );
                if ( rc != 0 )
                {
                    ERROR ( rc, "failed to map file 'src.%.*s%s'",
                            self -> owner_spec_size, self -> full_spec, relpath );
                }
                else
                {
                    uint64_t mm_pos;

                    /* get initial position and size */
                    rc = KMMapPosition ( mm, & mm_pos );
                    if ( rc != 0 )
                    {
                        ERROR ( rc, "failed to determine position of map file 'src.%.*s%s'",
                                self -> owner_spec_size, self -> full_spec, relpath );
                    }
                    else
                    {
                        size_t mm_size;
                        rc = KMMapSize ( mm, & mm_size );
                        if ( rc != 0 )
                        {
                            ERROR ( rc, "failed to determine size of map file 'src.%.*s%s'",
                                    self -> owner_spec_size, self -> full_spec, relpath );
                        }
                        else while ( mm_size != 0 )
                        {
                            size_t num_writ;
                            const void *mm_addr;

                            /* access address */
                            rc = KMMapAddrRead ( mm, & mm_addr );
                            if ( rc != 0 )
                            {
                                ERROR ( rc, "failed to access address of map file 'src.%.*s%s'",
                                        self -> owner_spec_size, self -> full_spec, relpath );
                                break;
                            }

                            /* write region to output */
                            rc = KFileWriteAll ( d, mm_pos, mm_addr, mm_size, & num_writ );
                            if ( rc != 0 )
                            {
                                ERROR ( rc, "failed to write to file 'dst.%.*s%s'",
                                        self -> owner_spec_size, self -> full_spec, relpath );
                                break;
                            }
                            if ( num_writ != mm_size )
                            {
                                rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
                                ERROR ( rc, "failed to write to file 'dst.%.*s%s'",
                                        self -> owner_spec_size, self -> full_spec, relpath );
                                break;
                            }

                            /* now try to shift region */
                            mm_pos += mm_size;
                            rc = KMMapReposition ( mm, mm_pos, & mm_size );
                            if ( rc != 0 )
                            {
                                if ( GetRCState ( rc ) == rcInvalid )
                                    rc = 0;
                                else
                                {
                                    ERROR ( rc, "failed to remap file 'src.%.*s%s'",
                                            self -> owner_spec_size, self -> full_spec, relpath );
                                }
                                break;
                            }
                        }
                    }

                    KMMapRelease ( mm );
                }

                KFileRelease ( s );
            }

            KFileRelease ( d );
        }
    }

    return rc;
}

static
rc_t DirPairCopyAlias ( const DirPair *self, const ctx_t *ctx,
    const KDirectory *src, KDirectory *dst, char *relpath )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

#if WINDOWS
    rc = RC ( rcExe, rcAlias, rcCopying, rcFunction, rcUnsupported );
    ERROR ( rc, "Windows... failed to create alias 'dst.%.*s%s'",
            self -> owner_spec_size, self -> full_spec, relpath );
#else
    char apath [ 4096 ];

    /* resolve the alias */
    rc = KDirectoryResolveAlias ( src, false, apath, sizeof apath, "%s", relpath );    
    if ( rc != 0 )
    {
        ERROR ( rc, "failed to resolve alias 'src.%.*s%s'",
                self -> owner_spec_size, self -> full_spec, relpath );
    }
    else
    {
        rc = KDirectoryCreateAlias ( dst, 0777, kcmInit, apath, relpath );
        if ( rc != 0 )
        {
            ERROR ( rc, "failed to create alias 'dst.%.*s%s'",
                    self -> owner_spec_size, self -> full_spec, relpath );
        }
    }
#endif

    return rc;
}

static
rc_t CC DirPairCopyEntry ( const KDirectory *local, uint32_t type, const char *name, void *data )
{
    DirPairCopyParams *pb = data;
    const ctx_t *ctx = pb -> ctx;

    FUNC_ENTRY ( ctx );

    const DirPair *self = pb -> self;
    char *relpath = pb -> relpath;
    size_t num_writ, psize = pb -> psize;

    /* since we are walking two directories in parallel,
       build a path relative to each base directory and go */
    rc_t rc = string_printf ( & relpath [ psize ], 4096 - psize, & num_writ, "/%s", name );
    if ( rc != 0 )
    {
        ERROR ( rc, "failed to build path for entry '%.*s%.*s/%s'",
                self -> owner_spec_size, self -> full_spec,
                ( uint32_t ) psize, relpath, name );
        return rc;
    }

    /* see what type of entry we have */
    switch ( type )
    {
    case kptFile:
        return DirPairCopyFile ( pb -> self, ctx, pb -> src, pb -> dst, relpath );
    case kptDir:
        return DirPairCopyDir ( pb -> self, ctx, pb -> src, pb -> dst, relpath, psize + num_writ );
    case kptFile | kptAlias:
    case kptDir | kptAlias:
        return DirPairCopyAlias ( pb -> self, ctx, pb -> src, pb -> dst, relpath );
    }

    rc = RC ( rcExe, rcDirectory, rcVisiting, rcType, rcUnsupported );
    ERROR ( rc, "cannot copy entry '%.*s%s' - not a supported file system type",
            self -> owner_spec_size, self -> full_spec, relpath );
    return rc;
}

static
rc_t DirPairCopyDir ( const DirPair *self, const ctx_t *ctx,
    const KDirectory *src, KDirectory *dst, char *relpath, size_t psize )
{
    FUNC_ENTRY ( ctx );

    /* get source access mode */
    uint32_t access;
    rc_t rc = KDirectoryAccess ( src, & access, "%s", relpath );
    if ( rc != 0 )
    {
        ERROR ( rc, "failed to determine access mode of directory 'src.%.*s%s'",
                self -> owner_spec_size, self -> full_spec, relpath );
    }
    else
    {
        /* create the destination directory */
        rc = KDirectoryCreateDir ( dst, access, kcmOpen | kcmParents, "%s", relpath );
        if ( rc != 0 )
            ERROR ( rc, "failed to create directory 'dst.%.*s%s'",
                    self -> owner_spec_size, self -> full_spec, relpath );
        else
        {
            DirPairCopyParams pb;

            pb . self = self;
            pb . ctx = ctx;
            pb . relpath = relpath;
            pb . psize = psize;
            pb . src = src;
            pb . dst = dst;

            /* going to perform shallow copy */
            rc = KDirectoryVisit ( src, false, DirPairCopyEntry, & pb, "%s", relpath );
        }
    }

    return rc;
}


void DirPairCopy ( DirPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    size_t psize;
    char relpath [ 4096 ];

    STATUS ( 2, "copying directory '%s'", self -> full_spec );

    rc = string_printf ( relpath, sizeof relpath, & psize, "./%s", self -> name );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "failed to build initial path string for '%s'", self -> full_spec );
    else
    {
        DirPairCopyDir ( self, ctx, self -> sdir, self -> ddir, relpath, psize );
    }
}


/* Init
 */
void DirPairInit ( DirPair *self, const ctx_t *ctx, const DirPair_vt *vt,
    const KDirectory *src, KDirectory *dst,
    const char *name, const char *full_spec )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcDirectory, rcConstructing, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad DirPair" );
        return;
    }

    memset ( self, 0, sizeof * self );
    self -> vt = vt;

    rc = KDirectoryAddRef ( self -> sdir = src );
    if ( rc != 0 )
        ERROR ( rc, "failed to duplicate dir 'src.%s./%s'", full_spec, name );
    else
    {
        rc = KDirectoryAddRef ( self -> ddir = dst );
        if ( rc != 0 )
            ERROR ( rc, "failed to duplicate dir 'dst.%s./%s'", full_spec, name );
        else
        {
            char *new_full_spec;

            self -> owner_spec_size = ( uint32_t ) string_size ( full_spec );
            self -> full_spec_size = self -> owner_spec_size + string_size ( name ) + 2;

            TRY ( new_full_spec = MemAlloc ( ctx, self -> full_spec_size + 1, false ) )
            {
                rc = string_printf ( new_full_spec, self -> full_spec_size + 1, NULL, "%s./%s", full_spec, name );
                if ( rc != 0 )
                    ABORT ( rc, "miscalculated string size" );
                else
                {
                    self -> full_spec = new_full_spec;
                    self -> name = new_full_spec + self -> owner_spec_size + 2;

                    KRefcountInit ( & self -> refcount, 1, "DirPair", "init", name );
                    return;
                }
            }

            KDirectoryRelease ( self -> ddir );
            self -> ddir = NULL;
        }

        KDirectoryRelease ( self -> sdir );
        self -> sdir = NULL;
    }
}

/* Destroy
 */
void DirPairDestroy ( DirPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    rc = KDirectoryRelease ( self -> ddir );
    if ( rc != 0 )
        WARN ( "KDirectoryRelease failed on 'dst.%s'", self -> full_spec );
    KDirectoryRelease ( self -> sdir );

    MemFree ( ctx, ( void* ) self -> full_spec, self -> full_spec_size + 1 );

    memset ( self, 0, sizeof * self );
}
