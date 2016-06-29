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
 
/* KeyRing Server process
 *  - install an IPC socket listener (name = "vdb-keyring")
 *  - listen for incoming IPC connections
 *  - handle IPC requests
 *      Request format: 
 *          <uint16_t length><body> 
 *      Request body:
 *          - "I" - init
 *          - "X" - shutDown
 *          - "PA<uint8 length>name<uint8 length>dl_ticket<uint8 length>enc_key" - add project
 *      Response format:
 *          <uint8_t length><body>
 *      Response body:
 *          - "IY" init successful
 *          - "IN" init failed (bad password?)
 */

#include "keyring-srv.h"

#include <klib/text.h>
#include <klib/log.h>
#include <klib/printf.h>

#include <kproc/thread.h>

#include <kns/manager.h>
#include <kns/socket.h>
#include <kns/endpoint.h>
#include <kns/stream.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/lockfile.h>

#include <vfs/keyring-priv.h>

#include <kapp/args.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

static rc_t WorkerThreadFn ( const KThread *self, void *data );

static bool shutDown = false;

/*TODO: add pid to log messages? */
static const char* initMsgSuccess = "\x02IY";
static const char* initMsgFailure = "\x02IN";

char keyRingFileName[MAX_PATH];
KKeyRing* keyring = NULL;

static 
rc_t 
Server( KNSManager* mgr )  
{   
    rc_t rc = 0;
    KEndPoint ep;
    String name; 

    CONST_STRING(&name, KEYRING_IPC_NAME);
    rc = KNSManagerInitIPCEndpoint(mgr, &ep, &name);
    if (rc == 0)
    {
        KSocket* listener;
        rc = KNSMakeListener ( &listener, &ep );
        if (rc == 0)
        {
            shutDown = false;
            while (!shutDown && rc == 0)
            {
                KStream* stream;
                LogMsg ( klogInfo, "KeyringServer: listening");
                rc = KNSListen ( listener, &stream ); /* may not return from here if no more incoming connections arrive */
                if (rc == 0)
                {
                    KThread* worker;
                    LogMsg ( klogInfo, "KeyringServer: detected connection");
                    rc = KThreadMake ( &worker, WorkerThreadFn, stream);
                    if (rc == 0 && worker != NULL)
                    {
                        KThreadWait(worker, NULL);
                        LogMsg ( klogInfo, "KeyringServer: out of worker");
                    }
                    else
                        LogErr(klogErr, rc, "KeyringServer: KThreadMake failed");
                }
                else
                    LogErr(klogErr, rc, "KeyringServer: KNSListen failed");
            }
            LogMsg ( klogInfo, "KeyringServer: shutting down");
            
            /* TODO: make sure no incoming messages get dropped (stop accepting connections? wait for all active threads to exit?) 
                - lock the server */
                
            if (keyring != NULL)
            {
                KeyRingRelease(keyring);
                keyring = NULL;
            }
                
            KSocketRelease(listener);
        }
        else
            LogErr(klogErr, rc, "KeyringServer: KNSMakeListener failed");
    }
    else
        LogErr(klogErr, rc, "KeyringServer: KNSManagerInitIPCEndpoint failed");
    LogMsg ( klogInfo, "KeyringServer: listener shut down");
    return rc;
}
    
static 
rc_t 
WorkerThreadFn ( const KThread *self, void *data )  
{
    KStream* stream = (KStream*)data;
    char buf[256];
    size_t num;
    rc_t rc = KStreamReadAll(stream, buf, 1, &num);
    if (rc == 0)
    {
        if (num == 1) 
        {
            size_t toRead = (unsigned char)buf[0];
            pLogMsg(klogInfo, "KeyringServer: worker received length=$(l)\n", "l=%d", toRead);
            rc = KStreamReadAll(stream, buf, toRead, &num);
            if (rc == 0)
            {
                /*pLogMsg(klogInfo, "KeyringServer: worker received msg='$(buf)'\n", "buf=%.*s", num, buf);*/
                switch (buf[0])
                {
                case 'I':
                    if (buf[0] == 'I') /* init */
                    {
                        LogMsg ( klogInfo, "KeyringServer: received Init");

                        if (keyring == 0)
                        {
                            const KFile* std_in;
                            
                            rc = KFileMakeStdIn ( &std_in );
                            if (rc == 0)
                            {
                                KFile* std_out;
                                rc = KFileMakeStdOut ( &std_out );
                                if (rc == 0)
                                {
                                    rc = KeyRingOpen(&keyring, keyRingFileName, std_in, std_out);
                                    if (rc == 0)
                                    {
                                        LogMsg ( klogInfo, "KeyringServer: Init successful");
                                        pLogMsg(klogInfo, "KeyringServer: sending '$(buf)'\n", "buf=%.*s", string_size(initMsgSuccess), initMsgSuccess);
                                        rc = KStreamWrite(stream, initMsgSuccess, string_size(initMsgSuccess), NULL);
                                    }
                                    else
                                    {
                                        rc_t rc2;
                                        LogErr(klogErr, rc, "KeyringServer: Init failed");
                                        rc2 = KStreamWrite(stream, initMsgFailure, string_size(initMsgFailure), NULL);
                                        if (rc == 0)
                                            rc = rc2;
                                    }
                                    KFileRelease(std_out);
                                }
                                KFileRelease(std_in);
                            }
                        }
                        else
                        {   /* already running */
                            LogMsg ( klogInfo, "KeyringServer: Init successful");
                            rc = KStreamWrite(stream, initMsgSuccess, string_size(initMsgSuccess), NULL);
                        }
                    }
                    break;
                case 'X':
                    if (buf[0] == 'X') /* shutDown */
                    {
                        LogMsg ( klogInfo, "KeyringServer: received Shutdown");
                        shutDown = true;
                    }
                    break;
                case 'P': /* project */
                    if (toRead > 1 && buf[1] == 'A') /* Add */
                    {
                        String pkey;
                        String dlkey;
                        String enckey;

                        size_t idx = 2;
                        /*TODO: make sure idx is in range*/
                        StringInit(&pkey, buf + idx + 1, buf[idx], buf[idx]);
                        pkey.len = string_len(pkey.addr, pkey.size);

                        idx += pkey.size + 1;
                        /*TODO: make sure idx is in range*/
                        StringInit(&dlkey, buf + idx + 1, buf[idx], buf[idx]);
                        dlkey.len = string_len(dlkey.addr, dlkey.size);
                        
                        idx += dlkey.size + 1;
                        /*TODO: make sure idx is in range*/
                        StringInit(&enckey, buf + idx + 1, buf[idx], buf[idx]);
                        enckey.len = string_len(enckey.addr, enckey.size);
                        
                        pLogMsg(klogInfo,  
                                "KeyringServer: received Project Add(pkey='$(pkey)',dlkey='$(dlkey)',enckey='$(.....)')'\n",
                                "pkey=%.*s,dlkey=%.*s,enckey=%.*s", 
                                pkey.len, pkey.addr, dlkey.len, dlkey.addr, enckey.len, enckey.addr);
                        
                        rc = KeyRingAddProject(keyring, &pkey, &dlkey, &enckey);
                        if (rc != 0)
                            LogErr(klogErr, rc, "KeyringServer: KeyRingAddProject() failed");
                    }
                    break;
                default:
                    LogMsg ( klogInfo, "KeyringServer: unrecognised message received");
                    break;
                }
            }
            else
                LogErr(klogErr, rc, "KeyringServer: KStreamRead(body) failed");
        }
        else /* end of stream = the client closed the connection */
            LogMsg(klogInfo, "KeyringServer: worker received EOF\n");
    }
    else
        LogErr(klogErr, rc, "KeyringServer: KStreamRead(length) failed");
    LogMsg ( klogInfo, "KeyringServer: worker done");
    return KThreadRelease(self);
}


rc_t UsageSummary (char const * progname)
{
    return 0;
}

char const UsageDefaultName[] = "keyring-srv";

rc_t CC Usage (const Args * args)
{
    return 0;
}

/*TODO: handle signals */
/*TODO: handle stale lock files */

rc_t CC KMain (int argc, char * argv[])
{
    rc_t rc = 0;
    KDirectory* wd;
    
    KLogLevelSet(klogInfo);
    LogMsg ( klogInfo, "KeyringServer: starting");

    rc = KDirectoryNativeDir (&wd);
    if (rc == 0)
    {
        KFile* lockedFile;
        const char* dataDir;
        
        char lockFileName[MAX_PATH];
        if (argc < 2 || argv[1] == NULL)
            dataDir = KeyRingDefaultDataDir;
        else
            dataDir = argv[1];
        rc = string_printf(lockFileName, sizeof(lockFileName)-1, NULL, "%s/keyring_lock", dataDir);
        if (rc == 0)
        {
            rc = KDirectoryCreateExclusiveAccessFile(wd, &lockedFile, true, 0600, kcmOpen, "%s", lockFileName);
            if (rc == 0)
            {
                KNSManager* mgr;
                rc = KNSManagerMake(&mgr);
                if (rc == 0)
                {
                    rc = string_printf(keyRingFileName, sizeof(keyRingFileName)-1, NULL, "%s/keyring", dataDir);
                    if (rc == 0)
                        rc = Server(mgr);
                    KNSManagerRelease(mgr);
                }
                else
                    LogErr(klogErr, rc, "KeyringServer: KNSManagerMake failed");
                KFileRelease(lockedFile); 
                LogMsg ( klogInfo, "KeyringServer: removing lock file.");
                KDirectoryRemove(wd, true, "%s", lockFileName);
            }
            else
            {   /*TODO: check for stale lock file*/
                LogMsg ( klogInfo, "KeyringServer: another instance appears to be running.");
                rc = 0;
            }
        }
        else
            LogErr ( klogErr, rc, "KeyringServer: failed to build the lock file name" );
        
        KDirectoryRelease(wd);
    }
    else
        LogErr(klogErr, rc, "KeyringServer: KDirectoryNativeDir failed");
    
    LogMsg ( klogInfo, "KeyringServer: finishing");
    
    return rc;
}

