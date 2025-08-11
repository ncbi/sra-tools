/******************************************************************************/
#include <kapp/main.h>

#include <sra/wsradb.h> /* spotid_t */

#include <vdb/manager.h> /* VDBManager */
#include <vdb/table.h> /* VDBTable */
#include <vdb/cursor.h> /* VCursor */
#include <kfs/directory.h> /* KDirectory */
#include <kfs/file.h> /* KFile */

#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/status.h> /* STSMSG */
#include <klib/debug.h> /* KDbgSetString */
#include <klib/rc.h> /* RC */

#include <ctype.h> /* isblank */
#include <string.h> /* memset */
#include <stdio.h> /* sscanf */
#include <assert.h>

#define DISP_RC(lvl,rc,msg) (void)((rc == 0) ? 0 : LOGERR(lvl, rc, msg))
#define DISP_RC_INT(rc,msg) DISP_RC(klogInt,rc,msg)

const char UsageDefaultName[] = "test-read-write-cursor";

MAIN_DECL( argc, argv )
{
    if ( VdbInitialize( argc, argv, 0 ) )
        return VDB_INIT_FAILED;

    const char table[] = "/home/klymenka/REDACT-IN";
    const char name[] = "READ_FILTER";

    rc_t rc = 0;

    bool locked = false;

    VDBManager* mgr;
    VTable *tbl;
    const VCursor *rCursor = NULL;

    int i;

    LogLevelSet("info");

    for (i = 1; i < argc; ++i) {
      const char * arg = argv [ i ];
      if ( arg [ 0 ] == '-' )
        if (!strcmp(argv[i], "-+")) {
            if (++i <= argc) {
#if _DEBUGGING
	        KDbgSetString(argv[i]);
#endif
	    }
        }
	switch ( arg [ 1 ] ) {
            case 'h':
	        HelpVersion ( UsageDefaultName, KAppVersion () );
                return 0;
            case 'V':
                HelpVersion ( UsageDefaultName, KAppVersion () );
                return 0;
            default:
                PLOGERR(klogErr, (klogErr, rc, "Invalid argument: $(A)",
		                  "A=%s", arg ));
                return 1;
	}
    }

  /*KDbgSetString("VDB");*/

    if (rc == 0) {
/* +01: ManagerMake */
        LOGMSG(klogInfo, "VDBManagerMakeUpdate");
        rc = VDBManagerMakeUpdate(&mgr, NULL);
        DISP_RC_INT(rc, "while calling VDBManagerMakeUpdate");
    }

    if (rc == 0) {
        rc = VDBManagerWritable(mgr, "%s", table);
        if (GetRCState(rc) == rcLocked) {
            LOGMSG(klogInfo, "VDBManagerUnlock");
            rc = VDBManagerUnlock(mgr, "%s", table);
            DISP_RC_INT(rc, "while calling VDBManagerUnlock");
            locked = true;
        }
    }

    if (rc == 0) {
/* +02: OpenTable */
        PLOGMSG(klogInfo, (klogInfo,
            "VDBManagerOpenTableUpdate(\"$(t)\")", "t=%s", table));
        rc = VDBManagerOpenTableUpdate
            (mgr, &tbl, NULL, "%s", table);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "while opening VTable '$(path)'", "path=%s", table));
        }
    }

    if (rc == 0) {
/* +03: CreateCursorRead */
        LOGMSG(klogInfo, "VDBManagerUnlock");
        rc = VTableCreateCursorRead(tbl, &rCursor);
        DISP_RC_INT(rc, "while creating read cursor");

#if 1
        if (rc == 0) {
            uint32_t idx;
            PLOGMSG(klogInfo, (klogInfo,
                "VCursorAddColumn(read cursor, \"$(n)\")", "n=%s", name));
            rc = VCursorAddColumn(rCursor, &idx, "%s", name);
            if (rc != 0) {
                PLOGERR(klogErr, (klogErr, rc,
                    "while adding $(name) to read cursor", "name=%s", name));
            }
        }
#endif
        if (rc == 0) {
            LOGMSG(klogInfo, "VCursorOpen(read cursor)");
            rc = VCursorOpen(rCursor);
            DISP_RC_INT(rc, "while opening read cursor");
        }
    }

    if (rc == 0) {
        VCursor *cursor;
        uint32_t idx;
/* +04: CreateCursorWrite */
        LOGMSG(klogInfo, "VTableCreateCursorWrite");
        rc = VTableCreateCursorWrite(tbl, &cursor, kcmInsert);
        DISP_RC_INT(rc, "while creating write cursor");
        if (rc == 0) {
            PLOGMSG(klogInfo, (klogInfo,
                "VCursorAddColumn(write cursor, \"$(n)\")", "n=%s", name));
            rc = VCursorAddColumn(cursor, &idx, "%s", name);
            if (rc != 0) {
                PLOGERR(klogErr, (klogErr, rc,
                    "while adding $(name) to write cursor", "name=%s", name));
            }
        }
        if (rc == 0) {
            LOGMSG(klogInfo, "VCursorOpen(write cursor)");
            rc = VCursorOpen(cursor);
            DISP_RC_INT(rc, "while opening write cursor");
        }
#if 1
        for (i = 0; i < 3 && rc == 0; ++i) {
            if (rc == 0) {
                PLOGMSG(klogInfo, (klogInfo,
                    "VCursorOpenRow(write cursor) $(i)", "i=%d", i));
                rc = VCursorOpenRow(cursor);
                DISP_RC_INT(rc, "while opening row to write");
            }
            if (rc == 0) {
                char buffer[1];
                char b;
                switch (i) {
                    case 0:
                        buffer[0] = SRA_READ_FILTER_CRITERIA;
                        buffer[0] = SRA_READ_FILTER_REJECT;
                        break;
                    case 1:
                        buffer[0] = SRA_READ_FILTER_REJECT;
                        buffer[0] = SRA_READ_FILTER_CRITERIA;
                        break;
                    case 2:
                        buffer[0] = SRA_READ_FILTER_REDACTED;
                        break;
                }
                buffer[0] = SRA_READ_FILTER_PASS;
                b = buffer[0];
                PLOGMSG(klogInfo, (klogInfo,
                    "VCursorWrite('$(v)') $(i)", "v=%s,i=%d",
                    b == SRA_READ_FILTER_REDACTED ? "SRA_READ_FILTER_REDACTED" :
                        "?",
                    i));
                rc = VCursorWrite(cursor, idx, 8, buffer, 0, 1);
                DISP_RC_INT(rc, "while writing");
            }
            if (rc == 0) {
                PLOGMSG(klogInfo, (klogInfo,
                    "VCursorCommitRow(write cursor) $(i)", "i=%d", i));
                rc = VCursorCommitRow(cursor);
                DISP_RC_INT(rc, "while committing row");
            }
            PLOGMSG(klogInfo, (klogInfo,
                "VCursorCloseRow(write cursor) $(i)", "i=%d", i));
            {
                rc_t rc2 = VCursorCloseRow(cursor);
                DISP_RC_INT(rc2, "while closing row");
                if (rc == 0)
                {   rc = rc2; }
            }
        }
#endif
        LOGMSG(klogInfo, "VCursorRelease(read cursor)");
/* -03: CreateCursorRead */
        VCursorRelease(rCursor);
        if (rc == 0) {
            LOGMSG(klogInfo, "VCursorCommit(write cursor)");
            rc = VCursorCommit(cursor);
            DISP_RC_INT(rc, "while committing cursor");
        }
        LOGMSG(klogInfo, "VCursorRelease(write cursor)");
/* -04: CreateCursorWrite */
        VCursorRelease(cursor);
    }

    LOGMSG(klogInfo, "VTableRelease");
/* -02: OpenTable */
    VTableRelease(tbl);
    LOGMSG(klogInfo, "VDBManagerLock");
    if (locked) {
        rc_t rc2 = VDBManagerLock(mgr, "%s", table);
        DISP_RC_INT(rc2, "while VDBManagerLock");
    }

/* -01: ManagerMake */
    LOGMSG(klogInfo, "VDBManagerRelease");
    VDBManagerRelease(mgr);

    if (rc == 0) {
        LOGMSG(klogInfo, "SUCCESS");
    }
    else { LOGMSG(klogInfo, "FAILURE"); }

    return VdbTerminate( rc );
}
