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
#include <kfs/impl.h> /* KSysDir */
#include <kfs/kfs-priv.h> /* KDirectoryPosixStringToSystemString */

#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/printf.h> /* string_printf */
#include <klib/rc.h>
#include <klib/status.h> /* STSMSG */

#include <Windows.h>

#include <assert.h>
#include <stdio.h> /* stderr */

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

#define STS_FIN 3

static void beat(uint64_t heartbeat, bool flush) {
    static int i = 0;
    static char c = '1';
    return;
    if (heartbeat == 0) {
        return;
    }
    if (flush) {
        fprintf(stderr, "\n");
/*      fflush(stderr); */
        return;
    }
    fprintf(stderr, "%c", c);
    if (++i >= 60) {
        fprintf(stderr,
            "\r                                                            \r");
        i = 0;
        ++c;
        switch (c) {
            case ':':
                c = 'A';
                break;
            case '[':
                c = 'a';
                break;
            case 127:
                c = '!';
                break;
        }
    }
}

static rc_t mkAscpCommand(char *buffer, size_t len,
    const char *path, const char *key,
    const char *src, const char *dest, const AscpOptions *opt)
{
    KDirectory *dir = NULL;
    const char *maxRate = NULL;
    const char *host = NULL;
    const char *user = NULL;
    size_t num_writ = 0;

    size_t pos = 0;

    char system[MAX_PATH] = "";

    rc_t rc = KDirectoryNativeDir(&dir);
    if (rc != 0) {
        return rc;
    }

    rc = KDirectoryPosixStringToSystemString(
        dir, system, sizeof system, "%s", dest);
    if (rc == 0) {
        if (opt != NULL) {
            if (opt->host != NULL && opt->host[0] != '\0') {
                host = opt->host;
            }
            if (opt->target_rate != NULL && opt->target_rate[0] != '\0') {
                maxRate = opt->target_rate;
            }
            if (opt->user != NULL && opt->user[0] != '\0') {
                user = opt->user;
            }
        }

        rc = string_printf(buffer, len, &num_writ,
            "\"%s\" -i \"%s\" -pQTk1%s%s%s%s%s%s %s %s",
            path, key,
            maxRate == NULL ? "" : " -l", maxRate == NULL ? "" : maxRate,
            host == NULL ? "" : " --host ", host == NULL ? "" : host,
            user == NULL ? "" : " --user ", user == NULL ? "" : host,
            src, system);
        if (rc != 0) {
            LOGERR(klogInt, rc, "while creating ascp command line");
        }
        else {
            assert(num_writ < len);
        }
    }

    RELEASE(KDirectory, dir);

    return rc;
}

static int execute(const char *command, uint64_t heartbeat,
    bool cacheKey, bool *writeFailed, bool *canceled,
    const char *name, TQuitting *quitting)
{
    DWORD exitCode = STILL_ACTIVE;
    HANDLE g_hChildStd_IN_Rd = NULL;
    HANDLE g_hChildStd_IN_Wr = NULL;
    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;
    SECURITY_ATTRIBUTES saAttr; 
    BOOL bSuccess = FALSE;
    PROCESS_INFORMATION pi;
    STARTUPINFO si;

    wchar_t wcommand[5000];

    assert(writeFailed && canceled);
    *writeFailed = false;

    STSMSG(STS_INFO, ("Starting %s ", command));

    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    // Create a pipe for the child process's STDOUT. 
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr,
        0))
    {
        rc_t rc = RC(rcExe, rcFileDesc, rcCreating, rcFileDesc, rcFailed);
        LOGERR(klogErr, rc, "Stdout CreatePipe");
        return 1;
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT,
        0))
    {
        rc_t rc = RC(rcExe, rcFileDesc, rcCreating, rcFileDesc, rcFailed);
        LOGERR(klogErr, rc, "Stdout SetHandleInformation");
        return 1;
    }

    // Create a pipe for the child process's STDIN. 
    if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr,
        0))
    {
        puts("Stdin CreatePipe");
        return 1;
    }

    // Ensure the write handle to the pipe for STDIN is not inherited.  
    if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT,
        0))
    {
        puts("Stdin SetHandleInformation");
        return 1;
    }

    // Set up members of the PROCESS_INFORMATION structure. 
    ZeroMemory(&pi, sizeof(pi)); 

    // Set up members of the STARTUPINFO structure. 
    // This structure specifies the STDIN and STDOUT handles for redirection.
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = g_hChildStd_OUT_Wr;
    si.hStdOutput = g_hChildStd_OUT_Wr;
    si.hStdInput = g_hChildStd_IN_Rd;
    si.dwFlags |= STARTF_USESTDHANDLES;

    mbstowcs(wcommand, command, strlen(command) + 1);

    bSuccess = CreateProcess(
        NULL,//_In_opt_ LPCTSTR              lpApplicationName,
        wcommand,//_Inout_opt_  LPTSTR lpCommandLine, command line 
        NULL,//_In_opt_ LPSECURITY_ATTRIBUTES process security attributes 
        NULL,//_In_opt_ LPSECURITY_ATTRIBUTES primary thread security attributes
        TRUE,//_In_     BOOL                  handles are inherited 
        0,   //_In_     DWORD                 creation flags 
        NULL,//_In_opt_ LPVOID                use parent's environment 
        NULL,//_In_opt_ LPCTSTR               use parent's current directory 
        &si, //_In_     LPSTARTUPINFO         STARTUPINFO pointer 
        &pi  //_Out_    LPPROCESS_INFORMATION receives PROCESS_INFORMATION 
    );
    if (bSuccess) {
        bool progressing = false;
        EAscpState state = eStart;
        const char y[] = "y\r\n";
        const char n[] = "n\r\n";
        const char *answer = cacheKey ? y : n;
        bool first = true;
        CHAR chBuf[4096];
        DWORD dwRead = 0;
        DWORD dwWritten = 0;
        DWORD dwFileSize = 0;
        HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMilliseconds = 1000;
        DWORD dwMillisecondsRemains = 1000;
        String line;
        StringInit(&line, NULL, 0, 0);
        if (heartbeat > 0) {
            dwMillisecondsRemains = (DWORD)heartbeat;
        }
        // Write to the pipe that is the standard input for a child process. 
        // Data is written to the pipe's buffers, so it is not necessary to wait
        // until the child process is running before writing data.
        assert(sizeof y == sizeof n);
        bSuccess = WriteFile(g_hChildStd_IN_Wr, answer, sizeof y - 1,
            &dwWritten, NULL);
        if (!bSuccess) {
            puts("failed to write to child's Stdin");
            return 1;
        }
        if (!CloseHandle(g_hChildStd_IN_Wr)) {
            puts("StdInWr CloseHandle");
            return 1;
        }
        CloseHandle(pi.hThread);

        // Successfully created the process.  Wait for it to finish.
        while (true) {
            char noprogress[] = "\r                                "
                "                                               \r";
            if (exitCode != STILL_ACTIVE) {
/*                  printf(">>GetExitCodeProcess = %d\n", exitCode); */
            }
            bSuccess = GetExitCodeProcess(pi.hProcess, &exitCode);
            if (exitCode != STILL_ACTIVE) {
/*                  printf(">>GetExitCodeProcess = %d\n", exitCode); */
            }
            if (!bSuccess) {
                rc_t rc = RC(rcExe,
                    rcProcess, rcExecuting, rcProcess, rcFailed);
                if (progressing) {
                    WriteFile(hParentStdOut, noprogress, sizeof noprogress - 1,
                        &dwWritten, NULL);
                }
                LOGERR(klogErr, rc,
                    "Executed command, couldn't get exit code");
                return 1;
            }
            if (exitCode == STILL_ACTIVE) {
                if (first) {
                    first = false;
                }
                else {
                    if (STS_DBG > KStsLevelGet()) {
                        beat(heartbeat, false);
                    }
                }
                WaitForSingleObject(pi.hProcess, dwMilliseconds);
                dwFileSize = GetFileSize(g_hChildStd_OUT_Rd,  NULL);
                if (dwFileSize > 0) {
                    STSMSG(STS_FIN + 1,
                        ("GetFileSize(ChildStdOut) = %d", dwFileSize));
                }
                while (dwFileSize > 0) {
                    bSuccess = ReadFile(g_hChildStd_OUT_Rd,
                        chBuf, sizeof chBuf, &dwRead, NULL);
                    if (!bSuccess) {
                        if (progressing) {
                            WriteFile(hParentStdOut, noprogress,
                                sizeof noprogress - 1, &dwWritten, NULL);
                        }
                        break;
                    }
                    ascpParse(chBuf, dwRead, name, &state, &line);
                    if (state == eWriteFailed) {
                        *writeFailed = true;
                    }
                    else if (state == eProgress) {
                        if (heartbeat > 0) {
                            if (dwMillisecondsRemains > dwMilliseconds) {
                                dwMillisecondsRemains -= dwMilliseconds;
                            }
                            else {
                                const char *p = chBuf;
                                size_t l = dwRead;
                                if (line.addr != NULL && line.len != 0) {
                                    p = line.addr;
                                    l = line.len;
                                    WriteFile(hParentStdOut, "\r", 1,
                                        &dwWritten, NULL);
                                    bSuccess = WriteFile(hParentStdOut, p,
                                        (DWORD)l, &dwWritten, NULL);
                                    dwMillisecondsRemains = (DWORD)heartbeat;
                                    progressing = true;
                                }
                            }
                        }
                    }
/*                      OUTMSG(("%.*s", dwRead, chBuf)); */
                    if (STS_DBG <= KStsLevelGet()) {
                        if (KStsMsg("%.*s", dwRead, chBuf) != 0) {
                            bSuccess = WriteFile(hParentStdOut, chBuf,
                                dwRead, &dwWritten, NULL);
                        }
                    }
                    dwFileSize = GetFileSize(g_hChildStd_OUT_Rd,  NULL);
                    if (dwFileSize > 0) {
                        STSMSG(STS_FIN + 1,
                            ("GetFileSize(ChildStdOut) = %d", dwFileSize));
                    }
                }
            }
            else {
                if (STS_DBG > KStsLevelGet()) {
//                  beat(heartbeat, true);
                }
                if (progressing) {
                    WriteFile(hParentStdOut, noprogress,
                        sizeof noprogress - 1, &dwWritten, NULL);
                }
                break;
            }
            if (quitting != NULL) {
                rc_t rc = quitting();
                if (rc != 0) {
                    if (progressing) {
                        WriteFile(hParentStdOut, noprogress,
                            sizeof noprogress - 1, &dwWritten, NULL);
                    }
                    *canceled = true;
                    break;
                }
            }
        }
        if (!*canceled) {
            WaitForSingleObject(pi.hProcess, INFINITE);

        // Get the exit code.
/*      printf(">GetExitCodeProcess = %d\n", exitCode); */
            bSuccess = GetExitCodeProcess(pi.hProcess, &exitCode);
            if (!bSuccess) {
                rc_t rc = RC(rcExe,
                    rcProcess, rcExecuting, rcProcess, rcFailed);
                LOGERR(klogErr,
                    rc, "Executed command but couldn't get exit code");
                return 1;
            }
        }
/*      printf("<GetExitCodeProcess = %d\n", exitCode); */

        CloseHandle(pi.hProcess);

        if (exitCode != 0 && STS_DBG > KStsLevelGet()) {
            if (KStsMsg("%.*s", dwRead, chBuf) != 0) {
                bSuccess = WriteFile(hParentStdOut, chBuf,
                    dwRead, &dwWritten, NULL);
            }
        }
        if (dwFileSize > 0) {
            STSMSG(STS_FIN, ("GetFileSize(ChildStdOut) = %d", dwFileSize));
        }
        while (dwFileSize > 0) { 
            bSuccess = ReadFile(g_hChildStd_OUT_Rd,
                chBuf, sizeof chBuf, &dwRead, NULL);
            if (!bSuccess || dwRead == 0) {
                break; 
            }
            ascpParse(chBuf, dwRead, name, &state, &line);
            if (state == eWriteFailed) {
                *writeFailed = true;
            }
            dwFileSize = GetFileSize(g_hChildStd_OUT_Rd,  NULL);
            if (dwFileSize > 0) {
                STSMSG(STS_FIN, ("GetFileSize(ChildStdOut) = %d", dwFileSize));
            }
        }

        return exitCode;
    }
    else {
        return GetLastError();
    }
}

rc_t run_ascp(const char *path, const char *key,
    const char *src, const char *dest, const AscpOptions *opt)
{
    rc_t rc = 0;
    uint64_t heartbeat = 0;
    char buffer[MAX_PATH * 3] = "";
    bool writeFailed = false;
    TQuitting *quitting = NULL;

    if (opt != NULL) {
        heartbeat = opt->heartbeat;
        quitting = opt->quitting;
    }

    rc = mkAscpCommand(buffer, sizeof buffer, path, key, src, dest, opt);
    if (rc == 0) {
        bool cacheKey = false;
        if (opt != NULL && opt->cache_key) {
            cacheKey = true;
        }
        {
            int value = 0;
            bool canceled = false;
            const char *name = strrchr(dest, '/');
            if (name == NULL) {
                name = dest;
            }
            else {
                if (*(name + 1) != '\0') {
                    ++name;
                }
                else {
                    name = dest;
                }
            }
            value = execute(buffer, heartbeat, cacheKey,
                &writeFailed, &canceled, name, quitting);
            if (value != 0) {
                if (writeFailed) {
                    rc = RC(rcExe, rcProcess, rcExecuting,
                        rcMemory, rcExhausted);
                }
                else {
                    rc = RC(rcExe, rcProcess, rcExecuting, rcProcess, rcFailed);
                }
            }
            if (value == 0 && rc == 0 && canceled && quitting != NULL) {
                rc = quitting();
            }
        }
    }

    return rc;
}

int silent_system(const char *command) {
    HANDLE g_hChildStd_OUT_Rd = NULL;
    HANDLE g_hChildStd_OUT_Wr = NULL;
    BOOL bSuccess = FALSE;
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    SECURITY_ATTRIBUTES saAttr; 

    wchar_t wcommand[5000];

    // Set the bInheritHandle flag so pipe handles are inherited. 
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 

    // Create a pipe for the child process's STDOUT. 
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0)) {
        rc_t rc = RC(rcExe, rcFileDesc, rcCreating, rcFileDesc, rcFailed);
        LOGERR(klogErr, rc, "Stdout CreatePipe");
        return 1;
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        rc_t rc = RC(rcExe, rcFileDesc, rcCreating, rcFileDesc, rcFailed);
        LOGERR(klogErr, rc, "Stdout SetHandleInformation");
        return 1;
    }

    ZeroMemory(&pi, sizeof(pi)); 

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = g_hChildStd_OUT_Wr;
    si.hStdOutput = g_hChildStd_OUT_Wr;
    si.dwFlags |= STARTF_USESTDHANDLES;

    mbstowcs(wcommand, command, strlen(command) + 1);

    bSuccess = CreateProcess(
        NULL,           //_In_opt_    LPCTSTR               lpApplicationName,
        wcommand,       //_Inout_opt_ LPTSTR                lpCommandLine,
        NULL,           //_In_opt_    LPSECURITY_ATTRIBUTES lpProcessAttributes,
        NULL,           //_In_opt_    LPSECURITY_ATTRIBUTES lpThreadAttributes,
        TRUE,           //_In_        BOOL                  bInheritHandles,
        0,              //_In_        DWORD                 dwCreationFlags,
        NULL,           //_In_opt_    LPVOID                lpEnvironment,
        NULL,           //_In_opt_    LPCTSTR               lpCurrentDirectory,
        &si,            //_In_        LPSTARTUPINFO         lpStartupInfo,
        &pi             //_Out_       LPPROCESS_INFORMATION lpProcessInformation
    );
    if (bSuccess) {
        return 0;
    }
    else {
        return GetLastError();
    }
}
