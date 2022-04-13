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
#ifndef _h_kapp_queue_file_
#define _h_kapp_queue_file_

#ifndef _h_kapp_extern_
#include <kapp/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*--------------------------------------------------------------------------
 * KQueueFile
 *  an extension to KFile that runs on a background thread
 */
struct KFile;


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
 *
 *  "timeout_ms" [ IN, DEFAULT ZERO ] - optional parameter specifying the period of time (in ms)
 *  at which the background thread will check whether it is to quit (e.g. the foregrount thread has sealed the buffer),
 *  when the queue cannot be written into.  If 0 specified, the timeout is set to 150 ms.
 */
KAPP_EXTERN rc_t CC KQueueFileMakeRead ( struct KFile const **qf, uint64_t pos,
    struct KFile const *src, size_t queue_bytes, size_t block_size, uint32_t timeout_ms );


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
 *
 *  "timeout_ms" [ IN, DEFAULT ZERO ] - optional parameter specifying the period of time (in ms)
 *  at which the background thread will check whether it is to quit (e.g. the foregrount thread has sealed the buffer),
 *  when the queue cannot be read from.  If 0 specified, the timeout is set to 150 ms.
 */
KAPP_EXTERN rc_t CC KQueueFileMakeWrite ( struct KFile **qf,
    struct KFile *dst, size_t queue_bytes, size_t block_size, uint32_t timeout_ms );


#ifdef __cplusplus
}
#endif

#endif /* _h_kapp_queue_file_ */
