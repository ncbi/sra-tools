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

typedef struct KQueueFile KQueueFile;
#define KFILE_IMPL KQueueFile

#include <kapp/extern.h>
#include <loader/queue-file.h>

#include <kfs/file.h>
#include <kfs/impl.h>
#include <kproc/timeout.h>
#include <kproc/thread.h>
#include <kproc/queue.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <os-native.h>
#include <klib/out.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if _DEBUGGING && 0
#define DEFAULT_BLOCK_SIZE 4 * 1024
#define QFMSG( msg, ... ) \
    KOutMsg ( msg, __VA_ARGS__ )
#else
#define DEFAULT_BLOCK_SIZE 64 * 1024
#define QFMSG( msg, ... ) \
    ( void ) 0
#endif

#define DEFAULT_TIMEOUT_MS 150

/*--------------------------------------------------------------------------
 * KQueueFileBuffer
 *  a block that describes its valid bytes and position
 */
typedef struct KQueueFileBuffer KQueueFileBuffer;
struct KQueueFileBuffer
{
    uint64_t pos;
    size_t bytes;
    rc_t rc;
    uint32_t align;
    uint8_t data [ 1 ];
};

static
bool KQueueFileBufferContains ( const KQueueFileBuffer *self, uint64_t pos )
{
    if ( self != NULL )
    {
        if ( self -> pos <= pos && ( self -> pos + self -> bytes ) > pos )
            return true;
    }
    return false;
}


/*--------------------------------------------------------------------------
 * KQueueFile
 *  an extension to KFile that runs on a background thread
 */
struct KQueueFile
{
    KFile dad;

    /* the source file's size on open */
    uint64_t apparent_size;

    /* starting position for read-ahead */
    uint64_t start_pos;

    /* the file really being accessed */
    KFile *f;

    /* buffer info */
    KQueueFileBuffer *b;
    size_t bsize;

    /* thread and buffer queue */
    KThread *t;
    KQueue *q;

    /* captured upon open */
    rc_t rc_from_size_on_open;

    /* timeout when queue is not responding */
    uint32_t timeout_ms;
};

/* Whack
 */
static
rc_t KQueueFileWhackRead ( KQueueFile *self )
{
    void *b;

    /* no more reading */
    QFMSG ( "%s: sealing queue\n", __func__ );
    KQueueSeal ( self -> q );

    /* flush the queue */
    QFMSG ( "%s: popping queue\n", __func__ );
    while ( KQueuePop ( self -> q, & b, NULL ) == 0 )
    {
        QFMSG ( "%s: dousing a buffer\n", __func__ );
        free ( b );
    }

    /* wait for thread to exit */
    QFMSG ( "%s: waiting for bg thread to exit\n", __func__ );
    KThreadWait ( self -> t, NULL );

    /* tear it down */
    QFMSG ( "%s: freeing object\n", __func__ );
    free ( self -> b );
    KThreadRelease ( self -> t );
    KQueueRelease ( self -> q );
    KFileRelease ( self -> f );
    free ( self );

    return 0;
}

static
rc_t KQueueFileFlush ( KQueueFile *self, KQueueFileBuffer *b )
{
    rc_t rc;

    /* timeout is in mS */
    timeout_t tm;
    QFMSG ( "%s: initializing timeout for %,umS\n", __func__, self->timeout_ms);
    TimeoutInit ( & tm, self->timeout_ms );

    /* push buffer */
    QFMSG ( "%s: pushing buffer...\n", __func__ );
    rc = KQueuePush ( self -> q, b, & tm );
    QFMSG ( "%s: ...done: rc = %R\n", __func__, rc );
    if ( rc != 0 )
    {
        /* see if the bg thread exited */
        if ( GetRCState ( rc ) == rcReadonly )
        {
            rc = RC ( rcApp, rcFile, rcWriting, rcTransfer, rcIncomplete );
            QFMSG ( "%s: resetting rc to %R\n", __func__, rc );
        }
        return rc;
    }

    QFMSG ( "%s: forgetting about buffer\n", __func__ );
    self -> b = NULL;
    return 0;
}

static
rc_t KQueueFileWhackWrite ( KQueueFile *self )
{
    void *b;
    rc_t rc = 0;

    /* flush last buffer */
    if ( self -> b != NULL )
    {
        QFMSG ( "%s: have non-null buffer\n", __func__ );
        if ( self -> b -> bytes != 0 )
        {
            QFMSG ( "%s: buffer has %zu bytes\n", __func__, self -> b -> bytes );
            rc = KQueueFileFlush ( self, self -> b );
        }

        free ( self -> b );
        self -> b = NULL;
    }

    /* no more writing */
    QFMSG ( "%s: sealing queue\n", __func__ );
    KQueueSeal ( self -> q );

    /* wait for thread to exit */
    QFMSG ( "%s: waiting for bg thread to exit\n", __func__ );
    KThreadWait ( self -> t, NULL );

    /* the file should be written
       but flush the queue if bg thread didn't */
    QFMSG ( "%s: popping queue\n", __func__ );
    while ( KQueuePop ( self -> q, & b, NULL ) == 0 )
    {
        QFMSG ( "%s: dousing a buffer\n", __func__ );
        free ( b );
    }

    /* tear it down */
    QFMSG ( "%s: freeing object\n", __func__ );
    KThreadRelease ( self -> t );
    KQueueRelease ( self -> q );
    KFileRelease ( self -> f );
    free ( self );

    return rc;
}

/* GetSysFile
 *  returns an underlying system file object
 *  and starting offset to contiguous region
 *  suitable for memory mapping, or NULL if
 *  no such file is available.
 */
static
struct KSysFile* CC KQueueFileGetSysFile ( const KQueueFile *self, uint64_t *offset )
{
    return NULL;
}

/* RandomAccess
 *  ALMOST by definition, the file is random access
 *
 *  certain file types will refuse random access
 *  these include FIFO and socket based files, but also
 *  wrappers that require serial access ( e.g. compression )
 *
 *  returns 0 if random access, error code otherwise
 */
static
rc_t CC KQueueFileRandomAccess ( const KQueueFile *self )
{
    return RC ( rcFS, rcFile, rcAccessing, rcFunction, rcUnsupported );
}

/* Type
 *  returns a KQueueFileDesc
 *  not intended to be a content type,
 *  but rather an implementation class
 */
static
uint32_t CC KQueueFileType ( const KQueueFile *self )
{
    return kptFIFO;
}

/* Size
 *  returns size in bytes of file
 *
 *  "size" [ OUT ] - return parameter for file size
 */
static
rc_t CC KQueueFileSize ( const KQueueFile *self, uint64_t *size )
{
    * size = self -> apparent_size;
    return self -> rc_from_size_on_open;
}

/* SetSize
 *  sets size in bytes of file
 *
 *  "size" [ IN ] - new file size
 */
static
rc_t CC KQueueFileSetSize ( KQueueFile *self, uint64_t size )
{
    return RC ( rcFS, rcFile, rcAccessing, rcFunction, rcUnsupported );
}


/* RunRead
 *  runs read loop on background thread
 */
static
rc_t CC KQueueFileRunRead ( const KThread *t, void *data )
{
    KQueueFile *self = data;

    rc_t rc;
    bool loop;
    uint64_t pos;
    size_t num_read;

    QFMSG ( "BG: %s: running read-ahead loop\n", __func__ );
    for ( rc = 0, loop = true, pos = self -> start_pos; loop; pos += num_read )
    {
        timeout_t tm;

        KQueueFileBuffer *b = malloc ( sizeof * b - sizeof b -> data + self -> bsize );
        if ( b == NULL )
        {
            rc = RC ( rcApp, rcFile, rcReading, rcMemory, rcExhausted );
            QFMSG ( "BG: %s: failed to allocate %zu byte buffer\n", __func__, self -> bsize );
            b = malloc ( sizeof * b );
            if ( b == NULL )
                break;

            num_read = 0;
            b -> rc = rc;
            QFMSG ( "BG: %s: created empty buffer with rc = %R.\n", __func__, b -> rc );
            loop = false;
        }
        else
        {
            QFMSG ( "BG: %s: reading %zu byte buffer\n", __func__, self -> bsize );
            b -> rc = KFileReadAll ( self -> f, pos, b -> data, self -> bsize, & num_read );
            QFMSG ( "BG: %s: read %zu bytes, rc = %R\n", __func__, num_read, b -> rc );
            if ( b -> rc != 0 || num_read == 0 )
                loop = false;
        }

        /* update buffer data */
        b -> pos = pos;
        b -> bytes = num_read;
#if _DEBUGGING
        b -> align = 0;
#endif

PushAgain:
        /* timeout is in mS */
        QFMSG ( "BG: %s: initializing timeout for %,umS\n", __func__, self->timeout_ms );
        TimeoutInit ( & tm, self->timeout_ms );

        /* push buffer */
        QFMSG ( "BG: %s: pushing buffer...\n", __func__ );
        rc = KQueuePush ( self -> q, b, & tm );
        QFMSG ( "BG: %s: ...done: rc = %R\n", __func__, rc );
        if ( rc != 0 )
        {
            /* if queue has been sealed by fg */
            if ( GetRCState ( rc ) == rcReadonly )
            {
                rc = 0;
                QFMSG ( "BG: %s: clearing rc\n", __func__ );

            } else if( GetRCObject(rc) == ( enum RCObject )rcTimeout && GetRCState(rc) == rcExhausted ) {
                goto PushAgain;
            }
            QFMSG ( "BG: %s: dousing buffer\n", __func__ );
            free ( b );
            break;
        }
    }
    
    /* going to exit thread */
    QFMSG ( "BG: %s: sealing thread\n", __func__ );
    KQueueSeal ( self -> q );

    QFMSG ( "BG: %s: exit with rc = %R\n", __func__, rc );
    return rc;
}

/* Read
 *  read file from known position
 *
 *  "pos" [ IN ] - starting position within file
 *
 *  "buffer" [ OUT ] and "bsize" [ IN ] - return buffer for read
 *
 *  "num_read" [ OUT ] - return parameter giving number of bytes
 *  actually read. when returned value is zero and return code is
 *  also zero, interpreted as end of file.
 */
static
rc_t CC KQueueFileNoRead ( const KQueueFile *self, uint64_t pos,
    void *buffer, size_t bsize, size_t *num_read )
{
    return RC ( rcFS, rcFile, rcReading, rcFile, rcWriteonly );
}

static
rc_t CC KQueueFileRead ( const KQueueFile *cself, uint64_t pos,
    void *buffer, size_t bsize, size_t *num_read )
{
    rc_t rc;
    size_t to_read;
    KQueueFile *self = ( KQueueFile* ) cself;
    KQueueFileBuffer *b = self -> b;

    assert ( num_read != NULL );
    assert ( * num_read == 0 );
    assert ( buffer != NULL && bsize != 0 );

    /* detect attempt to back up */
    if ( b != NULL && pos < b -> pos )
    {
        QFMSG ( "%s: detected attempt to back up from %lu to %lu\n", __func__, b -> pos, pos );
        return RC ( rcApp, rcFile, rcReading, rcConstraint, rcViolated );
    }
    
    QFMSG ( "%s: loop to read %zu bytes at offset %lu\n", __func__, bsize, pos );
    while ( ! KQueueFileBufferContains ( b, pos ) )
    {
        /* fetch next buffer */
        KQueueFileBuffer *tmp;

        /* timeout is in mS */
        timeout_t tm;
        QFMSG ( "%s: initializing timeout for %,umS\n", __func__, self->timeout_ms );
        TimeoutInit ( & tm, self->timeout_ms );

        /* pop buffer */
        QFMSG ( "%s: popping buffer...\n", __func__ );
        rc = KQueuePop ( self -> q, ( void** ) & tmp, & tm );
        QFMSG ( "%s: ...done: rc = %R\n", __func__, rc );
        if ( rc != 0 )
        {
            /* see if the bg thread is done */
            if ( GetRCState ( rc ) == rcDone && GetRCObject ( rc ) == ( enum RCObject )rcData )
            {
                QFMSG ( "%s: BG thread has exited with end-of-input\n", __func__ );
                return 0;
            } else if( GetRCObject(rc) == ( enum RCObject )rcTimeout && GetRCState(rc) == rcExhausted ) {
                continue; /* timeout is not a problem, try again */
            }

            QFMSG ( "%s: read failed with rc = %R\n", __func__, rc );
            return rc;
        }

        /* queue is not supposed to be able to contain NULL */
        assert ( tmp != NULL );

        /* buffer may indicate read error or end of input */
        if ( tmp -> rc != 0 || tmp -> bytes == 0 )
        {
            rc = tmp -> rc;
            QFMSG ( "%s: BG thread has delivered %zu size buffer with rc = %R\n", __func__, tmp -> bytes, rc );
            free ( tmp );
            return rc;
        }

        /* free existing buffer */
        if ( b != NULL )
        {
            QFMSG ( "%s: freeing buffer of %zu bytes at position %lu\n", __func__, b -> bytes, b -> pos );
            free ( b );
        }

        /* take new buffer */
        self -> b = b = tmp;
        QFMSG ( "%s: caching buffer of %zu bytes at position %lu\n", __func__, b -> bytes, b -> pos );
    }

    pos -= b -> pos;
    to_read = b -> bytes - ( size_t ) pos;
    if ( to_read > bsize )
        to_read = bsize;

    QFMSG ( "%s: copying %zu bytes from buffer at local offset %lu\n", __func__, to_read, pos );
    memmove ( buffer, & b -> data [ pos ], to_read );

    QFMSG ( "%s: successful read\n", __func__ );
    * num_read = to_read;
    return 0;
}


/* RunWrite
 *  runs write loop on background thread
 */
static
rc_t CC KQueueFileRunWrite ( const KThread *t, void *data )
{
    KQueueFile *self = data;

    rc_t rc;
    do
    {
        size_t num_writ;
        KQueueFileBuffer *b;

        /* timeout is in mS */
        timeout_t tm;
        TimeoutInit ( & tm, self->timeout_ms );

        /* pop buffer */
        rc = KQueuePop ( self -> q, ( void** ) & b, & tm );
        if ( rc != 0 )
        {
            /* see if the fg thread is done */
            if ( GetRCState ( rc ) == rcDone && GetRCObject ( rc ) == ( enum RCObject )rcData )
            {
                rc = 0;
            }
            else if ( GetRCObject(rc) == ( enum RCObject )rcTimeout && GetRCState(rc) == rcExhausted )
            {
                rc = 0;
                continue;
            }

            break;
        }

        /* queue won't accept NULL object references */
        assert ( b != NULL );

        /* look at buffer for end of input */
        if ( b -> rc != 0 || b -> bytes == 0 )
        {
            free ( b );
            break;
        }

        /* write to file */
        rc = KFileWriteAll ( self -> f, b -> pos, b -> data, b -> bytes, & num_writ );
        assert ( num_writ == b -> bytes || rc != 0 );

        /* TBD - need a better way to deal with this */
        if ( rc != 0 )
        {
            PLOGERR ( klogSys, ( klogSys, rc, "$(func): wrote $(num_writ) of $(bytes) bytes to file",
                                 "func=%s,num_writ=%zu,bytes=%zu", __func__, num_writ, b -> bytes ) );
        }

        /* done with buffer */
        free ( b );
    }
    while ( rc == 0 );

    /* going to exit thread */
    KQueueSeal ( self -> q );

    return rc;
}

/* Write
 *  write file at known position
 *
 *  "pos" [ IN ] - starting position within file
 *
 *  "buffer" [ IN ] and "size" [ IN ] - data to be written
 *
 *  "num_writ" [ OUT, NULL OKAY ] - optional return parameter
 *  giving number of bytes actually written
 */
static
rc_t CC KQueueFileNoWrite ( KQueueFile *self, uint64_t pos,
    const void *buffer, size_t size, size_t *num_writ )
{
    return RC ( rcApp, rcFile, rcWriting, rcFile, rcReadonly );
}

static
rc_t CC KQueueFileWrite ( KQueueFile *self, uint64_t pos,
    const void *buffer, size_t size, size_t *num_writ )
{
    rc_t rc;
    size_t total, to_write;

    for ( rc = 0, total = 0; total < size; pos += to_write, total += to_write )
    {
        /* current buffer */
        KQueueFileBuffer *b = self -> b;

        /* if we have a buffer, test for a write within bounds */
        if ( self -> b != NULL )
        {
            /* an attempt to write first byte BEFORE
               current buffer or BEYOND current contents
               or AT current end while being full provokes
               a flush of the current buffer */
            if ( pos < b -> pos ||
                 pos > b -> pos + b -> bytes ||
                 pos == b -> pos + self -> bsize )
            {
                rc = KQueueFileFlush ( self, b );
                if ( rc != 0 )
                {   /* the background thread is probably gone */
                    break;
                }
            }
        }

        /* if by now we don't have a buffer, allocate one */
        if ( self -> b == NULL )
        {
            b = malloc ( sizeof * b - sizeof b -> data + self -> bsize );
            if ( b == NULL )
            {
                rc = RC ( rcApp, rcFile, rcWriting, rcMemory, rcExhausted );
                break;
            }

            self -> b = b;
            b -> pos = pos/* + total*/;
            b -> bytes = 0;
            b -> rc = 0;
#if _DEBUGGING
            b -> align = 0;
#endif
        }

        assert ( b != NULL );
        assert ( b == self -> b );
/*        assert ( pos >= b -> pos ); */
        assert ( pos <= b -> pos + b -> bytes );
        assert ( pos < b -> pos + self -> bsize );

        /* write into the buffer */
        to_write = size - total;
        if ( b -> bytes + to_write > self -> bsize )
            to_write = self -> bsize - b -> bytes;

        memmove ( & b -> data [ b -> bytes ], & ( ( const uint8_t* ) buffer ) [ total ], to_write );
        b -> bytes += to_write;
    }


    * num_writ = total;
    if ( total != 0 )
        return 0;

    return rc;
}


static KFile_vt_v1 KQueueFileRead_vt_v1 =
{
    /* version 1.1 */
    1, 1,

    /* v1.0 */
    KQueueFileWhackRead,
    KQueueFileGetSysFile,
    KQueueFileRandomAccess,
    KQueueFileSize,
    KQueueFileSetSize,
    KQueueFileRead,
    KQueueFileNoWrite,

    /* v1.1 */
    KQueueFileType
};


static KFile_vt_v1 KQueueFileWrite_vt_v1 =
{
    /* version 1.1 */
    1, 1,

    /* v1.0 */
    KQueueFileWhackWrite,
    KQueueFileGetSysFile,
    KQueueFileRandomAccess,
    KQueueFileSize,
    KQueueFileSetSize,
    KQueueFileNoRead,
    KQueueFileWrite,

    /* v1.1 */
    KQueueFileType
};


/* MakeRead
 *  make a queue file for reading-ahead on background thread
 *
 *  when the file is created, a background thread is started
 *  that begins reading from "src" at position "pos", into
 *  buffers of size "buffer_size". each buffer is pushed into
 *  a cross-thread queue where it is consumed by the reading
 *  thread.
 *
 *  the background thread is throttled by queue capacity - determined
 *  by "queue_bytes" and "block_size", such that if the queue is full,
 *  the thread will sleep. the consumer thread is also throttled by the
 *  queue in that it will sleep if the queue is empty with pending data.
 *
 *  the background thread will exit upon reaching end of file,
 *  upon a permanent error, or if the queue is sealed by the consumer
 *  thread.
 *
 *  when the file is collected in response to a release message,
 *  the queue will be sealed against further inserts, pending buffers
 *  will be discarded, the background thread will be joined, and
 *  the source file will be released.
 *
 *  the intended usage is serial reading of the file. reads
 *  may only progress forward, i.e. backing up is not permitted.
 *
 *  "qf" [ OUT ] - return parameter for queue file
 *
 *  "pos" [ IN ] - starting position for reads from "src".
 *  NB - "src" must support being addressed at this position.
 *
 *  "src" [ IN ] - source file for read-ahead on background thread.
 *  must have read permissions.
 *
 *  "queue_bytes" [ IN ] - the read-ahead limit of the background
 *  thread, in bytes. this is the amount of data that will be queued
 *  for the consumer thread before the bg thread sleeps.
 *
 *  "block_size" [ IN, DEFAULT ZERO ] - optional parameter giving
 *  desired block size when reading from "src". this may be used
 *  to tune reading for source data, e.g. 64K blocks for gzip.
 */
LIB_EXPORT rc_t CC KQueueFileMakeRead ( const KFile **qfp, uint64_t pos,
    const KFile *src, size_t queue_bytes, size_t block_size, uint32_t timeout_ms )
{
    rc_t rc;

    if ( qfp == NULL )
        rc = RC ( rcApp, rcFile, rcConstructing, rcParam, rcNull );
    else
    {
        if ( src == NULL )
            rc = RC ( rcApp, rcFile, rcConstructing, rcFile, rcNull );
        else if ( ! src -> read_enabled )
        {
            if ( src -> write_enabled )
                rc = RC ( rcApp, rcFile, rcConstructing, rcFile, rcWriteonly );
            else
                rc = RC ( rcApp, rcFile, rcConstructing, rcFile, rcNoPerm );
        }
        else if ( pos == 0 || ( rc = KFileRandomAccess ( src ) ) == 0 )
        {
            KQueueFile *qf = malloc ( sizeof * qf );
            if ( qf == NULL )
                rc = RC ( rcApp, rcFile, rcConstructing, rcMemory, rcExhausted );
            else
            {
                rc = KFileInit ( & qf -> dad, ( const KFile_vt* ) & KQueueFileRead_vt_v1, "KQueueFile", "no-name", true, false );
                if ( rc == 0 )
                {
                    qf -> f = ( KFile* ) src;
                    rc = KFileAddRef ( src );
                    if ( rc == 0 )
                    {
                        uint32_t capacity;

                        /* zero block size means default */
                        if ( block_size == 0 )
                            block_size = DEFAULT_BLOCK_SIZE;

                        /* queue capacity is expressed in bytes originally
                           translate to buffer count */
                        capacity = ( queue_bytes + block_size - 1 ) / block_size;
                        if ( capacity == 0 )
                            capacity = 1;

                        /* actual capacity will be a power of 2 */
                        rc = KQueueMake ( & qf -> q, capacity );
                        if ( rc == 0 )
                        {
                            /* capture current size if supported */
                            qf -> rc_from_size_on_open = KFileSize ( src, & qf -> apparent_size );

                            /* starting position for read */
                            qf -> start_pos = pos;

                            /* requested buffer size */
                            qf -> b = NULL;
                            qf -> bsize = block_size;

                            /* timeout */
                            qf -> timeout_ms = timeout_ms == 0 ? DEFAULT_TIMEOUT_MS : timeout_ms;

                            /* finally, start the background thread */
                            rc = KThreadMake ( & qf -> t, KQueueFileRunRead, qf );
                            if ( rc == 0 )
                            {
                                * qfp = & qf -> dad;
                                return 0;
                            }

                            KQueueRelease ( qf -> q );
                        }

                        KFileRelease ( qf -> f );
                    }
                }

                free ( qf );
            }
        }

        * qfp = NULL;
    }

    return rc;
}


/* MakeWrite
 *  make a queue file for writing-behind on background thread
 *
 *  when the file is created, a background thread is started that
 *  waits for buffers to appear in the cross-thread queue. as the producer
 *  thread writes, data are accumulated into buffers which are pushed
 *  into the queue as they fill, and written in turn on the bg thread.
 *
 *  the producer thread is throttled by queue capacity - determined by
 *  "queue_bytes" and "block_size", such that if the queue is full,
 *  the thread will sleep. the background thread is also throttled by
 *  the queue in that it will sleep if the queue is empty with pending
 *  data.
 *
 *  the background thread will exit upon a permanent error, or if the
 *  queue is sealed by the producer thread.
 *
 *  when the file is collected in response to a release message,
 *  the queue will be sealed against further inserts, pending buffers
 *  will be written, the background thread will be joined, and
 *  the source file will be released.
 *
 *  the intended usage is serial writing of the file. random writes
 *  will be accepted, but may reduce the queue efficiency.
 *
 *  "qf" [ OUT ] - return parameter for queue file
 *
 *  "dst" [ IN ] - destination file for write-behind on background thread.
 *  must have write permissions.
 *
 *  "queue_bytes" [ IN ] - the write-behind limit of the producer
 *  thread, in bytes. this is the amount of data that will be queued
 *  for the background thread before the producer thread sleeps.
 *
 *  "block_size" [ IN, DEFAULT ZERO ] - optional parameter giving
 *  desired block size when writing to "dst". this may be used
 *  to tune writing for source data, e.g. 64K blocks for gzip.
 */
LIB_EXPORT rc_t CC KQueueFileMakeWrite ( KFile **qfp,
    KFile *dst, size_t queue_bytes, size_t block_size, uint32_t timeout_ms )
{
    rc_t rc;

    if ( qfp == NULL )
        rc = RC ( rcApp, rcFile, rcConstructing, rcParam, rcNull );
    else
    {
        if ( dst == NULL )
            rc = RC ( rcApp, rcFile, rcConstructing, rcFile, rcNull );
        else if ( ! dst -> write_enabled )
        {
            if ( dst -> read_enabled )
                rc = RC ( rcApp, rcFile, rcConstructing, rcFile, rcReadonly );
            else
                rc = RC ( rcApp, rcFile, rcConstructing, rcFile, rcNoPerm );
        }
        else
        {
            KQueueFile *qf = malloc ( sizeof * qf );
            if ( qf == NULL )
                rc = RC ( rcApp, rcFile, rcConstructing, rcMemory, rcExhausted );
            else
            {
                rc = KFileInit ( & qf -> dad, ( const KFile_vt* ) & KQueueFileWrite_vt_v1, "KQueueFile", "no-name", false, true );
                if ( rc == 0 )
                {
                    qf -> f = dst;
                    rc = KFileAddRef ( dst );
                    if ( rc == 0 )
                    {
                        uint32_t capacity;

                        /* zero block size means default */
                        if ( block_size == 0 )
                            block_size = DEFAULT_BLOCK_SIZE;

                        /* queue capacity is expressed in bytes originally
                           translate to buffer count */
                        capacity = ( queue_bytes + block_size - 1 ) / block_size;
                        if ( capacity == 0 )
                            capacity = 1;

                        /* actual capacity will be a power of 2 */
                        rc = KQueueMake ( & qf -> q, capacity );
                        if ( rc == 0 )
                        {
                            /* capture current size if supported */
                            qf -> rc_from_size_on_open = KFileSize ( dst, & qf -> apparent_size );

                            /* starting position not used */
                            qf -> start_pos = 0;

                            /* requested buffer size */
                            qf -> b = NULL;
                            qf -> bsize = block_size;

                            /* timeout */
                            qf -> timeout_ms = timeout_ms == 0 ? DEFAULT_TIMEOUT_MS : timeout_ms;

                            /* finally, start the background thread */
                            rc = KThreadMake ( & qf -> t, KQueueFileRunWrite, qf );
                            if ( rc == 0 )
                            {
                                * qfp = & qf -> dad;
                                return 0;
                            }

                            KQueueRelease ( qf -> q );
                        }

                        KFileRelease ( qf -> f );
                    }
                }

                free ( qf );
            }
        }

        * qfp = NULL;
    }

    return rc;
}
