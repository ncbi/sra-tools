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
#include <set>
#include <algorithm>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <chrono>
#include <cmath>
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
#define BASE_TYPE "INSDC:dna:text"

#define PROGRESS_MESSAGES 1

static int filterPipeIn; ///< it is replying here
static int filterPipeOut; ///< we are querying here
static bool doFilter(uint32_t const len, uint8_t const *const seq)
{
    static auto buffer = std::vector<uint8_t>();

    if (len == 0)
        return false;

    buffer.reserve(len + 1);
    buffer.assign(seq, seq + len);
    buffer.push_back('\n');

    auto const sent = write(filterPipeOut, buffer.data(), buffer.size());
    if (sent < 0 || decltype(buffer.size())(sent) != buffer.size()) {
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

static bool redactUnalignedReads(CellData::Typed<int64_t> const &prId, CellData const &readLenData, CellData const &readData)
{
    auto readLen = readLenData.typed<uint32_t>().begin();
    auto read = readData.typed<uint8_t>().begin();

    for (auto && id : prId) {
        auto const len = id == 0 ? *readLen : 0;

        if (shouldFilter(len, read))
            return true;

        readLen += 1;
        read += len;
    }
    return false;
}

static bool redactReads(CellData const &readStartData, CellData const &readTypeData, CellData const &readLenData, CellData const &readData)
{
    auto const readStart = readStartData.typed<int32_t>();
    auto const readLen = readLenData.typed<uint32_t>();
    auto const readType = readTypeData.typed<uint8_t>();
    auto const bases = readData.typed<uint8_t>();
    unsigned read = 0;

    for (auto &&type : readType) {
        auto const i = read++;

        if ((type & SRA_READ_TYPE_BIOLOGICAL) != SRA_READ_TYPE_BIOLOGICAL)
            continue;

        if (shouldFilter(readLen[i], &bases[readStart[i]]))
            return true;
    }
    return false;
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

#include <fcntl.h>
struct Redacted {
private:
    static int fd;

    static void open_redacted_alignments()
    {
        fd = open("redactedAlignments", O_RDWR | O_APPEND | O_CREAT | O_EXCL, 0700);
        if (fd >= 0)
            return;
        LogMsg(klogFatal, "Can't create file for redacted redacted alignments");
        exit(EX_CANTCREAT);
    }

    int64_t *start;
    size_t count_;

public:
    size_t const count() const { return count_; }
    int64_t const *begin() const { return start; }
    int64_t const *end() const { return begin() + count(); }

    Redacted(Redacted &&other) = delete;
    Redacted(Redacted const &other) = delete;
    Redacted() : start(nullptr), count_(0)
    {
        if (fd < 0) return;

        fsync(fd);

        auto const fsize = lseek(fd, 0, SEEK_END);
        assert(fsize % sizeof(*start) == 0);

        auto *const map = mmap(nullptr, fsize, PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, fd, 0);
        if (map == MAP_FAILED) {
            LogMsg(klogFatal, "failed to map redacted spots file");
            exit(EX_IOERR);
        }

        start = reinterpret_cast<int64_t *>(map);
        count_ = fsize / sizeof(*start);

        std::sort(start, start + count());

        auto last = *start;
        auto const inf = *(start + count() - 1) + 1;
        size_t dups = 0;
        for (auto i = start + 1; i < end(); ++i) {
            if (last == *i) {
                *i = inf;
                dups += 1;
            }
            else
                last = *i;
        }
        if (dups) {
            std::sort(start, start + count());
            count_ -= dups;

            ftruncate(fd, count() * sizeof(*start));
            lseek(fd, 0, SEEK_END);
        }
        mprotect(map, count() * sizeof(*start), PROT_READ);
    }
    ~Redacted() {
        if (start) {
            munmap(start, count() * sizeof(*start));
        }
    }
    bool contains(int64_t value) const {
        return std::lower_bound(begin(), end(), value) != end();
    }
    static void addSpot(int64_t const row) {
        if (fd >= 0) {
            if (write(fd, &row, sizeof(row)) == sizeof(row))
                return;
            LogMsg(klogFatal, "Can't record invalidated rows");
            exit(EX_IOERR);
        }
        open_redacted_alignments();
        addSpot(row);
    }
};

int Redacted::fd = -1;

using HiRezTimePoint = decltype(std::chrono::high_resolution_clock::now());

static std::tm make_tm(std::chrono::system_clock::time_point const &time) {
    std::tm result = {};
    auto const tt = std::chrono::system_clock::to_time_t(time);
    gmtime_r(&tt, &result);
    return result;
}

static std::string estimatedTimeOfCompletion(HiRezTimePoint const &start
                                                                       , uint64_t const completed
                                                                       , uint64_t const total)
{
    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);
    if (elapsed > elapsed.zero() && completed > 0) {
        auto const rate = double(completed) / (elapsed.count() / 1000.0);
        if (rate > 0.0) {
            auto const remainCount = total - completed;
            auto const remainTime = remainCount / rate;
            auto const eta_tm = make_tm(std::chrono::system_clock::now() + std::chrono::seconds((uint64_t)(std::ceil(remainTime))));
            char buffer[32];
            auto size = std::strftime(buffer, sizeof(buffer), "%FT%T", &eta_tm);
            return std::string(buffer, size);
        }
    }
    return "?";
}

static void redactAlignments(VCursor *const out, VCursor const *const in, bool const isPrimary, Redacted const &redacted, Output &output)
{
    auto const startTimer = std::chrono::high_resolution_clock::now();
    auto allfalse = std::vector<uint8_t>(256, 0);
    auto has_offset_type = false;

    /* MARK: input columns */
    auto const cid_spot_id     = addColumn(SPOT_ID   , "I64", in);

    /* MARK: redacted input columns */
    auto const cid_has_miss    = addColumn(HAS_MISS  , "U8" , in);
    auto const cid_has_offset  = addColumn(HAS_OFFSET, "U8" , in);
    auto const cid_mismatch    = addColumn(MISMATCH  , BASE_TYPE, in);
    auto const cid_ref_offset  = addColumn(REF_OFFSET, "I32", in);
    auto const cid_offset_type = addColumn(OFFSET_TYPE, "U8", in, has_offset_type);

    /* MARK: redacted output columns */
    auto const cid_out_has_miss    = addColumn(HAS_MISS  , "U8" , out);
    auto const cid_out_has_offset  = addColumn(HAS_OFFSET, "U8" , out);
    auto const cid_out_mismatch    = addColumn(MISMATCH  , BASE_TYPE, out);
    auto const cid_out_ref_offset  = addColumn(REF_OFFSET, "I32", out);
    auto const cid_out_offset_type = has_offset_type ? addColumn(OFFSET_TYPE, "U8", out) : 0;

    int64_t first = 0;
    uint64_t count = 0;
    uint64_t redactions = 0;
    unsigned complete = 0;

    openCursor(in, "input");
    openCursor(out, "output");

    output.changedColumns.assign({HAS_MISS, HAS_OFFSET, MISMATCH, REF_OFFSET});
    if (has_offset_type)
        output.changedColumns.push_back(OFFSET_TYPE);

    for (auto && name : output.changedColumns)
        pLogMsg(klogInfo, "redacting/updating $(col)", "col=%s", name.c_str());

    count = rowCount(in, &first, cid_spot_id);
    assert(first == 1);
    pLogMsg(klogInfo, "progress: about to process $(rows) $(kind) alignments", "kind=%s,rows=%lu", isPrimary ? "primary" : "secondary", count);

    /* MARK: Main loop over the alignments */
    for (uint64_t r = 0; r < count; ++r) {
        auto const pct = unsigned((100.0 * r) / count);
        int64_t const row = 1 + r;
        auto const spotId = cellData(SPOT_ID, cid_spot_id, row, in).value<int64_t>();
        auto has_miss    = cellData(HAS_MISS   , cid_has_miss   , row, in);
        auto has_offset  = cellData(HAS_OFFSET , cid_has_offset , row, in);
        auto ref_offset  = cellData(REF_OFFSET , cid_ref_offset , row, in);
        auto mismatch    = cellData(MISMATCH   , cid_mismatch   , row, in);
        auto offset_type = cellData(OFFSET_TYPE, cid_offset_type, row, in);

        if (pct > complete) {
            auto const etc = estimatedTimeOfCompletion(startTimer, r, count);

            pLogMsg(klogDebug, "progress: $(pct)%, $(etc) ETA", "pct=%u,etc=%s", complete = pct, etc.c_str());
            if (complete % 10 == 0)
                pLogMsg(klogInfo, "progress: $(pct)%, $(etc) ETA", "pct=%u,etc=%s", complete = pct, etc.c_str());
        }

        if (redacted.contains(spotId)) {
            if (allfalse.size() < has_miss.count)
                allfalse.resize(has_miss.count, 0);
            has_miss.data = allfalse.data();
            has_offset.data = allfalse.data();
            mismatch.count = 0;
            ref_offset.count = 0;
            offset_type.count = 0;
            ++redactions;
        }
        openRow(row, out);
        writeRow(row, has_miss   , cid_out_has_miss   , out);
        writeRow(row, has_offset , cid_out_has_offset , out);
        writeRow(row, mismatch   , cid_out_mismatch   , out);
        writeRow(row, ref_offset , cid_out_ref_offset , out);
        writeRow(row, offset_type, cid_out_offset_type, out);
        commitRow(row, out);
        closeRow(row, out);
    }
    commitCursor(out);
    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTimer).count() / 1000.0;
    pLogMsg(klogInfo, "progress: done in $(elapsed) seconds, alignments redacted: $(count)", "count=%lu,elapsed=%.0f"
            , (unsigned long)redactions
            , elapsed);
    VCursorRelease(out);
    VCursorRelease(in);
}

static void processAlignments(VCursor const *const in)
{
    auto const startTimer = std::chrono::high_resolution_clock::now();

    /* MARK: input columns */
    auto const cid_spot_id     = addColumn(SPOT_ID   , "I64", in);
    auto const cid_read        = addColumn(ALIGN_READ, "U8" , in);

    int64_t first = 0;
    uint64_t count = 0;
    uint64_t redactions = 0;
    unsigned complete = 0;

    openCursor(in, "input");

    count = rowCount(in, &first, cid_spot_id);
    assert(first == 1);
    pLogMsg(klogInfo, "progress: about to process $(rows) primary alignments", "rows=%lu", count);

    /* MARK: Main loop over the alignments */
    for (uint64_t r = 0; r < count; ++r) {
        auto const pct = unsigned((100.0 * r) / count);
        int64_t const row = 1 + r;
        auto const read = cellData(ALIGN_READ, cid_read, row, in);
        auto const spotId = cellData(SPOT_ID, cid_spot_id, row, in).value<int64_t>();

        if (pct > complete) {
            auto const etc = estimatedTimeOfCompletion(startTimer, r, count);

            pLogMsg(klogDebug, "progress: $(pct)%, $(etc) ETA", "pct=%u,etc=%s", complete = pct, etc.c_str());
            if (complete % 10 == 0)
                pLogMsg(klogInfo, "progress: $(pct)%, $(etc) ETA", "pct=%u,etc=%s", complete = pct, etc.c_str());
        }

        if (shouldFilter(read.count, (uint8_t const *)read.data)) {
            Redacted::addSpot(spotId);
            ++redactions;
        }
    }
    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTimer).count() / 1000.0;
    pLogMsg(klogInfo, "progress: done in $(elapsed) seconds, alignments to redact: $(count)", "count=%lu,elapsed=%.0f"
            , (unsigned long)redactions
            , elapsed);
    VCursorRelease(in);
}

static bool processSequenceCursors(VCursor *const out, VCursor const *const in, bool aligned, Redacted const &redacted, Output &output)
{
    auto const startTimer = std::chrono::high_resolution_clock::now();
    auto outRead = std::vector<uint8_t>(256, 'N');
    auto outReadFilter = std::vector<uint8_t>(2, SRA_READ_FILTER_REDACTED);
    char const *readFilterColName = nullptr;

    /* MARK: input columns */
    auto const readColName     = aligned ? CMP_READ : READ;
    auto const cid_pr_id       = aligned ? addColumn(ALIGN_ID, "I64", in) : 0;
    auto const cid_readstart   = addColumn("READ_START" , "I32", in);
    auto const cid_read_type   = addColumn("READ_TYPE"  , "U8" , in);
    auto const cid_readlen     = addColumn("READ_LEN"   , "U32", in);
    auto const cid_read        = addColumn(readColName  , BASE_TYPE, in);
    auto const cid_read_filter = addColumn("READ_FILTER", "RD_FILTER", "U8", in, readFilterColName);

    /* MARK: output columns */
    auto const cid_out_read = addColumn(readColName, BASE_TYPE, out);
    auto const cid_out_read_filter = addColumn("READ_FILTER", "U8", out);

    int64_t first = 0;
    uint64_t count = 0;
    unsigned complete = 0;
    auto someRedacted = false;

    auto redactedStart = redacted.begin() ? redacted.begin() : &first;
    auto const redactedEnd = redactedStart + redacted.count();

    openCursor(in, "input");
    openCursor(out, "output");

    output.changedColumns.assign({readColName, "RD_FILTER", "READ_FILTER"});
    if (aligned)
        output.changedColumns.push_back("CMP_ALTREAD");
    else
        output.changedColumns.push_back("ALTREAD");

    for (auto && name : output.changedColumns)
        pLogMsg(klogInfo, "redacting/updating $(col)", "col=%s", name.c_str());

    count = rowCount(in, &first, cid_read_type);
    assert(first == 1);
    pLogMsg(klogDebug, "using $(read) and $(filter)", "read=%s,filter=%s", readColName, readFilterColName);
    pLogMsg(klogInfo, "progress: about to process $(rows) spots", "rows=%lu", count);

    /* MARK: Main loop over the spots */
    for (uint64_t r = 0; r < count; ++r) {
        auto const pct = unsigned((100.0 * r) / count);
        int64_t const row = 1 + r;
        auto const readstart  = cellData("READ_START" , cid_readstart  , row, in);
        auto const readtype   = cellData("READ_TYPE"  , cid_read_type  , row, in);
        auto const readlen    = cellData("READ_LEN"   , cid_readlen    , row, in);
        auto const nreads = readlen.count;
        auto const prId = cellData(ALIGN_ID, cid_pr_id, row, in).typed<int64_t>();
        auto read = cellData(readColName, cid_read, row, in);
        auto readFilter = cellData(readFilterColName, cid_read_filter, row, in);
        bool redact = false;
        bool fromRedacted = false;
        auto bases = aligned ? readlen.typed<uint32_t>()[nreads - 1] + readstart.typed<int32_t>()[nreads - 1] : read.count;

        if (pct > complete) {
            auto const etc = estimatedTimeOfCompletion(startTimer, r, count);

            pLogMsg(klogDebug, "progress: $(pct)%, $(etc) ETA", "pct=%u,etc=%s", complete = pct, etc.c_str());
            if (complete % 10 == 0)
                pLogMsg(klogInfo, "progress: $(pct)%, $(etc) ETA", "pct=%u,etc=%s", complete = pct, etc.c_str());
        }

        read.copyTo(outRead);
        readFilter.copyTo(outReadFilter);

        if (redactedStart && redactedStart < redactedEnd && *redactedStart == row) {
            redact = true;
            fromRedacted = true;
            do {
                ++redactedStart;
            } while (redactedStart < redactedEnd && *redactedStart == row);
        }
        if (aligned) {
            auto const redactable = std::count_if(prId.begin(), prId.end(), [](int64_t const id) { return id == 0; });

            redact = redact || (redactable > 0 && redactUnalignedReads(prId, readlen, read));
        }
        else
            redact = redactReads(readstart, readtype, readlen, read);

        if (redact) {
            if (!fromRedacted)
                Redacted::addSpot(row);

            std::fill(outRead.begin(), outRead.end(), 'N');
            std::fill(outReadFilter.begin(), outReadFilter.end(), SRA_READ_FILTER_REDACTED);

            someRedacted = true;
            if (dispositionCount[dspcRedactedReads] == 0) {
                pLogMsg(klogInfo, "first redacted spot: $(row)", "row=%lu", (unsigned long)row);
            }
            dispositionCount[dspcRedactedReads] += nreads;
            dispositionCount[dspcRedactedSpots] += 1;
            dispositionBaseCount[dspcRedactedBases] += bases;

            read.data = outRead.data();
            readFilter.data = outReadFilter.data();
        }
        else {
            dispositionCount[dspcKeptReads] += nreads;
            dispositionCount[dspcKeptSpots] += 1;
            dispositionBaseCount[dspcKeptBases] += bases;
        }

        openRow(row, out);
        writeRow(row, read, cid_out_read, out);
        writeRow(row, readFilter, cid_out_read_filter, out);
        commitRow(row, out);
        closeRow(row, out);
    }
    commitCursor(out);

    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTimer).count() / 1000.0;
    pLogMsg(klogInfo, "progress: done in $(elapsed) seconds; redacted: $(spots) spots, $(reads) reads, $(bases) bases", "spots=%lu,reads=%lu,bases=%lu,elapsed=%.0f"
            , (unsigned long)dispositionCount[dspcRedactedSpots]
            , (unsigned long)dispositionCount[dspcRedactedReads]
            , (unsigned long)dispositionBaseCount[dspcRedactedBases]
            , elapsed);

    VCursorRelease(out);
    VCursorRelease(in);

    return someRedacted;
}

static void copyColumns(  Output const &output
                        , char const *const tableName
                        , char const *const from
                        , char const *const to
                        , VDBManager *const mgr)
{
    if (output.changedColumns.empty()) {
        pLogMsg(klogDebug, "no columns were changed in $(table) from $(source)", "table=%s,source=%s"
                , tableName ? tableName : "<implied>"
                , from);
        return;
    }

    /// This is split into three sections to ensure that
    /// the VDB objects are NOT open during the purely file system operations.
    auto dropped = std::vector<std::string>();
    {
        /// First drop columns from the destination.
        /// Done using VDB because the columns might be metadata only.
        auto const dstTbl = tableName ? openUpdateDb(to, tableName, mgr) : openUpdateTbl(to, mgr);

        for (auto && colName : output.changedColumns) {
            pLogMsg(klogDebug, "going to copy $(table).$(column) from $(source) to $(dest)", "table=%s,column=%s,source=%s,dest=%s"
                    , tableName ? tableName : "<implied>"
                    , colName.c_str()
                    , from
                    , to);

            if (dropColumn(dstTbl, colName.c_str()))
                dropped.push_back(colName);
            else {
                pLogMsg(klogDebug, "$(table).$(column) in $(dest) does not exist", "table=%s,column=%s,dest=%s"
                        , tableName ? tableName : "<implied>"
                        , colName.c_str()
                        , to);
            }
        }
        VTableRelease(dstTbl);
        /// The destination is closed;
    }

    /// First try purely file system operations.
    /// This will copy the big columns.
    auto not_copied = std::vector<std::string>();
    {
        char const *const fmt = "%s/tbl/%s/col";
        KDirectory *const dst = tableName
                              ? openDirUpdate(fmt, to, tableName)
                              : openDirUpdate(fmt + 7, to);
        KDirectory const *const src = tableName
                                    ? openDirRead(fmt, from, tableName)
                                    : openDirRead(fmt + 7, from);

        for (auto && colName : dropped) {
            auto const column = colName.c_str();
            auto const rc = KDirectoryCopyPaths(src, dst, true, column, column);
            if (rc)
                not_copied.push_back(colName);
        }

        KDirectoryRelease(src);
        KDirectoryRelease(dst);
    }
    if (not_copied.empty())
        return;

    /// Finally, copy the metadata only columns.
    /// By definition, these are tiny.
    auto const dstTbl = tableName ? openUpdateDb(to, tableName, mgr) : openUpdateTbl(to, mgr);
    for (auto && colName : not_copied) {
        auto const column = colName.c_str();
        pLogMsg(klogInfo, "couldn't copy physical column $(column); trying metadata copy", "column=%s", column);

        auto const srcTbl = tableName ? openReadDb(from, tableName, mgr) : openReadTbl(from, mgr);
        /* could not copy the physical column; try the metadata node */
        if (VTableHasStaticColumn(srcTbl, column)) {
            copyNodeValue(openNodeUpdate(dstTbl, "col/%s", column), openNodeRead(srcTbl, "col/%s", column));
        }
        else {
            pLogMsg(klogFatal, "can't copy replacement $(column) column", "column=%s", column);
            exit(EX_DATAERR);
        }
        VTableRelease(srcTbl);
    }
    VTableRelease(dstTbl);
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
        auto out = createOutputs(args, mgr, in, schema);
        auto const seqTableName = in.noDb ? nullptr : SEQUENCE_TABLE;

        if (isAligned)
            processAlignmentTable(in.primaryAlignment);

        auto const someRedacted = processSequenceTable(out.sequence, in.sequence, isAligned);

        if (someRedacted && isAligned) {
            redactAlignmentTable(out.primaryAlignment, in.primaryAlignment, true);
            if (in.secondaryAlignment != nullptr)
                redactAlignmentTable(out.secondaryAlignment, in.secondaryAlignment, false);
        }
        else {
            VTableRelease(in.primaryAlignment);
            VTableRelease(in.secondaryAlignment);
            VTableRelease(out.primaryAlignment.tbl);
            VTableRelease(out.secondaryAlignment.tbl);
        }

        if (someRedacted) {
            /// MARK: Copy changed columns to temp object

            copyColumns(out.sequence, seqTableName, TEMP_MAIN_OBJECT_NAME, input, mgr);
            copyColumns(out.primaryAlignment, PRI_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
            copyColumns(out.secondaryAlignment, SEC_ALIGN_TABLE, TEMP_MAIN_OBJECT_NAME, input, mgr);
        }
        /// MARK: Save redaction counts in metadata
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

static void redactAlignmentTable(Output &output, VTable const *const input, bool const isPrimary)
{
    VCursor *out = NULL;
    VCursor const *in = NULL;
    {
        rc_t const rc = VTableCreateCursorWrite(output.tbl, &out, kcmInsert);
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
    VTableRelease(output.tbl); output.tbl = nullptr;
    redactAlignments(out, in, isPrimary, Redacted(), output);
}

static void processAlignmentTable(VTable const *const input)
{
    VCursor const *in = NULL;
    {
        rc_t const rc = VTableCreateCursorRead(input, &in);
        if (rc != 0) {
            LogErr(klogFatal, rc, "Failed to create input cursor!");
            exit(EX_NOINPUT);
        }
    }
    processAlignments(in);
}

static bool processSequenceTable(Output &output, VTable const *const input, bool const aligned)
{
    VCursor *out = NULL;
    VCursor const *in = NULL;
    {
        rc_t const rc = VTableCreateCursorWrite(output.tbl, &out, kcmInsert);
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
    VTableRelease(output.tbl); output.tbl = nullptr;

    return processSequenceCursors(out, in, aligned, Redacted(), output);
}

static Outputs createOutputs(  Args *const args
                             , VDBManager *const mgr
                             , Inputs const &inputs
                             , VSchema const *schema)
{
    auto const schemaType = inputs.schemaType.c_str();
    Outputs out = {};

    if (inputs.noDb) {
        assert(inputs.primaryAlignment == nullptr && inputs.secondaryAlignment == nullptr);
        rc_t const rc = VDBManagerCreateTable(mgr, &out.sequence.tbl, schema
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
            rc_t const rc = VDatabaseCreateTable(db, &out.sequence.tbl, SEQUENCE_TABLE, kcmCreate | kcmMD5, SEQUENCE_TABLE);
            if (rc != 0) {
                LogErr(klogFatal, rc, "Failed to create output sequence table");
                exit(EX_CANTCREAT);
            }
        }
        if (inputs.primaryAlignment != nullptr) {
            rc_t const rc = VDatabaseCreateTable(db, &out.primaryAlignment.tbl, PRI_ALIGN_TABLE, kcmCreate | kcmMD5, PRI_ALIGN_TABLE);
            if (rc != 0) {
                LogErr(klogFatal, rc, "Failed to create output primary alignment table");
                exit(EX_CANTCREAT);
            }
        }
        if (inputs.secondaryAlignment != nullptr) {
            rc_t const rc = VDatabaseCreateTable(db, &out.secondaryAlignment.tbl, SEC_ALIGN_TABLE, kcmCreate | kcmMD5, SEC_ALIGN_TABLE);
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
        rc = KDirectoryResolvePath(ndir, true, pattern, len, "%s/XXXXXX", tmp);
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

    pLogMsg(klogFatal, "Can't cd to temp directory $(dir)", "dir=%s", pattern);
    exit(EX_TEMPFAIL);
}
