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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <kapp/main.h>
#include <kapp/log.h>
#include <klib/rc.h>
#include <kdb/database.h>
#include <kdb/table.h>
#include <kdb/index.h>

#include <sra/sradb.h>

uint32_t KAppVersion(void)
{
    return 1;
}

rc_t KMain ( int argc, char *argv [] )
{
    rc_t rc = 0;
    char const *table_dir = NULL;
    char const *idx_name = NULL;
    const KDBManager* kmgr = NULL;
    const KTable* ktbl = NULL;
    const KIndex* kidx = NULL;

    if( argc < 3 ) {
        rc = RC ( rcExe, rcArgv, rcParsing, rcPath, rcNull );
        PLOGERR((klogErr, rc, "Usage:\n $(a) <path> <index-name>", PLOG_S(a), argv[0]));
        return rc;
    }

    table_dir = argv[1];
    idx_name = argv[2];

    rc = KDBManagerMakeRead(&kmgr, NULL);
    if( rc == 0 ) {
        rc = KDBManagerOpenTableRead(kmgr, &ktbl, table_dir);
        if( rc == 0 ) {
            PLOGMSG((klogInfo, "Table $(p) index $(i)", PLOG_2(PLOG_S(p),PLOG_S(i)), table_dir, idx_name));
            rc = KTableOpenIndexRead(ktbl, &kidx, idx_name);
            if( rc == 0 ) {
                uint64_t off1 = 0, off2 = 0, sz = 0, id_q = 0;
                int64_t id = 0;
                while(true) {
                    rc = KIndexFindU64(kidx, off1, &off2, &sz, &id, &id_q);
                    if( rc != 0 ) {
                        break;
                    }
                    PLOGMSG((klogInfo, "$(i) index spot $(s) ($(q)) offset [$(f):$(t)]",
                            PLOG_5(PLOG_S(i),PLOG_I64(s),PLOG_U64(q),PLOG_U64(f),PLOG_U64(t)), idx_name, id, id_q, off2, off2 + sz - 1));
                    off1 = off2 + sz + 1;
                    {{
                        uint64_t f = off2, t = off2 + sz;
                        while( f != t ) {
                            uint64_t o, z;
                            int64_t newid;
                            if( (rc = KIndexFindU64(kidx, f, &o, &z, &newid, &id_q)) != 0 ) {
                                PLOGERR((klogErr, rc, "sub $(f)", PLOG_U64(f), f));
                                break;
                            }
                            if( id != newid ) {
                                PLOGMSG((klogErr, "no match on offset $(f): $(i) <-> $(n)",
                                    PLOG_3(PLOG_U64(f),PLOG_I64(i),PLOG_I64(n)), f, id, newid));
                            }
                            f++;
                        }
                    }}
                }
                KIndexRelease(kidx);
            }
            KTableRelease(ktbl);
        }
        KDBManagerRelease(kmgr);
    }
    LOGERR(rc == 0 ? klogInfo : klogErr, rc, "Done");
    return rc;
}
