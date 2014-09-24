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

#include "map-file.h"
#include "idx-mapping.h"
#include "ctx.h"
#include "caps.h"
#include "status.h"
#include "mem.h"
#include "sra-sort.h"

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/buffile.h>
#include <klib/refcount.h>
#include <klib/sort.h>
#include <klib/rc.h>

#include <string.h>
#include <endian.h>
#include <byteswap.h>

#include "except.h"

FILE_ENTRY ( map-file );


/*--------------------------------------------------------------------------
 * MapFile
 *  a file for storing id mappings
 */
struct MapFile
{
    int64_t first_id;
    uint64_t num_ids;
    uint64_t num_mapped_ids;
    int64_t max_new_id;
    KFile *f_old, *f_new, *f_pos;
    size_t id_size;
    KRefcount refcount;
};


/* Whack
 */
static
void MapFileWhack ( MapFile *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    rc_t rc = KFileRelease ( self -> f_old );
    if ( rc != 0 )
        SYSTEM_ERROR ( rc, "KFileRelease failed on old=>new" );
    else
    {
        rc = KFileRelease ( self -> f_new );
        if ( rc != 0 )
            ABORT ( rc, "KFileRelease failed on new=>old" );
        else if ( self -> f_pos != NULL )
        {
            rc = KFileRelease ( self -> f_pos );
            if ( rc != 0 )
                ABORT ( rc, "KFileRelease failed on global poslen temp column" );
        }
        
        MemFree ( ctx, self, sizeof * self );
    }
}


/* Make
 *  creates an id map
 */
static
void MapFileMakeFork ( KFile **fp, const ctx_t *ctx, const char *name,
    KDirectory *wd, const char *tmpdir, int pid, size_t bsize, const char *fork )
{
    FUNC_ENTRY ( ctx );

    /* create temporary KFile */
    rc_t rc;
    KFile *backing;

    rc = KDirectoryCreateFile ( wd, & backing, true,
        0600, kcmInit | kcmParents, "%s/sra-sort-%s.%s.%d", tmpdir, name, fork, pid );
    if ( rc != 0 )
        SYSTEM_ERROR ( rc, "failed to create %s id map file '%s'", fork, name );
    else
    {
#if ! WINDOWS
        /* never try to remove files on Windows */
        if ( ctx -> caps -> tool -> unlink_idx_files )
        {
            /* unlink KFile */
            rc = KDirectoryRemove ( wd, false, "%s/sra-sort-%s.%s.%d", tmpdir, name, fork, pid );
            if ( rc != 0 )
                WARN ( "failed to unlink %s id map file '%s'", fork, name );
        }
#endif

        /* create a read/write buffer file */
        rc = KBufFileMakeWrite ( fp, backing, true, bsize );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "failed to create buffer for %s id map file '%s'", fork, name );

        KFileRelease ( backing );
    }
}

static
MapFile *MapFileMakeInt ( const ctx_t *ctx, const char *name, bool random, bool for_poslen )
{
    MapFile *mf;
    TRY ( mf = MemAlloc ( ctx, sizeof * mf, true ) )
    {
        /* create KDirectory */
        KDirectory *wd;
        rc_t rc = KDirectoryNativeDir ( & wd );
        if ( rc != 0 )
            SYSTEM_ERROR ( rc, "failed to create native directory" );
        else
        {
            const Tool *tp = ctx -> caps -> tool;
            size_t bsize = random ? tp -> map_file_random_bsize : tp -> map_file_bsize;

            /* create old=>new id file */
            TRY ( MapFileMakeFork ( & mf -> f_old, ctx, name, wd, tp -> tmpdir, tp -> pid, bsize, "old" ) )
            {
                TRY ( MapFileMakeFork ( & mf -> f_new, ctx, name, wd, tp -> tmpdir, tp -> pid, 32 * 1024, "new" ) )
                {
                    if ( for_poslen )
                        MapFileMakeFork ( & mf -> f_pos, ctx, name, wd, tp -> tmpdir, tp -> pid, 32 * 1024, "pos" );

                    KDirectoryRelease ( wd );

                    if ( ! FAILED () )
                    {
                        /* this is our guy */
                        KRefcountInit ( & mf -> refcount, 1, "MapFile", "make", name );
                        
                        return mf;
                    }

                    KFileRelease ( mf -> f_new );
                }

                KFileRelease ( mf -> f_old );
            }

            KDirectoryRelease ( wd );
        }

        MemFree ( ctx, mf, sizeof * mf );
    }

    return NULL;
}

MapFile *MapFileMake ( const ctx_t *ctx, const char *name, bool random )
{
    FUNC_ENTRY ( ctx );
    return MapFileMakeInt ( ctx, name, random, false );
}

/* MakeForPoslen
 *  creates an id map with an additional poslen file
 *  this is specially for PRIMARY_ALIGNMENT_IDS and SECONDARY_ALIGNMENT_IDS
 *  that use *_ALIGNMENT global position and length as a sorting key
 *  we drop it to a file after its generation so that it can be
 *  picked up later when copying the corresponding alignment table.
 */
MapFile *MapFileMakeForPoslen ( const ctx_t *ctx, const char *name )
{
    FUNC_ENTRY ( ctx );
    return MapFileMakeInt ( ctx, name, false, true );
}


/* Release
 */
void MapFileRelease ( const MapFile *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "MapFile" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            MapFileWhack ( ( MapFile* ) self, ctx );
            break;
        case krefZero:
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcFile, rcDestroying, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to release MapFile" );
        }
    }
}

/* Duplicate
 */
MapFile *MapFileDuplicate ( const MapFile *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "MapFile" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcFile, rcAttaching, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to duplicate MapFile" );
            return NULL;
        }
    }

    return ( MapFile* ) self;
}


/* SsetIdRange
 *  required second-stage initialization
 *  must be called before any writes occur
 */
void MapFileSetIdRange ( MapFile *self, const ctx_t *ctx,
    int64_t first_id, uint64_t num_ids )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcFile, rcUpdating, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad self" );
    }
    else if ( self -> max_new_id != 0 )
    {
        rc = RC ( rcExe, rcFile, rcUpdating, rcConstraint, rcViolated );
        INTERNAL_ERROR ( rc, "cannot change id range after writing has begun" );
    }
    else
    {
        /* record new first id and count */
        self -> first_id = first_id;
        self -> num_ids = num_ids;

        /* determine id size for all ids PLUS 0 for NULL */
        for ( ++ num_ids, self -> id_size = 1; self -> id_size < 8; ++ self -> id_size )
        {
            if ( num_ids <= ( ( uint64_t ) 1 ) << ( self -> id_size * 8 ) )
                break;
        }
    }
}


/* First
 *  return first mapped id
 */
int64_t MapFileFirst ( const MapFile *self, const ctx_t *ctx )
{
    return self -> first_id;
}


/* Count
 *  return num_ids
 */
uint64_t MapFileCount ( const MapFile *self, const ctx_t *ctx )
{
    return self -> num_ids;
}


/* SetOldToNew
 *  write old=>new id mappings
 */
void MapFileSetOldToNew ( MapFile *self, const ctx_t *ctx, const IdxMapping *ids, size_t count )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcFile, rcWriting, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad self reference" );
    }
    else if ( self -> num_ids == 0 )
    {
        rc = RC ( rcExe, rcFile, rcWriting, rcRange, rcUndefined );
        INTERNAL_ERROR ( rc, "SetIdRange must be called with a non-empty range" );
    }
    else
    {
        size_t i;
        for ( i = 0; i < count; ++ i )
        {
            size_t num_writ;
            /* zero-based index */
            int64_t old_id = ids [ i ] . old_id - self -> first_id;
            /* position in bytes */
            uint64_t pos = old_id * self -> id_size;
            /* 1-based translated new-id */
            int64_t new_id = ids [ i ] . new_id - self -> first_id + 1;
            assert ( old_id >= 0 );
            assert ( new_id >= 0 );
#if __BYTE_ORDER == __BIG_ENDIAN
            new_id = bswap_64 ( new_id );
#endif
            rc = KFileWriteAll ( self -> f_old, pos, & new_id, self -> id_size, & num_writ );
            if ( rc != 0 )
            {
                SYSTEM_ERROR ( rc, "failed to write old=>new id mapping" );
                break;
            }

            if ( num_writ != self -> id_size )
            {
                rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
                SYSTEM_ERROR ( rc, "failed to write old=>new id mapping" );
                break;
            }
        }
    }
}


/* SetNewToOld
 *  write new=>old id mappings
 */
void MapFileSetNewToOld ( MapFile *self, const ctx_t *ctx, const IdxMapping *ids, size_t count )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcFile, rcWriting, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad self reference" );
    }
    else if ( self -> num_ids == 0 )
    {
        rc = RC ( rcExe, rcFile, rcWriting, rcRange, rcUndefined );
        INTERNAL_ERROR ( rc, "SetIdRange must be called with a non-empty range" );
    }
    else
    {
        if ( ! ctx -> caps -> tool -> write_new_to_old )
            self -> max_new_id = self -> first_id + count - 1;
        else
        {
            size_t i;
            for ( i = 0; i < count; ++ i )
            {
                size_t num_writ;
                /* zero based index */
                int64_t new_id = ids [ i ] . new_id - self -> first_id;
                /* zero based byte offset */
                uint64_t pos = new_id * self -> id_size;
                /* 1-based translated old-id */
                int64_t old_id = ids [ i ] . old_id - self -> first_id + 1;
                assert ( new_id >= 0 );
                assert ( old_id >= 0 );
#if __BYTE_ORDER == __BIG_ENDIAN
                old_id = bswap_64 ( old_id );
#endif
                rc = KFileWriteAll ( self -> f_new, pos, & old_id, self -> id_size, & num_writ );
                if ( rc != 0 )
                {
                    SYSTEM_ERROR ( rc, "failed to write new=>old id mapping" );
                    break;
                }
                
                if ( num_writ != self -> id_size )
                {
                    rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
                    SYSTEM_ERROR ( rc, "failed to write new=>old id mapping" );
                    break;
                }
                
                self -> max_new_id = new_id + self -> first_id;
            }
        }
    }
}


/* SetPoslen
 *  write global position/length in new-id order
 */
void MapFileSetPoslen ( MapFile *self, const ctx_t *ctx, const IdxMapping *ids, size_t count )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcFile, rcWriting, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad self reference" );
    }
    else if ( self -> f_pos == NULL )
    {
        rc = RC ( rcExe, rcFile, rcWriting, rcFile, rcIncorrect );
        INTERNAL_ERROR ( rc, "MapFile must be created with MapFileMakeForPoslen" );
    }
    else if ( self -> num_ids == 0 )
    {
        rc = RC ( rcExe, rcFile, rcWriting, rcRange, rcUndefined );
        INTERNAL_ERROR ( rc, "SetIdRange must be called with a non-empty range" );
    }
    else
    {
        /* start writing after the last new id recorded */
        uint64_t pos = ( self -> max_new_id - self -> first_id + 1 ) * sizeof ids -> new_id;

        size_t i;
        for ( i = 0; i < count; pos += sizeof ids -> new_id, ++ i )
        {
            size_t num_writ;
            int64_t poslen = ids [ i ] . new_id;
#if __BYTE_ORDER == __BIG_ENDIAN
            poslen = bswap_64 ( poslen );
#endif
            rc = KFileWriteAll ( self -> f_pos, pos, & poslen, sizeof ids -> new_id, & num_writ );
            if ( rc != 0 )
            {
                SYSTEM_ERROR ( rc, "failed to write poslen temporary column" );
                break;
            }

            if ( num_writ != sizeof ids -> new_id )
            {
                rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
                SYSTEM_ERROR ( rc, "failed to write poslen temporary column" );
                break;
            }
        }
    }
}


/* ReadPoslen
 *  read global position/length in new-id order
 *  starts reading at the indicated row
 *  returns the number read
 */
size_t MapFileReadPoslen ( const MapFile *self, const ctx_t *ctx,
    int64_t start_id, uint64_t *poslen, size_t max_count )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    size_t total = 0;

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcFile, rcReading, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad self reference" );
    }
    else if ( self -> f_pos == NULL )
    {
        rc = RC ( rcExe, rcFile, rcReading, rcFile, rcIncorrect );
        INTERNAL_ERROR ( rc, "MapFile must be created with MapFileMakeForPoslen" );
    }
    /* the column is read until end, but
       this is not an error. just return no-more-data */
    else if ( start_id == self -> first_id + self -> num_ids )
        return 0;
    else if ( start_id < self -> first_id || start_id > self -> first_id + self -> num_ids )
    {
        rc = RC ( rcExe, rcFile, rcReading, rcParam, rcInvalid );
        INTERNAL_ERROR ( rc, "start_id ( %ld ) is not within column range ( %ld .. %ld )",
            start_id, self -> first_id, self -> first_id + self -> num_ids - 1 );
    }
    else if ( max_count != 0 )
    {
        /* limit read to number of ids in index */
        if ( start_id + max_count > self -> first_id + self -> num_ids )
            max_count = ( size_t ) ( self -> first_id + self -> num_ids - start_id );

        if ( poslen == NULL )
        {
            rc = RC ( rcExe, rcFile, rcReading, rcBuffer, rcNull );
            INTERNAL_ERROR ( rc, "bad buffer parameter" );
        }
        else
        {
            /* read as many bytes of id as possible */
            size_t num_read, to_read = max_count * sizeof * poslen;
            uint64_t pos = ( start_id - self -> first_id ) * sizeof * poslen;
            rc = KFileReadAll ( self -> f_pos, pos, poslen, to_read, & num_read );
            if ( rc != 0 )
                SYSTEM_ERROR ( rc, "failed to read new=>old map" );
            else
            {
                total = ( size_t ) ( num_read / sizeof * poslen );

#if __BYTE_ORDER == __BIG_ENDIAN
                {
                    /* revert byte-order */
                    size_t i;
                    for ( i = 0; i < total; ++ i )
                        poslen [ i ] = bswap_64 ( poslen [ i ] );
                }
#endif
            }
        }
    }

    return total;
}


/* AllocMissingNewIds
 *  it is possible to have written fewer new=>old mappings
 *  than are supposed to be there, e.g. SEQUENCE that gets
 *  initially written by align tables. unaligned sequences
 *  need to fill in the remainder.
 */
static
size_t make_missing_entry ( MapFile *self, const ctx_t *ctx,
    IdxMapping *missing, size_t i, size_t max_missing_ids,
    int64_t old_id, int64_t new_id )
{
    if ( i == max_missing_ids )
    {
        ON_FAIL ( MapFileSetNewToOld ( self, ctx, missing, i ) )
            return i;

        i = 0;
    }

    /* insert into missing id map */
    missing [ i ] . old_id = old_id;
    missing [ i ] . new_id = new_id;

    /* update old=>new mapping */
    MapFileSetOldToNew ( self, ctx, & missing [ i ], 1 );

    return i + 1;
}

int64_t MapFileAllocMissingNewIds ( MapFile *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    int64_t first_allocated = 0;

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcFile, rcUpdating, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad self reference" );
    }
    else if ( self -> num_ids == 0 )
    {
        rc = RC ( rcExe, rcFile, rcWriting, rcRange, rcUndefined );
        INTERNAL_ERROR ( rc, "SetIdRange must be called with a non-empty range" );
    }
    else
    {
        uint64_t num_missing_ids = self -> num_ids - ( self -> max_new_id - self -> first_id + 1 );
        if ( num_missing_ids != 0 )
        {
            IdxMapping *missing;

            /* determine maximum missing id mappings */
            size_t max_missing_ids = ctx -> caps -> tool -> max_missing_ids;
            if ( ( uint64_t ) max_missing_ids > num_missing_ids )
                max_missing_ids = ( size_t ) num_missing_ids;

            /* allocate a buffer of IdxMapping */
            TRY ( missing = MemAlloc ( ctx, sizeof * missing * max_missing_ids, false ) )
            {
                /* use a scan buffer somewhere around 32K,
                   to keep page-file cache focused on a few pages */
                void *scan_buffer;
                const size_t scan_buffer_size = 32 * 1024;
                TRY ( scan_buffer = MemAlloc ( ctx, scan_buffer_size, false ) )
                {
                    size_t i, num_read;
                    int64_t new_id, old_id = self -> first_id;
                    uint64_t eof = self -> num_ids * self -> id_size;

                    /* this guy will be used to assign new ids */
                    int64_t max_new_id = self -> max_new_id;
                    int64_t entry_max_new_id = self -> max_new_id;


                    /* in a loop, read as many ids as possible into scan buffer
                       the number is dependent upon self->id_size */
                    for ( i = 0; ; old_id += num_read )
                    {
                        size_t j, to_read;

                        /* determine position from old_id */
                        uint64_t pos = ( old_id - self -> first_id ) * self -> id_size;
                        if ( pos == eof )
                            break;

                        /* determine bytes to read - this must be limited
                           or we risk tricking page-buffer into resizing */
                        to_read = scan_buffer_size;
                        if ( pos + to_read > eof )
                            to_read = ( size_t ) ( eof - pos );

                        /* read as many bytes as we can */
                        rc = KFileReadAll ( self -> f_old, pos, scan_buffer, to_read, & num_read );
                        if ( rc != 0 )
                        {
                            SYSTEM_ERROR ( rc, "failed to read old=>new map" );
                            break;
                        }

                        /* convert to count */                        
                        num_read /= self -> id_size;
                        if ( num_read == 0 )
                        {
                            rc = RC ( rcExe, rcFile, rcReading, rcTransfer, rcIncomplete );
                            SYSTEM_ERROR ( rc, "failed to read old=>new map" );
                            break;
                        }

                        /* scan for zeros, and for every zero found,
                           make entry into IdxMapping table, writing
                           new-id back to old=>new map */
                        switch ( self -> id_size )
                        {
                        case 1:
                            for ( j = 0; j < num_read; ++ j )
                            {
                                if ( ( ( const uint8_t* ) scan_buffer ) [ j ] == 0 )
                                {
                                    ON_FAIL ( i = make_missing_entry ( self, ctx, missing,
                                              i, max_missing_ids, old_id + j, ++ max_new_id ) )
                                        break;
                                }
                            }
                            break;
                        case 2:
                            for ( j = 0; j < num_read; ++ j )
                            {
                                if ( ( ( const uint16_t* ) scan_buffer ) [ j ] == 0 )
                                {
                                    ON_FAIL ( i = make_missing_entry ( self, ctx, missing,
                                              i, max_missing_ids, old_id + j, ++ max_new_id ) )
                                        break;
                                }
                            }
                            break;
                        case 3:
                            for ( num_read *= 3, j = 0; j < num_read; j += 3 )
                            {
                                if ( ( ( const uint8_t* ) scan_buffer ) [ j + 0 ] == 0 &&
                                     ( ( const uint8_t* ) scan_buffer ) [ j + 1 ] == 0 &&
                                     ( ( const uint8_t* ) scan_buffer ) [ j + 2 ] == 0 )
                                {
                                    ON_FAIL ( i = make_missing_entry ( self, ctx, missing,
                                              i, max_missing_ids, old_id + j / 3, ++ max_new_id ) )
                                        break;
                                }
                            }
                            num_read /= 3;
                            break;
                        case 4:
                            for ( j = 0; j < num_read; ++ j )
                            {
                                if ( ( ( const uint32_t* ) scan_buffer ) [ j ] == 0 )
                                {
                                    ON_FAIL ( i = make_missing_entry ( self, ctx, missing,
                                              i, max_missing_ids, old_id + j, ++ max_new_id ) )
                                        break;
                                }
                            }
                            break;
                        case 8:
                            for ( j = 0; j < num_read; ++ j )
                            {
                                if ( ( ( const uint64_t* ) scan_buffer ) [ j ] == 0 )
                                {
                                    ON_FAIL ( i = make_missing_entry ( self, ctx, missing,
                                              i, max_missing_ids, old_id + j, ++ max_new_id ) )
                                        break;
                                }
                            }
                            break;
                        default:
                            for ( num_read *= self -> id_size, j = 0; j < num_read; j += self -> id_size )
                            {
                                new_id = 0;
                                memcpy ( & new_id, & ( ( const uint8_t* ) scan_buffer ) [ j ], self -> id_size );
                                if ( new_id == 0 )
                                {
                                    ON_FAIL ( i = make_missing_entry ( self, ctx, missing,
                                              i, max_missing_ids, old_id + j / self -> id_size, ++ max_new_id ) )
                                        break;
                                }
                            }
                            num_read /= self -> id_size;
                        }
                    }

                    if ( ! FAILED () && i != 0 )
                        MapFileSetNewToOld ( self, ctx, missing, i );

                    assert ( FAILED () || self -> max_new_id == max_new_id );

                    if ( max_new_id != entry_max_new_id )
                        first_allocated = entry_max_new_id + 1;

                    MemFree ( ctx, scan_buffer, scan_buffer_size );
                }

                MemFree ( ctx, missing, sizeof * missing * max_missing_ids );
            }
        }
    }

    return first_allocated;
}


/* ReadNewToOld
 *  reads a bunch of new=>old mappings
 */
static
size_t MapFileReadNewToOld ( const MapFile *self, const ctx_t *ctx,
    int64_t start_id, IdxMapping *ids, size_t max_count )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    size_t total = 0;

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcFile, rcReading, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad self reference" );
    }
    /* the new=>old mapping is read until end, but
       this is not an error. just return no-more-data */
    else if ( start_id == self -> first_id + self -> num_ids )
        return 0;
    else if ( start_id < self -> first_id || start_id > self -> first_id + self -> num_ids )
    {
        rc = RC ( rcExe, rcFile, rcReading, rcParam, rcInvalid );
        INTERNAL_ERROR ( rc, "start_id ( %ld ) is not within map range ( %ld .. %ld )",
            start_id, self -> first_id, self -> first_id + self -> num_ids - 1 );
    }
    else if ( max_count != 0 )
    {
        /* limit read to number of ids in index */
        if ( start_id + max_count > self -> first_id + self -> num_ids )
            max_count = ( size_t ) ( self -> first_id + self -> num_ids - start_id );

        if ( ids == NULL )
        {
            rc = RC ( rcExe, rcFile, rcReading, rcBuffer, rcNull );
            INTERNAL_ERROR ( rc, "bad buffer parameter" );
        }
        else
        {
            /* read as many bytes of id as possible */
            size_t num_read, to_read = max_count * self -> id_size;
            uint64_t pos = ( start_id - self -> first_id ) * self -> id_size;
            rc = KFileReadAll ( self -> f_new, pos, ids, to_read, & num_read );
            if ( rc != 0 )
                SYSTEM_ERROR ( rc, "failed to read new=>old map" );
            else
            {
                size_t i;
                const char *packed = ( const char* ) ids;
                total = ( size_t ) ( num_read /self -> id_size );

                /* read each packed id as little-endian integer onto
                   pre-zeroed 64-bit integer, swap if needed */
                for ( num_read = total * self -> id_size, i = total; i > 0; )
                {
                    int64_t unpacked = 0;

                    num_read -= self -> id_size;
                    -- i;

                    memcpy ( & unpacked, & packed [ num_read ], self -> id_size );
#if __BYTE_ORDER == __BIG_ENDIAN
                    unpacked = bswap_64 ( unpacked );
#endif
                    if ( unpacked != 0 )
                        unpacked += self -> first_id - 1;

                    ids [ i ] . old_id = unpacked;
                    ids [ i ] . new_id = start_id + i;
                }
            }
        }
    }

    return total;
}


/* SelectOldToNewPairs
 *  specify the range of new ids to select from
 *  read them in old=>new order
 */
size_t MapFileSelectOldToNewPairs ( const MapFile *self, const ctx_t *ctx,
    int64_t start_id, IdxMapping *ids, size_t max_count )
{
    FUNC_ENTRY ( ctx );

    uint64_t eof;
    int64_t end_excl;
    size_t i, j, total;

    /* limit read to number of ids in index */
    if ( start_id + max_count > self -> first_id + self -> num_ids )
        max_count = ( size_t ) ( self -> first_id + self -> num_ids - start_id );

    /* range is start_id to end_excl */
    end_excl = start_id + max_count;

    /* eof for f_old */
    eof = self -> num_ids * self -> id_size;

    for ( total = i = 0; i < max_count; total += j )
    {
        rc_t rc;
        size_t off, num_read;
        char buff [ 32 * 1024 ];

        /* read some data from file */
        uint64_t pos = total * self -> id_size;
        size_t to_read = sizeof buff;
	if ( pos == eof )
	    break;
        if ( pos + to_read > eof )
            to_read = ( size_t ) ( eof - pos );
        rc = KFileReadAll ( self -> f_old, pos, buff, to_read, & num_read );
        if ( rc != 0 )
        {
            SYSTEM_ERROR ( rc, "failed to read old=>new map" );
            break;
        }

        /* reached end - requested more ids than we have */
        if ( num_read == 0 )
            break;

        /* the number of whole ids actually read */
        if ( ( num_read /= self -> id_size ) == 0 )
        {
            rc = RC ( rcExe, rcFile, rcReading, rcTransfer, rcIncomplete );
            SYSTEM_ERROR ( rc, "failed to read old=>new map - file incomplete" );
            break;
        }

        /* select new-ids from ids read */
        for ( off = j = 0; j < num_read; off += self -> id_size, ++ j )
        {
            int64_t unpacked = 0;
            memcpy ( & unpacked, & buff [ off ], self -> id_size );
#if __BYTE_ORDER == __BIG_ENDIAN
            unpacked = bswap_64 ( unpacked );
#endif
            /* only offset non-zero (NULL) ids */
            if ( unpacked != 0 )
                unpacked += self -> first_id - 1;

            /* if id meets criteria */
            if ( unpacked >= start_id && unpacked < end_excl )
            {
                ids [ i ] . old_id = self -> first_id + total + j;
                ids [ i ] . new_id = unpacked;
                if ( ++ i == max_count )
                    break;
            }
        }
    }

    return i;
}


/* SelectOldToNewSingle
 *  specify the range of new ids to select from
 *  read them in old=>new order
 */
size_t MapFileSelectOldToNewSingle ( const MapFile *self, const ctx_t *ctx,
    int64_t start_id, int64_t *ids, uint32_t *opt_ord, size_t max_count )
{
    FUNC_ENTRY ( ctx );

    uint64_t eof;
    int64_t end_excl;
    size_t i, j, total;

    /* limit read to number of ids in index */
    if ( start_id + max_count > self -> first_id + self -> num_ids )
        max_count = ( size_t ) ( self -> first_id + self -> num_ids - start_id );

    /* range is start_id to end_excl */
    end_excl = start_id + max_count;

    /* eof for f_old */
    eof = self -> num_ids * self -> id_size;

    for ( total = i = 0; i < max_count; total += j )
    {
        rc_t rc;
        size_t off, num_read;
        char buff [ 32 * 1024 ];

        /* read some data from file */
        uint64_t pos = total * self -> id_size;
        size_t to_read = sizeof buff;
	if ( pos == eof )
            break;
        if ( pos + to_read > eof )
            to_read = ( size_t ) ( eof - pos );
        rc = KFileReadAll ( self -> f_old, pos, buff, to_read, & num_read );
        if ( rc != 0 )
        {
            SYSTEM_ERROR ( rc, "failed to read old=>new map" );
            break;
        }

        /* reached end - requested more ids than we have */
        if ( num_read == 0 )
            break;

        /* the number of whole ids actually read */
        if ( ( num_read /= self -> id_size ) == 0 )
        {
            rc = RC ( rcExe, rcFile, rcReading, rcTransfer, rcIncomplete );
            SYSTEM_ERROR ( rc, "failed to read old=>new map - file incomplete" );
            break;
        }

        /* select new-ids from ids read */
        for ( off = j = 0; j < num_read; off += self -> id_size, ++ j )
        {
            int64_t unpacked = 0;
            memcpy ( & unpacked, & buff [ off ], self -> id_size );
#if __BYTE_ORDER == __BIG_ENDIAN
            unpacked = bswap_64 ( unpacked );
#endif
            /* only offset non-zero (NULL) ids */
            if ( unpacked != 0 )
                unpacked += self -> first_id - 1;

            /* if id meets criteria */
            if ( unpacked >= start_id && unpacked < end_excl )
            {
                ids [ i ] = self -> first_id + total + j;
                assert ( i <= 0xFFFFFFFF );
                if ( opt_ord != NULL )
                    opt_ord [ unpacked - start_id ] = ( uint32_t ) i;
                if ( ++ i == max_count )
                    break;
            }
        }
    }

    return i;
}


/* MapSingleOldToNew
 *  reads a single old=>new mapping
 *  returns new id or 0 if not found
 *  optionally allocates a new id if "insert" is true
 */
int64_t MapFileMapSingleOldToNew ( MapFile *self, const ctx_t *ctx,
    int64_t old_id, bool insert )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    int64_t new_id = 0;

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcFile, rcReading, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad self reference" );
    }
    else if ( old_id < self -> first_id || old_id >= self -> first_id + self -> num_ids )
    {
        rc = RC ( rcExe, rcFile, rcReading, rcParam, rcInvalid );
        INTERNAL_ERROR ( rc, "old_id ( %ld ) is not within map range ( %ld .. %ld )",
            old_id, self -> first_id, self -> first_id + self -> num_ids - 1 );
    }
    else
    {
        size_t num_read, to_read = self -> id_size;
        uint64_t pos = ( old_id - self -> first_id ) * self -> id_size;
        rc = KFileReadAll ( self -> f_old, pos, & new_id, to_read, & num_read );
        if ( rc != 0 )
            SYSTEM_ERROR ( rc, "failed to read old=>new map" );
        else
        {
#if __BYTE_ORDER == __BIG_ENDIAN
            new_id = bswap_64 ( new_id );
#endif
            if ( new_id != 0 )
                new_id += self -> first_id - 1;
            else if ( insert )
            {
                /* create a mapping using the last known
                   new id plus one as the id to assign on insert */
                IdxMapping mapping;
                mapping . old_id = old_id;
                mapping . new_id = self -> max_new_id + 1;

                TRY ( MapFileSetOldToNew ( self, ctx, & mapping, 1 ) )
                {
                    TRY ( MapFileSetNewToOld ( self, ctx, & mapping, 1 ) )
                    {
                        new_id = mapping . new_id;

                        if ( self -> num_ids >= 100000 )
                        {
                            uint64_t scaled = ++ self -> num_mapped_ids * 100;
                            uint64_t prior = scaled - 100;
                            if ( ( prior / self -> num_ids ) != ( scaled /= self -> num_ids ) )
                                STATUS ( 2, "have mapped %lu%% ids", scaled );
                        }
                    }
                }
            }
        }
    }

    return new_id;
}


/* ConsistencyCheck
 */
static
void MapFileCrossCheck ( const MapFile *self, const ctx_t *ctx, IdxMapping *ids, size_t max_ids )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    uint64_t total;
    size_t num_read;
    int64_t last_old, last_new;

    for ( last_old = last_new = -1, total = 0; total < self -> num_ids; total += num_read )
    {
        int64_t id;
        size_t i;

        STATUS ( 2, "reading new=>old ids" );

        ON_FAIL ( num_read = MapFileReadNewToOld ( self, ctx, self -> first_id + total, ids, max_ids ) )
        {
            ANNOTATE ( "failed to read new=>old ids" );
            return;
        }

        STATUS ( 2, "read %,u pairs", num_read );

        STATUS ( 2, "checking for repeats in new-id space" );
        for ( i = 0; i < num_read; last_new = id, ++ i )
        {
            id = ids [ i ] . new_id;
            if ( id == last_new )
            {
                rc = RC ( rcExe, rcIndex, rcValidating, rcId, rcDuplicate );
                ERROR ( rc, "new id %,ld repeats", id );
                return;
            }
        }

        STATUS ( 2, "sorting on old id" );

#if USE_OLD_KSORT
        ksort ( ids, num_read, sizeof ids [ 0 ], IdxMappingCmpOld, ( void* ) ctx );
#else
        IdxMappingSortOld ( ids, ctx, num_read );
#endif

        STATUS ( 2, "checking for repeats in old-id space" );
        for ( i = 0; i < num_read; last_old = id, ++ i )
        {
            id = ids [ i ] . old_id;
            if ( id == last_old )
            {
                rc = RC ( rcExe, rcIndex, rcValidating, rcId, rcDuplicate );
                ERROR ( rc, "old id %,ld repeats", id );
                return;
            }
        }

        STATUS ( 2, "cross-checking against old=>new index" );

        for ( i = 0; i < num_read; ++ i )
        {
            int64_t new_id;

            /* cast away const on "self" for prototype,
               but function is only non-const if "insert"
               were true - we use "false" */
            ON_FAIL ( new_id = MapFileMapSingleOldToNew ( ( MapFile* ) self, ctx, ids [ i ] . old_id, false ) )
                return;

            if ( new_id != ids [ i ] . new_id )
            {
                rc = RC ( rcExe, rcIndex, rcValidating, rcId, rcUnequal );
                ERROR ( rc, "mismatch of new ids %,ld != %,ld", new_id, ids [ i ] . new_id );
                return;
            }
        }
    }
}

void MapFileConsistencyCheck ( const MapFile *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    IdxMapping *ids;
    const size_t max_ids = 1024 * 1024;

    STATUS ( 2, "running consistency check on index files" );

    /* "self" is valid */
    if ( self == NULL )
    {
        ANNOTATE ( "checking the validity of a NULL MapFile" );
        return;
    }

    STATUS ( 2, "checking id_size" );

    /* number of ids requires some number of bits */
    if ( ( ( uint64_t ) 1 << ( self -> id_size * 8 ) ) < self -> num_ids )
    {
        rc = RC ( rcExe, rcIndex, rcValidating, rcSize, rcIncorrect );
        ERROR ( rc, "calculated id size ( %zu bytes ) is insufficient to represent count of %,lu ids",
                self -> id_size, self -> num_ids );
        return;
    }

    STATUS ( 2, "allocating id buffer" );

    TRY ( ids = MemAlloc ( ctx, sizeof * ids * max_ids, false ) )
    {
        MapFileCrossCheck ( self, ctx, ids, max_ids );
        MemFree ( ctx, ids, sizeof * ids * max_ids );
    }
    CATCH_ALL ()
    {
        ANNOTATE ( "failed to allocate memory ( %zu pairs ) for consistency check", max_ids );
        return;
    }

    STATUS ( 2, "finished idx consistency check" );
}
