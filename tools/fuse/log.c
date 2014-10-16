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
#include <kfs/directory.h>
#include <klib/out.h>
#include <klib/status.h>
#include <kproc/thread.h>

#include "log.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

static unsigned int g_sync = 0;
static char* g_path = NULL;
static int g_fd = -1;
static KThread* g_thread = NULL;
static bool g_foreground = false;

rc_t StrDup(const char* src, char** dst)
{
    if( src == NULL || dst == NULL ) {
        return RC(rcExe, rcString, rcCopying, rcParam, rcNull);
    } else {
        size_t sz = strlen(src);
        MALLOC(*dst, sz + 1);
        if( dst == NULL ) {
            return RC(rcExe, rcString, rcCopying, rcMemory, rcExhausted);
        }
        memcpy(*dst, src, sz);
        (*dst)[sz] = '\0';
    }
    return 0;
}

static
rc_t LogFileOpen(void)
{
    rc_t rc = 0;
    int old_fd = -1;

    if( g_path != NULL ) {
        int new_fd = open(g_path, O_WRONLY | O_APPEND | O_CREAT, 0664);
        if( new_fd < 0 ) {
            rc = RC(rcExe, rcLog, rcOpening, rcNoObj, rcIncomplete);
            PLOGERR(klogErr, (klogErr, rc, "'$(s)': $(e)", PLOG_2(PLOG_S(s),PLOG_S(e)), g_path, strerror(errno)));
        } else {
            old_fd = g_fd;
            g_fd = new_fd;
            
            DEBUG_MSG(1, ("Log file opened '%s'\n", g_path));
            
            if( g_foreground ) {
                if( dup2(g_fd, STDOUT_FILENO) < 0) {
                    PLOGMSG(klogErr, (klogErr, "Cannot dup2(stdout) on '$(s)'", PLOG_S(s), g_path));
                }
                if( dup2(g_fd, STDERR_FILENO) < 0) {
                    PLOGMSG(klogErr, (klogErr, "Cannot dup2(stderr) on '$(s)'", PLOG_S(s), g_path));
                }
            }
        }
    }
    if( old_fd > -1 ) {
        sleep(2);
        close(old_fd);
    }
    return rc;
}

static
rc_t LogThread( const KThread *self, void *data )
{
    PLOGMSG(klogInfo, (klogInfo, "Log rotation thread started with $(s) sec", PLOG_U32(s), g_sync));
    while( g_sync > 0 ) {
        sleep(g_sync);
        DEBUG_MSG(1, ("Log rotation thread checking %s\n", g_path));
        if( g_sync < 1 || g_path == NULL ) {
            break;
        }
        LogFileOpen();
    }
    LOGMSG(klogInfo, "Log rotation thread ended");
    return 0;
}

static
rc_t LogFileWrite(void *self, const char *buffer, size_t bufsize, size_t *num_writ)
{
    rc_t rc = 0;

    *num_writ = bufsize;
    if( g_fd > -1 ) {
        char tid[64 * 1024];
        if( bufsize <= (sizeof(tid) - (1 + 5 + 1 + 15 + 2)) ) {
            int x = sprintf(tid, "[%i/%x] ", getpid(), (unsigned int)pthread_self());
            memcpy(&tid[x], buffer, bufsize);
            bufsize += x;
            if( pwrite(g_fd, tid, bufsize, SEEK_END) != bufsize ) {
                rc = RC(rcExe, rcLog, rcWriting, rcNoObj, rcIncomplete);
            }
        } else {
            if( pwrite(g_fd, buffer, bufsize, SEEK_END) != bufsize ) {
                rc = RC(rcExe, rcLog, rcWriting, rcNoObj, rcIncomplete);
            }
        }
    }
    return rc;
}

rc_t LogFile_Init(const char* path, unsigned int sync, bool foreground, int* log_fd)
{
    rc_t rc = 0;

    if( path != NULL ) {
        KDirectory *dir = NULL;
        if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
            char buf[4096];
            if( (rc = KDirectoryResolvePath(dir, true, buf, 4096, "%s", path)) == 0 ) {
                if( (rc = StrDup(buf, &g_path)) == 0 ) {
                    if( (rc = KOutHandlerSet(LogFileWrite, NULL)) == 0 &&
                        (rc = KDbgHandlerSet(LogFileWrite, NULL)) == 0 &&
                        (rc = KLogHandlerSet(LogFileWrite, NULL)) == 0 &&
                        (rc = KLogLibHandlerSet(LogFileWrite, NULL)) == 0 &&
                        (rc = KStsHandlerSet(LogFileWrite, NULL)) == 0 &&
                        (rc = KStsLibHandlerSet(LogFileWrite, NULL)) == 0 ) {
                        g_sync = sync;
                        g_foreground = foreground;
                        if( g_sync > 0 ) {
                            PLOGMSG(klogInfo, (klogInfo, "Log $(x) rotation set to $(s) seconds",
                                     PLOG_2(PLOG_S(x),PLOG_U32(s)), g_path, g_sync));
                        }
                    }
                }
            }
            ReleaseComplain(KDirectoryRelease, dir);
        }
    }
    rc = LogFileOpen();
    if( rc == 0 && path == NULL && g_sync > 0 ) {
        if( (rc = KThreadMake(&g_thread, LogThread, NULL)) != 0 ) {
            LOGERR(klogErr, rc, "Log rotation thread");
        } else {
            DEBUG_MSG(1, ("Log rotation thread launched %s\n", g_sync));
        }
    }
    if( log_fd != NULL && g_fd != -1) {
        *log_fd = g_fd;
    }
    return rc;
}

void LogFile_Fini(void)
{
    g_sync = 0;
    if( g_thread != NULL ) {
        ReleaseComplain(KThreadCancel, g_thread);
        ReleaseComplain(KThreadRelease, g_thread);
    }
    if( g_fd >= 0 ) {
        close(g_fd);
    }
    FREE(g_path);
    g_path = NULL;
    g_fd = -1;
    g_thread = NULL;
}
