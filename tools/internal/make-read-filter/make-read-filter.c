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

/* Synopsis: Updates read filter based on quality scores
 * Purpose: In preparation for removing quality scores
 * Example usage:
 *  make-read-filter --temp /tmp SRR123456
 */

#include <sys/stat.h> /* stat */

#include "make-read-filter.h" /* contains mostly boilerplate code */

/* NOTE: Needs to be in same order as Options array */
enum OPTIONS {
    OPT_OUTPUT,     /* temp path */
    OPT_CACHE,      /* vdbcache path */
    OPTIONS_COUNT
};

/* NOTE: Rules for filtering
    Quote:
        Reads that have more than half of quality score values <20 will be
        flagged ‘reject’.
        Reads that begin or end with a run of more than 10 quality scores <20
        are also flagged ‘reject’.
 */
typedef enum FilterReason {
    keep = 0,
    low_quality_count = 1,
    low_quality_back = 2,
    low_quality_front = 4,
    original_filter = 8,
} FilterReason;

/** @brief Apply the rules to determine if a read should be filtered
 **/
static FilterReason shouldFilter(uint32_t const len, uint8_t const *const qual)
{
    uint32_t under = 0;
    uint32_t lastgood = len;
    uint32_t i;
    FilterReason reason = keep;

    for (i = 0; i < len; ++i) {
        int const qval = qual[i];
        if (qval < 20)
            ++under;
        else
            lastgood = i; /* last good value */
    }
    if (under * 2 > len)
        reason |= low_quality_count;
    if (len <= 10) /* if length <= 10, then rest of rules can't apply */
        return reason;

    if (lastgood < len - 11)
        reason |= low_quality_back;

    for (i = 0; i < len; ++i) {
        int const qval = qual[i];
        if (qval >= 20)
            break;
        if (i == 10) {
            reason |= low_quality_front;
            break;
        }
    }
    return reason;
}

uint64_t dispositionCount[6];
uint64_t dispositionBaseCount[6];
static void updateCounts(FilterReason const reason, uint32_t const length)
{
    if (reason == keep) {
        dispositionCount[0] += 1;
        dispositionBaseCount[0] += length;
    }
    else {
        dispositionCount[1] += 1;
        dispositionBaseCount[1] += length;
    }
    if ((reason & original_filter) != 0) {
        dispositionCount[2] += 1;
        dispositionBaseCount[2] += length;
    }
    if ((reason & low_quality_count) != 0) {
        dispositionCount[3] += 1;
        dispositionBaseCount[3] += length;
    }
    if ((reason & low_quality_front) != 0) {
        dispositionCount[4] += 1;
        dispositionBaseCount[4] += length;
    }
    if ((reason & low_quality_back) != 0) {
        dispositionCount[5] += 1;
        dispositionBaseCount[5] += length;
    }
}

static void computeReadFilter(uint8_t *const out_filter
                             , CellData const *const filterData
                             , CellData const *const typeData
                             , CellData const *const startData
                             , CellData const *const lenData
                             , CellData const *const qualData
                             , int64_t const row)
{
    int const nreads = filterData->count;
    uint8_t const *const filter = filterData->data;
    uint8_t const *const type = typeData->data;
    int32_t const *const start = startData->data;
    uint32_t const *const len = lenData->data;
    uint8_t const *const qual = qualData->data;
    int i;

    assert(nreads == typeData->count);
    assert(nreads == startData->count);
    assert(nreads == lenData->count);
    if (nreads == 0)
        return;
    if (qualData->count != start[nreads - 1] + len[nreads - 1]) {
        pLogErr(klogErr, RC(rcExe, rcFile, rcReading, rcData, rcInvalid), "invalid length of QUALITY ($(actual)), should be $(expect) in row $(row)", "row=%ld,actual=%u,expect=%u", row, (unsigned)qualData->count, (unsigned)(start[nreads - 1] + len[nreads - 1]));
        exit(EX_DATAERR);
    }

    for (i = 0; i < nreads; ++i) {
        uint8_t filt = filter[i];

        if ((type[i] & SRA_READ_TYPE_BIOLOGICAL) == SRA_READ_TYPE_BIOLOGICAL) {
            FilterReason reason = original_filter;

            if (filt == SRA_READ_FILTER_PASS) {
                reason = shouldFilter(len[i], qual + start[i]);
                if (reason != keep)
                    filt = SRA_READ_FILTER_REJECT;
            }
            updateCounts(reason, len[i]);
        }
        out_filter[i] = filt;
    }
}

static bool didReadFilterChange(unsigned const n, uint8_t const *const out, uint8_t const *const in)
{
    int i;

    for (i = 0; i < n; ++i) {
        if (out[i] != in[i])
            return true;
    }
    return false;
}

int invalidated = -1;
#include <fcntl.h>
static void open_invalidated()
{
    invalidated = open("invalidedRows", O_RDWR | O_APPEND | O_CREAT | O_EXCL, 0700);
    if (invalidated >= 0)
        return;
    LogMsg(klogFatal, "Can't create file for invalidated rows");
    exit(EX_CANTCREAT);
}

static void invalidateRow(int64_t const row)
{
    if (invalidated >= 0) {
        if (write(invalidated, &row, sizeof(row)) == sizeof(row))
            return;
        LogMsg(klogFatal, "Can't record invalidated rows");
        exit(EX_IOERR);
    }
    open_invalidated();
    invalidateRow(row);
}

static void processCursors(VCursor *const out, VCursor const *const in, bool const haveCache)
{
    /* MARK: input columns */
    uint32_t const cid_pr_id       = haveCache ? addColumn("PRIMARY_ALIGNMENT_ID", "I64" , in) : 0;
    uint32_t const cid_read_filter = addColumn("READ_FILTER", "U8" , in);
    uint32_t const cid_readstart   = addColumn("READ_START" , "I32", in);
    uint32_t const cid_read_type   = addColumn("READ_TYPE"  , "U8" , in);
    uint32_t const cid_readlen     = addColumn("READ_LEN"   , "U32", in);
    uint32_t const cid_qual        = addColumn("QUALITY"    , "U8" , in);

    /* MARK: output column */
    uint32_t const cid_rd_filter = addColumn("READ_FILTER", "U8", out);

    int64_t first = 0;
    uint64_t count = 0;

    uint64_t r = 0;
    size_t out_filter_count = 0;
    uint8_t *out_filter = NULL;

    openCursor(in, "input");
    openCursor(out, "output");

    count = rowCount(in, &first, cid_qual);
    assert(first == 1);
    pLogMsg(klogInfo, "progress: about to process $(rows) rows", "rows=%lu", count);

    /* MARK: Main loop over the input */
    for (r = 0; r < count; ++r) {
        int64_t const row = 1 + r;
        CellData const readfilter = cellData("READ_FILTER", cid_read_filter, row, in);
        CellData const readstart  = cellData("READ_START" , cid_readstart  , row, in);
        CellData const readtype   = cellData("READ_TYPE"  , cid_read_type  , row, in);
        CellData const readlen    = cellData("READ_LEN"   , cid_readlen    , row, in);
        CellData const quality    = cellData("QUALITY"    , cid_qual       , row, in);
#if 0
        if ((row & 0xFFFF) == 0) {
            pLogMsg(klogInfo, "progress: $(row) rows", "row=%li", row);
        }
#endif
        out_filter = growFilterBuffer(out_filter, &out_filter_count, readfilter.count);
        computeReadFilter(out_filter, &readfilter, &readtype, &readstart, &readlen, &quality, row);
        if (haveCache && didReadFilterChange(readfilter.count, out_filter, readfilter.data)) {
            CellData const pridData = cellData("PRIMARY_ALIGNMENT_ID", cid_pr_id, row, in);
            int64_t const *prid = pridData.data;
            size_t i;
            for (i = 0; i < pridData.count; ++i) {
                invalidateRow(prid[i]);
            }
        }
        openRow(row, out);
        writeRow(row, readfilter.count, out_filter, cid_rd_filter, out);
        commitRow(row, out);
        closeRow(row, out);
    }
    LogMsg(klogInfo, "progress: done");
    commitCursor(out);
    free(out_filter);
    VCursorRelease(out);
    VCursorRelease(in);
}

static void copyColumn(char const *const column, char const *const table, char const *const source, char const *const dest, VDBManager *const mgr)
{
    VTable *tbl = table ? openUpdateDb(dest, table, mgr) : openUpdateTbl(dest, mgr);

    dropColumn(tbl, column);
    {
        rc_t rc = 0;
        {
            char const *const fmt = "%s/tbl/%s/col";
            KDirectory const *const src = table ? openDirRead(fmt, source, table) : openDirRead(fmt + 7, source);
            KDirectory *const dst = table ? openDirUpdate(fmt, dest, table) : openDirUpdate(fmt + 7, dest);

            rc = KDirectoryCopy(src, dst, true, column, column);
            KDirectoryRelease(dst); KDirectoryRelease(src);
        }
        if (rc) {
            /* could not copy the physical column; try the metadata node */
            VTable const *const tmp = table ? openReadDb(source, table, mgr) : openReadTbl(source, mgr);

            if (VTableHasStaticColumn(tmp, column))
                copyNodeValue(openNodeUpdate(tbl, "col/%s", column), openNodeRead(tmp, "col/%s", column));
            else {
                pLogMsg(klogFatal, "can't copy replacement $(column) column", "column=%s", column);
                exit(EX_DATAERR);
            }
            VTableRelease(tmp);
        }
    }
    VTableRelease(tbl);
}

static void saveCounts(char const *const table, char const *const dest, VDBManager *const mgr)
{
    VTable *tbl = table ? openUpdateDb(dest, table, mgr) : openUpdateTbl(dest, mgr);
    KMDataNode *const stats = openNodeUpdate(tbl, "%s", "STATS");

    /* rename STATS/QUALITY to STATS/ORIGINAL_QUALITY */
    {
        rc_t const rc = KMDataNodeRenameChild(stats, "QUALITY", "ORIGINAL_QUALITY");
        if (rc) {
            LogErr(klogInfo, rc, "can't rename QUALITY stats");
        }
    }
    /* write new STATS/QUALITY */
    {
        KMDataNode *const node = openChildNodeUpdate(stats, "QUALITY");
        writeChildNode(node, "PHRED_3", sizeof(dispositionBaseCount[1]), &dispositionBaseCount[1]);
        writeChildNode(node, "PHRED_30", sizeof(dispositionBaseCount[0]), &dispositionBaseCount[0]);
        KMDataNodeRelease(node);
    }
    /* record stats about the other changes made */
    {
        KMDataNode *node = openNodeUpdate(tbl, "READ_FILTER_CHANGES");
        writeChildNode(node, "FILTERED_READS", sizeof(dispositionCount[1]), &dispositionCount[1]);
#define writeCounts(NAME, N) \
    writeChildNode(node, NAME "_BASES", sizeof(dispositionBaseCount[N]), &dispositionBaseCount[N]); \
    writeChildNode(node, NAME "_READS", sizeof(dispositionCount[N]), &dispositionCount[N]);

        writeCounts("ORIGINAL_FILTERED", 2);
        writeCounts("TOTAL_LOW_QUALITY", 3);
        writeCounts("FRONT_LOW_QUALITY", 4);
        writeCounts("BACK_LOW_QUALITY", 5);

#undef writeCounts
        KMDataNodeRelease(node);
    }
    KMDataNodeRelease(stats);
    VTableRelease(tbl);
}

static void updateCacheColumn1(  char const *const type
                               , char const *const name
                               , VCursor *const out
                               , VCursor const *const in
                               , VCursor const *const gate
                               , size_t const changedCount
                               , int64_t const *const changedRows)
{
    uint32_t const cid_gate = addColumn(name, type, gate);
    uint32_t const cid_out = addColumn(name, type, out);
    uint32_t const cid_in = addColumn2(strlen(name) - 6, name, type, in);

    int64_t first = 0;
    uint64_t count = 0;
    uint64_t r;
    size_t i = 0;

    uint64_t not_empty = 0;
    uint64_t invalidate = 0;

    openCursor(in, "input");
    openCursor(out, "output");
    openCursor(gate, "vdbcache");

    count = rowCount(gate, &first, cid_gate);
    pLogMsg(klogInfo, "progress: about to process $(rows) rows for $(col)", "rows=%lu,col=%s", count, name);

    for (r = 0; r < count; ++r) {
        int64_t const row = first + r;
        CellData const gated = cellData(name, cid_gate, row, gate);
        CellData data = gated;
        if (gated.count > 0) {
            bool invalidated = true;
            if (changedRows) {
                while (i < changedCount && changedRows[i] < row) {
                    ++i;
                }
                if (i < changedCount && changedRows[i] == row) {
                    invalidated = true;
                    ++i;
                }
                else
                    invalidated = false;
            }
            if (invalidated) {
                CellData const orig = cellData(name, cid_in, row, in);

                assert(gated.count == orig.count);
                data = orig;
                ++invalidate;
            }
            ++not_empty;
        }
#if 0
        if ((row & 0xFFFF) == 0) {
            pLogMsg(klogInfo, "progress: $(row) rows for $(col)", "row=%li,col=%s", row, name);
        }
#endif
        if (row == first)
            setRow(first, out);

        openRow(row, out);
        writeCell(row, &data, cid_out, out);
        commitRow(row, out);
        closeRow(row, out);
    }
    pLogMsg(klogInfo, "column $(name); rows: $(rows); non-empty rows: $(notempty); updated rows: $(updated);", "name=%s,rows=%lu,notempty=%lu,updated=%lu", name, count, not_empty, invalidate);
    pLogMsg(klogInfo, "progress: done with $(col)", "col=%s", name);
    commitCursor(out);
    VCursorRelease(in);
    VCursorRelease(out);
    VCursorRelease(gate);
}

static void updateCacheColumn(  char const *const type
                              , char const *const name
                              , VTable *const outT
                              , VTable const *const inT
                              , VTable const *const gateT
                              , size_t const changedCount
                              , int64_t const *const changedRows)
{
    VCursor *out = NULL;
    VCursor const *in = NULL;
    VCursor const *gate = NULL;
    {
        rc_t const rc = VTableCreateCursorWrite(outT, &out, kcmInsert);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create output cursor!");
            exit(EX_CANTCREAT);
        }
    }
    {
        rc_t const rc = VTableCreateCursorRead(inT, &in);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create input cursor!");
            exit(EX_NOINPUT);
        }
    }
    {
        rc_t const rc = VTableCreateCursorRead(gateT, &gate);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create gate cursor!");
            exit(EX_NOINPUT);
        }
    }
    updateCacheColumn1(type, name, out, in, gate, changedCount, changedRows);
}

static void updateCacheTable(  VTable *const out
                             , VTable const *const in
                             , VTable const *const gate
                             , size_t const changedCount
                             , int64_t const *const changedRows)
{
    if (changedCount > 0) {
        static char const *columns[] = {
            "U8", "RD_FILTER_CACHE",
            "U32", "SAM_FLAGS_CACHE",
            NULL
        };
        char const *const *col;
        for (col = columns; *col; ) {
            char const *const type = *col++;
            char const *const name = *col++;
            updateCacheColumn(type, name, out, in, gate, changedCount, changedRows);
        }
    }
    updateCacheColumn("I32", "TEMPLATE_LEN_CACHE", out, in, gate, 0, NULL);
    VTableRelease(out);
    VTableRelease(in);
    VTableRelease(gate);
}

/** length of a common prefix
 */
static size_t prefixlen(char const *const a, char const *const b)
{
    size_t i = 0;
    for ( ; ; ) {
        int const cha = a[i];
        int const chb = b[i];
        if (cha == '\0' || chb == '\0' || cha != chb)
            return i;
        ++i;
    }
}

/** check if cachePath == inPath + ".vdbcache"
 */
static bool checkForActiveCache(char const *const cachePath, char const *const inPath)
{
    size_t prefixLength;
    if (cachePath == NULL)
        return false;
    prefixLength = prefixlen(cachePath, inPath);
    if (inPath[prefixLength] != '\0')
        return false;
    return strcmp(cachePath + prefixLength, ".vdbcache") == 0;
}

static void updateCache2(char const *const cachePath, char const *const inPath, size_t const changedCount, int64_t const *const changedRows, VDBManager *const mgr)
{
    VTable const *const in = dbOpenTable(inPath, "PRIMARY_ALIGNMENT", mgr, NULL, NULL);
    VSchema *const schema = makeSchema(mgr); // this schema will get a copy of the input's schema
    char const *schemaType = NULL; // schema type of input
    VTable const *const gate = dbOpenTable(cachePath, "PRIMARY_ALIGNMENT", mgr, &schemaType, schema);
    VDatabase *db = NULL;
    VTable *tbl = NULL;

    if (changedCount)
        pLogMsg(klogInfo, "Updating READ_FILTER changed $(count) rows, updating vdbcache", "count=%zu", changedCount);
    else
        LogMsg(klogInfo, "Updating vdbcache");
    {
        rc_t const rc = VDBManagerCreateDB(mgr, &db, schema, schemaType, kcmInit | kcmMD5, TEMP_CACHE_OBJECT_NAME);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create temp database");
            exit(EX_CANTCREAT);
        }
    }
    {
        rc_t const rc = VDatabaseCreateTable(db, &tbl, "PRIMARY_ALIGNMENT", kcmCreate | kcmMD5, "PRIMARY_ALIGNMENT");
        VDatabaseRelease(db);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create output table");
            exit(EX_CANTCREAT);
        }
    }
    pLogMsg(klogInfo, "Created temp database of type $(type)", "type=%s", schemaType);
    free((void *)schemaType);
    VSchemaRelease(schema);

    updateCacheTable(tbl, in, gate, changedCount, changedRows);
    if (changedCount > 0) {
        copyColumn("RD_FILTER_CACHE", "PRIMARY_ALIGNMENT", TEMP_CACHE_OBJECT_NAME, cachePath, mgr);
        copyColumn("SAM_FLAGS_CACHE", "PRIMARY_ALIGNMENT", TEMP_CACHE_OBJECT_NAME, cachePath, mgr);
    }
    copyColumn("TEMPLATE_LEN_CACHE", "PRIMARY_ALIGNMENT", TEMP_CACHE_OBJECT_NAME, cachePath, mgr);
}

static int cmp_int64_t(void const *A, void const *B)
{
    int64_t const a = *(int64_t const *)A;
    int64_t const b = *(int64_t const *)B;
    return a < b ? -1 : b < a ? 1 : 0;
}

static void updateCache(char const *const cachePath, char const *const inPath, VDBManager *const mgr)
{
    if (invalidated < 0)
        updateCache2(cachePath, inPath, 0, NULL, mgr);
    else {
        off_t const fsize = invalidated < 0 ? 0 : lseek(invalidated, 0, SEEK_END);
        if (fsize % sizeof(int64_t) == 0) {
            void *const map = mmap(NULL, fsize, PROT_READ|PROT_WRITE, MAP_PRIVATE, invalidated, 0);
            close(invalidated);
            if (map) {
                qsort(map, fsize / sizeof(int64_t), sizeof(int64_t), cmp_int64_t);
                updateCache2(cachePath, inPath, fsize / sizeof(int64_t), map, mgr);
                munmap(map, fsize);
                return;
            }
            LogMsg(klogFatal, "failed to map invalidated rows file");
            exit(EX_IOERR);
        }
        LogMsg(klogFatal, "size of invalidated rows file is not a whole number of row IDs");
        exit(EX_SOFTWARE);
    }
}

static char const *temporaryDirectory(Args *const args);
static char const *absolutePath(char const *const path, char const *const wd);

/* MARK: the main action starts here */
void main_1(int argc, char *argv[])
{
    char const *const wd = getcwd(NULL, 0);
    if (wd) {
        Args *const args = getArgs(argc, argv);
        char const *const input = getParameter(args, wd);
        char const *const cachePath = absolutePath(getOptArgValue(OPT_CACHE, args), wd);
        bool const isActiveCache = checkForActiveCache(cachePath, input);
        char const *const tempDir = temporaryDirectory(args); // also cd's to temp dir
        VDBManager *const mgr = manager();
        VSchema *const schema = makeSchema(mgr); // this schema will get a copy of the input's schema
        bool noDb = false; // is input a table or a database
        char const *schemaType = NULL; // schema type of input
        VTable const *const in = openInput(input, mgr, &noDb, &schemaType, schema);
        VTable *const out = createOutput(args, mgr, noDb, schemaType, schema);

        if (isActiveCache) {
            pLogMsg(klogWarn, "vdbcache should NOT be named $(inpath).vdbcache; rename it or put it in a different directory!!!", "inpath=%s", input);
        }
        processTables(out, in, cachePath != NULL);
        copyColumn("RD_FILTER", noDb ? NULL : "SEQUENCE", TEMP_MAIN_OBJECT_NAME, input, mgr);
        saveCounts(noDb ? NULL : "SEQUENCE", input, mgr);
        if (cachePath) {
            if (invalidated >= 0)
                fsync(invalidated);
            updateCache(cachePath, input, mgr);
        }

        VDBManagerRelease(mgr);
        ArgsWhack(args);

        chdir(wd);
        removeTempDir(tempDir);
        free((void *)tempDir);
        free((void *)cachePath);
        free((void *)input);
        free((void *)wd);
        return;
    }
    LogMsg(klogFatal, "Can't get working directory");
    exit(EX_TEMPFAIL);
}

static void processTables(VTable *const output, VTable const *const input, bool const haveCache)
{
    VCursor *out = NULL;
    VCursor const *in = NULL;
    {
        rc_t const rc = VTableCreateCursorWrite(output, &out, kcmInsert);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create output cursor!");
            exit(EX_CANTCREAT);
        }
    }
    {
        rc_t const rc = VTableCreateCursorRead(input, &in);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create input cursor!");
            exit(EX_NOINPUT);
        }
    }
    VTableRelease(input);
    processCursors(out, in, haveCache);
    VTableRelease(output);
}

static VTable *createOutput(  Args *const args
                            , VDBManager *const mgr
                            , bool noDb
                            , char const *schemaType
                            , VSchema const *schema)
{
    VTable *out = NULL;
    VDatabase *db = NULL;

    if (noDb) {
        rc_t const rc = VDBManagerCreateTable(mgr, &out, schema
                                              , schemaType
                                              , kcmInit | kcmMD5
                                              , TEMP_MAIN_OBJECT_NAME);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create temp table");
            exit(EX_CANTCREAT);
        }
        pLogMsg(klogInfo, "Created temp table of type $(type)", "type=%s", schemaType);
    }
    else {
        {
            rc_t const rc = VDBManagerCreateDB(mgr, &db, schema
                                              , schemaType
                                              , kcmInit | kcmMD5
                                              , TEMP_MAIN_OBJECT_NAME);
            if (rc != 0) {
                LogErr(klogFatal, rc, "Failed to create temp database");
                exit(EX_CANTCREAT);
            }
        }
        {
            rc_t const rc = VDatabaseCreateTable(db, &out, "SEQUENCE", kcmCreate | kcmMD5, "SEQUENCE");
            VDatabaseRelease(db);
            if (rc != 0) {
                LogErr(klogFatal, rc, "Failed to create output table");
                exit(EX_CANTCREAT);
            }
        }
        pLogMsg(klogInfo, "Created temp database of type $(type)", "type=%s", schemaType);
    }
    free((void *)schemaType);
    VSchemaRelease(schema);
    return out;
}

static uint8_t *growFilterBuffer(uint8_t *out_filter, size_t *const out_filter_count, size_t needed)
{
    if (*out_filter_count < needed) {
        free(out_filter);
        *out_filter_count = needed;
        pLogMsg(klogInfo, "increasing max read count to $(max)", "max=%zu", needed);
        out_filter = malloc(needed * sizeof(out_filter[0]));
        if (out_filter == NULL)
            OUT_OF_MEMORY();
    }
    return out_filter;
}

static void test(void)
{
    uint8_t qual[30];
    int i;
    int j;

    assert(shouldFilter(0, NULL) == keep);

    for (i = 0; i < 30; ++i)
        qual[i] = 30;
    assert(shouldFilter(30, qual) == keep);

    for (i = 0; i < 30; ++i)
        qual[i] = (i & 1) ? 30 : 3;
    qual[19] = 3;
    assert((shouldFilter(30, qual) & low_quality_count) == low_quality_count);

    for (i = 0; i < 30; ++i)
        qual[i] = 30;
    for (i = 0; i < 10; ++i)
        qual[i] = 19;
    assert(shouldFilter(30, qual) == keep);
    qual[10] = 19;
    assert((shouldFilter(30, qual) & low_quality_front) == low_quality_front);

    for (i = 0; i < 30; ++i)
        qual[i] = 30;
    for (i = 0; i < 10; ++i)
        qual[i] = 19;
    for (i = 0, j = 29; i < j; ++i, --j) {
        int vi = qual[i];
        int vj = qual[j];
        qual[i] = vj;
        qual[j] = vi;
    }
    assert(shouldFilter(30, qual) == keep);

    for (i = 0; i < 30; ++i)
        qual[i] = 30;
    for (i = 0; i < 10; ++i)
        qual[i] = 19;
    qual[10] = 19;
    for (i = 0, j = 29; i < j; ++i, --j) {
        int vi = qual[i];
        int vj = qual[j];
        qual[i] = vj;
        qual[j] = vi;
    }
    assert((shouldFilter(30, qual) & low_quality_back) == low_quality_back);
}

#define OPT_DEF_VALUE(N, A, H) { N, A, NULL, H, 1, true, true }
#define OPT_DEF_REQUIRED_VALUE(N, A, H) { N, A, NULL, H, 1, true, true }
#define OPT_NAME(n) Options[n].name

static char const *temp_help[] = { "temp directory to use for scratch space, default: $TMPDIR or $TEMPDIR or $TEMP or $TMP or /tmp", NULL };
static char const *vdbcache_help[] = { "location of .vdbcache to update", NULL };

/* MARK: Options array */
static OptDef Options [] = {
    { "temp", "t", NULL, temp_help, 1, true, false },
    { "vdbcache", "", NULL, vdbcache_help, 1, true, false }
};

/* MARK: Mostly boilerplate from here */

const char UsageDefaultName[] = "make-read-filter";

rc_t CC UsageSummary ( const char *progname )
{
    return KOutMsg ( "\n"
                     "Usage:\n"
                     "  %s [options] <input>\n"
                     "\n"
                     "Summary:\n"
                     "  Make/Update RD_FILTER from QUALITY.\n"
                     , progname
        );
}

rc_t CC Usage ( const Args *args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    KOutMsg ("Options:\n");
    HelpOptionLine(Options[0].aliases, Options[0].name, "path", Options[0].help);
    HelpOptionLine(Options[1].aliases, Options[1].name, "path", Options[1].help);

    KOutMsg ("Common options:\n");
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion () );

    return rc;
}

MAIN_DECL( argc, argv )
{
    if ( VdbInitialize( argc, argv, 0 ) )
        return VDB_INIT_FAILED;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

#if _DEBUGGING
    test();
#endif
    main_1(argc, argv);
    return 0;
}

static Args *getArgs(int argc, char *argv[])
{
    Args *args = NULL;
    rc_t const rc = ArgsMakeAndHandle(&args, argc, argv, 1, Options, OPTIONS_COUNT);
    if (rc == 0)
        return args;

    LogErr(klogErr, rc, "failed to parse arguments");
    exit(EX_USAGE);
}

static VTable const *openInput(char const *const input, VDBManager const *const mgr, bool *const noDb, char const **const schemaType, VSchema *const schema)
{
    int const inputType = pathType(mgr, input);

    if (PATH_TYPE_ISA_DATABASE(inputType)) {
        LogMsg(klogInfo, "input is a database");
        *noDb = false;
        return dbOpenTable(input, "SEQUENCE", mgr, schemaType, schema);
    }
    else if (PATH_TYPE_ISA_TABLE(inputType)) {
        LogMsg(klogInfo, "input is a table");
        *noDb = true;
        return openInputTable(input, mgr, schemaType, schema);
    }
    else {
        LogMsg(klogFatal, "input is not a table or database");
        exit(EX_NOINPUT);
    }
}

static char const *getOptArgValue(int opt, Args *const args)
{
    void const *value = NULL;
    rc_t const rc = ArgsOptionValue(args, Options[opt].name, 0, &value);
    if (rc == 0)
        return value;
    return NULL;
}

static char const *absolutePath(char const *const path, char const *const wd)
{
    if (path == NULL) return NULL;
    if (path[0] != '/') {
        KDirectory const *const dir = openDirRead(wd);
        char *buffer = NULL;
        size_t bufsize = strlen(wd) + strlen(path) + 10;
        for ( ; ; ) {
            void *const temp = realloc(buffer, bufsize);
            if (temp) {
                rc_t const rc = KDirectoryResolvePath(dir, true, temp, bufsize, "%s", path);
                buffer = temp;
                bufsize *= 2;
                if (rc == 0) {
                    char const *const result = string_dup_measure(buffer, NULL);
                    free(buffer);
                    return result;
                }
            }
            else
                OUT_OF_MEMORY();
        }
    }
    else
        return string_dup_measure(path, NULL);
}

static char const *getParameter(Args *const args, char const *const wd)
{
    char const *value = NULL;
    uint32_t count = 0;
    rc_t rc = ArgsParamCount(args, &count);
    if (rc != 0) {
        LogErr(klogFatal, rc, "Failed to get parameter count");
        exit(EX_SOFTWARE);
    }
    if (count != 1) {
        Usage(args);
        exit(EX_USAGE);
    }
    rc = ArgsParamValue(args, 0, (void const **)&value);
    if (rc != 0) {
        LogErr(klogFatal, rc, "Failed to get parameter value");
        exit(EX_SOFTWARE);
    }
    return absolutePath(value, wd);
}

static VSchema *makeSchema(VDBManager *mgr)
{
    VSchema *schema = NULL;
    rc_t const rc = VDBManagerMakeSchema(mgr, &schema);
    if (rc == 0)
        return schema;

    LogErr(klogFatal, rc, "Failed to make a schema object!");
    exit(EX_TEMPFAIL);
}

static char const *temporaryDirectory(Args *const args)
{
    KDirectory *ndir = rootDir();
    rc_t rc;
    char *pattern = NULL;
    size_t len = 0;
    char const *tmp = getOptArgValue(OPT_OUTPUT, args);

    if (tmp == NULL)
        tmp = getenv("TMPDIR");
    if (tmp == NULL)
        tmp = getenv("TEMPDIR");
    if (tmp == NULL)
        tmp = getenv("TEMP");
    if (tmp == NULL)
        tmp = getenv("TMP");
    if (tmp == NULL)
        tmp = "/tmp";

    len = 4096;
    for ( ; ; ) {
        pattern = malloc(len);
        if (pattern == NULL)
            OUT_OF_MEMORY();
        rc = KDirectoryResolvePath(ndir, true, pattern, len, "%s/mkf.XXXXXX", tmp);
        if (rc == 0)
            break;
        free(pattern);
        len *= 2;
    }
    KDirectoryRelease(ndir);

    {
        /* delete directory if it exists */
        struct stat st = {0};
        if (stat(pattern, &st) == 0)
            removeTempDir(pattern);
    }

    mkdir(pattern, 0700);
    pLogMsg(klogDebug, "output to $(out)", "out=%s", pattern);
    if (chdir(pattern) == 0)
        return pattern;

    LogMsg(klogFatal, "Can't cd to temp directory");
    exit(EX_TEMPFAIL);
}
