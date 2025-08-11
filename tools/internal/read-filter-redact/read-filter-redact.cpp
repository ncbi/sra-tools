/*******************************************************************************
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
 * =============================================================================
 */

//#define NOMINMAX /* std::max on Windows */
#include <kapp/main.h>

#include <sra/wsradb.h> /* spotid_t */
#include <sra/sradb-priv.h> /* SRASchemaMake */

#include <vdb/cursor.h> /* VCursor */
#include <vdb/database.h> /* VDatabase */
#include <vdb/manager.h> /* VDBManager */
#include <vdb/schema.h> /* VSchemaRelease */
#include <vdb/table.h> /* VDBTable */
#include <vdb/vdb-priv.h> /* VDatabaseIsCSRA */

#include <kdb/manager.h> /* KDBPathType */
#include <kdb/meta.h> /* KMetadataRelease */
#include <kdb/namelist.h> /* KMDataNodeListChild */

#include <kfs/directory.h> /* KDirectory */
#include <kfs/file.h> /* KFile */

#include <klib/debug.h> /* DBGMSG */
#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/printf.h> /* string_printf */
#include <klib/rc.h> /* RC */
#include <klib/status.h> /* STSMSG */
#include <klib/time.h> /* KTimeLocal */

#include <fingerprint.hpp>

#include <os-native.h>

#include <assert.h>
#include <ctype.h> /* isblank */
#include <stdio.h> /* sscanf */
#include <inttypes.h> /* PRId64 */
#include <stdlib.h> /* exit */
#include <string.h> /* memset */

using namespace std;

#define DISP_RC(rc,msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))

static KDirectory* __SpotIteratorDirectory = nullptr;

typedef struct CmdLine {
    const char* run = nullptr;
    const char* file = nullptr;
    bool redactReads = false;
} CmdLine;

typedef struct SpotIterator {
    spotid_t crnSpotId;
    spotid_t spotToReduct;

    const char* filename;
    const KFile* file;

    uint64_t maxSpotId;

    char buffer[256];
    size_t inBuffer; /* characters in buffer */

    size_t filePos;
    bool eof;
    size_t line;

    bool hasCh;
    char ch;
} SpotIterator;

struct Db {
    bool redactReads = false;

    const char* run = nullptr;

    VDBManager* mgr = nullptr;
    VTable *tbl = nullptr;

    const VCursor *rCursor = nullptr;
    uint32_t rFilterIdx = 0;
    uint32_t rReadIdx = 0;
    uint32_t rReadLenIdx = 0;

    Fingerprint update_in_fp; /* updated spots before redaction*/
    Fingerprint update_out_fp;/* updated spots after redaction */
    Fingerprint in_fp; /* entire input run */
    Fingerprint out_fp;/* entire output run */

    VCursor *wCursor = nullptr;
    uint32_t wFilterIdx = 0;
    uint32_t wReadIdx = 0;

    KMetadata *meta = nullptr;

    bool locked = false;

    spotid_t nSpots = 0;
    spotid_t redactedSpots = 0;
};
/** Init the static directory object */
static rc_t SpotIteratorInitDirectory(void) {
    if (__SpotIteratorDirectory) {
        return 0;
    }
    else {
        rc_t rc = KDirectoryNativeDir(&__SpotIteratorDirectory);
        DISP_RC(rc, "while calling KDirectoryNativeDir");
        return rc;
    }
}

/** Check file existance */
static bool SpotIteratorFileExists(const char* path, ...) {
    bool found = false;

    if (SpotIteratorInitDirectory() == 0) {
        va_list args;
        va_start(args, path);
        {
            uint32_t type
                = KDirectoryVPathType(__SpotIteratorDirectory, path, args);
            found = (type != kptNotFound);
            va_end(args);
        }
    }

    return found;
}

static bool SpotIteratorBufferAdd(SpotIterator* self, char ch)
{
    assert(self);

    if (self->inBuffer >= (sizeof self->buffer - 1))
    {   return false; }

    self->buffer[self->inBuffer++] = ch;
    self->buffer[self->inBuffer] = '\0';

    return true;
}

/** Read a character from input file */
static rc_t SpotIteratorFileReadCharWithEof(SpotIterator* self,
    char* buffer)
{
    rc_t rc = 0;
    size_t num_read = 0;

    assert(self);

    /* get back the saved character */
    if (self->hasCh) {
        buffer[0] = self->ch;
        self->hasCh = false;
    }
    else {
        rc = KFileRead(self->file, self->filePos, buffer, 1, &num_read);
        if (rc == 0) {
            if (num_read == 0) {
                self->eof = true;
            }
            else { self->filePos += num_read; }
        }
        else {
            PLOGERR(klogErr, (klogErr, rc,
                "on line $(lineno) while reading file '$(path)'",
                PLOG_U64(lineno) ",path=%s", self->line, self->filename));
        }
    }

    return rc;
}

/** Read a line from input file */
static rc_t SpotIteratorReadLine(SpotIterator* self)
{
    rc_t rc = 0;
    bool overflow = false; /* input buffer overflow */

    /* to skip leading/traling spaces */
    enum ELane {
        eBefore,
        eIn,
        eAfter
    } state = eBefore;

    if (self->eof) {
        return rc;
    }

    assert(self);

    ++self->line;
    self->inBuffer = 0;

    while (!overflow && !self->eof) /* do until in-buffer overflow or EOF*/{
        char ch = 0;
        /* get next characted */
        if ((rc = SpotIteratorFileReadCharWithEof(self, &ch)) != 0) {
            return rc;
        }

        if (!self->eof) {
            /* treat leading/trailing spaces */
            switch (state) {
                /* skip leading spaces */
                case eBefore:
                    if (isblank(ch)) {
                        continue;
                    }
                    else {
                        state = eIn;
                    }
                    break;
                case eIn:
                    if (isblank(ch)) {
                        state = eAfter;
                        continue;
                    }
                    break;
                /* skip trailing spaces */
                case eAfter:
                    if (isblank(ch)) {
                        continue;
                    }
                    else if (ch != '\n' && ch != '\r') {
                        rc = RC(rcExe, rcFile, rcReading, rcChar, rcUnexpected);
                        PLOGERR(klogErr, (klogErr, rc, "bad symbol '$(char)' "
                            "on line $(lineno) in file '$(path)': '$(line)...'",
                            "char=%c," PLOG_U64(lineno) ",path=%s,line=%s",
                            ch, self->line, self->filename, self->buffer))
                        ;
                        return rc;
                    }
                    break;
            }

            /* add next non-blank characted */
            overflow = !SpotIteratorBufferAdd(self, ch);
            if (!overflow) {
      /* all combinations as "\r", "\n", "\r\n", "\n\r" are considered as EOL */
                if (ch == '\n' || ch == '\r') {
                    char c1 = 0;
                    if ((rc = SpotIteratorFileReadCharWithEof(self, &c1)) != 0)
                    {
                        return rc;
                    }
                    if (self->eof) {
                        break;
                    }
                    else if ((c1 != '\n' && c1 != '\r') || (ch == c1)) {
       /* save the character when EOL is a single character (WINDOWS):
          will be get back in SpotIteratorFileReadCharWithEof */
                        self->ch = c1;
                        self->hasCh = true;
                        break;
                    }
                    else {
                        overflow = !SpotIteratorBufferAdd(self, c1);
                        break;
                    }
                }
            }
        }
    }

    /* remove EOL */
    if (!overflow) {
        bool done = false;

        while (self->inBuffer > 0 && !done) {
            switch (self->buffer[self->inBuffer - 1]) {
                case '\n': case '\r':
                    self->buffer[--self->inBuffer] = '\0';
                    break;
                default:
                    done = true;
                    break;
            }
        }
    }
    else {
        rc = RC(rcExe, rcFile, rcReading, rcString, rcTooLong);
        PLOGERR(klogErr, (klogErr, rc,
            "on line $(lineno) while reading file '$(path)': '$(line)...'",
            PLOG_U64(lineno) ",path=%s,line=%s",
            self->line, self->filename, self->buffer));
    }

    return rc;
}

/** Get next spot from input file */
static rc_t SpotIteratorReadSpotToRedact(SpotIterator* self)
{
    rc_t rc = 0;

    assert(self);

    while (rc == 0 && ! self->eof) {
        rc = SpotIteratorReadLine(self);

        /* skip empty lines */
        if ((rc == 0) && (self->inBuffer > 0)) {
            spotid_t spot = 0;

            /* make sure the line contains digits only */
            for (size_t i = 0; i < self->inBuffer; ++i) {
                if (!isdigit(self->buffer[i])) {
                    rc = RC(rcExe, rcFile, rcReading, rcChar, rcUnexpected);
                    PLOGERR(klogErr, (klogErr, rc,
                            "character '$(char)' on line $(lineno)"
                            " while reading file '$(path)': '$(line)'",
                        "char=%c," PLOG_U64(lineno) ",path=%s,line=%s",
                        self->buffer[i], self->line,
                        self->filename, self->buffer));
                    return rc;
                }
            }

            sscanf(self->buffer, "%" PRId64 "", &spot);

            if (spot == 0) {
                rc = RC(rcExe, rcFile, rcReading, rcString, rcInvalid);
                PLOGERR(klogErr, (klogErr, rc,
                    "bad spot id '0' on line $(lineno) "
                    "while reading file '$(path)': '$(line)'",
                    PLOG_U64(lineno) ",path=%s,line=%s",
                    self->line, self->filename, self->buffer));
            }
            else if (spot == self->spotToReduct) {
                rc = RC(rcExe, rcFile, rcReading, rcString, rcInvalid);
                PLOGERR(klogErr, (klogErr, rc, "duplicated spot id '$(spot)' "
                    "on line $(lineno) while reading file '$(path)': '$(line)'",
                    PLOG_U32(spot) "," PLOG_U64(lineno) ",path=%s,line=%s",
                    spot, self->line, self->filename, self->buffer));
            }
            else if (spot < self->spotToReduct) {
                rc = RC(rcExe, rcFile, rcReading, rcString, rcInvalid);
                PLOGERR(klogErr, (klogErr, rc, "File '$(path)' is unsorted. "
                    "$(id) < $(last). See line $(lineno): '$(line)'",
                    "path=%s," PLOG_U32(id) "," PLOG_U32(last) ","
                        PLOG_U64(lineno) ",line=%s",
                    self->filename, spot, self->spotToReduct,
                    self->line, self->buffer));
            }
            else if (spot > (spotid_t)self->maxSpotId) {
                rc = RC(rcExe, rcFile, rcReading, rcString, rcInvalid);
                PLOGERR(klogErr, (klogErr, rc,
                    "spotId $(spot) on line $(lineno) "
                    "of file '$(path)' is bigger that the max spotId $(max): "
                        "'$(line)'",
                    PLOG_U32(spot) "," PLOG_U64(lineno) ",path=%s,"
                    PLOG_U32(max) ",line=%s",
                    spot, self->line, self->filename, self->maxSpotId,
                    self->buffer));
            }
            else {
                self->spotToReduct = spot;
                self->inBuffer = 0;
            }
            break;
        }
    }

    return rc;
}

static rc_t SpotIteratorInit(const char* redactFileName,
    const Db* db, SpotIterator* self)
{
    rc_t rc = 0;
    int64_t first = 0;

    assert(self && db);

    memset(self, 0, sizeof *self);

    self->crnSpotId = 1;

    rc = VCursorIdRange
        (db->rCursor, db->rFilterIdx, &first, &self->maxSpotId);
    DISP_RC(rc, "while calling VCursorIdRange");

    self->spotToReduct = first - 1;

    if (rc == 0) {
        rc = SpotIteratorInitDirectory();
    }

    if (rc == 0) {
        self->filename = redactFileName;
        rc = KDirectoryOpenFileRead
            (__SpotIteratorDirectory, &self->file, "%s", self->filename);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "while opening file '$(path)'", "path=%s", self->filename));
        }
    }

    if (rc == 0) {
        rc = SpotIteratorReadSpotToRedact(self);
    }

    return rc;
}

static rc_t SpotIteratorDestroy(SpotIterator* it)
{
    rc_t rc = 0;

    assert(it);

    rc = KFileRelease(it->file);

    it->file = nullptr;
    it->inBuffer = 0;
    it->hasCh = false;

    {
        rc_t rc2 = KDirectoryRelease(__SpotIteratorDirectory);
        if (rc == 0)
        {   rc = rc2; }
        __SpotIteratorDirectory = nullptr;
    }

    return rc;
}

/** Get next spot id, check whether it should be redacted.
Returns false if maxSpotId reached */
static bool SpotIteratorNext(SpotIterator* self, rc_t* rc,
    int64_t* spot, bool* toRedact)
{
    bool hasNext = false;

    assert(self && rc && spot && toRedact);

    *rc = 0;
    *toRedact = false;

    if (self->crnSpotId <= (spotid_t)self->maxSpotId) {
        hasNext = true;
        *spot = self->crnSpotId++;
    }

    if (*spot == self->spotToReduct) {
        *toRedact = true;
        *rc = SpotIteratorReadSpotToRedact(self);
        if (*rc != 0) {
            hasNext = false;
        }
    }

    return hasNext;
}

static rc_t DbInit(rc_t rc, const CmdLine* args, Db* db)
{
    const char read_name[] = "READ";
    const char read_filter_name[]   = "READ_FILTER";
    const char read_len_name[] = "READ_LEN";

    assert(args && db);

    db->redactReads = args->redactReads;
    db->run = args->run;

    if (rc == 0) {
        rc = VDBManagerMakeUpdate(&db->mgr, nullptr);
        DISP_RC(rc, "while calling VDBManagerMakeUpdate");
    }

    if (rc == 0) {
        rc = VDBManagerWritable(db->mgr, args->run);
        if (rc != 0) {
            if (GetRCState(rc) == rcLocked)
            {
                rc = VDBManagerUnlock(db->mgr, args->run);
                if (rc != 0) {
                    PLOGERR(klogErr, (klogErr, rc,
                        "while calling VDBManagerUnlock('$(run)')",
                        "run=%s", args->run));
                }
                db->locked = true;
            }
            else {
                PLOGERR(klogErr, (klogErr, rc,
                    "while calling VDBManagerWritable('$(run)')",
                    "run=%s", args->run));
                if (rc == RC(rcDB, rcPath, rcAccessing, rcPath, rcReadonly)) {
                    PLOGERR(klogErr, (klogErr, rc,
                        "N.B. It is possible '$(run)' was not locked properly"
                        , "run=%s", args->run));
                }
            }
        }
    }

    if (rc == 0) {
        db->locked = true; /* has to be locked in production mode */
        rc = VDBManagerOpenTableUpdate (db->mgr, &db->tbl, nullptr, args->run);
        if (rc != 0) {
            VDatabase *vdb;
            rc_t rc2 = VDBManagerOpenDBUpdate ( db->mgr, &vdb, nullptr,
                                                args->run );
            if( rc2 == 0) {
                if (VDatabaseIsCSRA(vdb)) {
                    rc2 = RC(rcExe,
                        rcDatabase, rcAccessing, rcDatabase, rcWrongType);
                    PLOGERR(klogErr, (klogErr, rc, "'$(run)' is cSRA",
                        "run=%s", args->run));
                }
                if (rc2 == 0) {
                    rc2 = VDatabaseOpenTableUpdate(vdb, &db->tbl, "SEQUENCE");
                    if (rc2 == 0)
                        rc = 0;
                }
                VDatabaseRelease ( vdb );
            }
        }
        if(rc != 0){
            PLOGERR(klogErr, (klogErr, rc,
                "while opening run '$(run)'", "run=%s", args->run));
        }
    }

    if( rc == 0) {
        rc = VTableOpenMetadataUpdate ( db->tbl, & db->meta );
        DISP_RC(rc, "while Opening Metadata");
    }

    if( rc == 0) {
        rc = VTableCreateCursorRead(db->tbl, &db->rCursor);
        DISP_RC(rc, "while creating read cursor");
    }

    if (rc == 0) {
        rc = VCursorAddColumn(db->rCursor, &db->rFilterIdx,
            "%s", read_filter_name);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "while adding $(name) to read cursor",
                "name=%s", read_filter_name));
        }
    }

    if (rc == 0) {
        rc = VCursorAddColumn(db->rCursor, &db->rReadIdx, "%s", read_name);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "while adding $(name) to read cursor", "name=%s", read_name));
        }
    }

    if (rc == 0) {
        rc = VCursorAddColumn(db->rCursor, &db->rReadLenIdx,
            "%s", read_len_name);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "while adding $(name) to read cursor",
                "name=%s", read_len_name));
        }
    }

    if (rc == 0) {
        rc = VCursorOpen(db->rCursor);
        DISP_RC(rc, "while opening read cursor");
    }

    if (rc == 0) {
        rc = VTableCreateCursorWrite(db->tbl, &db->wCursor, kcmInsert);
        DISP_RC(rc, "while creating write cursor");
        if (rc == 0) {
            rc = VCursorAddColumn(db->wCursor, &db->wFilterIdx,
                "%s", read_filter_name);
            if (rc != 0) {
                PLOGERR(klogErr, (klogErr, rc,
                    "while adding $(name) to write cursor", "name=%s", read_filter_name));
            }
        }
        if (rc == 0 && db->redactReads) {
            rc = VCursorAddColumn(db->wCursor, &db->wReadIdx, "%s", read_name);
            if (rc != 0)
                PLOGERR(klogErr, (klogErr, rc,
                    "while adding $(name) to write cursor", "name=%s", read_name));
        }
        if (rc == 0) {
            rc = VCursorOpen(db->wCursor);
            DISP_RC(rc, "while opening write cursor");
        }
    }

    return rc;
}

static rc_t DbDestroy(Db* db)
{
    rc_t rc = 0;

    assert(db);

    {
        rc_t rc2 = KMetadataRelease(db->meta);
        db->meta = nullptr;
        if (rc == 0)
        {
            rc = rc2;
        }
    }

    {
        rc_t rc2 = VCursorRelease(db->rCursor);
        db->rCursor = nullptr;
        if (rc == 0)
        {   rc = rc2; }
    }

    {
        rc_t rc2 = VCursorRelease(db->wCursor);
        db->wCursor = nullptr;
        if (rc == 0)
        {   rc = rc2; }
    }

    {
        rc_t rc2 = VTableRelease(db->tbl);
        db->tbl = nullptr;
        if (rc == 0)
        {   rc = rc2; }
    }

    if (db->locked) {
        rc_t rc2 = VDBManagerLock(db->mgr, "%s", db->run);
        if (rc == 0)
        {   rc = rc2; }
    }

    {
        rc_t rc2 = VDBManagerRelease(db->mgr);
        db->mgr = nullptr;
        if (rc == 0)
        {   rc = rc2; }
    }

    return rc;
}

static rc_t Work(Db* db, SpotIterator* it)
{
    rc_t rc = 0;
    bool toRedact = false;
    int64_t row_id = 0;
    spotid_t nSpots = 0;
    spotid_t redactedSpots = 0;

    void* filter = nullptr;
    uint32_t filter_nreads_max = 0;

    void* read = nullptr;
    uint32_t spot_len_max = 0;

    assert(db && it);

    while (rc == 0 && SpotIteratorNext(it, &rc, &row_id, &toRedact)) {
        uint32_t filter_nreads = 0;
        const void* filter_buffer = nullptr;
        const void* filter_buffer_out = nullptr;

        uint32_t spot_len = 0;
        char* read_buffer = nullptr;
        const char* read_buffer_out = nullptr;

        uint32_t read_len_len = 0;
        uint32_t* read_len_buffer = nullptr;

        rc = Quitting();

        ++nSpots;

        if (rc == 0) {
            rc = VCursorCellDataDirect(db->rCursor, row_id, db->rFilterIdx,
                nullptr, &filter_buffer, nullptr, &filter_nreads);
            DISP_RC(rc, "while reading READ_FILTER");
        }

        if (rc == 0) {
            rc = VCursorCellDataDirect(db->rCursor, row_id, db->rReadLenIdx,
                nullptr, (const void**)&read_len_buffer, nullptr,
                &read_len_len);
            DISP_RC(rc, "while reading READ_LEN");
        }

        if (rc == 0 && db->redactReads) {
            rc = VCursorCellDataDirect(db->rCursor, row_id, db->rReadIdx,
                nullptr, (const void**)&read_buffer, nullptr, &spot_len);
            DISP_RC(rc, "while reading READ");

            for (uint32_t i = 0, start = 0; i < read_len_len;
                start += read_len_buffer[i++])
            {
                db->in_fp.record(
                    string(read_buffer + start, read_len_buffer[i]));
            }
        }

        if (rc == 0 && toRedact) {
            if (filter_nreads > filter_nreads_max) {
                filter_nreads_max = filter_nreads * 2;
                if (filter == nullptr)
                    filter = malloc(filter_nreads_max);
                else
                    filter = realloc(filter, filter_nreads_max);
                if (filter == nullptr)
                    rc = RC(rcExe, rcData, rcAllocating, rcMemory, rcExhausted);
                else
                    memset(filter, SRA_READ_FILTER_REDACTED, filter_nreads_max);
            }
            filter_buffer_out = filter;

            ++redactedSpots;
            DBGMSG(DBG_APP,DBG_COND_1,
                ("Redacting spot %d: %d reads\n", row_id, filter_nreads));

            if (rc == 0) {
                if (!db->redactReads) {
                    rc = VCursorCellDataDirect(
                        db->rCursor, row_id, db->rReadIdx,nullptr,
                        (const void**)&read_buffer, nullptr, &spot_len);
                    DISP_RC(rc, "while reading READ");
                }
                else {
                    if (spot_len > spot_len_max) {
                        spot_len_max = spot_len * 2;
                        if (read == nullptr)
                            read = malloc(spot_len_max);
                        else
                            read = realloc(read, spot_len_max);
                        if (read == nullptr)
                            rc = RC(rcExe,
                                rcData, rcAllocating, rcMemory, rcExhausted);
                        else
                            memset(read, 'N', spot_len_max);
                    }
                    read_buffer_out = (char*)read;
                }
            }

            for (uint32_t i = 0, start = 0; db->redactReads && i < read_len_len;
                start += read_len_buffer[i++])
            {
                db->update_in_fp.record(
                    string(read_buffer + start, read_len_buffer[i]));
                db->update_out_fp.record(
                    string((char*)read + start, read_len_buffer[i]));
            }
        }
        else {
            filter_buffer_out = filter_buffer;
            read_buffer_out = read_buffer;
        }

        if (rc == 0 && db->redactReads)
            for (uint32_t i = 0, start = 0; i < read_len_len;
                start += read_len_buffer[i++])
            {
                db->out_fp.record(
                    string(read_buffer_out + start, read_len_buffer[i]));
            }

        if (rc == 0) {
            rc = VCursorOpenRow(db->wCursor);
            DISP_RC(rc, "while opening row to write");

            if (rc == 0) {
                rc = VCursorWrite(db->wCursor, db->wFilterIdx,
                    8 * filter_nreads, filter_buffer_out, 0, 1);
                DISP_RC(rc, "while writing READ_FILTER");
            }

            if (rc == 0 && db->redactReads) {
                rc = VCursorWrite(db->wCursor, db->wReadIdx,
                    8 * spot_len, read_buffer_out, 0, 1);
                DISP_RC(rc, "while writing READ");
            }

            if (rc == 0) {
                rc = VCursorCommitRow(db->wCursor);
                DISP_RC(rc, "while committing row");
            }
            if (rc == 0) {
                rc = VCursorCloseRow(db->wCursor);
                DISP_RC(rc, "while closing row");
            }
        }
    }

    free(filter); filter = nullptr;
    free(read); read = nullptr;

    db->nSpots = nSpots;
    db->redactedSpots = redactedSpots;

    if (rc == 0) {
        rc = VCursorCommit(db->wCursor);
        DISP_RC(rc, "while committing cursor");
    }

    return rc;
}

static uint32_t get_child_count( KMDataNode *node )
{
    uint32_t res = 0;
    KNamelist *names;
    rc_t rc = KMDataNodeListChild ( node, &names );
    DISP_RC( rc, "get_child_count:KMDataNodeListChild() failed" );
    if ( rc == 0 )
    {
        rc = KNamelistCount ( names, &res );
        DISP_RC( rc, "get_child_count:KNamelistCount() failed" );
        KNamelistRelease ( names );
    }
    return res;
}

static rc_t fill_timestring( char * s, size_t size )
{
    KTime tr;
    rc_t rc;

    KTimeLocal ( &tr, KTimeStamp() );
    rc = string_printf ( s, size, nullptr, "%lT", &tr );

    DISP_RC( rc, "fill_timestring:string_printf( date/time ) failed" );
    return rc;
}

static rc_t enter_time( KMDataNode *node, const char * key )
{
    char timestring[ 160 ];
    rc_t rc = fill_timestring( timestring, sizeof timestring );
    if ( rc == 0 )
    {
        rc = KMDataNodeWriteAttr ( node, key, timestring );
        DISP_RC( rc, "enter_time:KMDataNodeWriteAttr( timestring ) failed" );
    }
    return rc;
}

static rc_t enter_version( KMDataNode *node, const char * key )
{
    char buff[ 32 ];
    rc_t rc;

    rc = string_printf ( buff, sizeof( buff ), nullptr, "%.3V", KAppVersion() );
    assert ( rc == 0 );
    rc = KMDataNodeWriteAttr ( node, key, buff );
    DISP_RC( rc, "enter_version:KMDataNodeWriteAttr() failed" );
    return rc;
}

static rc_t enter_date_name_vers( KMDataNode *node, bool read_updated )
{
    rc_t rc = enter_time( node, "run" );
    DISP_RC( rc, "enter_date_name_vers:enter_time() failed" );
    if ( rc == 0 )
    {
        rc = KMDataNodeWriteAttr ( node, "tool", "read-filter-redact" );
        DISP_RC( rc, "enter_date_name_vers:"
            "KMDataNodeWriteAttr(tool=read-filter-redact) failed" );
        if ( rc == 0 )
        {
            rc = enter_version ( node, "vers" );
            DISP_RC( rc, "enter_date_name_vers:enter_version() failed" );
            if ( rc == 0 )
            {
                rc = KMDataNodeWriteAttr ( node, "build", __DATE__ );
                DISP_RC( rc, "enter_date_name_vers:KMDataNodeWriteAttr"
                    "(build=_DATE_) failed" );
            }
        }
    }
    if (rc == 0) {
        rc = KMDataNodeWriteAttr(node, "updated",
            read_updated ? "READ_FILTER,READ" : "READ_FILTER");
        DISP_RC(rc, "enter_date_name_vers:"
            "KMDataNodeWriteAttr(updated) failed");
    }

    return rc;
}

static rc_t enter_fingerprint( KMDataNode * node, Fingerprint & fp )
{
    KMDataNode * fp_node = nullptr;
    rc_t rc = KMDataNodeOpenNodeUpdate ( node, &fp_node, "FINGERPRINT" );
    DISP_RC(rc, "cannot KMDataNodeOpenNodeUpdate FINGERPRINT");
    if ( rc == 0 )
    {
        std::ostringstream strm;
        JSON_ostream json(strm, true);
        json << '{';
        fp.canonicalForm( json );
        json << '}';
        rc = KMDataNodeWrite ( fp_node, strm.str().data(), strm.str().size() );
        DISP_RC(rc, "cannot KMDataNodeWrite FINGERPRINT");
    }
    KMDataNodeRelease(fp_node);
    return rc;
}

static
rc_t copy_current_fingerprint(const KMDataNode* src, KMDataNode* dst)
{
    KMDataNode* fp_node = nullptr;
    rc_t rc = KMDataNodeOpenNodeUpdate(dst, &fp_node, "original");
    DISP_RC(rc, "cannot KMDataNodeOpenNodeUpdate original");

    if (rc == 0) {
        rc = KMDataNodeCopy(fp_node, src);
        DISP_RC(rc, "cannot KMDataNodeCopy original");
    }

    rc_t r2 = KMDataNodeRelease(fp_node);
    if (rc == 0 && r2 != 0)
        rc = r2;

    return rc;
}

static rc_t add_meta_node(KMDataNode* current,
    const char* name, const string& value)
{
    KMDataNode* node = nullptr;
    rc_t rc = KMDataNodeOpenNodeUpdate(current, &node, name);
    DISP_RC(rc, "cannot KMDataNodeOpenNodeUpdate QC/current");

    if (rc == 0) {
        rc = KMDataNodeWrite(node, value.c_str(), value.size());
        DISP_RC(rc, "cannot write KMDataNode QC/current");
    }

    rc_t r2 = KMDataNodeRelease(node);
    if (rc == 0 && r2 != 0)
        rc = r2;

    return rc;
}

static rc_t record_fp(KMDataNode* node, const Fingerprint& fp) {
    static string tm;

    rc_t rc = add_meta_node(node, "algorithm", fp.algorithm());
    if (rc == 0)
        rc = add_meta_node(node, "digest", fp.digest());
    if (rc == 0)
        rc = add_meta_node(node, "fingerprint", fp.JSON());
    if (rc == 0)
        rc = add_meta_node(node, "format", fp.format());
    if (rc == 0)
        rc = add_meta_node(node, "version", fp.version());
    if (rc == 0) {
        if (tm.empty()) {
            time_t t = time(nullptr);
            tm = string((const char*) & t, sizeof t);
        }
        rc = add_meta_node(node, "timestamp", tm);
    }

    return rc;
}

static rc_t add_fp_node(KMDataNode* node,
    const char* name,const Fingerprint& fp)
{
    KMDataNode* fp_node = nullptr;
    rc_t rc = KMDataNodeOpenNodeUpdate(node, &fp_node, name);

    if (rc == 0)
        rc = record_fp(fp_node, fp);

    rc_t r2 = KMDataNodeRelease(fp_node);
    if (rc == 0 && r2 != 0)
        rc = r2;

    return rc;
}

static rc_t add_fingerprint_event_meta ( Db & db, bool history = true  )
{
    KMetadata *dst_meta = db.meta;
    rc_t rc = 0;
    rc_t r2 = 0;
    char event_name[ 32 ] = "";

    const KMDataNode *cur_node = nullptr;
    if (!history)
        /* can be not found */
        rc = KMetadataOpenNodeRead(dst_meta, &cur_node, "QC/current");

    const char* top = history ? "HISTORY" : "QC/history";
    const char* next = history ? "EVENT" : "event";

    KMDataNode *hist_node = nullptr;
    rc = KMetadataOpenNodeUpdate ( dst_meta, &hist_node, top );
    DISP_RC(rc, "while Opening HISTORY Metadata");
    if ( rc == 0 ) {
        uint32_t index = get_child_count( hist_node );

        if ( index > 0 ) { /* make sure EVENT_[i-1] exists */
            const KMDataNode *node = nullptr;
            rc_t r = KMDataNodeOpenNodeRead ( hist_node, & node, "%s_%u",
                                                                  next, index );
            if ( r != 0 )
                PLOGERR(klogErr, (klogErr, r,
                    "while opening metanode HISTORY/EVENT_'$(n)'",
                    "n=%u", index));
            else
                KMDataNodeRelease ( node );
        }

        ++ index;

        { /* make sure EVENT_[i] does not exist */
            const KMDataNode *node = nullptr;
            rc_t r = KMDataNodeOpenNodeRead ( hist_node, & node, "%s_%u",
                                                                  next, index );
            if ( r == 0 ) {
                uint32_t mx = index * 2;
                KMDataNodeRelease ( node );
                r = RC ( rcExe, rcMetadata, rcReading, rcNode, rcExists );
                PLOGERR(klogErr, (klogErr, r,
                    "while opening metanode HISTORY/EVENT_'$(n)'",
                    "n=%u", index));
                r = 0;
                for ( ; index < mx; ++ index ) {
                    r = KMDataNodeOpenNodeRead ( hist_node, & node, "%s_%u",
                                                                  next, index );
                    if ( r == 0 ) {
                        KMDataNodeRelease ( node );
                        r = RC ( rcExe, rcMetadata, rcReading,
                                        rcNode, rcExists );
                        PLOGERR(klogErr, (klogErr, r,
                            "while opening metanode HISTORY/%(N)_'$(n)'",
                            "N=%s,n=%u", next, index));
                        r = 0;
                    }
                    else
                        break;
                }
                if ( r == 0 ) {
                    rc = RC ( rcExe, rcMetadata, rcReading, rcNode, rcExists );
                    LOGERR( klogErr, rc,
                        "cannot find next event metanode in HISTORY" );
                }
            }
            else
                KMDataNodeRelease ( node );
        }

        rc = string_printf ( event_name, sizeof( event_name ), nullptr, "%s_%u",
                                                               next, index );
        DISP_RC( rc, "add_fingerprint_event_meta:string_printf(EVENT_NR) failed" );

        if ( rc == 0 )
        {
            KMDataNode *event_node;
            rc = KMDataNodeOpenNodeUpdate ( hist_node, &event_node,
                                            event_name );
            DISP_RC( rc, "add_fingerprint_event_meta:KMDataNodeOpenNodeUpdate"
                "('EVENT_NR') failed" );
            if ( rc == 0 )
            {
              if ( history) {
                rc = enter_date_name_vers( event_node, db.redactReads );
/*
                if ( rc == 0 )
                {
                    rc = enter_fingerprint ( event_node, db.update_in_fp );
                }
*/
              }
              else {
                  if (rc == 0)
                      rc = add_meta_node(event_node, "reason", "REDACTION");
                  if (rc == 0) {
                      if (cur_node != nullptr)
                          rc = copy_current_fingerprint(cur_node, event_node);
                      else {
                          rc = add_fp_node(event_node, "original", db.in_fp);
                          DISP_RC(rc,
                              "cannot create original fingerprint node");
                      }
                  }
                  if (rc == 0) {
                      rc = add_fp_node(event_node,
                          "removed", db.update_in_fp);
                      DISP_RC(rc, "cannot create removed fingerprint node");
                  }
                  if (rc == 0) {
                      rc = add_fp_node(event_node,
                          "added", db.update_out_fp);
                      DISP_RC(rc, "cannot create added fingerprint node");
                  }
                  KMDataNodeRelease(cur_node);
              }
              KMDataNodeRelease ( event_node );
            }
        }
    }

    r2 = KMDataNodeRelease ( hist_node );
    if ( r2 != 0 && rc == 0 )
        rc = r2;
    return rc;
}

static rc_t add_history_meta(Db& db) {
    return add_fingerprint_event_meta(db);
}

static rc_t add_current_meta(Db& db) {
    return add_fingerprint_event_meta(db, false);
}

static rc_t update_current_meta(Db& db) {
    KMDataNode* node = nullptr;

    rc_t rc = KMetadataOpenNodeUpdate(db.meta, &node, "QC/current");
    DISP_RC(rc, "while Opening QC/current Metadata");

    if (rc == 0) {
        rc = record_fp(node, db.out_fp);
        DISP_RC(rc, "cannot create QC/current fingerprint node");
    }

    rc_t r2 = KMDataNodeRelease(node);
    if (rc == 0 && r2 != 0)
        rc = r2;

    DISP_RC(rc, "while writing QC/current Metadata");
    return rc;
}

static rc_t Run(const CmdLine* args)
{
    rc_t rc = 0;

    Db db;
    SpotIterator it;

    assert(args);

    memset(&it, 0, sizeof it);

    if (!SpotIteratorFileExists(args->file)) {
        rc = RC(rcExe, rcFile, rcOpening, rcFile, rcNotFound);
        PLOGERR(klogErr,
            (klogErr, rc, "Cannot find '$(path)'", "path=%s", args->file));
    }
    else if (!SpotIteratorFileExists(args->run)) {
        rc = RC(rcExe, rcTable, rcOpening, rcTable, rcNotFound);
        PLOGERR(klogErr,
            (klogErr, rc, "Cannot find '$(path)'", "path=%s", args->run));
    }

    {
        rc_t rc2 = DbInit(rc, args, &db);
        if (rc == 0)
        {   rc = rc2; }
    }

    if (rc == 0) {
        rc = SpotIteratorInit(args->file, &db, &it);
    }

    if (rc == 0) {
        rc = Work(&db, &it);
    }

    if (rc == 0)
        rc = add_history_meta(db);
    if (rc == 0 && db.redactReads) {
        rc = add_current_meta(db);
    }
    if (rc == 0 && db.redactReads)
        rc = update_current_meta(db);

    if (rc == 0) {
        PLOGMSG(klogInfo, (klogInfo,
            "Success: redacted $(redacted) spots out of $(all)",
            "redacted=%d,all=%d", db.redactedSpots, db.nSpots));
        PLOGMSG(klogInfo, (klogInfo,
            "READ column was $(was)updated",
            "was=%s", db.redactReads ? "" : "not "));
    }

    {
        rc_t rc2 = SpotIteratorDestroy(&it);
        if (rc == 0)
        {   rc = rc2; }
    }

    {
        rc_t rc2 = DbDestroy(&db);
        if (rc == 0)
        {   rc = rc2; }
    }

    return rc;
}

#define FILE_OPTION  "file"
#define FILE_ALIAS   "F"
static const char* FILE_USAGE []
= { "File containing SpotId-s to redact" , nullptr };

#define REDACT_OPTION  "redact"
#define REDACT_ALIAS   "r"
static const char* REDACT_USAGE[]
= { "Update the run to mask spots with N according to filter list" , nullptr };

OptDef Options[] =  {
/* name          alias        HelpGen   help MaxCount NeedVal Required */
{ FILE_OPTION  , FILE_ALIAS  , nullptr, FILE_USAGE  ,  1, true , true  },
{ REDACT_OPTION, REDACT_ALIAS, nullptr, REDACT_USAGE,  1, false, false },
};

rc_t CC UsageSummary (const char* progname)
{
    return KOutMsg (
        "Usage:\n"
        "  %s [Options] -" FILE_ALIAS " <file> <run>\n", progname);
}

const char UsageDefaultName[] = "read-filter-redact";

rc_t CC Usage(const Args* args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == nullptr)
        rc = RC (rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    KOutMsg ("\nOptions:\n");

    HelpOptionLine(FILE_ALIAS, FILE_OPTION, "file", FILE_USAGE);
    HelpOptionLine(REDACT_ALIAS, REDACT_OPTION, nullptr, REDACT_USAGE);

    KOutMsg("\n");

    HelpOptionsStandard();

    HelpVersion(fullpath, KAppVersion());

    return rc;
}


static rc_t CmdLineInit(const Args* args, CmdLine* cmdArgs)
{
    rc_t rc = 0;

    assert(args && cmdArgs);

    while (rc == 0) {
        uint32_t pcount = 0;

        {
            const char option_name[](FILE_OPTION);
            /* file path parameter */
            rc = ArgsOptionCount(args, option_name, &pcount);
            if (rc != 0) {
                PLOGERR(klogErr, (klogErr, rc, "Failure parsing $(N)",
                    "N=%s", option_name));
                break;
            }
            if (pcount < 1) {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
                PLOGERR(klogErr, (klogErr, rc, "Missing $(N) parameter",
                    "N=%s", option_name));
                break;
            }
            if (pcount > 1) {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
                LOGERR(klogErr, rc, "Too many file parameters");
                break;
            }
            rc = ArgsOptionValue(args, option_name, 0,
                (const void**)&cmdArgs->file);
            if (rc != 0) {
                PLOGERR(klogErr, (klogErr, rc, "Failure retrieving $(N)",
                    "N=%s", option_name));
                break;
            }
        }

        {
            const char option_name[](REDACT_OPTION);
            /* file path parameter */
            rc = ArgsOptionCount(args, option_name, &pcount);
            if (rc != 0) {
                PLOGERR(klogErr, (klogErr, rc, "Failure parsing $(N)",
                    "N=%s", option_name));
                break;
            }
            if (pcount > 0)
                cmdArgs->redactReads = true;
        }

        /* run path parameter */
        rc = ArgsParamCount(args, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing run name");
            break;
        }
        if (pcount < 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR(klogErr, rc, "Missing run parameter");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many run parameters");
            break;
        }
        rc = ArgsParamValue(args, 0, (const void **)&cmdArgs->run);
        if (rc) {
            LOGERR(klogErr, rc, "Failure retrieving run name");
            break;
        }

        break;
    }

    if (rc != 0) {
        MiniUsage (args);
    }

    return rc;
}

MAIN_DECL(argc, argv)
{
    VDB::Application app( argc, argv );
    if (!app)
    {
        return VDB_INIT_FAILED;
    }

    rc_t rc = 0;
    Args* args = nullptr;

    CmdLine cmdArgs;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    LogLevelSet("info");

    rc = ArgsMakeAndHandle
        (&args, argc, argv, 1, Options, sizeof Options / sizeof(Options[0]));

    if (rc == 0) {
        rc = CmdLineInit(args, &cmdArgs);
    }

    if (rc == 0) {
        rc = Run(&cmdArgs);
    }

    ArgsWhack(args);

    if (rc == SILENT_RC(rcVDB, rcTable, rcOpening, rcSchema, rcNotFound))
    {   exit(10); }

    app.setRc( rc );
    return app.getExitCode();
}
