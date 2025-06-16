#include <kapp/main.h>
#include <kapp/args.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/rc.h>
#include <string.h> /* memset */
#include <stdio.h>  /* printf */
#include <stdlib.h> /* malloc */
#include <assert.h>
#include <os-native.h>

#define OPTION_TABLE "table-path"
#define OPTION_ROW   "row-count"
#define ALIAS_TABLE  "t"
#define ALIAS_ROW    "r"

static char buff [81];
static const char * table_usage[] = { "Table path.  Defaults to", buff, NULL };
static const char * row_usage[]   = { buff, NULL };
OptDef MyOptions[] =
{
    { OPTION_TABLE, ALIAS_TABLE, NULL, table_usage, 1, true, false },
    { OPTION_ROW,   ALIAS_ROW,   NULL, row_usage,   1, true, false }
};

#define COLUMNS 5
#define ROWLEN 64
#define ROWS 0x400000


char tablePath[512];


const char UsageDefaultName[] = "rowwritetest";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [-t|--table-path <path>] [-r|--row-count <rows>]\n"
                    "\n", progname);
}


rc_t CC Usage (const Args * args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);

    UsageSummary (progname);

    OUTMSG (("Options:\n"));

    rc = string_printf (buff, sizeof buff, NULL, "%s", tablePath);

    HelpOptionLine (ALIAS_TABLE, OPTION_TABLE, "path", table_usage);

    rc = string_printf (buff, sizeof buff, NULL, "Number of Rows.  Defaults to %u", ROWS);

    HelpOptionLine (ALIAS_ROW, OPTION_ROW, "row", row_usage);

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}

#define BUFFERS 3

rc_t run (const char * table_path, uint64_t N )
{
    static const char *colInf[] = {
        "C1: Same value, same length",
        "C2: Var. value, same length",
        "C3: Var. value, var. legnth",
        "C4: Same value except I row",
        "C5: Same value except L row" };
    rc_t rc;
    uint64_t row = 0;
    VDBManager *mgr = NULL;
    VSchema *schema = NULL;
    VTable *table = NULL;
    VCursor *cursor;
    uint32_t idx[COLUMNS];
    uint64_t total[COLUMNS];
    int i = 0, j = 0, prev = 0;
    char *buffer[BUFFERS];




    /* Initialize arrays */
    memset(&idx, 0, sizeof idx);
    memset(&total, 0, sizeof total);
    for (i = 0; i < BUFFERS; ++i) {
        char c;
        size_t sz = ROWLEN + 1;
        if (i == (BUFFERS - 1))
            sz += ROWLEN;
        buffer[i] = malloc(sz);
        for (j = 0, c = 0; j < sz - 1; ++j, ++c) {
            if (c >= ROWLEN)
                c -= ROWLEN;
            buffer[i][j] = '0' + c;
        }
        buffer[i][j] = '\0';
    }
    /* Create manager */
    rc = VDBManagerMakeUpdate(&mgr, NULL);
    if (rc != 0) {
        LOGERR(klogInt, rc, "failed to open vdb library");
    }
    /* Initialize schema */
    if (rc == 0) {
        rc = VDBManagerMakeSchema(mgr, &schema);
        if (rc != 0)
            LOGERR(klogInt, rc, "failed to create empty schema");
    }
    if (rc == 0) {
        char text[512];
        char *end = text;
        int n = snprintf(end, &text[sizeof(text)] - end, "table Table #1 {\n");
        assert(end + n < &text[sizeof(text)]);
        end += n;

        for (i = 1; i <=  COLUMNS; ++i) {
            n = snprintf(end, &text[sizeof(text)] - end,
                        "  column ascii C%d = .C%d; physical ascii .C%d = C%d;\n",
                        i, i, i, i);
            assert(end + n < &text[sizeof(text)]);
            end += n;
        }
        n = snprintf(end, &text[sizeof(text)] - end, "};");
        assert(end + n < &text[sizeof(text)]);
        end += n;

        STSMSG(1,("Parsing schema:\n%s", text));
        rc = VSchemaParseText(schema, "Schema", text, &text[sizeof(text)] - end);
        if (rc != 0)
            LOGERR(klogInt, rc, "failed to parse schema");
    }
    /* Create table */
    if (rc == 0) {
        STSMSG(1,("Creating %s", tablePath));
        rc = VDBManagerCreateTable(mgr, &table, schema, "Table", kcmInit,
            tablePath);
        if (rc != 0)
            LOGERR(klogInt, rc, "failed to create table");
    }
    /* Initialize cursor */
    if (rc == 0) {
        rc = VTableCreateCursorWrite(table, &cursor, kcmInsert);
        if (rc != 0)
            LOGERR(klogInt, rc, "failed to create cursor");
    }
    for (i = 0; rc == 0 && i < COLUMNS; ++i) {
        char col[16];
        int n = snprintf(col, sizeof(col), "C%d", i + 1);
        assert(n < sizeof(col));
        STSMSG(2,("Adding column %s to cursor", col));
        rc = VCursorAddColumn(cursor, &idx[i], "%s", col);
        if (rc != 0)
            PLOGERR(klogInt, (klogInt, rc,
                              "failed to add $(c) to cursor", "c=%s", col));
    }
    if (rc == 0) {
        rc = VCursorOpen(cursor);
        if (rc != 0)
            LOGERR(klogInt, rc, "failed to open cursor");
    }
    /* Write data */
    for (row = 0; row < N && rc == 0; ++row) {
        int max = 2 * ROWLEN - 1;
        int sz = 0;
        if ((row % 2) == 0) {
            int min = 1;
            sz = min + (int) (max * (rand() / (RAND_MAX + 1.0)));
            prev = sz;
            buffer[1][0] = '1';
        }
        else {
            sz = max + 1 - prev;
            buffer[1][0] = '2';
        }
        rc = Quitting();
        if (rc == 0) {
            KStsLevel lvl = 2;
            if (row > 0 && ((row % ROWS) == 0)) {
                lvl = 1;
            }
            STSMSG (lvl, ("Writing row %ji / %ji",
                          row + 1, N));
            rc = VCursorOpenRow(cursor);
            if (rc != 0)
                LOGERR(klogInt, rc, "failed to open row");
        }
        for (j = 0; j < COLUMNS && rc == 0; ++j) {
            uint32_t count = 0;
            int buf = j;
            switch (j) {
                case 0:
                case 1:
                    count = strlen(buffer[j]);
                    break;
                case 2:
                    count = sz;
                    break;
                case 3:
                    buf = 0;
                    if (row == 0)
                        buf = 1;
                    count = strlen(buffer[buf]);
                    break;
                case 4:
                    buf = 0;
                    if (row == (N - 1))
                        buf = 1;
                    count = strlen(buffer[buf]);
                    break;
                default:
                    assert(0);
                    break;
            }
            STSMSG (3, ("Row %ji/Col.%d: %sd %.*s\n",
                        row + 1, j + 1, count, count, buffer[buf]));
            rc = VCursorWrite
                (cursor, idx[j], 8, buffer[buf], 0, count);
            if (rc != 0)
                PLOGERR(klogInt, (klogInt, rc, "failed to write row[$i]", "i=%d", j + 1));
            total[j] += count;
        }
        if (rc == 0) {
            rc = VCursorCommitRow(cursor);
            if (rc != 0)
                LOGERR(klogInt, rc, "failed to commit row");
        }
        if (rc == 0) {
            rc = VCursorCloseRow(cursor);
            if (rc != 0)
                LOGERR(klogInt, rc, "failed to close row");
        }
    }
    if (rc == 0) {
        STSMSG (1, ("Commiting cursor\n"));
        rc = VCursorCommit(cursor);
        if (rc != 0)
            LOGERR(klogInt, rc, "failed to commit cursor");
    }
    /* Cleanup */
    VCursorRelease(cursor);
    VTableRelease(table);
    VSchemaRelease(schema);
    VDBManagerRelease(mgr);
    for (i = 0; i < BUFFERS; ++i) {
        free(buffer[i]);
    }
    /* Log */
    if (rc == 0) {
        PLOGMSG(klogInfo, (klogInfo, "$(t)", "t=%s", tablePath));
        PLOGMSG(klogInfo,(klogInfo,
            "$(n)($(N)) rows written - $(b) bytes per row",
                          PLOG_I64(n) "," PLOG_X64(N) ",b=%d", N, N, ROWLEN));
        for (i = 0; i < COLUMNS; ++i) {
            PLOGMSG(klogInfo,(klogInfo,
                              "$(i): $(n)($(N)) bytes",
                              "i=%s," PLOG_I64(n) "," PLOG_X64(N),
                              colInf[i], total[i], total[i]));
        }
    }
    if (rc == 0) {
        KDirectory *dir = NULL;
        uint64_t sizes[COLUMNS];
        memset(&sizes, 0, sizeof sizes);
        rc = KDirectoryNativeDir(&dir);
        if (rc != 0)
            LOGERR(klogInt, rc, "failed to KDirectoryNativeDir");
        else {
            for (i = 1; i <= COLUMNS; ++i) {
                uint64_t size = 0;
#define  FORMAT    "%s/col/%s/data"
#define KFORMAT "$(d)/col/$(n)/data", "d=%s,n=%s"
#undef STATUS
#define  STATUS(action) (action FORMAT, tablePath, name)
                char name[16];
                int n = snprintf(name, sizeof(name), "C%d", i);
                assert(n < sizeof(name));
                STSMSG (1, STATUS("checking "));
                rc = KDirectoryFileSize(dir, &size, FORMAT, tablePath, "%s", name);
                if (rc != 0) {
                    if (GetRCState(rc) == rcNotFound) {
                        STSMSG (2, STATUS("not found "));
                        rc = 0;
                    }
                    else
                        PLOGERR(klogInt, (klogInt, rc,
                                          "failed to check " KFORMAT, tablePath, name));
                }
                else {
                    STSMSG (2, STATUS("found "));
                }
                PLOGMSG(klogInfo, (klogInfo, "Size of $(d)/col/$(n)/data = $(s)",
                                   "d=%s,n=%s," PLOG_I64(s), tablePath, name, size));
                sizes[i - 1] = size;
            }
        }
        KDirectoryRelease(dir);
        if (rc == 0) {
            puts("");
            KOutMsg("%ld rows, %d bytes per row:\n", N, ROWLEN);
            for (i = 0; i < COLUMNS; ++i) {
                puts(colInf[i]);
            }
            puts("");
            for (i = 0; i < COLUMNS; ++i) {
                int64_t over = sizes[i] - total[i];
                KOutMsg("C%d: %9ld bytes written; "
                    "%9ld in 'data'", i + 1, total[i], sizes[i]);
                if (over > 0) {
                    double p = 100.0 * over / sizes[i];
                    printf(": %7ld extra bytes (%.4f%%)\n", over, p);
                }
                else {
                    puts("");
                }
            }
        }
    }

    return rc;
}


MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    rc_t rc = 0;
    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc = ArgsMakeStandardOptions (&args);
    if (rc == 0)
    {
        do
        {
            uint32_t pcount;

            rc = ArgsAddOptionArray (args, MyOptions, sizeof MyOptions / sizeof (OptDef));
            if (rc)
                break;

            rc = ArgsParse (args, argc, (const char**)argv);
            if (rc)
                break;

            /* quirky way default path is generated means this comes
             * before standard argument handling */
            rc = ArgsOptionCount (args, OPTION_TABLE, &pcount);
            if (rc)
                break;

            if (pcount == 0)
            {
                static char * default_name = "RowWriteTestOutTable";
                char * user;

                user = getenv ("USER");

                if (user)
                    snprintf (tablePath, sizeof (tablePath),
                              "/home/%s/%s", user, default_name);
                else
                    strncpy (tablePath, default_name, sizeof (tablePath) - 1);
            }
            else
            {
                const char * pc;

                ArgsOptionValue (args, OPTION_TABLE, 0, (const void **)&pc);
                strncpy (tablePath, pc, sizeof (tablePath) - 1);
            }

            rc = ArgsHandleStandardOptions (args);
            if (rc)
                break;

            rc = ArgsParamCount (args, &pcount);
            if (rc)
                break;

            if (pcount)
            {
                const char * pc;

                rc = ArgsArgvValue (args, 0, &pc);
                if (rc)
                    break;

                rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcExcessive);

                PLOGERR (klogFatal, (klogFatal, rc, "$(P) takes no parameters", PLOG_S(P), pc));
                break;
            }

            rc = ArgsOptionCount (args, OPTION_ROW, &pcount);
            if (rc == 0)
            {
                uint64_t row_count;

                if (pcount)
                {
                    const char * pc;

                    rc = ArgsOptionValue (args, OPTION_ROW, 0, (const void **)&pc);
                    if (rc)
                        break;

                    row_count = AsciiToU32 (pc, NULL, NULL);
                }
                else
                    row_count = ROWS;

                rc = run (tablePath, row_count);
            }
        } while (0);

        ArgsWhack (args);
    }
    return VDB_TERMINATE( rc );
}
