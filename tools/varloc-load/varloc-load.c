/*==============================================================================
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

/* export LD_LIBRARY_PATH=/export/home/sybase/clients-mssql/current/lib */

#include <kapp/loader-meta.h> /* KLoaderMeta_Write */

#include "varloc-load.vers.h"

#include <kapp/main.h>

#include <vdb/manager.h> /* VDBManager */
#include <vdb/database.h>
#include <vdb/table.h> /* VTable */
#include <vdb/schema.h> /* VSchema */
#include <vdb/cursor.h>

#include <kdb/meta.h> /* KMetadata */

#include <rdbms/sybase.h> /* DBManager */

#include <kfg/config.h> /* KConfig */

#include <kfs/file.h> /* KFileMakeStdIn */

#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/printf.h> /* string_printf */
#include <klib/status.h> /* KStsLevel */
#include <klib/rc.h>

#include <strtol.h> /* strtoi64 */

#include <stdlib.h>
#include <assert.h>
#include <ctype.h> /* isspace */
#include <errno.h>
#include <stdarg.h> /* va_list */
#include <string.h> /* string */

#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))

#define DISP_RC2(rc, name, msg) (void)((rc == 0) ? 0 : \
    PLOGERR(klogInt, (klogInt,rc, "$(msg): $(name)","msg=%s,name=%s",msg,name)))

#define DESTRUCT_PTR(type, ptr) do { rc_t rc2 = type##Release(ptr); \
    if (rc2 && !rc) { rc = rc2; } ptr = NULL; } while (false)

#define DESTRUCT(name, obj) do { rc_t rc2 = name##Destruct(&obj); \
    if (rc2 && !rc) { rc = rc2; } } while (false)

ver_t CC KAppVersion(void) { return VARLOC_LOAD_VERS; }

/*===========================  ===========================*/

typedef struct Buffer {
    bool dirty;
    bool* buf;
} Buffer;

static void BufferMark(Buffer* self,
    int64_t from, int64_t to, int64_t sz)
{
    int64_t i;

    assert(self && sz);

    if (from >= sz) {
        return;
    }

    if (from < 0) {
        from = 0;
    }

    for (i = from; i <= to && i < sz; ++i) {
        self->buf[i] = 1;
    }

    self->dirty = true;
}

typedef rc_t (*WriterCallback)(const bool* buf, size_t sz, void* data);

static rc_t BufferFlush(Buffer* self,
    int64_t sz, WriterCallback writer, void* data)
{
    rc_t rc = 0;

    assert(self && sz && writer);

    rc = writer(self->buf, sz, data);

    if (rc == 0 && self->dirty) {
        memset(self->buf, 0 , sz);
        self->dirty = false;
    }

    return rc;
}

typedef struct Hitmap {
    Buffer buf[2];
    int i_buf;
    int i_buf_next;

    int64_t offset;

    int64_t sz;  /* sizeof buffer/chunk */
    int64_t len; /* total sequence length */

    WriterCallback writer;
    void* data;

    int64_t last_from;
    int64_t last_to;
} Hitmap;

static void HitmapMark(Hitmap* self, int64_t from, int64_t to)
{
    assert(self);

    BufferMark(&self->buf[self->i_buf], from, to, self->sz);

    if (to >= self->sz) {
        BufferMark(&self->buf[self->i_buf_next],
                 from - self->sz, to - self->sz, self->sz);
    }
}

static rc_t HitmapSwitch(Hitmap* self)
{
    rc_t rc = 0;

    assert(self);

    {
        int tmp = self->i_buf;

        self->i_buf = self->i_buf_next;
        self->i_buf_next = tmp;
        self->offset += self->sz;
        if (self->offset < 0) {
            rc = RC(rcExe, rcBuffer, rcAppending, rcRange, rcExcessive);
        }
    }
    return rc;
}

static rc_t implHitmapFlush(Hitmap* self, int64_t len)
{
    rc_t rc = 0;

    assert(self);

    if (len > self->sz) {
        len = self->sz;
    }

    rc = BufferFlush(&self->buf[self->i_buf], len, self->writer, self->data);

    if (rc == 0) {
        rc = HitmapSwitch(self);
    }

    return rc;
}

static rc_t HitmapFlush(Hitmap* self)
{
    assert(self);

    return implHitmapFlush(self, self->sz);
}

static rc_t HitmapFlushLast(Hitmap* self)
{
    rc_t rc = 0;

    int64_t len = 0;

    assert(self);

    do {
        len = self->len - self->offset;
        rc = implHitmapFlush(self, len);
        if (rc) {
            return rc;
        }
    } while (len > self->sz);

    return rc;
}

/* #define TODO -1 */

static rc_t HitmapAdd(Hitmap* self, int64_t pos_from, int64_t pos_to)
{
    rc_t rc = 0;

    int64_t from = 0;
    int64_t to = 0;

    assert(self && self->sz);

    if (pos_from < self->last_from) {
        return RC(rcExe, rcBuffer, rcAppending, rcParam, rcOutoforder);
    }
    else if (pos_from == self->last_from && pos_to < self->last_to) {
        return RC(rcExe, rcBuffer, rcAppending, rcParam, rcOutoforder);
    }

    if (pos_from > pos_to) {
        return RC(rcExe, rcBuffer, rcAppending, rcParam, rcOutoforder);
    }

    if (pos_to >= self->len) {
        return RC(rcExe, rcBuffer, rcAppending, rcParam, rcExcessive);
    }

    from = pos_from - self->offset;
    to   = pos_to - self->offset;

    if (from < 0) {
        if (to < 0) {
            return 0;
        }
        else {
            from = 0;
        }
    }

    self->last_from = from;
    self->last_to   = to;

    while (from > self->sz) { /* next window from > end of buffer */
        rc = HitmapFlush(self);
        if (rc != 0) {
            return rc;
        }

        from -= self->sz;
        to -= self->sz;
    }

    HitmapMark(self, from, to);

    while (to > 2 * self->sz) { /* next window to > end of 2nd buffer */
        rc = HitmapFlush(self);
        if (rc != 0) {
            return rc;
        }

        from = 0;
        to -= self->sz;
        HitmapMark(self, from, to);
    }

    return rc;
}

static rc_t HitmapRelease(Hitmap* self)
{
    rc_t rc = 0;

    if (self == NULL) {
        return rc;
    }

    free(self->buf[0].buf);
    free(self->buf[1].buf);

    self->buf[0].buf = self->buf[1].buf = NULL;

    free(self);

    return rc;
}
/* sz : size of Hitmap chunk
   len: total sequence length */
static rc_t CreateHitmap(Hitmap** hitmap,
    int64_t sz, size_t len, WriterCallback writer, void* data)
{
    rc_t rc = 0;
    Hitmap* self = NULL;
    assert(hitmap && sz && len && writer);

    self = calloc(1, sizeof *self);
    if (self == NULL) {
        return RC(rcExe, rcSelf, rcInitializing, rcMemory, rcExhausted);
    }

    self->buf[0].buf = calloc(1, sz);
    self->buf[1].buf = calloc(1, sz);
    if (self->buf[0].buf == NULL || self->buf[1].buf == NULL) {
        rc = RC(rcExe, rcSelf, rcInitializing, rcMemory, rcExhausted);
    }
    else {
        self->i_buf_next = 1;
        self->sz = sz;
        self->len = len;
        self->writer = writer;
        self->data = data;
    }

    if (rc == 0) {
        *hitmap = self;
    }
    else {
        HitmapRelease(self);
    }

    return rc;
}

/*=========================== COMMAND LINE & USAGE ===========================*/

#define ALIAS_STATS    "s"
#define OPTION_STATS   "statistics"
static const char* USAGE_STATS[]
    = { "print statistics for a given accession", NULL };
/*
#define ALIAS_ACCESSION    "A"
#define OPTION_ACCESSION   "accession"
static const char* USAGE_ACCESSION[] = { "accession", NULL };
*/

#define ALIAS_OUTPUT    "o"
#define OPTION_OUTPUT   "output-path"
static const char* USAGE_OUTPUT[] = { "target location", NULL };

#define ALIAS_START    "b"
#define OPTION_START   "start"
static const char* USAGE_START[]
    = { "starting VarLoc record number ( default 0 )", NULL };

#define ALIAS_STOP    "e"
#define OPTION_STOP   "stop"
static const char* USAGE_STOP[]
= { "ending VarLoc record number ( default max )", NULL };

#define ALIAS_TEST    "t"
#define OPTION_TEST   "test"
static const char* USAGE_TEST[] = { "run unit tests", NULL };

OptDef Options[] = {
      { OPTION_OUTPUT, ALIAS_OUTPUT, NULL, USAGE_OUTPUT, 1, true , true  }
    , { OPTION_STATS , ALIAS_STATS , NULL, USAGE_STATS , 1, false, false }
    , { OPTION_START , ALIAS_START , NULL, USAGE_START , 1, true , false }
    , { OPTION_STOP  , ALIAS_STOP  , NULL, USAGE_STOP  , 1, true , false }
    , { OPTION_TEST  , ALIAS_TEST  , NULL, USAGE_TEST  , 1, true , false }
};

const char UsageDefaultName[] = "valroc-load";

rc_t CC UsageSummary (const char * progname) {
    return KOutMsg (
        "Usage:\n"
        "  %s [options] <accession>\n"
        "\n"
        "Summary:\n"
        "  Update variation location DB\n",
        progname);
}

rc_t CC Usage(const Args* args) {
    rc_t rc = 0;

    const char* progname = UsageDefaultName;
    const char* fullpath = UsageDefaultName;

    if (args == NULL)
    {    rc = RC(rcExe, rcArgv, rcAccessing, rcSelf, rcNull); }
    else
    {    rc = ArgsProgram(args, &fullpath, &progname); }

    UsageSummary(progname);

    KOutMsg("\nOptions:\n");

    HelpOptionLine(ALIAS_OUTPUT, OPTION_OUTPUT, NULL, USAGE_OUTPUT);
    KOutMsg("\n");
    HelpOptionLine(ALIAS_STATS , OPTION_STATS , NULL, USAGE_STATS );
    HelpOptionLine(ALIAS_START , OPTION_START , NULL, USAGE_START );
    HelpOptionLine(ALIAS_STOP  , OPTION_STOP  , NULL, USAGE_STOP  );

    KOutMsg("\n");

    HelpOptionsStandard ();

    HelpVersion(fullpath, KAppVersion());

    return rc;
}

typedef struct Refseq {
    int64_t MAX_SEQ_LEN;
    int64_t TOTAL_SEQ_LEN;
    char gi[65];
} Refseq;

static void RefseqReset(Refseq* self) {
    assert(self);
    memset(self, 0, sizeof *self);
    self->MAX_SEQ_LEN = self->TOTAL_SEQ_LEN = -1;
}

typedef struct Params {
    Args* args;
    const char* acc;
    const char* path;
    bool testHitmap;
    bool testStdin;
    bool verbose;
    bool stats;
    int64_t start;
    int64_t stop;

    Refseq refseq;

    bool empty;
} Params;

static rc_t ParamsConstruct(rc_t rc, int argc, char* argv[],
    Params* prm)
{
    Args* args = NULL;
    assert(argc && argv && prm);
    memset(prm, 0, sizeof *prm);
    prm->stop = -1;
    RefseqReset(&prm->refseq);
    if (argc < 2) {
        prm->empty = true;
        return UsageSummary(UsageDefaultName);
    }
    if (rc != 0) {
        return rc;
    }
    do {
        uint32_t i = 0;
        uint32_t pcount = 0;
        rc = ArgsMakeAndHandle(&args, argc, argv, 1,
            Options, sizeof Options / sizeof (OptDef));
        if (rc) {
            LOGERR(klogErr, rc, "while calling ArgsMakeAndHandle");
            break;
        }
        prm->args = args;

        /* START */
        rc = ArgsOptionCount (args, OPTION_START, &pcount);
        if (rc) {
            LOGERR(klogErr,
                rc, "Failure to get '" OPTION_START "' argument");
            break;
        }
        if (pcount > 0) {
            const char* rows = NULL;
            rc = ArgsOptionValue(args, OPTION_START, 0, &rows);
            if (rc) {
                LOGERR(klogErr,
                    rc, "Failure retrieving '" OPTION_START "' parameter");
                break;
            }
            prm->start = AsciiToI64(rows, NULL, NULL);
            /*else {
                char* end = NULL;
                int64_t val = strtoi64(gi, &end, 0);
                if (errno || (end && end[0]) || val <= 0) {
                    rc = RC(rcExe, rcArgv, rcParsing, rcType, rcIncorrect);
                    LOGERR(klogErr, rc, "GI is a positive number. Isn't it?");
                    break;
                }
                prm->gi = gi;
            }*/
        }

        /* STOP */
        rc = ArgsOptionCount (args, OPTION_STOP, &pcount);
        if (rc) {
            LOGERR(klogErr,
                rc, "Failure to get '" OPTION_STOP "' argument");
            break;
        }
        if (pcount > 0) {
            const char* rows = NULL;
            rc = ArgsOptionValue(args, OPTION_STOP, 0, &rows);
            if (rc) {
                LOGERR(klogErr,
                    rc, "Failure retrieving '" OPTION_STOP "' parameter");
                break;
            }
            prm->stop = AsciiToI64(rows, NULL, NULL);
        }

        /* STATS */
        rc = ArgsOptionCount (args, OPTION_STATS, &pcount);
        if (rc) {
            LOGERR(klogErr,
                rc, "Failure to get '" OPTION_STATS "' argument");
            break;
        }
        if (pcount > 0) {
            prm->stats = true;
        }

        /* TEST */
        rc = ArgsOptionCount (args, OPTION_TEST, &pcount);
        if (rc) {
            LOGERR(klogErr,
                rc, "Failure to get '" OPTION_TEST "' argument");
            break;
        }
        for (i = 0; i < pcount; ++i) {
            const char* v = NULL;
            rc = ArgsOptionValue(args, OPTION_TEST, i, &v);
            if (rc) {
                LOGERR(klogErr,
                    rc, "Failure retrieving '" OPTION_TEST "' parameter");
                break;
            }
            if (!strcmp("hitmap", v)) {
                prm->testHitmap = true;
            }
            else if (!strcmp("stdin", v)) {
                prm->testStdin = true;
            }
            else {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                LOGERR(klogErr, rc, "Invalid '" OPTION_TEST "' value. "
                    "Choose between: hitmap and stdin");
                break;
            }
        }
        if (rc) {
            break;
        }

        /* OUTPUT */
        if (!prm->stats && !prm->testHitmap) {
            rc = ArgsOptionCount (args, OPTION_OUTPUT, &pcount);
            if (rc) {
                LOGERR(klogErr,
                    rc, "Failure to get '" OPTION_OUTPUT "' argument");
                break;
            }
            if (pcount < 1) {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
                LOGERR(klogErr, rc, "Missing '" OPTION_OUTPUT "' parameter");
                break;
            }
            if (pcount > 1) {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
                LOGERR(klogErr, rc, "Too many '" OPTION_OUTPUT "' parameters");
                break;
            }
            {
                rc = ArgsOptionValue(args, OPTION_OUTPUT, 0, &prm->path);
                if (rc) {
                    LOGERR(klogErr,
                        rc, "Failure retrieving '" OPTION_OUTPUT "' parameter");
                    break;
                }
            }
        }

        /* ACCESSION */
        rc = ArgsParamCount(args, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing accession");
            break;
        }
        if (pcount < 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR(klogErr, rc, "Missing accession parameter");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many accession parameters");
            break;
        }
        rc = ArgsParamValue(args, 0, &prm->acc);
        if (rc) {
            LOGERR(klogErr, rc, "Failure retrieving accession path");
            break;
        }
    } while (0);

    if (rc == 0) {
        if (KStsLevelGet() > 0) {
            prm->verbose = true;
        }
    }

    return rc;
}

static rc_t ParamsDestruct(Params* prm) {
    rc_t rc = 0;

    assert(prm);

    DESTRUCT_PTR(Args, prm->args);

    return rc;
}

/*=========================== IO DB DATA ===========================*/

enum SqlType {
      eStr
    , eU8
    , eI32
    , eU64
};

typedef struct Column {
    const char* r_name;
    enum SqlType type;
    bool allowNull;
    bitsz_t elem_bytes;
    uint32_t r_idx;
    uint32_t v_idx;
    union {
        const String* s;
        uint8_t  u8;
        int32_t  i32;
        uint64_t u64;
    } val;
} Column;

#define UNDEF 255

typedef struct Columns {
    Column* col;
    size_t num;
    int iFrom;
    int iTo;
} Columns;

static Columns* GiveColumns() {
    int i = 0;
    static Column column[] = {
      { "entrez_id"    , eI32, false, sizeof( int32_t), UNDEF, UNDEF, { NULL } }
     ,{ "gi"           , eU64, false, sizeof(uint64_t), UNDEF, UNDEF, { NULL } }
     ,{ "parent_var_id", eStr, true , 1               , UNDEF, UNDEF, { NULL } }
     ,{ "pos_from"     , eI32, false, sizeof( int32_t), UNDEF, UNDEF, { NULL } }
     ,{ "pos_to"       , eI32, false, sizeof( int32_t), UNDEF, UNDEF, { NULL } }
     ,{ "score"        , eI32, false, sizeof( int32_t), UNDEF, UNDEF, { NULL } }
     ,{ "var_id"       , eStr, false, 1               , UNDEF, UNDEF, { NULL } }
     ,{ "var_source"   , eU8 , false, sizeof( uint8_t), UNDEF, UNDEF, { NULL } }
     ,{ "var_type"     , eU8 , false, sizeof( uint8_t), UNDEF, UNDEF, { NULL } }
    };

    static Columns cols
        = { column, sizeof(column) / sizeof(column[0]), -1, -1 };

    cols.iFrom = cols.iTo = -1;

    for (i = 0; i < cols.num; ++i) {
        const Column* col = &cols.col[i];
        if (strcmp(col->r_name, "pos_from") == 0) {
            cols.iFrom = i;
        }
        else if (strcmp(col->r_name, "pos_to") == 0) {
            cols.iTo = i;
        }
    }

    assert(cols.iFrom >= 0 && cols.iTo >= 0);

    return &cols;
}

/*=========================== MsQSL ===========================*/

typedef struct PString {
    char* data;
    size_t len;
} PString;

static void PStringInit(PString* s, char* c, size_t l)
{
    assert(s && c && l);
    s->data = c;
    s->len = l;
}

static rc_t SqlAppend(PString* sql, const char* rhs)
{
    rc_t rc = 0;

    assert(sql && rhs);

    if (strlen(sql->data) + strlen(rhs) + 1 > sql->len) {
        rc = RC
            (rcExe, rcQuery, rcConstructing, rcBuffer, rcInsufficient);
    }
    else {
        strcat(sql->data, rhs);
    }

    return rc;
}

rc_t BuildQuery(PString* sql, Columns* cols, Params* prm) {
    rc_t rc = 0;
    int i = 0;
    const char* gi;

    assert(sql && sql->len && cols && prm);

    sql->data[0] = '\0';
    rc = SqlAppend(sql, "SELECT "); /* TOP 7000 "); */
    for (i = 0; i < cols->num && rc == 0;
        ++i)
    {
        Column* col = &cols->col[i];
        assert(col);
        if (strlen(sql->data) + 2 + strlen(col->r_name) > sql->len - 1)
        {
            rc = RC
                (rcExe, rcQuery, rcConstructing, rcBuffer, rcInsufficient);
            break;
        }
        if (i > 0) {
            strcat(sql->data, ", ");
        }
        strcat(sql->data, col->r_name);
        col->r_idx = i;
    }

    if (rc == 0) {
        rc = SqlAppend(sql, " FROM VarLoc_dbSNP");
    }

    gi = prm->refseq.gi;
    if (gi[0] && rc == 0) {
        const char where[] = " WHERE gi=";
        if (strlen(sql->data) + strlen(where) + strlen(gi) + 1 > sql->len) {
            rc = RC
                (rcExe, rcQuery, rcConstructing, rcBuffer, rcInsufficient);
        }
        else {
            strcat(sql->data, where);
            strcat(sql->data, gi);
        }
    }

    if (rc == 0) {
        rc = SqlAppend(sql, " ORDER BY pos_from, pos_to");
    }

    DISP_RC(rc, "");

    return rc;
}

static rc_t DatabaseRelease(Database* a) { return DatabaseWhack(a); }
static rc_t DBResultSetRelease(DBResultSet* a) { return DBResultSetWhack(a); }
static rc_t DBRowRelease(DBRow* a) { return DBRowWhack(a); }

typedef struct RDB {
    const DBManager* mgr;
    Database* db;
    DBResultSet* rs;
} RDB;

static rc_t RDBRelease(RDB* self)
{
    rc_t rc = 0;

    if (self == NULL) {
        return rc;
    }

    DESTRUCT_PTR(DBResultSet, self->rs);
    DESTRUCT_PTR(Database, self->db);
    DESTRUCT_PTR(DBManager, self->mgr);

    free(self);

    return rc;
}

static rc_t CreateRDB(RDB** rdb)
{
    rc_t rc = 0;
    RDB* self = NULL;
    assert(rdb);

    self = calloc(1, sizeof *self);
    if (self == NULL) {
        return RC(rcExe, rcSelf, rcInitializing, rcMemory, rcExhausted);
    }

    if (rc == 0) {
        rc = SybaseInit(OS_CS_VERSION);
        DISP_RC(rc, "failed to init Sybase");
    }

    if (rc == 0) {
        rc = DBManagerInit(&self->mgr, "sybase");
        DISP_RC(rc, "failed to init Sybase");
    }

    if (rc == 0) {
        const char server[] = "DBVAR_LOAD";
        const char dbname[] = "VarLoc";
        const char user[] = "anyone";
        rc = DBManagerConnect
            (self->mgr, server, dbname, user, "allowed", &self->db);
        DISP_RC(rc, "failed to connect to database");
    }

    if (rc == 0) {
        *rdb = self;
    }
    else {
        RDBRelease(self);
    }

    return rc;
}

static rc_t RDBExecute(RDB* r, const char* sql)
{
    rc_t rc = 0;

    assert(r && sql);

    rc = DatabaseExecute(r->db, &r->rs, sql);
    DISP_RC(rc, "failed to execute query");

    return rc;
}

/* ============= BEGIN copy-paste from libs/align/refseq-mgr.c ============== */

static
rc_t RefSeqMgr_KfgReadStr(const KConfig* kfg,
    const char* path, char* value, size_t value_sz)
{
    rc_t rc = 0;
    const KConfigNode *node;

    if ( (rc = KConfigOpenNodeRead(kfg, &node, "%s", path)) == 0 ) {
        size_t num_read, remaining;
        if( (rc = KConfigNodeRead
            (node, 0, value, value_sz - 1, &num_read, &remaining)) == 0 )
        {
            if( remaining != 0 ) {
                rc = RC(rcExe, rcString, rcReading, rcSize, rcExcessive);
            }
            else {
                value[num_read] = 0;
            }
        }
        KConfigNodeRelease(node);
    } else if( GetRCState(rc) == rcNotFound ) {
        rc = 0;
    }
    return rc;
}

typedef bool (*RefSeqMgr_ForEachVolume_callback)
    (char const server[], char const volume[], void *data);

static rc_t RefSeqMgr_ForEachVolume(const KConfig* kfg,
    RefSeqMgr_ForEachVolume_callback cb, void* data)
{
    rc_t rc = 0;
    char servers[4096] = ".";
    char volumes[4096] = "";
    
    if( kfg == NULL || cb == NULL ) {
        return RC(rcExe, rcTable, rcSearching, rcParam, rcNull);
    }

    /* try local dir first */
    if( cb(servers, NULL, data) ) {
        return rc;
    }

    if( (rc = RefSeqMgr_KfgReadStr
        (kfg, "refseq/paths", servers, sizeof(servers))) != 0 )
    {
        LOGERR(klogInt, rc, "while calling RefSeqMgr_KfgReadStr(refseq/paths)");
    }
    else {
        bool found = false;
        char *srv_sep;
        char *srv_rem = servers;

        if( servers[0] != '\0' ) {
          do {
            char const* server = srv_rem;

            srv_sep = strchr(server, ':');
            if(srv_sep) {
                srv_rem = srv_sep + 1;
                *srv_sep = 0;
            }
            if( cb(server, NULL, data) ) {
                found = true;
                break;
            }
          } while(srv_sep);
        }

        if( !found ) {
            if( (rc = RefSeqMgr_KfgReadStr
                    (kfg, "refseq/servers", servers, sizeof(servers))) != 0 ||
                (rc = RefSeqMgr_KfgReadStr
                    (kfg, "refseq/volumes", volumes, sizeof(volumes))) != 0 )
            {
                LOGERR(klogInt, rc,
                  "while calling RefSeqMgr_KfgReadStr(refseq/servers|volumes)");
            }
            else {
                char *srv_sep;
                char *srv_rem = servers;

                /* servers and volumes are deprecated and optional */
                if( servers[0] != '\0' || volumes[0] != '\0' ) {
                    do {
                        char vol[4096];
                        char const *server = srv_rem;
                        char *vol_rem = vol;
                        char *vol_sep;
                        
                        strcpy(vol, volumes);
                        srv_sep = strchr(server, ':');
                        if(srv_sep) {
                            srv_rem = srv_sep + 1;
                            *srv_sep = 0;
                        }
                        do {
                            char const *volume = vol_rem;
                            
                            vol_sep = strchr(volume, ':');
                            if(vol_sep) {
                                vol_rem = vol_sep + 1;
                                *vol_sep = 0;
                            }
                            found = cb(server, volume, data);
                        } while(!found && vol_sep);
                    } while(!found && srv_sep);
                }
            }
        }
    }

    return rc;
}

/* ============== END copy-paste from libs/align/refseq-mgr.c =============== */

typedef struct RefseqFileCtx {
    rc_t rc;
    const VDBManager* mgr;
    const VTable* tbl;
    const char* name;
} RefseqFileCtx;

static bool FindRefseq(char const server[], char const volume[],
    void* data)
{
    rc_t rc = 0;
    RefseqFileCtx* ctx = data;

    assert(ctx && ctx->name);

    if (ctx->rc)
        return false;

    if (volume == NULL ) {
        rc = VDBManagerOpenTableRead(ctx->mgr, &ctx->tbl, NULL,
            "%s/%s", server, ctx->name);
    }
    else {
        rc = VDBManagerOpenTableRead(ctx->mgr, &ctx->tbl, NULL,
            "%s/%s/%s", server, volume, ctx->name);
    }

    if (rc == 0) {
        return true;
    }
    else if (GetRCState(rc) != rcNotFound) {
        ctx->rc = rc;
        PLOGERR(klogInt, (klogInt, rc, "$(s)/$(v)/$(n)", "s=%s,v=%s,n=%s",
            server, volume, ctx->name));
    }

    return false;
}

typedef union {
    const void* v;
    const uint32_t* u32;
    const uint64_t* u64;
} Input;

static rc_t ReadCursor(const VCursor* cursor, uint32_t idx,
    Input* in, uint32_t* len, const char* name)
{
    rc_t rc = 0;

    rc = VCursorCellData(cursor, idx, NULL, &in->v, NULL, len);
    DISP_RC(rc, name);

    return rc;
}

static rc_t FindRefseqTable(const VDBManager* mgr, const char* name,
    const VTable** tbl)
{
    rc_t rc = 0;

    KConfig* kfg = NULL;

    assert(mgr && name && tbl);

    if (rc == 0) {
        rc = KConfigMake(&kfg, NULL);
        DISP_RC(rc, "while calling KConfigMake");
    }

    if (rc == 0) {
        RefseqFileCtx ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.mgr = mgr;
        ctx.name = name;
        rc = RefSeqMgr_ForEachVolume(kfg, FindRefseq, &ctx);

        if (rc == 0) {
            rc = ctx.rc;
            if (rc == 0) {
                *tbl = ctx.tbl;
                if (*tbl == NULL) {
                    rc = RC(rcExe, rcTable, rcOpening, rcTable, rcNotFound);
                    DISP_RC(rc, name);
                }
            }
        }
    }

    DESTRUCT_PTR(KConfig, kfg);

    return rc;
}

enum EColType {
      eMAX_SEQ_LEN
    , eSEQ_ID
    , eTOTAL_SEQ_LEN
};

typedef struct RefseqCol {
    enum EColType type;
    const char* name;
    uint32_t idx;
} RefseqCol;

static rc_t AddColumn(rc_t rc, const VCursor* cursor, RefseqCol* col)
{
    if (rc) {
        return rc;
    }

    assert(cursor && col);

    rc = VCursorAddColumn(cursor, &col->idx, "%s", col->name);
    DISP_RC(rc, col->name);

    return rc;
}

static rc_t ExtractGi(const void* seq_id, uint32_t len, Refseq* refseq)
{
    rc_t rc = 0;

    assert(seq_id && refseq);

    if (len < 5) {
        rc = RC(rcExe, rcId, rcParsing, rcId, rcTooShort);
    }
    else {
        char* s = malloc(len + 1);

        if (s == NULL) {
            rc = RC(rcExe, rcId, rcParsing, rcMemory, rcExhausted);
        }
        else {
            memcpy(s, seq_id, len);
            s[len] = '\0';

            if (s[0] != 'g' || s[1] != 'i' || s[2] != '|') {
                rc = RC(rcExe, rcId, rcParsing, rcId, rcUnrecognized);
            }
            else {
                const int GI_S_LEN = 3;
                char* p = memchr(s + GI_S_LEN, '|', len - GI_S_LEN);
                if (p != NULL) {
                    int gi_len = p - s - GI_S_LEN;
                    if (gi_len > sizeof refseq->gi) {
                        rc = RC(rcExe, rcCursor,
                            rcReading, rcBuffer, rcInsufficient);
                    }
                    else {
                        memcpy(refseq->gi, s + GI_S_LEN, gi_len);
                    }
                }
            }

            free(s);
        }
    }

    return rc;
}

static rc_t ReadColumns(const VCursor* cursor, Params* prm,
    const RefseqCol* refseq_col, size_t refseq_col_num)
{
    rc_t rc = 0;
    int i = 0;
    Refseq* refseq = NULL;
    bool dump_refseq = false;
    assert(cursor && prm);
    refseq = &prm->refseq;
    dump_refseq = prm->verbose;
    for (i = 0; i < refseq_col_num && rc == 0; ++i) {
        Input in = { NULL };
        uint32_t len = 0;
        rc = ReadCursor
            (cursor, refseq_col[i].idx, &in, &len, refseq_col[i].name);
        if (rc != 0) {
            break;
        }
        switch (refseq_col[i].type) {
            case eMAX_SEQ_LEN:
                if (len != 1) {
                    rc = RC(rcExe, rcCursor, rcReading, rcSize, rcUnexpected);
                }
                else {
                    refseq->MAX_SEQ_LEN = *in.u32;
                    if (dump_refseq)
                        OUTMSG(("MAX_SEQ_LEN=%d\t", refseq->MAX_SEQ_LEN));
                }
                break;
            case eTOTAL_SEQ_LEN:
                if (len != 1) {
                    rc = RC(rcExe, rcCursor, rcReading, rcSize, rcUnexpected);
                }
                else {
                    refseq->TOTAL_SEQ_LEN = *in.u64;
                    if (dump_refseq)
                        OUTMSG(("TOTAL_SEQ_LEN=%ld\t", refseq->TOTAL_SEQ_LEN));
                }
                break;
            case eSEQ_ID:
                if (dump_refseq)
                    OUTMSG(("SEQ_ID=%.*s\t", len, in.v));
                rc = ExtractGi(in.v, len, refseq);
                if (dump_refseq)
                    OUTMSG(("GI=%s\t", refseq->gi));
                break;
            default:
                assert(0);
                break;
        }
    }
    if (dump_refseq)
        OUTMSG(("\n\n"));
    return rc;
}

static rc_t RefseqRead(rc_t rc, const VDBManager* mgr, Params* prm)
{
    RefseqCol refseq_col[] = {
          { eMAX_SEQ_LEN  , "MAX_SEQ_LEN"  , UNDEF }
        , { eSEQ_ID       , "SEQ_ID"       , UNDEF }
        , { eTOTAL_SEQ_LEN, "TOTAL_SEQ_LEN", UNDEF }
    };

    const VTable* tbl = NULL;
    const VCursor* cursor = NULL;
    /* uint32_t iMAX_SEQ_LEN = 0;
    uint32_t iSEQ_ID = 0;
    uint32_t iTOTAL_SEQ_LEN = 0; */
    int i = 0;
    if (rc) {
        return rc;
    }
    assert(mgr);
    if (rc == 0) {
        rc = FindRefseqTable(mgr, prm->acc, &tbl);
    }
    if (rc == 0) {
        assert(tbl);
        rc = VTableCreateCursorRead(tbl, &cursor);
        DISP_RC(rc, "while calling VTableCreateCursorRead");
    }
    for (i = 0; i < sizeof refseq_col / sizeof refseq_col[0] && rc == 0;
        ++i)
    {
        rc = AddColumn(rc, cursor, &refseq_col[i]);
    }
    if (rc == 0) {
        /* iMAX_SEQ_LEN = refseq_col[0].idx;
        iSEQ_ID = refseq_col[1].idx;
        iTOTAL_SEQ_LEN = refseq_col[2].idx; */
        rc = VCursorOpen(cursor);
        DISP_RC(rc, "while opening read cursor");
    }
    if (rc == 0) {
        rc = VCursorSetRowId(cursor, 1);
        DISP_RC(rc, "while calling VCursorSetRowId");
    }
    if (rc == 0) {
        rc = VCursorOpenRow(cursor);
        DISP_RC(rc, "while calling VCursorOpenRow");
    }
    if (rc == 0) {
        rc = ReadColumns(cursor, prm,
            refseq_col, sizeof refseq_col / sizeof refseq_col[0]);
    }
    DESTRUCT_PTR(VCursor, cursor);
    DESTRUCT_PTR(VTable, tbl);
    return rc;
}

/*=========================== VDB (OUTPUT) ===========================*/

typedef struct VDB {
    VDatabase* db;
    VSchema* schema;
} VDB;

static rc_t VDBRelease(VDB* self) {
    rc_t rc = 0;
    if (self == NULL) {
        return rc;
    }
    DESTRUCT_PTR(VSchema, self->schema);
    DESTRUCT_PTR(VDatabase, self->db);
    free(self);
    return rc;
}

static rc_t CreateVDB(VDB** vdb, VDBManager* mgr, const char* path)
{
    rc_t rc = 0;
    VDB* self = NULL;
    assert(vdb && mgr && path);
    self = calloc(1, sizeof *self);
    if (self == NULL) {
        return RC(rcExe, rcSelf, rcInitializing, rcMemory, rcExhausted);
    }
    if (rc == 0) {
        rc = VDBManagerMakeSchema(mgr, &self->schema);
        DISP_RC(rc, "while calling VDBManagerMakeSchema");
    }
    if (rc == 0) {
        rc = VSchemaParseFile(self->schema, "ncbi/varloc.vschema");
        DISP_RC(rc, "while calling VSchemaParseFile");
    }
    if (rc == 0) {
        rc = VDBManagerCreateDB(mgr, &self->db, self->schema,
                                "NCBI:var:db:varloc", kcmOpen, "%s", path);
        DISP_RC2(rc, path, "while opening DB");
    }
    if (rc == 0) {
        *vdb = self;
    }
    else {
        VDBRelease(self);
    }
    return rc;
}

static rc_t VDBWriteLoaderMeta(const VDB* self, const char* argv0) {
    rc_t rc = 0;
    KMetadata* meta = NULL;
    KMDataNode* node = NULL;
    assert(self);
    if (rc == 0) {
        rc = VDatabaseOpenMetadataUpdate(self->db, &meta);
        DISP_RC(rc, "while calling VDatabaseOpenMetadataRead");
    }
    if (rc == 0) {
        const char path[] = "/";
        rc = KMetadataOpenNodeUpdate(meta, &node, "%s", path);
        DISP_RC2(rc, path, "while calling KMetadataOpenNodeUpdate");
    }
    if (rc == 0) {
        rc = KLoaderMeta_Write(node, argv0, __DATE__, "VarLoc", KAppVersion());
        DISP_RC(rc, "while calling KLoaderMeta_Write");
    }
    DESTRUCT_PTR(KMDataNode, node);
    DESTRUCT_PTR(KMetadata, meta);
    return rc;
}

static rc_t InitVarlocCursor(VCursor** cursor,
    VTable* table, Columns* cols)
{
    rc_t rc = 0;

    assert(cursor && table && cols);

    if (rc == 0) {
        rc = VTableCreateCursorWrite(table, cursor, kcmInsert);
        DISP_RC(rc, "while creating VARLOC cursor");
    }

    if (rc == 0) {
        int i = 0;

        assert(cols);

        for (i = 0; i < cols->num && rc == 0; ++i) {
            char name[256] = "";
            size_t sz = 0;
            Column* col = &cols->col[i];
            assert(col);

            sz = toupper_copy(name, sizeof name,
                col->r_name, strlen(col->r_name) + 1);
            if (sz >= sizeof name) {
                rc = RC(rcExe, rcCursor, rcCreating, rcBuffer, rcInsufficient);
                DISP_RC2(rc, col->r_name, "while adding to VARLOC cursor");
            }

            if (col->elem_bytes == 0) {
                break;
            }

            if (rc == 0 && col->r_idx != UNDEF) {
                rc = VCursorAddColumn
                    (*cursor, &col->v_idx, "%s", name);
                DISP_RC2(rc, name, "while adding to VARLOC cursor");
            }
        }
    }

    if (rc == 0) {
        rc = VCursorOpen(*cursor);
        DISP_RC(rc, "while opening VARLOC cursor");
    }

    return rc;
}

static rc_t CreateVarlocCursor(VCursor** cursor,
    VDB* db, Columns* cols)
{
    rc_t rc = 0;
    VTable* table = NULL;

    assert(cursor && db && cols);

    if (rc == 0) {
        const char member[] = "VARLOC";
        rc = VDatabaseCreateTable
            (db->db, &table, member, kcmOpen, "%s", member);
        DISP_RC2(rc, member, "while opening table");
    }

    if (rc == 0) {
        rc = InitVarlocCursor(cursor, table, cols);
    }

    DESTRUCT_PTR(VTable, table);

    return rc;
}

static rc_t VarlocCursorCommit(VCursor* cursor) {
    rc_t rc = VCursorCommit(cursor);
    DISP_RC(rc, "while committing VARLOC cursor");
    return rc;
}

/*=========================== RESULT PROCESSING ===========================*/

typedef rc_t(*HandlerCallback)(Columns* cols, int64_t row, void* data);

typedef struct Command {
    HandlerCallback execute;
    void* data;
    struct Command* next;
} Command;
static void CommandInit(Command* self,
    HandlerCallback callback, void* data, Command* next)
{
    assert(self && callback);
    memset(self, 0, sizeof *self);
    self->execute = callback;
    self->data = data;
    self->next = next;
}
static rc_t CommandsRun(const Command* cmd, Columns* cols, int64_t row) {
    rc_t rc = 0;
    while (rc == 0 && cmd) {
        if (cmd->execute) {
            rc = cmd->execute(cols, row, cmd->data);
        }
        cmd = cmd->next;
    }
    return rc;
}
/*======================== RESULT PROCESSING: >STDOUT ========================*/

static rc_t PrintHandler(Columns* cols, int64_t row, void* data)
{
    rc_t rc = 0;

    int i = 0;
    bool first = true;

/*  OUTMSG(("%d\t", row)); */
    for (i = 0; i < cols->num && rc == 0; ++i) {
        Column* col = &cols->col[i];
        assert(col);
        if (col->r_idx == UNDEF) {
            continue;
        }
        OUTMSG(("%s%s=", first ? "" : ",\t", col->r_name));
        first = false;

        switch (col->type) {
            case eU8:
                OUTMSG(("%u", col->val.u8));
                break;
            case eI32:
                OUTMSG(("%d", col->val.i32));
                break;
            case eU64:
                OUTMSG(("%lu", col->val.u64));
                break;
            case eStr:
                OUTMSG(("%.*s",
                    col->val.s->size, col->val.s->addr));
                break;
            default:
                break;
        }
    }

    if (rc == 0) {
        OUTMSG(("\n"));
    }

    return rc;
}

/*======================== RESULT PROCESSING: >VDB ========================*/

static rc_t VDBSaveHandler(Columns* cols, int64_t row, void* data) {
    rc_t rc = 0;
    VCursor* varloc_cursor = (VCursor*)data;
    assert(cols && data);

    if (rc == 0) {
        rc = VCursorOpenRow(varloc_cursor);
        DISP_RC(rc, "while opening VARLOC row");
    }

    if (rc == 0) {
        int i = 0;
        for (i = 0; i < cols->num && rc == 0; ++i) {
            bitsz_t elem_bits = 0;
            const void* buffer = NULL;
            uint64_t count = 1;
            Column* col = &cols->col[i];
            assert(col);
            elem_bits = col->elem_bytes * 8;
            buffer = &col->val.u64;
            if (col->elem_bytes == 0) {
                break;
            }
            if (col->v_idx == UNDEF) {
                continue;
            }
            if (col->type == eStr) {
                elem_bits = 8;
                count = col->val.s->size;
                buffer = col->val.s->addr;
            }
            if (col->allowNull && !buffer && !count) {
                buffer = "";
            }
            rc = VCursorWrite
                (varloc_cursor, col->v_idx, elem_bits, buffer, 0, count);
            DISP_RC(rc, col->r_name);
        }
    }

    if (rc == 0) {
        rc = VCursorCommitRow(varloc_cursor);
        DISP_RC(rc, "while committing VARLOC row");
    }

    if (rc == 0) {
        rc = VCursorCloseRow(varloc_cursor);
        DISP_RC(rc, "while closing VARLOC row");
    }

    return rc;
}

static rc_t HitmapWriterCallback(const bool* buf, size_t sz, void* data);

#define MAX_I32  0x80000000
static rc_t HitmapAddHandler(Columns* cols, int64_t row, void* data)
{
    rc_t rc = 0;
    Column* colFrom = NULL;
    Column* colTo   = NULL;
    Hitmap* hitmap = (Hitmap*)data;

    assert(cols && hitmap);

    if (cols->iFrom == -1 || cols->iTo == -1) {
        return 0;
    }

    colFrom = &cols->col[cols->iFrom];
    colTo   = &cols->col[cols->iTo  ];

    assert(!strcmp(colFrom->r_name, "pos_from"));
    assert(!strcmp(colTo->r_name, "pos_to"));

    if (rc == 0) {
        int64_t from = colFrom->val.i32;
        int64_t to   = colTo  ->val.i32;
        if (from >= MAX_I32) {
            rc = RC(rcExe, rcColumn, rcReading, rcRange, rcExcessive);
            PLOGERR(klogErr, (klogErr, rc, "while adding "
                "(from='$(from)') to hitmap", "from=%d", colFrom->val.i32));
        }
        else if (to >= MAX_I32) {
            rc = RC(rcExe, rcColumn, rcReading, rcRange, rcExcessive);
            PLOGERR(klogErr, (klogErr, rc, "while adding "
                "(to='$(to)') to hitmap", "to=%d", colTo->val.i32));
        }
        else {
            rc = HitmapAdd(hitmap, from, to);
            if (rc) {
                PLOGERR(klogErr, (klogErr, rc, "while adding "
                    "(from='$(from)', to='$(to)') to hitmap(len='$(len)')",
                    "from=%lu,to=%lu,len=%lu", from, to, hitmap->len));
            }
        }
    }

    return rc;
}

typedef struct HitmapTbl {
    VCursor* cursor;
    uint32_t iHITS;
} HitmapTbl;
static rc_t HitmapTblCommit(HitmapTbl* self) {
    rc_t rc = 0;
    assert(self);
    rc = VCursorCommit(self->cursor);
    DISP_RC(rc, "while committing HITMAP cursor");
    return rc;
}
static rc_t HitmapTblRelease(HitmapTbl* self) {
    rc_t rc = 0;
    assert(self);
    DESTRUCT_PTR(VCursor, self->cursor);
    free(self);
    return rc;
}
static rc_t CreateHitmapTbl(HitmapTbl** tbl,
    VDatabase* db, uint32_t max_seq_len)
{
    rc_t rc = 0;
    VTable* table = NULL;
    HitmapTbl* self = NULL;
    assert(tbl && db && max_seq_len);
    self = calloc(1, sizeof *self);
    if (self == NULL) {
        return RC(rcExe, rcSelf, rcInitializing, rcMemory, rcExhausted);
    }
    if (rc == 0) {
        const char member[] = "HITMAP";
        rc = VDatabaseCreateTable(db, &table, member, kcmOpen, "%s", member);
        DISP_RC2(rc, member, "while opening table");
    }
    if (rc == 0) {
        rc = VTableCreateCursorWrite(table, &self->cursor, kcmInsert);
        DISP_RC(rc, "while creating HITMAP cursor");
    }
    if (rc == 0) {
        uint32_t iMAX_SEQ_LEN = 0;
        char name[] = "MAX_SEQ_LEN";
        rc = VCursorAddColumn(self->cursor, &iMAX_SEQ_LEN, "%s", name);
        DISP_RC2(rc, name, "while adding to HITMAP cursor");
        if (rc == 0) {
            uint32_t buffer = max_seq_len;
            rc = VCursorDefault(self->cursor,
                iMAX_SEQ_LEN, 8 * sizeof(uint32_t), &buffer, 0, 1);
            DISP_RC2(rc, name, "while VCursorDefault(table=HITMAP)");
        }
    }
    if (rc == 0) {
        char name[] = "HITS";
        rc = VCursorAddColumn(self->cursor, &self->iHITS, "%s", name);
        DISP_RC2(rc, name, "while adding to HITMAP cursor");
    }

    if (rc == 0) {
        rc = VCursorOpen(self->cursor);
        DISP_RC(rc, "while opening HITMAP cursor");
    }
    DESTRUCT_PTR(VTable, table);
    if (rc == 0) {
        *tbl = self;
    }
    else {
        HitmapTblRelease(self);
    }
    return rc;
}
static rc_t HitmapWriterCallback(const bool* buf, size_t sz,
    void* data)
{
    rc_t rc = 0;
    HitmapTbl* db = (HitmapTbl*)data;
    assert(db);
    if (db->cursor == NULL) {
        return rc;
    }
    if (rc == 0) {
        rc = VCursorOpenRow(db->cursor);
        DISP_RC(rc, "while opening HITMAP row");
    }
    if (rc == 0) {
        bitsz_t elem_bits = 8 * sz;
        if (sz > elem_bits) {
            rc = RC(rcExe, rcCursor, rcWriting, rcBuffer, rcExhausted);
            DISP_RC(rc, "HITS");
        }
        else {
            rc = VCursorWrite(db->cursor, db->iHITS, elem_bits, buf, 0, 1);
            DISP_RC(rc, "HITS");
        }
    }
    if (rc == 0) {
        rc = VCursorCommitRow(db->cursor);
        DISP_RC(rc, "while committing HITMAP row");
    }
    if (rc == 0) {
        rc = VCursorCloseRow(db->cursor);
        DISP_RC(rc, "while closing HITMAP row");
    }
    return rc;
}

/*======================== RESULT PROCESSING: FACTORY ========================*/
typedef struct Stats {
    uint64_t n;
    const char* gi;
    uint64_t TOTAL_SEQ_LEN;
} Stats;
rc_t StatsHandler(Columns* cols, int64_t row, void* data) {
    Stats* c = (Stats*)data;
    assert(c);
    ++(c->n);
    return 0;
}
const Command* CreateCommands(Params* prm,
    Hitmap* hitmap, VCursor* varloc_cursor, Stats* stats)
{
    static Command cmdP;
    static Command cmdH;
    static Command cmd2;
    static Command cmdStats;

    Command* cmd = &cmdP;

    assert(prm);

    if (!prm->verbose) {
        cmd = &cmdH;
        if (prm->stats) {
            cmd = &cmdStats;
        }
    }

    CommandInit(&cmdP, PrintHandler, NULL, &cmdH);

    if (prm->stats) {
        assert(stats);
        cmdP.next = &cmdStats;
        CommandInit(&cmdStats, StatsHandler, stats, NULL);
    }
    else {
        assert(hitmap && varloc_cursor);
        CommandInit(&cmdH, HitmapAddHandler, hitmap, &cmd2);
        CommandInit(&cmd2, VDBSaveHandler, varloc_cursor, NULL);
    }

    return cmd;
}

/*======================== FETCH & PROCESS ========================*/

static rc_t ReadMssqlAndProcess(DBResultSet* rs, Columns* cols,
    const Command* commands, const Params* prm)
{
    rc_t rc = 0;
    int64_t iRow = 0;
    assert(rs && cols && prm);

    while (rc == 0) {
        int i = 0;
        DBRow* row = NULL;
        rc = Quitting();
        if (rc) {
            break;
        }
        rc = DBResultSetNextRow(rs, &row);
        if (rc == RC(rcRDBMS, rcData, rcRetrieving, rcData, rcNotAvailable)) {
            rc = 0;
            break;
        }
        DISP_RC(rc, "failed to fetch next row");

        if (iRow >= prm->start) {
            for (i = 0; i < cols->num && rc == 0; ++i) {
                Column* col = &cols->col[i];
                assert(col);
                switch (col->type) {
                    case eU8:
                        rc = DBRowGetAsU8(row, col->r_idx, &col->val.u8);
                        DISP_RC2(rc, col->r_name, "failed to read");
                        break;
                    case eI32:
                        rc = DBRowGetAsI32(row, col->r_idx, &col->val.i32);
                        DISP_RC2(rc, col->r_name, "failed to read");
                        if (rc == 0 && col->val.i32 < 0) {
                            char msg[256] = "invalid number";
                            rc = RC
                                (rcExe, rcTable, rcReading, rcData, rcInvalid);
                            PLOGERR(klogInt, (klogInt, rc,
                                "col[$(col)]: $(msg): $(val)",
                                "col=%s,msg=%s,val=%d",
                                col->r_name, msg, col->val.i32));
                        }
                        break;
                    case eU64:
                        rc = DBRowGetAsU64(row, col->r_idx, &col->val.u64);
                        DISP_RC2(rc, col->r_name, "failed to read");
                        break;
                    case eStr:
                        rc = DBRowGetAsString(row, col->r_idx, &col->val.s);
                        if (rc) {
                            if (GetRCObject(rc) == (enum RCObject)rcColumn &&
                                GetRCState(rc) == rcNull && col->allowNull)
                            {
                                static String empty;
                                StringInit(&empty, NULL, 0, 0);
                                col->val.s = &empty;
                                rc = 0;
                            }
                        }
                        DISP_RC2(rc, col->r_name, "failed to read");
                        break;
                    default:
                        assert(0);
                        break;
                }
            }
            if (rc == 0) {
                rc = CommandsRun(commands, cols, iRow);
            }
        }
        DESTRUCT_PTR(DBRow, row);
        ++iRow;
        if (prm->stop >=0 && iRow > prm->stop) {
            break;
        }
    }

    return rc;
}

/*======================== TEST varloc.vschema ========================*/

static rc_t writeTest() {
    rc_t rc = 0;

    VDBManager* mgr;
    VSchema* schema;
    VDatabase* db;
    VTable* tbl;
    VCursor* cursor;

    if ((rc = VDBManagerMakeUpdate(&mgr, NULL)))
        return rc;

    if ((rc = VDBManagerMakeSchema(mgr, &schema)))
        return rc;

    if ((rc = VSchemaParseFile(schema, "ncbi/varloc.vschema")))
        return rc;

    if ((rc = VDBManagerCreateDB(mgr, &db, schema,
            "NCBI:var:db:varloc", kcmOpen, "a")))
        return rc;

    if ((rc = VDatabaseCreateTable(db, &tbl, "VARLOC", kcmOpen, "VARLOC")))
        return rc;

    if ((rc = VTableCreateCursorWrite(tbl, &cursor, kcmInsert)))
        return rc;

    if ((rc = VCursorOpen(cursor)))
        return rc;

    {
        const char name[] = "VAR_ID";
        uint32_t idx;

        OUTMSG(("VCursorAddColumn(%s)...\n", name));

        rc = VCursorAddColumn(cursor, &idx, "%s", name);
        if (rc == 0)
            OUTMSG(("OK\n"));
        else
            LOGERR(klogInt, rc, "?");
    }

    return rc;
}
/*
static rc_t testRead() {
    rc_t rc = 0;

    uint32_t i;
    VDBManager* mgr;
    const VTable* tbl;
    const VCursor* cursor;

    if ((rc = VDBManagerMakeUpdate(&mgr, NULL)))
        return rc;

    if ((rc = VDBManagerOpenTableRead
            (mgr, &tbl, NULL, "NT_113947.1")))
        return rc;

    if ((rc = VTableCreateCursorRead(tbl, &cursor)))
        return rc;

    if ((rc = VCursorAddColumn(cursor, &i, "MAX_SEQ_LEN")))
        return rc;

    if ((rc = VCursorOpen(cursor)))
        return rc;

    return rc;
}
*/
#if 0
static rc_t TestWriterCallback(const char* buf, size_t sz) {
    int32_t i = 0;

    assert(buf);

    for (i = 0; i < sz; ++i) {
        char c = '0';

        if (buf[i]) {
            c = '1';
        }

        OUTMSG(("%c", c));
    }

    OUTMSG(("-"));

    return 0;
}
#endif

static rc_t TestSaverCallbackImpl(const bool* buf, size_t sz,
    const char** res, int32_t* res_sz)
{
    static char save[512];
    static int32_t offset = 0;

    int32_t i = 0;

    if (!buf && !res) {
        memset(save, 0, sizeof save);
        offset = 0;
        return 0;
    }

    if (buf && sz > 0) {
        for (i = 0; i < sz; ++i) {
            char c = '0';
            if (buf[i]) {
                c = '1';
            }

            assert(offset + i + 1 < sizeof save);

            save[offset++] = c;
        }

        save[offset++] = '-';
    }

    if (res) {
        *res = save;
    }

    if (res_sz) {
        *res_sz = offset;
    }

    return 0;
}
static rc_t TestSaverCallback(const bool* buf, size_t sz, void* data)
{   return TestSaverCallbackImpl(buf, sz, NULL, NULL); }

static rc_t implHMTest(rc_t prev_rc, const char* name, int64_t sz, int64_t len,
    WriterCallback writer, const char* expected, va_list args,
    rc_t exp_rc)
{
    rc_t rc = 0;
    int arg = 0;
    Hitmap* hitmap = NULL;

    assert(expected);

    TestSaverCallbackImpl(NULL, 0, NULL, NULL);
    rc = CreateHitmap(&hitmap, sz, len, writer, NULL);
    while ((arg = va_arg(args, int)) != -1 && rc == 0) {
        int from = arg;
        int to = va_arg(args, int);
        if (to == -1) {
            break;
        }
        rc = HitmapAdd(hitmap, from, to);
    }

    if (rc == 0) {
        rc = HitmapFlushLast(hitmap);
    }
    DESTRUCT_PTR(Hitmap, hitmap);

    {
        const char* res = NULL;
        int32_t sz = 0;
        rc_t rc2 = TestSaverCallbackImpl(NULL, 0, &res, &sz);
        if (rc == 0 && rc2 != 0) {
            rc = rc2;
        }
        OUTMSG(("\"%s\"", name));
        if (exp_rc == 0) {
            OUTMSG((" \"%.*s\"", sz, res));
            if (strlen(expected) == sz && memcmp(expected, res, sz) == 0) {
                OUTMSG((" +\n"));
            }
            else {
                OUTMSG((" (\"%s\" expected) FAILED\n", expected));
/*              rc = RC(rcExe, rcTest, rcExecuting, rcTest, rcFailed); */
                rc = RC(rcExe, rcString, rcComparing, rcData, rcInvalid);
            }
        }
        else {
            OUTMSG((" (rc=%d)", rc));
            if (exp_rc != rc) {
                OUTMSG((" (%d expected) FAILED\n", exp_rc));
            }
            else {
                OUTMSG((" +\n"));
                rc = 0;
            }
        }
    }

    return prev_rc != 0 ? prev_rc : rc;
}
static rc_t HMfTest(rc_t rc, const char* name,
    int64_t sz, int64_t len, WriterCallback writer, rc_t expected, ...)
{
    va_list args;
    va_start(args, expected);
    rc = implHMTest(rc, name, sz, len, writer, "", args, expected);
    va_end(args);
    return rc;
}
static rc_t HMTest(rc_t rc, const char* name,
    size_t sz, int64_t len, WriterCallback writer, const char* expected,
    ...)
{
    va_list args;
    va_start(args, expected);
    rc = implHMTest(rc, name, sz, len, writer, expected, args, 0);
    va_end(args);
    return rc;
}
static rc_t testHitmap() {
    rc_t rc = 0;
                               /* buffer_size, seq_len */
rc = HMfTest(rc,"failure interval>seq.len", 1, 2, TestSaverCallback,
    RC(rcExe, rcBuffer, rcAppending, rcParam, rcExcessive ), 0, 2, -1);
rc = HMfTest(rc,"failure reversed from-to", 1, 2, TestSaverCallback,
    RC(rcExe, rcBuffer, rcAppending, rcParam, rcOutoforder), 1, 0, -1);
rc = HMTest(rc, "overlapping intervals   ", 4, 7, TestSaverCallback,
                                                "0001-100-", 3, 4, -1);
rc = HMTest(rc, "2 buffers               ", 2, 3, TestSaverCallback, "00-0-",
                                                                   -1);
rc = HMTest(rc, "small sequence          ", 5, 3, TestSaverCallback, "000-" ,
                                                                   -1);
rc = HMTest(rc, "size == length          ", 4, 4, TestSaverCallback, "0000-",
                                                                   -1);
rc = HMTest(rc, "small seque., 1 interval", 5, 3, TestSaverCallback, "010-",
                                                             1, 1, -1);
rc = HMTest(rc, "small sequence, last int", 5, 3, TestSaverCallback, "001-",
                                                             2, 2, -1);
rc = HMTest(rc, "sz=len, last interval   ", 5, 5, TestSaverCallback, "00001-",
                                                             4, 4, -1);
rc = HMTest(rc, "2 buffers, 2 intervals  ", 4, 6, TestSaverCallback, "1000-01-",
                                                             0, 0, 5, 5, -1);
rc = HMTest(rc, "huge int,next from-small", 3, 9, TestSaverCallback,
                                             "011-111-111-", 1, 8, 1, 8, -1);

    return rc;
}
typedef struct Elm {
    char* data;
    struct Elm* next;
} Elm;
static Elm* ElmFree(Elm* self) {
    if (self == NULL) {
        return NULL;
    }
    free(self->data);
    self->data = NULL;
    return self->next;
}
static void ElmListFree(Elm* elm) {
    while (elm) {
        elm = ElmFree(elm);
    }
}
static rc_t ElmCreate(Elm** elm, const char* s) {
    rc_t rc = 0;
    Elm* e = NULL;
    assert(elm && s);
    e = calloc(1, sizeof *e);
    if (e == NULL) {
        return RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
    }
    else {
        e->data = strdup(s);
        if (e->data == NULL) {
            ElmFree(e);
            return RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
        }
        *elm = e;
    }
    return rc;
}
typedef rc_t ReaderCallback(const void* self, uint64_t pos,
    void* buffer, size_t bsize, size_t* num_read);
typedef struct TestFile {
    char* buf;
    size_t blen;
    size_t read;
} TestFile;
static rc_t TestFileRelease(TestFile* self) {
    if (self == NULL) {
        return 0;
    }
    free(self->buf);
    self->buf = NULL;
    free(self);
    return 0;
}
static rc_t CreateTestFile(TestFile** file, const char* data) {
    rc_t rc = 0;
    TestFile* self = NULL;
    assert(file && data);
    self = calloc(1, sizeof *self);
    if (self == NULL) {
        return RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
    }
    self->buf = strdup(data);
    if (self->buf == NULL) {
        rc = RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
    }
    else {
        self->blen = strlen(self->buf);
    }
    if (rc == 0) {
        *file = self;
    }
    else {
        TestFileRelease(self);
    }
    return rc;
}
static rc_t ReadLine(ReaderCallback read, char** l, const void* f) {
    rc_t rc = 0;
    static char* buf = NULL;
    static size_t blen = 0;
    static size_t in_buf = 0;
    static uint64_t pos = 0;
    assert(read && l && f);
    if (buf == NULL) {
        ++blen;
        buf = malloc(blen);
        if (buf == NULL) {
            return RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
        }
    }
    while (rc == 0) {
        size_t sz = 0;
        bool eof = false;
        const char *eol = memchr(buf, '\n', in_buf);
        if (eol == NULL) {
            size_t num_read = 0;
            size_t to_read = blen - in_buf;
            assert(blen >= in_buf);
            if (to_read == 0) {
                void* tmp = NULL;
                assert(blen);
                blen *= 2;
                tmp = realloc(buf, blen);
                if (tmp == NULL) {
                    return
                        RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
                }
                buf = tmp;
                to_read = blen - in_buf;
            }
            rc = read(f, pos, buf + in_buf, to_read, &num_read);
            if (rc == 0) {
                pos += num_read;
                in_buf += num_read;
                assert(in_buf <= blen);
                if (num_read == 0) {                
                    eof = true;
                    sz = in_buf;
                }
            }
            else {
                return rc;
            }
        }
        else {
            sz = eol - buf;
        }
        if (sz) {
            *l = malloc(sz + 1);
            if (*l == NULL) {
                return RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
            }
            memcpy(*l, buf, sz);
            (*l)[sz] = '\0';
            if (eof) {
                in_buf = 0;
            }
            else {
                size_t n = in_buf - sz - 1;
                assert(in_buf - sz - 1 >= 0);
                memmove(buf, eol + 1, n);
                in_buf -= sz + 1;
            }
            return 0;
        }
        else if (eof) {
            *l = NULL;
            return 0;
        }
    }
    return rc;
}
static rc_t KFileReaderCallback(const void* self, uint64_t pos,
    void* buffer, size_t bsize, size_t* num_read)
{   return KFileRead((const KFile*)self, pos, buffer, bsize, num_read); }
static rc_t TestReaderCallback(const void* data, uint64_t pos,
    void* buffer, size_t bsize, size_t* num_read)
{
    size_t to_read = 0;
    TestFile* self = (TestFile*)data;
    assert(self && buffer && bsize && num_read);
    if (self->read >= self->blen) {
        *num_read = 0;
        return 0;
    }
    to_read = self->blen - self->read;
    if (to_read > bsize) {
        to_read = bsize;
    }
    memcpy(buffer, self->buf + self->read, to_read);
    *num_read = to_read;
    self->read += to_read;
    return 0;
}
typedef struct TestData {
    Elm* head;
    const Elm* next;
} TestData;
static void TestDataFree(TestData* self) {
    if (self == NULL) {
        return;
    }
    ElmListFree(self->head);
    self->next = self->head = NULL;
}
static rc_t TestDataCreate(TestData** res) {
    rc_t rc = 0;
    const KFile* std_in = NULL;
    Elm* tail = NULL;
    TestData* data = NULL;
    assert(res);
    *res = NULL;
    rc = KFileMakeStdIn(&std_in);
    DISP_RC(rc, "KFileMakeStdIn");
    if (rc) {
        return rc;
    }
    data = calloc(1, sizeof *data);
    if (data == NULL) {
        return RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
    }
    while (rc == 0) {
        char* l = NULL;
        rc = ReadLine(KFileReaderCallback, &l, std_in);
        if (rc) {
            break;
        }
        else if (l == NULL) {
            break;
        }
        else {
            Elm* e = NULL;
            rc = ElmCreate(&e, l);
            if (rc == 0) {
                if (tail) {
                    assert(data->head);
                    tail->next = e;
                    tail = e;
                }
                else {
                    assert(data && data->head == NULL);
                    data->head = tail = e;
                }
            }
        }
    }
    if (rc == 0) {
        *res = data;
    }
    return rc;
}
static rc_t testStdinRead() {
    rc_t rc = 0;
    const KFile* std_in = NULL;
    rc = KFileMakeStdIn(&std_in);
    DISP_RC(rc, "KFileMakeStdIn");
    while (rc == 0) {
        char* l = NULL;
        rc = ReadLine(KFileReaderCallback, &l, std_in);
        if (rc) {
            break;
        }
        else if (l == NULL) {
            break;
        }
        else {
            OUTMSG(("%s\n", l));
        }
    }
    return rc;
}
static rc_t TestRead(Elm** list) {
    int i = 0;
    Elm* head = NULL;
    Elm* tail = NULL;
/* TESTDATA */
    const char data[] =
            "refseq\n"
            "MAX_SEQ_LEN TOTAL_SEQ_LEN SEQ_ID\n"
            "250000000    14748365    gi|18446744073709551615|\n"
            "VarLoc\n"
 "gi pos_from pos_to entrez_id parent_var_id score var_id var_source var_type\n"
 "18446744073709551615 0  14748364 147483648 12345678980000000005a 2147483647 abcdefghij04294967295 254      255       \n"
/*
 "18446744073709551615 6          9  7 pvid   13   vid001 14         15      \n"
 "18446744073709551615 9 2147483647 12 012pvi 16    123    17         18      \n"
*/
#if 0
 "pos_from pos_to parent_var_id var_id\n"
 
 "6        9      012p          a\n"
 "9        10     bv            c\n"
 /*
 "6        9      pvid          vid001\n"
 "9        10     012pvi        123   \n"
 */
#endif
        ;
    TestFile* f = NULL;
    rc_t rc = CreateTestFile(&f, data);
    if (rc != 0) {
        return rc;
    }
    assert(list);
    for (i = 0; rc == 0; ++i) {
        char* l = NULL;
        rc = ReadLine(TestReaderCallback, &l, f);
        if (rc) {
            break;
        }
        else {
          /*const char* e = NULL;*/
            if (l) {
                Elm* e = NULL;
                rc = ElmCreate(&e, l);
                if (rc == 0) {
                    if (tail) {
                        assert(head);
                        tail->next = e;
                        tail = e;
                    }
                    else {
                        assert(head == NULL);
                        head = tail = e;
                    }
                }
            }
#if 0
            if (rc == 0) {
                switch (i) {
                    case 0:
                        e = "refseq";
                        break;
                    case 1:
                        e = "MAX_SEQ_LEN TOTAL_SEQ_LEN SEQ_ID";
                        break;
                    case 2:
                        e = "1           11            gi|2|";
                        break;
                    case 3:
                        e = "VarLoc";
                        break;
                    case 4:
                        e =
  "gi pos_from pos_to entrez_id parent_var_id score var_id var_source var_type";
                        break;
                    case 7:
                        e =
  "2  3        4      5         NULL          9     vid123 11         8       ";
                        break;
                    case 5:
                        e =
  "2  6        9      7         pvid          13    vid001 14         15      ";
                        break;
                    case 6:
                        e =
  "2  9        10     12        012pvi        16    123    17         18      ";
                        break;
                    case 8:
                        break;
                    default: assert(0); break;
                }
                if (i == 7) {
                    assert(l == NULL);
                    break;
                }
                else {
                    assert(!strcmp(l, e));
                }
            }
#endif
            if (l == NULL) {
                break;
            }
        }
    }
    *list = head;
    DESTRUCT_PTR(TestFile, f);
    return rc;
}
typedef struct TestColumn {
    const char* name;
    const char* val;
} TestColumn;
static char* skipWhitesOrBlacks(char* p, bool white) {
    assert(p);
    while (p) {
        bool cond = isspace(*p);
        if (white) {
            cond = !cond;
        }
        if (cond) {
            return p;
        }
        ++p;
        if (*p == '\0') {
            return p;
        }
    }
    return p;
}
static char* skipWhites(char* p) { return skipWhitesOrBlacks(p, true ); }
static char* skipBlacks(char* p) { return skipWhitesOrBlacks(p, false); }
static void ExtractGiTest(Refseq* refseq, const char* s) {
    assert(refseq);
    if (ExtractGi(s, strlen(s), refseq)
        != RC(rcExe, rcId, rcParsing, rcId, rcTooShort))
    {   assert(0); }
}
static rc_t ExtractGiTests() {
    Refseq refseq;
    RefseqReset(&refseq);
    ExtractGiTest(&refseq, "");
    ExtractGiTest(&refseq, "g");
    ExtractGiTest(&refseq, "gi");
    ExtractGiTest(&refseq, "gi|");
    ExtractGiTest(&refseq, "gi||");
    {
        const char s[] = "gi|9|";
        rc_t rc = ExtractGi(s, strlen(s), &refseq);
        if (rc <= 0 || rc > 0)
        {   assert(rc == 0 && refseq.gi[0] == '9' && refseq.gi[1] == '\0'); }
    }
    return 0;
}
static void PrepareTestCols(TestData* data, Columns* cols) {
    const Elm* eCols = NULL;
    uint32_t i = 0;
    char* p = NULL;
    assert(data && cols);
    eCols = data->next;
    assert(eCols && eCols->data);
    for (p = eCols->data, i = 0; *p; ++i) {
        int j = 0;
        p = skipWhites(p);
        for (j = 0; j < cols->num; ++j) {
            Column* col = &cols->col[j];
            const char* name = NULL;
            assert(col);
            name = col->r_name;
            if (strncmp(p, name, strlen(name)) == 0) {
                col->r_idx = i;
                break;
            }
        }
        p = skipBlacks(p);
    }
    data->next = eCols->next;
}
static rc_t ReadTestAndProcess(const TestData* data,
    Columns* cols, const Command* commands, const Refseq* refseq)
{
    rc_t rc = 0;
    int64_t iRow = 0;
    char* p = NULL;
    uint32_t i = 0;
    const Elm* eNext = NULL;

    const Elm* eColsData = NULL;

    assert(data && cols && refseq);

    eColsData = data->next;

    eNext = eColsData;
    assert(cols->iFrom >= 0 && cols->iTo >= 0 && cols->iFrom != cols->iTo
        && cols->col);
    for (eNext = eColsData; eNext && rc == 0; eNext = eNext->next) {
        for (p = eNext->data, i = 0 && rc == 0; *p; ++i) {
            const char* val = NULL;
            int j = 0;
            p = skipWhites(p);
            for (j = 0; j < cols->num; ++j) {
                Column* col = &cols->col[j];
                assert(col);
                if (col->r_idx == i) {
                    val = p;
                    break;
                }
            }
            p = skipBlacks(p);
            if (*p) {
                *p = '\0';
                ++p;
            }
            if (val) {
                char* end = NULL;
                Column* col = &cols->col[j];
                assert(col);
                errno = 0;
                switch (col->type) {
                    case eU8:
                        col->val.u8 = atoi(val);
                        break;
                    case eI32:
                        col->val.i32 = strtoi32(val, &end, 10);
                        if (errno != 0 || val == end || *end != '\0' ||
                            col->val.i32 < 0)
                        {
                            char msg[256] = "invalid number";
                            char* error = NULL;
                            if (errno && ! string_printf(msg, sizeof msg, NULL, "%!", errno) ) 
                                error = msg;                                
                            rc = RC
                                (rcExe, rcTable, rcReading, rcData, rcInvalid);
                            PLOGERR(klogInt, (klogInt, rc,
                                "col[$(col)]: $(msg): $(val)",
                                "col=%s,msg=%s,val=%s",
                                col->r_name, error ? error : msg, val));
                        }
                        break;
                    case eU64:
                        col->val.u64 = strtou64(val, &end, 10);
                        if (errno != 0 || val == end || *end != '\0') {
                            char msg[256] = "Invalid number";
                            char* error = NULL;
                            if (errno && ! string_printf(msg, sizeof msg, NULL, "%!", errno) ) 
                                error = msg;                                
                            rc = RC
                                (rcExe, rcTable, rcReading, rcData, rcInvalid);
                            PLOGERR(klogInt, (klogInt, rc,
                                "col[$(col)]$(msg): $(val)",
                                "col=%s,msg=%s,val=%s",
                                col->r_name, error ? error : msg, val));
                        }
                        break;
                    case eStr: {
                        String* s = NULL;
                        free((void*)col->val.s);
                        col->val.s = NULL;
                        s = calloc(1, sizeof *s);
                        if (s == NULL) {
                            rc = RC(rcExe,
                                rcStorage, rcAllocating, rcMemory, rcExhausted);
                        }
                        else if (strcmp(val, "NULL") == 0) {
                            StringInit(s, NULL, 0, 0);
                            col->val.s = s;
                        }
                        else {
                            StringInit(s, val, strlen(val), strlen(val));
                            col->val.s = s;
                        }
                        break;
                    }
                    default: assert(0); break;
                }
            }
        }
        if (rc == 0) {
            rc = CommandsRun(commands, cols, iRow);
        }
        ++iRow;
    }
    return rc;
}
static rc_t ReadRefseqEtc(rc_t rc, TestData* in, Refseq* refseq) {
    char* p = NULL;
    uint64_t num = 0;
    uint64_t i = 0;
    TestColumn* refseq_cols = NULL;
    const Elm* eRefseqCols = NULL;
    const Elm* eRefseqColsData = NULL;
    assert(in && refseq);
    RefseqReset(refseq);
    if (rc) {
        return rc;
    }
    assert(in->head);
    in->next = in->head;
    assert(in->next && !strcmp(in->next->data, "refseq"));
    eRefseqCols = in->next->next;
    assert(eRefseqCols && eRefseqCols->data);
    eRefseqColsData = eRefseqCols->next;
    assert(eRefseqColsData && eRefseqColsData->data);
    in->next = eRefseqColsData->next;
    assert(in->next && !strcmp(in->next->data, "VarLoc"));
    in->next = in->next->next;
    for (p = eRefseqCols->data, num = 0; *p; ++num) {
        p = skipWhites(p);
        p = skipBlacks(p);
    }
    refseq_cols = calloc(num, sizeof *refseq_cols);
    if (refseq_cols == NULL) {
        return RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
    }
    for (p = eRefseqCols->data, num = 0; *p; ++num) {
        p = skipWhites(p);
        p = skipBlacks(p);
    }
    for (p = eRefseqCols->data, i = 0; i < num; ++i) {
        TestColumn* col = &refseq_cols[i];
        assert(col);
        p = skipWhites(p);
        col->name = p;
        p = skipBlacks(p);
        if (*p) {
            *p = '\0';
            ++p;
        }
    }
    for (p = eRefseqColsData->data, i = 0; i < num; ++i) {
        TestColumn* col = &refseq_cols[i];
        p = skipWhites(p);
        col->val = p;
        p = skipBlacks(p);
        if (*p) {
            *p = '\0';
            ++p;
        }
    }
    for (i = 0; rc == 0 && i < num; ++i) {
        bool testErrNo = true;
        const char* val = refseq_cols[i].val;
        char* end = NULL;
        errno = 0;
        if (strcmp(refseq_cols[i].name, "MAX_SEQ_LEN") == 0) {
            refseq->MAX_SEQ_LEN = strtou64(val, &end, 10);
        }
        else if (strcmp(refseq_cols[i].name, "SEQ_ID") == 0) {
            testErrNo = false;
            rc = ExtractGi(val, strlen(val), refseq);
        }
        else if (strcmp(refseq_cols[i].name, "TOTAL_SEQ_LEN") == 0) {
            refseq->TOTAL_SEQ_LEN = strtou64(val, &end, 10);
        }
        if (testErrNo && (errno != 0 || val == end || *end != '\0')) {
            char msg[256] = "Invalid number";
            char* error = NULL;
            if (errno && ! string_printf(msg, sizeof msg, NULL, "%!", errno) ) 
                error = msg;                                
            rc = RC
                (rcExe, rcString, rcComparing, rcData, rcInvalid);
            PLOGERR(klogInt, (klogInt, rc,
                "$(msg): $(val)", "msg=%s,val=%s",
                error ? error : msg, val));
        }
    }
    return rc;
}
static rc_t testTestReader() {
    rc_t rc = 0;
    TestData* data = NULL;
    const Elm* eVarlocCols = NULL;
    Elm* eVarlocColsData = NULL;
    Refseq refseq;
    Columns* cols = GiveColumns();
    rc = ExtractGiTests();
    assert(rc == 0);
    data = calloc(1, sizeof *data);
    if (data == NULL) {
        return RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
    }
    rc = TestRead(&data->head);
    rc = ReadRefseqEtc(rc, data, &refseq);
    eVarlocCols = data->next;
    assert(eVarlocCols && eVarlocCols->data);
    eVarlocColsData = eVarlocCols->next;
    assert(eVarlocColsData && eVarlocColsData->data);
    if (rc == 0) {
        Command cmd;
        CommandInit(&cmd, PrintHandler, NULL, NULL);
        rc = ReadTestAndProcess(data, cols, &cmd, &refseq);
    }
    TestDataFree(data);
    return rc;
}
#if 0
static rc_t StdinReader() {
    rc_t rc = 0;
    const KFile* std_in = NULL;
    uint64_t pos = 0;
    char buffer[1024];
    rc = KFileMakeStdIn(&std_in);
    DISP_RC(rc, "KFileMakeStdIn");
    while (rc == 0) {
        size_t num_read = 0;
        rc = KFileRead(std_in, pos, buffer, sizeof buffer, &num_read);
        if (rc == 0 && num_read == 0) {
            break;
        }
    }
    DESTRUCT_PTR(KFile, std_in);
    return rc;
}
static rc_t StdinTest() {
    return StdinReader();
}
#endif
/*======================== MAIN ========================*/

rc_t CC KMain(int argc, char* argv[])
{
    rc_t rc = 0;
    TestData* testData = NULL;
    VDBManager* mgr = NULL;
    Columns* cols = GiveColumns();

    Params prm;
    RDB* mssql = NULL; /* RDB(MsSQL) connection object */
    VDB* vdb = NULL;   /* VDB Database connection object */

    if (false)
        return testStdinRead();
    if (false)
        return testTestReader();
    rc = ParamsConstruct(rc, argc, argv, &prm);
    if (rc == 0 && prm.empty) {
        DESTRUCT(Params, prm);
        exit(EXIT_FAILURE);
    }

    if (prm.testHitmap)
        return testHitmap();
/*  if (0)
        return testRead(); */
    if (0)
        return writeTest();

    if (rc == 0) {
        rc = CreateRDB(&mssql);
    }

    if (rc == 0) {
        rc = VDBManagerMakeUpdate(&mgr, NULL);
        DISP_RC(rc, "while calling VDBManagerMakeUpdate");
    }

    if (rc == 0) {
        if (prm.testStdin) { /* is turned on by "-test stdin" */
            if (false) rc = TestDataCreate(&testData);
            else {
                /* there is a possibility to feed input data
                (normally read from MsSQL/VDB) from stdin.
                This feature was completed (almost?) but never used
                as script to generate the data was never created */
                testData = calloc(1, sizeof *testData);
                rc = TestRead(&testData->head);
            }
             /* Parse Refseq read from stdin */
            rc = ReadRefseqEtc(rc, testData, &prm.refseq);
        }
        else { /* Read Refseq from VDB database */
            rc = RefseqRead(rc, mgr, &prm);
        }
    }
    if (rc == 0) { /* Prepare Varloc input: */
        if (prm.testStdin) { /* test case: to be read from stdin */
            PrepareTestCols(testData, cols);
        }
        else { /* from MsSQL */
            char buf[1025] = "";
            PString sql;
            PStringInit(&sql, buf, sizeof buf);
            rc = BuildQuery(&sql, cols, &prm);
            if (rc == 0) {
                rc = RDBExecute(mssql, sql.data);
            }
        }
    }

    if (rc == 0 && !prm.stats) {
        /* Create output DB */
        rc = CreateVDB(&vdb, mgr, prm.path);
    }
    if (rc == 0) {
        Stats stats;
        Hitmap* hitmap = NULL;
        VCursor* varloc_cursor = NULL;
        HitmapTbl* hitmap_tbl = NULL;
        const Command* cmd = NULL;
        memset(&stats, 0, sizeof stats);
        if (rc == 0) {
            stats.gi = prm.refseq.gi;
            stats.TOTAL_SEQ_LEN = prm.refseq.TOTAL_SEQ_LEN;
            if (!prm.stats) {
                rc = CreateVarlocCursor(&varloc_cursor, vdb, cols);
                if (rc == 0) {
                    rc = CreateHitmapTbl(&hitmap_tbl,
                        vdb->db, prm.refseq.MAX_SEQ_LEN);
                }
            }
        }
        if (rc == 0) {
            rc = CreateHitmap(&hitmap, prm.refseq.MAX_SEQ_LEN,
                prm.refseq.TOTAL_SEQ_LEN, HitmapWriterCallback, hitmap_tbl);
        }
        if (rc == 0) {
            cmd = CreateCommands(&prm, hitmap, varloc_cursor, &stats);
            if (prm.testStdin) {
                rc = ReadTestAndProcess(testData, cols, cmd, &prm.refseq);
            }
            else {
                rc = ReadMssqlAndProcess(mssql->rs, cols, cmd, &prm);
            }
        }
        if (rc == 0 && prm.stats) {
            OUTMSG(("accession: %s\tgi: %s\thits: %lu\ttotal_seq_len: %lu\n",
                prm.acc, stats.gi, stats.n, stats.TOTAL_SEQ_LEN));
        }

        if (rc == 0) {
            rc = HitmapFlushLast(hitmap);
        }
        DESTRUCT_PTR(Hitmap, hitmap);

        if (rc == 0) {
            rc = HitmapTblCommit(hitmap_tbl);
        }
        DESTRUCT_PTR(HitmapTbl, hitmap_tbl);

        if (rc == 0) {
            rc = VarlocCursorCommit(varloc_cursor);
        }
        if (rc == 0) {
            rc = VDBWriteLoaderMeta(vdb, argv[0]);
        }
        DESTRUCT_PTR(VCursor, varloc_cursor);
    }

    TestDataFree(testData);

    DESTRUCT_PTR(VDBManager, mgr);
    DESTRUCT_PTR(RDB, mssql);
    DESTRUCT_PTR(VDB, vdb);
    DESTRUCT(Params, prm);

    return rc;
}

/*=================================== EOF ====================================*/
