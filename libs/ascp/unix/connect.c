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

#include "ascp-priv.h" /* STS_DBG */

#include <kfs/directory.h> /* KDirectory */

#include <klib/time.h> /* KTimeStamp */
#include <klib/printf.h> /* string_printf */
#include <klib/text.h> /* string_copy */
#include <klib/log.h> /* LOGERR */
#include <klib/status.h> /* STSMSG */
#include <klib/out.h> /* OUTMSG */
#include <klib/rc.h> /* RC */

#include <assert.h>
#include <ctype.h> /* isspace */
#include <os-native.h>
#include <errno.h>
#include <fcntl.h> /* open */
#include <signal.h> /* kill */
#include <stdlib.h> /* system */
#include <stdio.h> /* fflush */
#include <unistd.h> /* dup */
#include <sys/wait.h> /* waitpid */

#define DISP_RC(rc, err) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, err))

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)

static int logevp(const char *file, char *const argv[], bool dryRun) {
    int result = 0;
    KStsLevel lvl = STAT_PWR;
    if (dryRun)
        lvl = STAT_USR;
    if (lvl > KStsLevelGet()) {
        return 0;
    }
    result = printf("%s", file);
    while (true) {
        int res = printf(" %s", *argv);
        if (result >= 0) {
            if (res < 0) {
                result = res;
            }
            else {
                result += res;
            }
        }
        if (*(++argv) == NULL) {
            break;
        }
    }
    printf("\n");
    return result;
}

static
uint64_t humanize(uint64_t number, char *sfx, uint64_t *fraction)
{
    assert(sfx);
    if (fraction != NULL) {
        *fraction = 0;
    }
    *sfx = 'B';
    if (number > 1024L * 1024L * 1024L) {
        if (fraction != NULL) {
            number *= 1000;
        }
        number /=     1024L * 1024L * 1024L;
        if (fraction != NULL) {
            *fraction = number % 1000;
            number /= 1000;
        }
        *sfx = 'G';
    }
    else if (number > 1024L * 1024L) {
        number /=     1024L * 1024L;
        *sfx = 'M';
    }
    else if (number > 1024) {
        number /=     1024;
        *sfx = 'K';
    }
    return number;
}

static void progress(const char *acc, uint64_t sz,
    uint64_t srcSz, uint64_t hSrc, char sfSrc, KTime_t date)
{
    if (sz > 0) {
        if (srcSz > 0) {
            uint64_t p = 100 * sz / srcSz;
            char sf = 'B';
            uint64_t fr = 0;
            uint64_t h = humanize(sz, &sf, &fr);
            if (p > 0) {
                if (sfSrc != 'B' && sf != 'B') {
                    if (fr == 0) {
                        if (date == 0) {
                            OUTMSG(("%s %,ld/%,ld %ld%c/%,ld%c %ld%%        \r",
                                    acc, sz, srcSz, h,sf,hSrc,sfSrc,p));
                        }
                        else {
                            OUTMSG(("%s %,ld/%,ld %ld%c/%,ld%c %ld%% %ld    \r",
                                    acc, sz, srcSz, h,sf,hSrc,sfSrc,p,
                                    KTimeStamp() - date));
                        }
                    }
                    else {
                        OUTMSG(("%s %,ld/%,ld %ld.%03ld%c/%,ld%c %ld%%      \r",
                                acc, sz, srcSz,h,fr,sf,hSrc,sfSrc,p));
                    }
                }
                else {
                    OUTMSG(("%s %,ld/%,ld %ld%%                             \r",
                        acc, sz, srcSz, p));
                }
            }
            else {
                if (sfSrc != 'B' && sf != 'B') {
                    if (fr == 0) {
                        OUTMSG((
                        "%s %,ld/%,ld %ld%c/%ld%c                           \r",
                         acc, sz,srcSz,h, sf,hSrc,sfSrc));
                    }
                    else {
                        OUTMSG((
                        "%s %,ld/%,ld %ld.%03ld%c/%ld%c                     \r",
                         acc, sz,srcSz,h, fr,sf,hSrc,sfSrc));
                    }
                }
                else {
                    OUTMSG(("%s %,ld/%,ld                 \r", acc, sz, srcSz));
                }
            }
        }
        else {
            OUTMSG(("%s %,ld                                     \r", acc, sz));
        }
    }
    else {
        OUTMSG(("                                                \r%s\r", acc));
    }
}

/******************************************************************************/
rc_t run_ascp(const char *path, const char *key,
    const char *src, const char *dest, const AscpOptions *opt)
{
    const char *host = NULL;
    const char *user = NULL;
    const char *maxRate = NULL;
    bool cache_key = false;
    uint64_t heartbeat = 0;
    const char *acc = NULL;
    uint64_t srcSz = 0;
    uint64_t id = 0;
    TProgress *callback = NULL;
    TQuitting *quitting = NULL;
    rc_t rc = 0;
    pid_t nPid = 0;
    int pipeto[2];      /* pipe to feed the exec'ed program input */
    int pipefrom[2];    /* pipe to get the exec'ed program output */
#define ARGV_SZ 64
    char *argv[ARGV_SZ];
    char extraOptions[4096] = "";

    int i = 0;
    int ret = 0;
    i = 0;

    if (opt != NULL) {
        acc = opt->name;
        cache_key = opt->cache_key;
        callback = opt->callback;
        heartbeat = opt->heartbeat;
        host = opt->host;
        id = opt->id;
        quitting = opt->quitting;
        srcSz = opt->src_size;
        user = opt->user;

        if (opt->ascp_options != NULL) {
            size_t s = string_size(opt->ascp_options);
            if (s >= sizeof extraOptions) {
                LOGERR(klogErr,
                    RC(rcExe, rcProcess, rcCreating, rcBuffer, rcInsufficient),
                    "extra ascp options are ignored");
                
                maxRate = opt->target_rate;
            }
            else {
                string_copy
                    (extraOptions, sizeof extraOptions, opt->ascp_options, s);
            }
        }
        else {
            maxRate = opt->target_rate;
        }
    }

    if (acc == NULL) {
        acc = dest;
    }

    if (heartbeat > 0) {
        heartbeat /= 1000;
        if (heartbeat == 0) {
            heartbeat = 1;
        }
    }

    argv[i++] = (char*)path;
    argv[i++] = "-i";
    argv[i++] = (char*)key;
    argv[i++] = "-pQTk1";
    if (maxRate != NULL && maxRate[0] != '\0') {
        argv[i++] = "-l";
        argv[i++] = (char*)maxRate;
    }
    if (user != NULL) {
        argv[i++] = "--user";
        argv[i++] = (char*)user;
    }
    if (host != NULL) {
        argv[i++] = "--host";
        argv[i++] = (char*)user;
    }

    if (extraOptions[0] != '\0') {
        bool done = false;
        char *c = extraOptions;
        while (!done) {
            while (true) {
                if (*c == '\0') {
                    break;
                }
                else if (isspace(*c)) {
                    ++c;
                }
                else {
                    break;
                }
            }
            if (*c == '\0') {
                break;
            }
            else {
                argv[i++] = c;
            }
            while (true) {
                if (*c == '\0') {
                    done = true;
                    break;
                }
                else if (isspace(*c)) {
                    *(c++) = '\0';
                    break;
                }
                ++c;
            }
            if (i > ARGV_SZ - 4) {
                LOGERR(klogErr,
                    RC(rcExe, rcProcess, rcCreating, rcBuffer, rcInsufficient),
                    "too mary extra ascp options - some of them are ignored");
                break;
            }
        }
    }

    argv[i++] = (char*)src;
    argv[i++] = (char*)dest;
    argv[i++] = NULL;

    logevp(path, argv, opt->dryRun);

    if (opt->dryRun)
        return 0;

    if (pipe(pipeto) != 0) {
        perror("pipe() to");
        rc = RC(rcExe, rcFileDesc, rcCreating, rcFileDesc, rcFailed);
        LOGERR(klogErr, rc, "while pipe");
        return rc;
    }
    if (pipe(pipefrom) != 0) {
        perror("pipe() from");
        rc = RC(rcExe, rcFileDesc, rcCreating, rcFileDesc, rcFailed);
        LOGERR(klogErr, rc, "while pipe");
        return rc;
    }

    if (quitting) {
        rc = quitting();
    }
    if (rc != 0) {
        return rc;
    }

    nPid = fork();
    if (nPid < 0 ) {
        perror("fork() 1");
        rc = RC(rcExe, rcProcess, rcCreating, rcProcess, rcFailed);
        LOGERR(klogErr, rc, "after fork");
        return rc;
    }
    else if (nPid == 0) {
        /* dup pipe read/write to stdin/stdout */
        dup2(pipeto  [0], STDIN_FILENO);
        dup2(pipefrom[1], STDOUT_FILENO);
        dup2(pipefrom[1], STDERR_FILENO);
        close(pipeto[1]);
        ret = execvp(path, argv);
        STSMSG(STS_DBG, ("CHILD: Done %s %s %s = %d", path, src, dest, ret));
        exit(EXIT_FAILURE);
    }
    else {
        bool progressing = false;
        bool writeFailed = false;
        EAscpState state = eStart;
        const char y[] = "y\n";
        const char n[] = "n\n";
        int status = 0;
        int w = 0;
        int fd = pipefrom[0];
        const char *answer = n;
        String line;
        StringInit(&line, NULL, 0, 0);
        if (cache_key) {
            answer = y;
        }

        {
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        close(pipeto[0]);
        close(pipefrom[1]);
        assert(sizeof y == sizeof n);

        {
            int hang = 0;
            uint64_t prev = 0;
            KTime_t tPrev = 0;
            char sfSrc = 'B';
            uint64_t hSrc = humanize(srcSz, &sfSrc, NULL);
            int sig = 0;
            uint64_t i = 0;
            KDirectory *dir = NULL;
            rc_t rc = KDirectoryNativeDir(&dir);
            DISP_RC(rc, "KDirectoryNativeDir");
            if (rc != 0) {
                return rc;
            }
            while (w == 0) {
                bool quit = false;
                w = waitpid(nPid, &status, WNOHANG);
                if (w == 0) {
                    bool got = false;
                    rc_t rc = 0;
                    if (quitting) {
                        rc = quitting();
                    }
                    if (rc != 0 || quit) {
                        if (sig == 0) {
                            sig = SIGINT;
                        }
                        else if (sig >= SIGKILL) {
                            break;
                        }
                        else {
                            ++sig;
                        }
                        if (progressing) {
                            OUTMSG(("\n"));
                        }
                        PLOGMSG(klogInfo, (klogInfo, "^C pressed: "
                            "Senging $(sgn) to ascp", "sgn=%s", sig));
                        kill(nPid, sig);
                    }
                    while (true) {
                        char buf[4096];
                        int s = read(fd, buf, sizeof buf);
                        if (s == 0) {
                            break;
                        }
                        else if (s < 0) {
                            if (errno != EAGAIN) {
                                if (progressing) {
                                    OUTMSG(("\n"));
                                }
                                perror("read(child)");
                            }
                            break;
                        }
                        ascpParse(buf, s, dest, &state, &line);
                        switch (state) {
                            case eKeyEnd:
                                write(pipeto[1], answer, sizeof y - 1);
                                break;
                            case eWriteFailed:
                                writeFailed = true;
                                break;
                            default:
                                break;
                        }
                        got = true;
                    }
                    if (!got) {
                        KSleepMs(1000);
                        ++i;
                        if ((heartbeat > 0 && i >= heartbeat) || (i > 99)) {
                            uint64_t size = 0;
                            rc_t rc = KDirectoryFileSize
                                (dir, &size, "%s", dest);
                            if (rc != 0) {
                                size = 0;
                            }
                            else {
                                if (size != prev) {
                                   prev = size;
                                   tPrev = 0;
                                   hang = 0;
                                }
                                else {
                                    KTime_t date = 0;
                                    rc_t rc = KDirectoryDate
                                        (dir, &date, "%s", dest);
                                    if (rc == 0) {
                                        tPrev = date;
                                        if ((KTimeStamp() - date) > 60 * 99) {
                                            /* no file update during 99' */
                                            if (hang == 0) {
                                                write(pipeto[1],
                                                    answer, sizeof y - 1);
                                                ++hang;
                                            }
                                            else if (hang < 9) {
                                                ++hang;
                                                sig = 0;
                                            }
                                            else {
                                                if (sig == 0) {
                                                    sig = SIGINT;
                                                }
                                                else {
                                                    ++sig;
                                                }
                                                if (progressing) {
                                                   OUTMSG(("\n"));
                                                }
                                                if (sig > SIGKILL) {
                                                   rc = RC(rcExe,
                                                       rcProcess, rcExecuting,
                                                       rcProcess,rcDetached);
                                                   return rc;
                                                }

                                                PLOGMSG(klogInfo, (klogInfo,
                                                    "Senging $(sgn) to ascp",
                                                    "sgn=%s", sig));
                                                kill(nPid, sig);
                                            }
                                        }
                                    }
                                }
                            }
                            if (heartbeat > 0) {
                                if (callback) {
                                    quit = !callback(id,
                                        eAscpStateRunning, size, 0);
                                }
                                else {
                                    progress(acc, size, srcSz, hSrc, sfSrc,
                                        tPrev);
                                }
                                progressing = true;
                            }
                            i = 0;
                        }
                    }
                }
            }
            RELEASE(KDirectory, dir);
        }

        if (progressing) {
            OUTMSG(("\n"));
        }

        while (1) {
            char buf[4096];
            int s = read(fd, buf, sizeof buf);
            if (s == 0) {
                break;
            }
            else if (s < 0) {
                if (errno != EAGAIN) {
                    perror("read(child)");
                    break;
                }
                continue;
            }
            ascpParse(buf, s, dest, &state, &line);
            if (state == eWriteFailed) {
                writeFailed = true;
            }
        }
        STSMSG(    STS_DBG, ("ascp exited with pid=%d status=%d", w, status));
        if (WIFEXITED(status)) {
            STSMSG(STS_DBG, ("ascp exited with exit status %d",
                WEXITSTATUS(status)));
        }
        else {
            STSMSG(STS_DBG, ("ascp has not terminated correctly"));
        }
        if (w == -1) {
            perror("waitpid");
            rc = RC(rcExe, rcProcess, rcWaiting, rcProcess, rcFailed);
            LOGERR(klogErr, rc, "after waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
    	    if (WEXITSTATUS(status) == 0) {
	            STSMSG(STS_DBG, ("ascp succeed"));
                if (callback) {
                    callback(id, eAscpStateExitSuccess, 0, 0);
                }
            }
            else if (writeFailed) {
                rc = RC(rcExe, rcProcess, rcExecuting, rcMemory, rcExhausted);
                if (callback) {
                    callback(id, eAscpStateExitWriteFailure, 0, 0);
                }
            }
            else {
                if (rc == 0) {
                    switch ( state ) {
                        case eFailedAuthenticate:
                        case eFailedInitiation:
                            rc = RC(rcExe, rcProcess, rcWaiting, rcProcess,
                                                                  rcIncomplete);
                            break;
                        default:
                            rc = RC(rcExe, rcProcess, rcWaiting, rcProcess,
                                                                  rcFailed);
                            break;
                    }
                }
                PLOGERR(klogErr, (klogErr, rc,
                    "ascp failed with $(ret)", "ret=%d", WEXITSTATUS(status)));
                if (callback) {
                    callback(id, eAscpStateExitFailure, 0, 0);
                }
            }
        }
        else if (WIFSIGNALED(status)) {
            if (rc == 0) {
                if (quitting) {
                    rc = quitting();
                    if (rc == 0) {
                        rc = RC(rcExe,
                            rcProcess, rcWaiting, rcProcess, rcFailed);
                    }
                }
            }
            if (rc != SILENT_RC(rcExe, rcProcess, rcExecuting,
                rcProcess, rcCanceled))
            {
                PLOGERR(klogErr, (klogErr, rc, "ascp killed by signal $(sig)",
                    "sig=%d", WTERMSIG(status)));
                if (callback) {
                    callback(id, eAscpStateExitFailure, 0, 0);
                }
            }
        }

        close(pipefrom[0]);
        pipefrom[0] = 0;

        close(pipeto[1]);
        pipeto[1] = 0;
    }
    return rc;
}

int silent_system(const char *command) {
    int ret = 0;

    int oldOut = 0;
    int youngOut = 0;
    int oldErr = 0;
    int youngErr = 0;

    fflush(stdout);
    oldOut = dup(STDOUT_FILENO);
    youngOut = open("/dev/null", O_WRONLY);
    dup2(youngOut, STDOUT_FILENO);
    close(youngOut);

    fflush(stderr);
    oldErr = dup(STDERR_FILENO);
    youngErr = open("/dev/null", O_WRONLY);
    dup2(youngErr, STDERR_FILENO);
    close(youngErr);

    ret = system(command);

    fflush(stdout);
    dup2(oldOut, STDOUT_FILENO);
    close(oldOut);    

    fflush(stderr);
    dup2(oldErr, STDERR_FILENO);
    close(oldErr);    

    return ret;
}
