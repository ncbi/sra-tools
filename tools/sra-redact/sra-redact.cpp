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
 *  sra-redact --temp /tmp SRR123456
 */

#include <vector>
#include <algorithm>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "sra-redact.hpp" /* contains mostly boilerplate code */

/* NOTE: Needs to be in same order as Options array */
enum OPTIONS {
    OPT_OUTPUT,     /* temp path */
    OPTIONS_COUNT
};

#define SEQUENCE_TABLE "SEQUENCE"
#define PRI_ALIGN_TABLE "PRIMARY_ALIGNMENT"
#define SEC_ALIGN_TABLE "SECONDARY_ALIGNMENT"
#define READ "READ"
#define CMP_READ "CMP_READ"
#define ALIGN_READ "RAW_READ"
#define ALIGN_ID "PRIMARY_ALIGNMENT_ID"
#define SPOT_ID "SEQ_SPOT_ID"
#define HAS_MISS "HAS_MISMATCH"
#define HAS_OFFSET "HAS_REF_OFFSET"
#define MISMATCH "MISMATCH"
#define REF_OFFSET "REF_OFFSET"
#define OFFSET_TYPE "REF_OFFSET_TYPE"
#define BASE_REPRESENTATION_TYPE "INSDC:dna:text"

#define PROGRESS_MESSAGES 1

static int filterPipeIn; ///< it is replying here
static int filterPipeOut; ///< we are querying here
static bool doFilter(uint32_t const len, uint8_t const *const seq)
{
    static auto buffer = std::vector<uint8_t>();

    buffer.reserve(len + 1);
    buffer.assign(seq, seq + len);
    buffer.push_back('\n');

    auto const sent = write(filterPipeOut, buffer.data(), buffer.size());
    if (sent != buffer.size()) {
        LogErr(klogErr, RC(rcExe, rcProcess, rcWriting, rcData, rcNotFound), "Failed to send to filter process!");
        exit(EX_TEMPFAIL);
    }
    buffer.resize(5);
    if (5 == read(filterPipeIn, buffer.data(), 5)) {
        auto const reply = std::string(buffer.begin(), buffer.end());
        if (reply == "pass\n")
            return false;
        if (reply == "fail\n")
            return true;
    }
    LogErr(klogErr, RC(rcExe, rcProcess, rcReading, rcData, rcNotFound), "Failed to read reply from filter process!");
    exit(EX_TEMPFAIL);
}

static bool noFilter(uint32_t const len, uint8_t const *const seq)
{
    return false;
}

using FilterFunction = decltype(doFilter);
static FilterFunction&& filterFunction() {
#if _DEBUGGING || DEBUG
    auto const SKIP = getenv("SRA_REDACT_NONE");
    if (SKIP && *SKIP == '1') {
        LogMsg(klogWarn, "NOT ACTUALLY REDACTING");
        return noFilter;
    }
#endif
    return doFilter;
}
auto const &&shouldFilter = filterFunction();

static bool redactUnalignedReads(uint8_t *const out_read, CellData const &prIdData, CellData const &readLenData, CellData const &readData)
{
    auto const nreads = readLenData.count;
    auto const readLen = reinterpret_cast<uint32_t const *>(readLenData.data);
    auto const prID = reinterpret_cast<int64_t const *>(prIdData.data);
    auto bases = reinterpret_cast<uint8_t const *>(readData.data);

    assert(prIdData.count == nreads);
    if (nreads == 0)
        return false;

    std::copy(bases, bases + readData.count, out_read);
    for (auto i = 0; i < nreads; ++i) {
        auto const read = bases;
        auto const len = readLen[i];

        if (prID[i] != 0)
            continue;

        bases += len;

        if (shouldFilter(len, read)) {
            std::fill(out_read, out_read + readData.count, 'N');
            return true;
        }
    }
    return false;
}

static bool redactReads(uint8_t *const out_read, CellData const &readStartData, CellData const &readTypeData, CellData const &readLenData, CellData const &readData)
{
    auto const nreads = readLenData.count;
    auto const readStart = reinterpret_cast<int32_t const *>(readStartData.data);
    auto const readLen = reinterpret_cast<uint32_t const *>(readLenData.data);
    auto const readType = reinterpret_cast<uint8_t const *>(readTypeData.data);
    auto const bases = reinterpret_cast<uint8_t const *>(readData.data);
    bool redacted = false;

    assert(readStartData.count == nreads);
    assert(readTypeData.count == nreads);
    if (nreads == 0)
        return false;

    assert(readStart[nreads - 1] + readLen[nreads - 1] <= readData.count);

    std::copy(bases, bases + readData.count, out_read);
    for (auto i = 0; i < nreads; ++i) {
        if ((readType[i] & SRA_READ_TYPE_BIOLOGICAL) != SRA_READ_TYPE_BIOLOGICAL)
            continue;
        if (!redacted && !shouldFilter(readLen[i], bases + readStart[i]))
            continue;

        redacted = true;

        auto const outStart = out_read + readStart[i];
        std::fill(outStart, outStart + readLen[i], 'N');
    }
    return redacted;
}

enum {
    dspcRedactedReads,
    dspcKeptReads,
    dspcRedactedSpots,
    dspcKeptSpots,
    dspcRedactedBases = dspcRedactedReads,
    dspcKeptBases = dspcKeptReads,
};
uint64_t dispositionCount[4];
uint64_t dispositionBaseCount[2];

int redactedSpots = -1;
#include <fcntl.h>
static void open_redacted_alignments()
{
    redactedSpots = open("redactedAlignments", O_RDWR | O_APPEND | O_CREAT | O_EXCL, 0700);
    if (redactedSpots >= 0)
        return;
    LogMsg(klogFatal, "Can't create file for redacted redacted alignments");
    exit(EX_CANTCREAT);
}

static void redactedSpot(int64_t const row)
{
    if (redactedSpots >= 0) {
        if (write(redactedSpots, &row, sizeof(row)) == sizeof(row))
            return;
        LogMsg(klogFatal, "Can't record invalidated rows");
        exit(EX_IOERR);
    }
    open_redacted_alignments();
    redactedSpot(row);
}

static void processAlignmentCursors(VCursor *const out, VCursor const *const in, bool const isPrimary, bool &has_offset_type)
{
    auto allfalse = std::vector<uint8_t>();

    /* MARK: input columns */
    auto const cid_spot_id     = addColumn(SPOT_ID   , "I64", in);
    auto const cid_read        = addColumn(ALIGN_READ, "U8" , in);
    auto const cid_has_miss    = addColumn(HAS_MISS  , "U8" , in);
    auto const cid_has_offset  = addColumn(HAS_OFFSET, "U8" , in);
    auto const cid_mismatch    = addColumn(MISMATCH  , BASE_REPRESENTATION_TYPE, in);
    auto const cid_ref_offset  = addColumn(REF_OFFSET, "I32", in);
    auto const cid_offset_type = addColumnOptional(OFFSET_TYPE, "U8", in, has_offset_type);

    /* MARK: output columns */
    auto const cid_out_has_miss    = addColumn(HAS_MISS  , "U8" , out);
    auto const cid_out_has_offset  = addColumn(HAS_OFFSET, "U8" , out);
    auto const cid_out_mismatch    = addColumn(MISMATCH  , BASE_REPRESENTATION_TYPE, out);
    auto const cid_out_ref_offset  = addColumn(REF_OFFSET, "I32", out);
    auto const cid_out_spot_id     = addColumn(SPOT_ID   , "I64", out);
    auto const cid_out_offset_type = has_offset_type ? addColumn(OFFSET_TYPE, "U8", out) : 0;

    int64_t first = 0;
    uint64_t count = 0;

    allfalse.reserve(1024);

    openCursor(in, "input");
    openCursor(out, "output");

    count = rowCount(in, &first, cid_spot_id);
    assert(first == 1);
    pLogMsg(klogInfo, "progress: about to process $(rows) $(kind) alignments", "kind=%s,rows=%lu", isPrimary ? "primary" : "secondary", count);

    /* MARK: Main loop over the alignments */
    for (uint64_t r = 0; r < count; ++r) {
        int64_t const row = 1 + r;
        auto const read = cellData(ALIGN_READ, cid_read, row, in);

        auto spot_id     = cellData(SPOT_ID    , cid_spot_id    , row, in);
        auto has_miss    = cellData(HAS_MISS   , cid_has_miss   , row, in);
        auto has_offset  = cellData(HAS_OFFSET , cid_has_offset , row, in);
        auto ref_offset  = cellData(REF_OFFSET , cid_ref_offset , row, in);
        auto mismatch    = cellData(MISMATCH   , cid_mismatch   , row, in);
        auto offset_type = cellData(OFFSET_TYPE, cid_offset_type, row, in);

#if PROGRESS_MESSAGES
        if ((row & 0xFFFF) == 0) {
            pLogMsg(klogDebug, "progress: $(row) rows, bases redacted: $(count)", "row=%lu,count=%lu", (unsigned long)row, (unsigned long)dispositionBaseCount[dspcRedactedBases]);
        }
#endif
        auto const redact = shouldFilter(read.count, (uint8_t const *)read.data);
        auto spotId = *reinterpret_cast<uint64_t const *>(spot_id.data);

        spot_id.data = &spotId;
        if (redact) {
            if (isPrimary) {
                dispositionBaseCount[dspcRedactedBases] += read.count;
                redactedSpot(spotId);
            }
            allfalse.resize(has_miss.count, 0);
            mismatch.count = 0;
            ref_offset.count = 0;
            offset_type.count = 0;
            has_miss.data = allfalse.data();
            has_offset.data = allfalse.data();
            spotId = 0;
        }
        else if (isPrimary) {
            dispositionBaseCount[dspcKeptBases] += read.count;
        }
        openRow(row, out);
        writeRow(row, spot_id    , cid_out_spot_id, out);
        writeRow(row, has_miss   , cid_out_has_miss   , out);
        writeRow(row, has_offset , cid_out_has_offset , out);
        writeRow(row, mismatch   , cid_out_mismatch   , out);
        writeRow(row, ref_offset , cid_out_ref_offset , out);
        writeRow(row, offset_type, cid_out_offset_type, out);
        commitRow(row, out);
        closeRow(row, out);
    }
    LogMsg(klogInfo, "progress: done");
    commitCursor(out);
    VCursorRelease(out);
    VCursorRelease(in);
}

static void processSequenceCursors(VCursor *const out, VCursor const *const in, bool aligned, int64_t const *const redacted, size_t const redactedCount)
{
    auto outRead = std::vector<uint8_t>();
    auto allZero = std::vector<int64_t>();

    /* MARK: input columns */
    auto const readColName = aligned ? CMP_READ : READ;
    auto const cid_pr_id       = aligned ? addColumn(ALIGN_ID, "I64", in) : 0;
    auto const cid_readstart   = addColumn("READ_START" , "I32", in);
    auto const cid_read_type   = addColumn("READ_TYPE"  , "U8" , in);
    auto const cid_readlen     = addColumn("READ_LEN"   , "U32", in);
    auto const cid_read        = addColumn(readColName  , BASE_REPRESENTATION_TYPE, in);

    /* MARK: output columns */
    auto const cid_out_read = addColumn(readColName, BASE_REPRESENTATION_TYPE, out);
    auto const cid_out_pr_id = aligned ? addColumn(ALIGN_ID, "I64", out) : 0;

    int64_t first = 0;
    uint64_t count = 0;

    uint64_t r = 0;
    auto redactedStart = redacted ? redacted : &first;
    auto const redactedEnd = redactedStart + redactedCount;

    outRead.reserve(1024);
    allZero.reserve(2);

    openCursor(in, "input");    
    openCursor(out, "output");
    
    count = rowCount(in, &first, cid_read_type);
    assert(first == 1);
    pLogMsg(klogInfo, "progress: about to process $(rows) spots", "rows=%lu", count);

    /* MARK: Main loop over the input */
    for (r = 0; r < count; ++r) {
        int64_t const row = 1 + r;
        auto const readstart  = cellData("READ_START" , cid_readstart  , row, in);
        auto const readtype   = cellData("READ_TYPE"  , cid_read_type  , row, in);
        auto const readlen    = cellData("READ_LEN"   , cid_readlen    , row, in);
        auto const nreads = readlen.count;
        auto read = cellData(readColName, cid_read, row, in);
        auto prId = cellData(ALIGN_ID, cid_pr_id, row, in);
        bool redact = false;

#if PROGRESS_MESSAGES
        if ((row & 0xFFFF) == 0) {
            pLogMsg(klogDebug, "progress: $(row) rows, bases redacted: $(count)", "row=%lu,count=%lu", (unsigned long)row, (unsigned long)dispositionBaseCount[dspcRedactedBases]);
        }
#endif
        outRead.resize(read.count, 'N');
        allZero.resize(prId.count, 0);

        if (redactedStart && redactedStart < redactedEnd && *redactedStart == row) {
            redact = true;
            do {
                ++redactedStart;
            } while (redactedStart < redactedEnd && *redactedStart == row);
        }
        else if (aligned)
            redact = redactUnalignedReads(outRead.data(), prId, readlen, read);
        else
            redact = redactReads(outRead.data(), readstart, readtype, readlen, read);

        if (redact) {
            dispositionCount[dspcRedactedReads] += nreads;
            dispositionCount[dspcRedactedSpots] += 1;
            dispositionBaseCount[dspcRedactedBases] += read.count;

            read.data = outRead.data();
            prId.data = allZero.data();
        }
        else {
            dispositionCount[dspcKeptReads] += nreads;
            dispositionCount[dspcKeptSpots] += 1;
            dispositionBaseCount[dspcKeptBases] += read.count;
        }

        openRow(row, out);
        writeRow(row, read, cid_out_read, out);
        writeRow(row, prId, cid_out_pr_id, out);
        commitRow(row, out);
        closeRow(row, out);
    }
    pLogMsg(klogInfo, "progress: redacted $(spots) spots, $(reads) reads, $(bases) bases", "spots=%lu,reads=%lu,bases=%lu"
            , (unsigned long)dispositionCount[dspcRedactedSpots]
            , (unsigned long)dispositionCount[dspcRedactedReads]
            , (unsigned long)dispositionBaseCount[dspcRedactedBases]);
    commitCursor(out);
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

static void saveCounts(KMDataNode *node) {
    writeChildNode(node, "REDACTED_SPOTS", sizeof(uint64_t), &dispositionCount[dspcRedactedSpots]);
    writeChildNode(node, "KEPT_SPOTS", sizeof(uint64_t), &dispositionCount[dspcKeptSpots]);
    writeChildNode(node, "REDACTED_READS", sizeof(uint64_t), &dispositionCount[dspcRedactedReads]);
    writeChildNode(node, "KEPT_READS", sizeof(uint64_t), &dispositionCount[dspcKeptReads]);
    writeChildNode(node, "REDACTED_BASES", sizeof(uint64_t), &dispositionBaseCount[dspcRedactedBases]);
    writeChildNode(node, "KEPT_BASES", sizeof(uint64_t), &dispositionBaseCount[dspcKeptBases]);
    KMDataNodeRelease(node);
}

static void saveCounts(bool isDb, char const *const dest, VDBManager *const mgr)
{
    if (isDb) {
        auto const db = openUpdateDb(dest, mgr);
        saveCounts(openNodeUpdate(db, "REDACTION"));
        VDatabaseRelease(db);
    }
    else {
        auto const tbl = openUpdateTbl(dest, mgr);
        saveCounts(openNodeUpdate(tbl, "REDACTION"));
        VTableRelease(tbl);
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
        char const *const tempDir = temporaryDirectory(args); // also cd's to temp dir
        VDBManager *const mgr = manager();
        VSchema *const schema = makeSchema(mgr); // this schema will get a copy of the input's schema
        auto const in = openInputs(input, mgr, schema);
        auto const isAligned = in.primaryAlignment != nullptr;
        auto const out = createOutputs(args, mgr, in, schema);
        bool has_offset_type[2] = {false, false};

        if (isAligned)
            processAlignmentTables(out.primaryAlignment, in.primaryAlignment, true, has_offset_type[0]);

        processSequenceTables(out.sequence, in.sequence, isAligned);

        if (isAligned && in.secondaryAlignment != nullptr)
            processAlignmentTables(out.secondaryAlignment, in.secondaryAlignment, false, has_offset_type[1]);

        /// MARK: COPY COLUMNS TO OUTPUT
        if (isAligned) {
            copyColumn(CMP_READ, SEQUENCE_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
            copyColumn(ALIGN_ID, SEQUENCE_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);

            copyColumn(HAS_MISS   , PRI_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
            copyColumn(HAS_OFFSET , PRI_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
            copyColumn(MISMATCH   , PRI_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
            copyColumn(REF_OFFSET , PRI_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
            copyColumn(SPOT_ID    , PRI_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
            if (has_offset_type[0])
                copyColumn(OFFSET_TYPE, PRI_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);

            if (in.secondaryAlignment != nullptr) {
                copyColumn(HAS_MISS   , SEC_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
                copyColumn(HAS_OFFSET , SEC_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
                copyColumn(MISMATCH   , SEC_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
                copyColumn(REF_OFFSET , SEC_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
                copyColumn(SPOT_ID    , SEC_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
                if (has_offset_type[1])
                    copyColumn(OFFSET_TYPE, SEC_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
            }
        }
        else if (in.noDb) {
            copyColumn(READ, nullptr, TEMP_MAIN_OBJECT_NAME, input, mgr);
        }
        else {
            copyColumn(READ, SEQUENCE_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
        }
        saveCounts(!in.noDb, input, mgr);

        VDBManagerRelease(mgr);
        ArgsWhack(args);

        chdir(wd);
        removeTempDir(tempDir);
        free((void *)tempDir);
        free((void *)input);
        free((void *)wd);
        return;
    }
    LogMsg(klogFatal, "Can't get working directory");
    exit(EX_TEMPFAIL);
}

static void processAlignmentTables(VTable *const output, VTable const *const input, bool const isPrimary, bool &has_offset_type)
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
    processAlignmentCursors(out, in, isPrimary, has_offset_type);
    VTableRelease(output);
}

static void processSequenceTables(VTable *const output, VTable const *const input, bool const aligned)
{
    int64_t *redacted = nullptr;
    size_t redactedCount = 0;
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
    VTableRelease(output);

    if (redactedSpots >= 0) {
        fsync(redactedSpots);

        off_t const fsize = lseek(redactedSpots, 0, SEEK_END);
        assert(fsize % sizeof(int64_t) == 0);

        void *const map = mmap(nullptr, fsize, PROT_READ|PROT_WRITE, MAP_FILE|MAP_PRIVATE, redactedSpots, 0);

        close(redactedSpots);
        if (map == MAP_FAILED) {
            LogMsg(klogFatal, "failed to map redacted spots file");
            exit(EX_IOERR);
        }
        redactedCount = fsize / sizeof(int64_t);
        redacted = reinterpret_cast<int64_t *>(map);
        std::sort(redacted, redacted + redactedCount);
    }
    processSequenceCursors(out, in, aligned, redacted, redactedCount);
}

static Outputs createOutputs(  Args *const args
                             , VDBManager *const mgr
                             , Inputs const &inputs
                             , VSchema const *schema)
{
    auto const schemaType = inputs.schemaType.c_str();
    Outputs out = {nullptr, nullptr, nullptr};

    if (inputs.noDb) {
        assert(inputs.primaryAlignment == nullptr && inputs.secondaryAlignment == nullptr);
        rc_t const rc = VDBManagerCreateTable(mgr, &out.sequence, schema
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
        VDatabase *db = nullptr;
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
            rc_t const rc = VDatabaseCreateTable(db, &out.sequence, SEQUENCE_TABLE, kcmCreate | kcmMD5, SEQUENCE_TABLE);
            if (rc != 0) {
                LogErr(klogFatal, rc, "Failed to create output sequence table");
                exit(EX_CANTCREAT);
            }
        }
        if (inputs.primaryAlignment != nullptr) {
            rc_t const rc = VDatabaseCreateTable(db, &out.primaryAlignment, PRI_ALIGN_TABLE, kcmCreate | kcmMD5, PRI_ALIGN_TABLE);
            if (rc != 0) {
                LogErr(klogFatal, rc, "Failed to create output primary alignment table");
                exit(EX_CANTCREAT);
            }
        }
        if (inputs.secondaryAlignment != nullptr) {
            rc_t const rc = VDatabaseCreateTable(db, &out.secondaryAlignment, SEC_ALIGN_TABLE, kcmCreate | kcmMD5, SEC_ALIGN_TABLE);
            if (rc != 0) {
                LogErr(klogFatal, rc, "Failed to create output secondary alignment table");
                exit(EX_CANTCREAT);
            }
        }
        VDatabaseRelease(db);
        pLogMsg(klogInfo, "Created temp database of type $(type)", "type=%s", schemaType);
    }
    VSchemaRelease(schema);
    return out;
}

static void test(void)
{
}

#define OPT_DEF_VALUE(N, A, H) { N, A, NULL, H, 1, true, true }
#define OPT_DEF_REQUIRED_VALUE(N, A, H) { N, A, NULL, H, 1, true, true }
#define OPT_NAME(n) Options[n].name

static char const *temp_help[] = { "temp directory to use for scratch space, default: $TMPDIR or $TEMPDIR or $TEMP or $TMP or /tmp", NULL };

/* MARK: Options array */
static OptDef Options [] = {
    { "temp", "t", NULL, temp_help, 1, true, false },
};

/* MARK: Mostly boilerplate from here */

static std::string which(std::string const &prog) {
    if (access(prog.c_str(), X_OK) == 0)
        return prog;

    static auto const defaultPATH = ".";
    auto PATH = const_cast<char const *>(getenv("PATH"));
    if (PATH == nullptr || PATH[0] == '\0') {
        LogErr(klogWarn, RC(rcExe, rcProcess, rcCreating, rcPath, rcNotFound), "PATH is not set; using .");
        PATH = defaultPATH;
    }
    auto path = std::string(PATH);
    while (!path.empty()) {
        auto const sep = path.find(':');
        auto const part = path.substr(0, sep);
        auto const full = part + '/' + prog;

        path = path.substr(sep == std::string::npos ? path.size() : (sep + 1));
        if (access(full.c_str(), X_OK) == 0)
            return full;
    }
    PLOGERR(klogErr, (klogErr, RC(rcExe, rcProcess, rcCreating, rcPath, rcNotFound), "no $(exe) in $(PATH)", "exe=%s,PATH=%s", prog.c_str(), PATH));
    exit(EX_TEMPFAIL);
}

extern "C" {
    static int kid;
    static void notify_and_reap_child(void)
    {
        int exitstatus = 0;

        close(filterPipeOut);
        close(filterPipeIn);

        waitpid(kid, &exitstatus, 0);
    }

    rc_t CC KMain(int argc, char *argv[])
    {
#if _DEBUGGING
        test();
#endif
        for (auto i = 1; i < argc; ++i) {
            auto const arg = std::string(argv[i]);
            if (arg == "--") {
                auto const child_argv = &argv[i + 1];
                auto const filter = which(child_argv[0]);
                int stdin_fds[2];
                int stdout_fds[2];

                if (pipe(stdin_fds) == 0 && pipe(stdout_fds) == 0) {
                    filterPipeOut = stdin_fds[1];
                    filterPipeIn = stdout_fds[0];

                    kid = fork();
                    if (kid < 0) {
                        LogErr(klogErr, RC(rcExe, rcProcess, rcCreating, rcProcess, rcFailed), "Failed to fork process");
                        exit(EX_TEMPFAIL);
                    }
                    if (kid == 0) {
                        // in child
                        close(filterPipeIn);
                        close(filterPipeOut);
                        if (dup2(stdin_fds[0], 0) < 0 || dup2(stdout_fds[1], 1) < 0) {
                            LogErr(klogErr, RC(rcExe, rcProcess, rcCreating, rcFile, rcFailed), "Failed to create stdin/stdout for filter process");
                            exit(EX_IOERR);
                        }
                        close(stdin_fds[0]);
                        close(stdout_fds[1]);
                        execv(filter.c_str(), child_argv);
                        PLOGERR(klogErr, (klogErr, RC(rcExe, rcProcess, rcExecuting, rcProcess, rcFailed), "Failed to exec $(exe)", "exe=%s", filter.c_str()));
                        exit(EX_TEMPFAIL);
                    }
                    atexit(notify_and_reap_child);
                    close(stdin_fds[0]);
                    close(stdout_fds[1]);

                    argv[i] = nullptr;
                    main_1(i, argv);
                    return 0;
                }
                else {
                    LogErr(klogErr, RC(rcExe, rcProcess, rcCreating, rcFile, rcFailed), "Failed to create I/O pipes for filter process");
                    exit(EX_IOERR);
                }
            }
        }
        LogErr(klogErr, RC(rcExe, rcArgv, rcParsing, rcParam, rcNotFound), "no filter command was given");
        exit(EX_USAGE);
    }

    const char UsageDefaultName[] = "sra-redact";

    rc_t CC UsageSummary ( const char *progname )
    {
        return KOutMsg ( "\n"
                        "Usage:\n"
                        "  %s [options] <input> -- <filter command>\n"
                        "\n"
                        "Summary:\n"
                        "  Redact based on output of filter tool.\n"
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

        KOutMsg ("Common options:\n");
        HelpOptionsStandard ();
        HelpVersion ( fullpath, KAppVersion () );

        return rc;
    }
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

static Inputs openInputs(char const *const input, VDBManager const *const mgr, VSchema *const schema)
{
    Inputs result;
    int const inputType = pathType(mgr, input);

    if (PATH_TYPE_ISA_DATABASE(inputType)) {
        auto const db = openDatabase(input, mgr);
        LogMsg(klogInfo, "input is a database");
        result.noDb = false;
        result.sequence = dbOpenTable(db, SEQUENCE_TABLE, mgr, result.schemaType, schema);
        result.primaryAlignment = dbOpenTable(db, PRI_ALIGN_TABLE, mgr, result.schemaType, nullptr, true);
        if (result.primaryAlignment != nullptr)
            result.secondaryAlignment = dbOpenTable(db, SEC_ALIGN_TABLE, mgr, result.schemaType, nullptr, true);
        VDatabaseRelease(db);
    }
    else if (PATH_TYPE_ISA_TABLE(inputType)) {
        LogMsg(klogInfo, "input is a table");
        result.noDb = true;
        result.sequence = openInputTable(input, mgr, result.schemaType, schema);
    }
    else {
        LogMsg(klogFatal, "input is not a table or database");
        exit(EX_NOINPUT);
    }
    return result;
}

static char const *getOptArgValue(int opt, Args *const args)
{
    void const *value = NULL;
    rc_t const rc = ArgsOptionValue(args, Options[opt].name, 0, &value);
    if (rc == 0)
        return reinterpret_cast<char const *>(value);
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
                rc_t const rc = KDirectoryResolvePath(dir, true, (char *)temp, bufsize, "%s", path);
                buffer = reinterpret_cast<char *>(temp);
                bufsize *= 2;
                if (rc == 0) {
                    char const *const result = strdup(buffer);
                    free(buffer);
                    return result;
                }
            }
            else
                OUT_OF_MEMORY();
        }
    }
    else
        return strdup(path);
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
        pattern = (char *)malloc(len);
        if (pattern == NULL)
            OUT_OF_MEMORY();
        rc = KDirectoryResolvePath(ndir, true, pattern, len, "%s/mkf.XXXXXX", tmp);
        if (rc == 0)
            break;
        free(pattern);
        len *= 2;
    }
    KDirectoryRelease(ndir);
    
    mkdtemp(pattern);
    pLogMsg(klogDebug, "output to $(out)", "out=%s", pattern);
    if (chdir(pattern) == 0)
        return pattern;

    LogMsg(klogFatal, "Can't cd to temp directory");
    exit(EX_TEMPFAIL);
}
