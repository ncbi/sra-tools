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
 */

#include <kapp/main.h> /* KMain */
#include <kapp/log.h> /* plogmsg */
#include <kfs/file.h> /* KFile */
#include <klib/rc.h> /* GetRCState */
#include <sra/wsradb.h> /* SRAMgr */
#include <sra/sradb-priv.h> /* SRATableGetKTableUpdate */
#include <vdb/types.h> /* vdb_uint8_t */
#include <kdb/database.h> /* KDBManagerMakeUpdate */
#include <kdb/table.h> /*  KTableRelease */
#include <ctype.h> /* isdigit */
#include <string.h> /* memset */
#include <stdio.h> /* sscanf */
#include <assert.h>
#include <sys/stat.h> /* umask */

#include <stdlib.h> /* system */


typedef struct Context
{
    const char* file_path;
    const char* table_path;
    const char * schema_path;

    KDirectory * pwd;
    VDBManager * mgr;
    VSchema * schema;
    VTable * table;


    const KFile * file;



    bool force;
    
} Context

struct SArgs {
};
struct SData {
    SRATable* _wrTbl;
    const SRATable* _rdTbl;
    const SRAColumn* _origFilterCol;
    const SRAColumn* _NReadsCol;
    const char* _origFilterColName;
    bool _existedRD_FILTER;
    bool _locked;
};



static KDirectory* __SpotIteratorDirectory = NULL;
/** SpotIterator: iterate spot numbers from 1 to max;
input spots to reduct from the file */
struct SpotIterator {
    spotid_t m_crnSpotId;
    spotid_t m_maxSpotId;

    spotid_t m_spotToReduct;

    const char* m_filename;
    size_t m_line;
    const KFile* m_file;
    size_t m_filePos;
    bool m_eof;

    char m_buffer[512];
    size_t m_inBuffer;
    char m_ch;
    bool m_hasCh;
};

/** Init the static directory object */
static rc_t SpotIteratorInitDirectory   (void)
{
    if (__SpotIteratorDirectory)
    {
        return 0;
    }
    else
    {
        rc_t rc = KDirectoryNativeDir(&__SpotIteratorDirectory);
        if (rc != 0)
        {
            logerr(klogErr, rc, "while calling KDirectoryNativeDir");
        }
        return rc;
    }
}

static
bool PathExists (const KDirectory * dir, KPathType desired_type, const char * path, ...)
{
    KPathType found_type;
    bool found;
    va_list args;

    va_start (args, path);

    found_type = KDirectoryVPathType (dir, path, args);
    found_type &= ~kptAlias;

    found = (found_type == desired_type)

    va_end (args);
    return found;
}



/** Check file existance */
static bool SpotIteratorFileExists(const char* path, ...)
{
    bool found = false;
    if (SpotIteratorInitDirectory() == 0)
    {
        uint32_t type
        va_list args;
        va_start(args, path);

        type = KDirectoryVPathType(__SpotIteratorDirectory, path, args);
        found = (type != kptNotFound);
        va_end(args);
    }
    return found;
}

static bool SpotIteratorBufferAdd(struct SpotIterator* self, char ch)
{
    assert(self);

    if (self->m_inBuffer >= (sizeof self->m_buffer - 1))
    {
        return false;
    }

    self->m_buffer[self->m_inBuffer++] = ch;
    self->m_buffer[self->m_inBuffer] = '\0';

    return true;
}

/** Read a character from the input file */
static rc_t SpotIteratorFileReadWithEof(struct SpotIterator* self,
    void* buffer, size_t bsize)
{
    rc_t rc = 0;
    size_t num_read = 0;

    assert(self);

    rc = KFileRead(self->m_file, self->m_filePos, buffer, bsize, &num_read);
    if (rc == 0)
    {
        if (num_read == 0)
        {
            self->m_eof = true;
        }
        else
        {
            self->m_filePos += num_read;
        }
    }
    else
    {
        plogerr(klogErr, rc, "on line $(lineno) while reading file '$(path)'",
                PLOG_U64(lineno) ",path=%s", self->m_line, self->m_filename);
    }

    return rc;
}

/** Read a line from the input file */
static rc_t SpotIteratorReadLine(struct SpotIterator* self)
{
    rc_t rc = 0;
    bool ok = true; /* ok means 'no input buffer overflow' */

    /* to skip leading/traling spaces */
    enum ELane
    {
        eBefore,
        eIn,
        eAfter
    } state = eBefore;

    if (self->m_eof) {
        return rc;
    }

    assert(self);

    ++self->m_line;
    self->m_inBuffer = 0;

    /* get back the saved character */
    if (self->m_hasCh) {
        SpotIteratorBufferAdd(self, self->m_ch);
        if (!isblank(self->m_ch)) {
            state = eIn;
        }
        self->m_hasCh = false;
    }

    while (ok && !self->m_eof) /* do until in-buffer overflow or EOF*/{
        char ch = 0;
        /* get next characted */
        if ((rc = SpotIteratorFileReadWithEof(self, &ch, 1)) != 0) {
            return rc;
        }

        if (!self->m_eof) {
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
                        plogerr(klogErr, rc, "bad symbol '$(char)' "
                            "on line $(lineno) in file '$(path)': '$(line)...'",
                            "char=%c," PLOG_U64(lineno) ",path=%s,line=%s",
                            ch, self->m_line, self->m_filename, self->m_buffer);
                        return rc;
                    }
                    break;
            }

            /* add next non-blank characted */
            if ((ok = SpotIteratorBufferAdd(self, ch))) {
      /* all combinations as "\r", "\n", "\r\n", "\n\r" are considered as EOL */
                if (ch == '\n' || ch == '\r') {
                    char c1 = 0;
                    if ((rc = SpotIteratorFileReadWithEof(self, &c1, 1)) != 0) {
                        return rc;
                    }
                    if (self->m_eof) {
                        break;
                    }
                    else if ((c1 != '\n' && c1 != '\r') || (ch == c1)) {
       /* save the character when EOL is a single character (WINDOWS) */
                        self->m_ch = c1;
                        self->m_hasCh = true;
                        break;
                    }
                    else {
                        ok = SpotIteratorBufferAdd(self, c1);
                        break;
                    }
                }
            }
        }
    }

    /* remove EOL */
    if (ok) {
        bool done = false;

        while (self->m_inBuffer > 0 && !done) {
            switch (self->m_buffer[self->m_inBuffer - 1]) {
                case '\n': case '\r':
                    self->m_buffer[--self->m_inBuffer] = '\0';
                    break;
                default:
                    done = true;
                    break;
            }
        }
    }
    else {
        rc = RC(rcExe, rcFile, rcReading, rcString, rcTooLong);
        plogerr(klogErr, rc,
            "on line $(lineno) while reading file '$(path)': '$(line)...'",
            PLOG_U64(lineno) ",path=%s,line=%s",
            self->m_line, self->m_filename, self->m_buffer);
    }

    return rc;
}

/** Get next spot from the input file */
static rc_t SpotIteratorReadSpotToRedact(struct SpotIterator* self)
{
    rc_t rc = 0;

    assert(self);

    while (rc == 0 && ! self->m_eof) {
        rc = SpotIteratorReadLine(self);

                         /* skip empty lines */ 
        if ((rc == 0) && (self->m_inBuffer > 0)) {
            spotid_t spot = 0;

            /* make sure the line contains digits only */
            int i = 0;
            for (i = 0; i < self->m_inBuffer; ++i) {
                if (!isdigit(self->m_buffer[i])) {
                    rc = RC(rcExe, rcFile, rcReading, rcChar, rcUnexpected);
                    plogerr(klogErr, rc, "character '$(char)' on line $(lineno)"
                            " while reading file '$(path)': '$(line)'",
                        "char=%c," PLOG_U64(lineno) ",path=%s,line=%s",
                        self->m_buffer[i], self->m_line,
                        self->m_filename, self->m_buffer);
                    return rc;
                }
            }

            sscanf(self->m_buffer, "%uld", &spot);

            if (spot == 0) {
                rc = RC(rcExe, rcFile, rcReading, rcString, rcInvalid);
                plogerr(klogErr, rc,
                    "bad spot id '0' on line $(lineno) "
                    "while reading file '$(path)': '$(line)'",
                    PLOG_U64(lineno) ",path=%s,line=%s",
                    self->m_line, self->m_filename, self->m_buffer);
            }
            else if (spot == self->m_spotToReduct) {
                rc = RC(rcExe, rcFile, rcReading, rcString, rcInvalid);
                plogerr(klogErr, rc, "duplicated spot id '$(spot)' "
                    "on line $(lineno) while reading file '$(path)': '$(line)'",
                    PLOG_U32(spot) "," PLOG_U64(lineno) ",path=%s,line=%s",
                    spot, self->m_line, self->m_filename, self->m_buffer);
            }
            else if (spot < self->m_spotToReduct) {
                rc = RC(rcExe, rcFile, rcReading, rcString, rcInvalid);
                plogerr(klogErr, rc, "File '$(path)' is unsorted. "
                    "$(id) < $(last). See line $(lineno): '$(line)'",
                    "path=%s," PLOG_U32(id) "," PLOG_U32(last) ","
                        PLOG_U64(lineno) ",line=%s",
                    self->m_filename, spot, self->m_spotToReduct,
                    self->m_line, self->m_buffer);
            }
            else if (spot > self->m_maxSpotId) {
                rc = RC(rcExe, rcFile, rcReading, rcString, rcInvalid);
                plogerr(klogErr, rc, "spotId $(spot) on line $(lineno) "
                    "of file '$(path)' is bigger that the max spotId $(max): "
                        "'$(line)'",
                    PLOG_U32(spot) "," PLOG_U64(lineno) ",path=%s,"
                    PLOG_U32(max) ",line=%s",
                    spot, self->m_line, self->m_filename, self->m_maxSpotId,
                    self->m_buffer);
            }
            else {
                self->m_spotToReduct = spot;
                self->m_inBuffer = 0;
            }
            break;
        }
    }

    return rc;
}

static rc_t SpotIteratorInit(struct SpotIterator* self,
    const SRATable* tbl, const char* redactFileName)
{
    rc_t rc = 0;

    assert(self && tbl && redactFileName);

    memset(self, 0, sizeof *self);

    self->m_crnSpotId = 1;

    rc = SRATableMaxSpotId(tbl, &self->m_maxSpotId);
    if (rc != 0) {
        logerr(klogErr, rc, "while calling SRATableMaxSpotId");
    }
    else {
        plogmsg(klogInfo,
            "MaxSpotId = $(spot)", PLOG_U32(spot), self->m_maxSpotId);
    }

    if (rc == 0) {
        rc = SpotIteratorInitDirectory();
    }

    if (rc == 0) {
        self->m_filename = redactFileName;
        plogmsg(klogInfo, "Opening '$(path)'", "path=%s", self->m_filename);
        rc = KDirectoryOpenFileRead(
            __SpotIteratorDirectory, &self->m_file, "%s", self->m_filename);
        if (rc != 0) {
            plogerr(klogErr, rc,
                "while opening file '$(path)'", "path=%s", self->m_filename);
        }
    }

    if (rc == 0) {
        rc = SpotIteratorReadSpotToRedact(self);
    }

    return rc;
}

/** Get next spot id, check whether it should be redacted.
Returns false if maxSpotId reached */
static bool SpotIteratorNext(struct SpotIterator* self, rc_t* rc,
    spotid_t* spot, bool* toRedact)
{
    bool hasNext = false;

    assert(self && rc && spot && toRedact);

    *rc = 0;
    *toRedact = false;

    if (self->m_crnSpotId <= self->m_maxSpotId) {
        hasNext = true;
        *spot = self->m_crnSpotId++;
    }

    if (*spot == self->m_spotToReduct) {
        *toRedact = true;
        *rc = SpotIteratorReadSpotToRedact(self);
        if (*rc != 0) {
            hasNext = false;
        }
    }

    return hasNext;
}

static rc_t SpotIteratorDestroy(struct SpotIterator* self)
{
    assert(self);
    KDirectoryRelease(__SpotIteratorDirectory);
    __SpotIteratorDirectory = NULL;
    return KFileRelease(self->m_file);
}

/** The main data structure */

static rc_t SDataInit(struct SData* self,
    const char* tablePath, const SRAMgr* rdMgr, rc_t rc)
{
    SRAMgr* wrMgr = NULL;

    assert(self);
    memset(self, 0, sizeof *self);
    if (rc)
    {   return rc; }

    rc = SRAMgrMakeUpdate(&wrMgr, NULL);
    if (rc != 0) {
        logerr(klogErr, rc, "while calling SRAMgrMakeUpdate");
    }
    else {
        plogmsg(klogInfo,
            "Opening Table $(path) for read", "path=%s", tablePath);
        rc = SRAMgrOpenTableRead(rdMgr, &self->_rdTbl, "%s", tablePath);
        if (rc != 0) {
            plogerr(klogErr, rc,
                "cannot open table $(path) for read", "path=%s", tablePath);
        }
    }

    if (rc == 0) {
        plogmsg(klogInfo,
            "Opening Table $(path) for update", "path=%s", tablePath);
        rc = SRAMgrOpenTableUpdate(wrMgr, &self->_wrTbl, "%s", tablePath);
        if (rc != 0) {
            plogerr(klogErr, rc,
                "cannot open table $(path) for update", "path=%s", tablePath);
        }
    }

    SRAMgrRelease(wrMgr);

    return rc;
}

static rc_t SDataPrepareCols(struct SData* self,
    const char* rdFilterName, const char* readFilterName)
{
    rc_t rc = 0;

    assert(self);

    if (rc == 0) {
        const char name[] = "NREADS";
        rc = SRATableOpenColumnRead(self->_rdTbl,
            &self->_NReadsCol, name, vdb_uint8_t);
        if (rc != 0) {
            if (GetRCState(rc) == rcNotFound) {
                rc = 0;
            }
            else {
                plogerr(klogErr, rc,
                    "while calling SRATableOpenColumnRead($name)", "name=%s",
                    name);
            }
        }
    }

    /* It the physical column exists, open it */
    if (rc == 0) {
        rc = SRATableOpenColumnRead(self->_rdTbl,
            &self->_origFilterCol, rdFilterName, sra_read_filter_t);
        if (rc == 0) {
            plogmsg(klogDebug1,
                "Found '$(name)' column", "name=%s", rdFilterName);
            self->_existedRD_FILTER = true;
            self->_origFilterColName = rdFilterName;
        }
        else if (GetRCState(rc) == rcNotFound) {
            plogmsg(klogDebug1,
                "Column '$(name)' does not exist", "name=%s", rdFilterName);
            rc = 0;
        }
        else {
            plogerr(klogErr, rc, "while calling SRATableOpenColumnRead($name)",
                "name=%s", rdFilterName);
        }
    }

    /* Otherwise open the virtual one */
    if (rc == 0 && ! self->_existedRD_FILTER) {
        rc = SRATableOpenColumnRead(self->_rdTbl,
            &self->_origFilterCol, readFilterName, sra_read_filter_t);
        if (rc != 0) {
            plogerr(klogErr, rc,
                "while calling SRATableOpenColumnRead($name)", "name=%s",
                readFilterName);
        }
    }

    return rc;
}

/** Keeps track when to cut the blob */
struct SBlob {
    bool m_new;
    spotid_t m_maxSpotId;
    const struct SData* m_data;
};

static rc_t SBlobInit(struct SBlob* self,
    const struct SData* data, const struct SpotIterator* it)
{
    rc_t rc = 0;
    bool newColumn = true;

    assert(self && data && it);

    memset(self, 0, sizeof *self);

    if (data->_existedRD_FILTER) {
        /* blob range will be read from the existing blob */
        newColumn = false;
    }

    self->m_new = newColumn;
    self->m_maxSpotId = it->m_maxSpotId;
    self->m_data = data;

    return rc;
}

static rc_t SBlobGetRange(const struct SBlob* self,
    spotid_t id, spotid_t* last)
{
    rc_t rc = 0;
    spotid_t first = 0;

    assert(self && last);

    if (self->m_new) {
        first = (id & ~0xFFFF) + 1;
        *last = first + 0xFFFF;
        if (*last > self->m_maxSpotId) {
            *last = self->m_maxSpotId;
        }
        plogmsg(klogDebug1, "New blob range for spot $(id) is "
                "$(first) - $(last) ($(xfirst) - $(xlast))",
            PLOG_U32(id) "," PLOG_U32(first) "," PLOG_U32(last) ","
                PLOG_X32(xfirst) "," PLOG_X32(xlast),
            id, first, *last, first, *last);
    }
    else {
        assert(self->m_data && self->m_data->_origFilterCol);
        rc = SRAColumnGetRange(self->m_data->_origFilterCol, id, &first, last);
        if (rc != 0) {
            plogerr(klogErr, rc, "Cannot SRAColumnGetRange $(id)",
                PLOG_U32(id), id);
        }
        else {
            plogmsg(klogDebug1, "Existing blob range for spot $(id) is "
                    "$(first) - $(last) ($(xfirst) - $(xlast))",
                PLOG_U32(id) "," PLOG_U32(first) "," PLOG_U32(last) ","
                    PLOG_X32(xfirst) "," PLOG_X32(xlast),
                id, first, *last, first, *last);
        }
    }

    return rc;
}

static rc_t SDataUpdate(struct SData* self,
    const char* newColName, const char* redactFileName,
    spotid_t* redactedSpots, spotid_t* all)
{
    struct SBlob blob;
    uint8_t filter[32];
    rc_t rc = 0, rc2 = 0;
    uint32_t colIdx = 0;
    spotid_t spot = 0, last = 0;
    bool toRedact = false;
    struct SpotIterator it;

    assert(self && redactedSpots && all);

    memset(filter, SRA_READ_FILTER_REDACTED, sizeof filter);

    if ((rc = SpotIteratorInit(&it, self->_rdTbl, redactFileName))
        == 0)
    {
        rc = SRATableOpenColumnWrite
            (self->_wrTbl, &colIdx, NULL, newColName, sra_read_filter_t);
        if (rc != 0) {
            plogerr(klogErr, rc,
                "cannot open Column $(path) for Write", "path=%s", newColName);
            return rc;
        }
    }
    else {
        return rc;
    }

    rc = SBlobInit(&blob, self, &it);
    if (rc != 0) {
        return rc;
    }

    while (rc == 0 && SpotIteratorNext(&it, &rc, &spot, &toRedact)) {
        bitsz_t offset = 0, size = 0;
        const void *base = NULL;
        uint8_t nReads = 0;

        if (rc != 0) {
            break;
        }

        plogmsg(klogDebug2, "Spot $(spot): $(action)",
            PLOG_U32(spot) ",action=%s",
            spot, toRedact ? "redact" : "original");

        /* GET NEXT BLOB RANGE */
        if (spot == 1 || spot > last) {
            rc = SBlobGetRange(&blob, spot, &last);
            if (rc != 0) {
                break;
            }
        }

        assert(spot <= last);

        /* GET NREADS */
        if ((rc = SRAColumnRead
            (self->_NReadsCol, spot, &base, &offset, &size)) != 0)
        {
            logerr(klogErr, rc, "cannot SRAColumnRead");
            break;
        }
        else if (offset != 0 || size != sizeof nReads * 8) {
            rc = RC(rcExe, rcColumn, rcReading, rcData, rcInvalid);
            plogerr(klogErr, rc,
                "Bad SRAColumnRead(\"NREADS\", $(spot)) result",
                PLOG_U32(spot), spot);
        }
        else {
            nReads = *((uint8_t*) base);
            if (spot == 1) {
                if (nReads == 1) {
                    plogmsg(klogInfo, "The first spot has $(nreads) read",
                        "nreads=%d", nReads);
                }
                else {
                    plogmsg(klogInfo, "The first spot has $(nreads) reads",
                        "nreads=%d", nReads);
                }
            }
        }

        /* GET READ_FILTER */
        if (toRedact) {
            base = filter;
            ++(*redactedSpots);
        }
        else {
            if ((rc = SRAColumnRead(self->_origFilterCol,
                spot, &base, &offset, &size)) != 0)
            {
                plogerr(klogErr, rc,
                    "while calling SRAColumnRead($(name))", "name=%s",
                    "READ_FILTER");
                break;
            }
            else if (offset != 0
                  || size != sizeof (uint8_t) * 8 * nReads)
            {
                rc = RC(rcExe, rcColumn, rcReading, rcData, rcInvalid);
                plogerr(klogErr, rc, "Bad SRAColumnRead($(spot)) result",
                    PLOG_U32(spot), spot);
            }
        }

        if ((rc = SRATableOpenSpot(self->_wrTbl, spot)) != 0) {
            plogerr(klogErr, rc, "cannot open Spot $(id)", PLOG_U32(id), spot);
            break;
        }
        if ((rc = SRATableWriteIdxColumn(self->_wrTbl,
            colIdx, base, 0, sizeof (uint8_t) * 8 * nReads)) != 0)
        {
            logerr(klogErr, rc, "cannot SRATableWriteIdxColumn");
            break;
        }
        if ((rc = SRATableCloseSpot(self->_wrTbl)) != 0) {
            logerr(klogErr, rc, "cannot SRATableCloseSpot");
            break;
        }

        /* CUT THE BLOB */
        if (spot == last) {
            rc = SRATableCloseCursor(self->_wrTbl);
            if (rc != 0) {
                plogerr(klogErr, rc, "cannot SRATableCloseCursor $(id)",
                    PLOG_U32(id), spot);
                break;
            }
        }
    }

    rc2 = SpotIteratorDestroy(&it);
    if (rc == 0)
    {   rc = rc2; }

    *all = spot;

    return rc;
}

static rc_t SDataDestroy(struct SData* self, bool commit) {
    rc_t rc = 0, rc2 = 0;
    assert(self);

    rc2 = SRAColumnRelease(self->_origFilterCol);
    self->_origFilterCol = NULL;
    if (rc == 0)
    { rc = rc2; }

    rc2 = SRAColumnRelease(self->_NReadsCol);
    self->_NReadsCol = NULL;
    if (rc == 0)
    { rc = rc2; }

    rc2 = SRATableRelease(self->_rdTbl);
    self->_rdTbl = NULL;
    if (rc == 0)
    { rc = rc2; }

    if (commit && rc == 0) {
        rc = SRATableCommit(self->_wrTbl);
    }

    rc2 = SRATableRelease(self->_wrTbl);
    self->_wrTbl = NULL;
    if (rc == 0)
    { rc = rc2; }

    return rc;
}

/** Command line arguments */


static bool __BadLogLevel = false;
static const char *__dummy4NextLogLevel(void *data)
{
    __BadLogLevel = true;
    return "info";
}

static KLogLevel __defaultLogLevel;

static void Usage(const char *argv0, const char *msg, ...)
{
    if (msg) {
        va_list args;
        va_start(args, msg);
        vfprintf(stderr, msg, args);
        fprintf(stderr, "\n\n");
        va_end(args);
    }

    fprintf(stderr, "Usage:\n"
        "%s -D <table> -F <file> [ -l <level> ]\n"
        "\t-D --table-path\tSRA Table directory path\n"
        "\t-F --file      \tFile containing SpotId-s to redact\n"
        "\t-f --force     \tForce overwriting of already redacted table\n"
        "\t-l --level     \t"
                 "Log level: 0-13 or fatal|sys|int|err|warn|info|debug[1-10].",
        argv0);

    {
        char logLevel[64];
        rc_t rc = KLogLevelExplain
            (__defaultLogLevel, logLevel, sizeof logLevel, NULL);
        if (rc == 0) {
            fprintf(stderr, " Default: %s", logLevel);
        }
    }

    fprintf(stderr, "\n");
}

/** Get command line key with argument */
static bool GetArg(int* argI, int argc, char* argv[],
    const char* shortArg, const char* longArg,
    const char** out, const char* error, rc_t* errorRc)
{
    bool found = false;

    assert(argI && (*argI < argc) && longArg && argv && out && errorRc);

    if (strcmp(argv[*argI], longArg) == 0) {
        found = true;
    }
    else if (shortArg && (strcmp(argv[*argI], shortArg) == 0)) {
        found = true;
    }
    if (!found) {
        return false;
    }

    if ((*argI + 1) >= argc) {
        Usage(argv[0], error);
        *errorRc = RC(rcExe, rcArgv, rcReading, rcParam, rcInvalid);
    }
    else {
        *out = argv[++(*argI)];
    }

    return true;
}

#define OPTION_TABLE "table-path"
#define ALIAS_TABLE  "D"

#define OPTION_FILE  "file"
#define ALIAS_FILE   "F"

#define OPTION_SCHEMA  "schema"
#define ALIAS_SCHEMA   "S"

#define OPTION_FORCE  "force"
#define ALIAS_FORCE   "F"

OptDef Options[] = 
{
    { OPTION_TABLE,  ALIAS_TABLE,  NULL, table_usage,  1, true,  true },
    { OPTION_FILE,   ALIAS_FILE,   NULL, file_usage,   1, true,  true },
    { OPTION_SCHEMA, ALIAS_SCHEMA, NULL, schema_usage, 1, true,  true },
    { OPTION_FORCE,  ALIAS_FORCE,  NULL, force_usage,  0, false, false }
};


static
rc_t open_and_run (Context * context)
{
    KDirectory * pwd;
    rc_t rc;

    rc = KDirectoryNativeDir (&pwd);
    if (rc)
        LOGERR (klogFatal, rc, "Unable to open file system");
    else
    {
        if (PathExists (pwd, kptDile, context->file_path))
        {
            if (PathExists (pwd, kptDile, context->file_path))
            {
                VDBManager * vmgr;

                rc = VDBManagerMakeUpdate (&vmgr, pwd);
                if (rc)
                    LOGERR (kptInt, "Failed to create a database manager");
                else
                {
                    VSchema * vschema;

                    rc = VDBManagerMakeSchema (vmgr, &vschema);
                    if (rc)
                        LOGERR (kptInt, "Failed to create schema");
                    else
                    {
                        rc = VSchemaParseFile (vschema, args->schema);
                        if (rc)
                            PLOGERR (klogFatal, (klogFatal, 
                                                 "Unable to parse schema file ($S)",
                                                 "S=%s", args->schema));
                        else
                        {
                            VTable * vtable;
                            rc = VDBManagerOpenTableUpdate (vmgr, &vtable, SCHEMA, args->table);
                            if (rc)
                                PLOGERR (klogFatal, (klogFatal, "Unable to open table ($T)",
                                             "T=%s", args->table));
                            else
                            {

                        
                                VTableRelease (vtable);
                            }
                        }
                        VSchemaRelease (vschema);
                    }
                    VDBManagerRelease (vmgr);
                }
            }
            else
                PLOGERR (kptFatal, (kptFatal, "table paramaeter is not a table directory ($F)",
                                    "F=%s", args->table));
        }
        else
            PLOGERR (kptFatal, (kptFatal, "file paramaeter is not a file ($F)",
                                "F=%s", context->file_path));



        KPathType pt;

        pt = KDirectoryPathType (arg->file);
        if ((pt & ~kptAlias) != kptFile)
        else
        {
        }
        KDirectoryRelease (pwd);
    }
    return rc;
}

    rc_t rc = 0;

    spotid_t all = 0, redacted = 0;
    bool locked = false;

    SRAMgr* rdMgr = NULL;
    KTable* ktbl = NULL;

    struct SData data;

    const char readFilterName   [] = "READ_FILTER";
    const char rdFilterName     [] = "RD_FILTER";
    const char readFilterNameNew[] = "TMP_READ_FILTER";
    const char rdFilterNameNew  [] = "TMP_RD_FILTER";
    const char colNameSave      [] = "RD_FILTER_BAK";

    const char* redactFileName = NULL;
    const char* tablePath = NULL;

    struct SArgs args;

    /* PARSE COMMAND LINE, MAKE SURE INPUTS EXIST */
    rc = SArgsMake(&args, argc, argv);
    if (rc)
    {   return rc; }

    tablePath      = args._table;
    redactFileName = args.file;

    /* WAS THIS RUN ALREADY REDUCTED ? */
    if (! args._force
        && SpotIteratorFileExists("%s/col/%s", tablePath, colNameSave))
    {
        rc = RC(rcExe, rcTable, rcOpening, rcColumn, rcExists);
        plogerr(klogErr, rc,
            "'$(path)' was redacted already", "path=%s", tablePath);
    }

    umask(2);

    /* OPEN THE MANAGER */
    if (rc == 0) {
        rc = SRAMgrMakeUpdate(&rdMgr, NULL);
        if (rc != 0) {
            logerr(klogErr, rc, "while calling SRAMgrMakeUpdate(rd)");
        }
    }

    /* UNLOCK THE RUN */
    if (rc == 0) {
        rc = SRAMgrUnlock(rdMgr, "%s", tablePath);
        if (rc) {
            if (GetRCState(rc) == rcUnlocked) {
                plogmsg(klogInfo,
                    "'$(path)' was not locked", "path=%s", tablePath);
                rc = 0;
            }
            else {
                plogerr(klogErr, rc, "while calling SRAMgrUnlock($(path))",
                    "path=%s", tablePath);
            }
        }
        else {
            plogmsg(klogInfo, "'$(path)' was unlocked", "path=%s", tablePath);
            locked = true;
        }
    }

    /* INITIALIZE */
    rc = SDataInit(&data, tablePath, rdMgr, rc);

    if (rc == 0) {
        logmsg(klogDebug2, "Calling SRATableGetKTableUpdate");
        rc = SRATableGetKTableUpdate(data._wrTbl, &ktbl);
        if (rc != 0) {
            logerr(klogErr, rc, "while calling SRATableGetKTableUpdate");
        }
    }

    /* OPEN INPUT COLUMNS */
    if (rc == 0) {
        rc = SDataPrepareCols(&data, rdFilterName, readFilterName);
    }

    if (rc == 0) {
        /* just for fun, tell ktable to drop this column */
        KTableDropColumn(ktbl, "%s", rdFilterNameNew);
        KTableDropColumn(ktbl, "%s", colNameSave);

        /* THE MAIN WORKING FUNCTION */
        rc = SDataUpdate
            (&data, readFilterNameNew, redactFileName, &redacted, &all);
    }

    /* CLEANUP */

    {
        rc_t rc2 = SDataDestroy(&data, rc == 0);
        if (rc == 0) {
            rc = rc2;
        }
    }

    /* REMAME THE PHYSICAL COLUMNS */
    if (rc == 0) {
        if (data._existedRD_FILTER) {
            plogmsg(klogDebug1, "Renaming '$(from)' to '$(to)'",
                "from=%s,to=%s", rdFilterName, colNameSave);
            rc = KTableRenameColumn(ktbl, rdFilterName, colNameSave);
            if (rc != 0) {
                plogerr(klogErr, rc,
                    "while renaming column from '$(from)' to '$(to)'",
                    "from=%s,to=%s", rdFilterName, colNameSave);
            }
        }
        if (rc == 0) {
            plogmsg(klogDebug1, "Renaming '$(from)' to '$(to)'",
                "from=%s,to=%s",
                rdFilterNameNew, rdFilterName);
            rc = KTableRenameColumn(ktbl, rdFilterNameNew, rdFilterName);
            if (rc != 0) {
                plogerr(klogErr, rc,
                    "while renaming column from '$(from)' to '$(to)'",
                    "from=%s,to=%s", rdFilterNameNew, rdFilterName);
            }
        }
    }
    else if (ktbl) {
        rc_t rc2 = KTableDropColumn(ktbl, "%s", rdFilterNameNew);
        if (rc2 != 0) {
            plogerr(klogErr, rc2, "while dropping column '$(name)'",
                "name=%s", rdFilterNameNew);
        }
    }

    {
        rc_t rc2 = KTableRelease(ktbl);
        if (rc == 0)
        { rc = rc2; }
    }

    if (locked) {
        rc_t rc2 = 0;
        plogmsg(klogInfo, "Relocking '$(path)'", "path=%s", tablePath);
        rc2 = SRAMgrLock(rdMgr, "%s", tablePath);
        if (rc2 != 0) {
            plogerr(klogErr, rc2, "while calling SRAMgrLock($(path))",
                "path=%s", tablePath);
            if (rc == 0)
            { rc = rc2; }
        }
    }

    {
        rc_t rc2 = SRAMgrRelease(rdMgr);
        if (rc == 0)
        { rc = rc2; }
    }

    if (rc == 0) {
        plogmsg(klogInfo, "'$(path)': redacted $(redacted) spots out of $(all)",
            "path=%s," PLOG_U32(redacted) "," PLOG_U32(all),
            tablePath, redacted, all);
    }
    else {
        plogmsg(klogInfo, "Failed to redact '$(path)'", "path=%s", tablePath);
    }

    return rc;
}


static
rc_t run (Context * context)
{
    rc_t rc;

    return rc;
}


static
rc_t open_and_run (Context * context)
{
    KDirectory * pwd;
    rc_t rc;

    /* first get a toe hold in the file system */
    rc = KDirectoryNativeDir (&context->pwd);
    if (rc)
        LOGERR (klogFatal, rc, "Unable to open file system");
    else
    {
        /* try to open the file of redactions */
        rc = KDirectoryOpenFileRead (context->pwd, &context->file, "%s", conext->file_path);
        if (rc)
            PLOGERR (kptFatal, (kptFatal, rc, "Failed to open redactions file ($(F))",
                                context->file_path));
        else
        {
            /* okay we've got the non-data base stuff up so get
             * a toe hold in the VDatabase */
            rc = VDBManagerMakeUpdate (&context->mgr, context->pwd);
            if (rc)
                LOGERR (kptInt, "Failed to create a database manager");
            else
            {
                /* build a schema object */
                rc = VDBManagerMakeSchema (context->mgr, &ontext->schema);
                if (rc)
                    LOGERR (kptInt, "Failed to create schema");
                else
                {
                    /* fill it with the specified schema */
                    rc = VSchemaParseFile (context->schema, "%s", context->schema_path);
                    if (rc)
                        PLOGERR (klogFatal, (klogFatal, 
                                             "Unable to parse schema file ($S)",
                                             "S=%s", args->schema));
                    else
                    {
                        /* now open the table with that schema */
                        rc = VDBManagerOpenTableUpdate (context->mgr, &context->table,
                                                        context->schema, "%s", context->table_path);
                        if (rc)
                            PLOGERR (klogFatal, (klogFatal, "Unable to open table ($T)",
                                                 "T=%s", args->table));
                        else
                        {
                            /* most stuff is open */
                            rc = run (context);

                            VTableRelease (context->table);
                        }
                    }
                    VSchemaRelease (context->schema);
                }
                VDBManagerRelease (context->mgr);
            }
            KFileRelease (context->file);
        }
    }
    return rc;
}

rc_t KMain(int argc, char* argv[])
{
    Args * args;
    rc_t rc;

    rc = ArgsMakeAndHandle (&args, argc, argv, 1, Options, sizeof Options / sizeof (Options[0]));
    if (rc)
        LOGERR (klogFatal, rc, "Failure reading command line");
    else do
    {
        Context context;
        uint32_t pcount;

        /* force option: Replace extisting redact column */
        rc = ArgsOptionCount (args, OPTION_FORCE, &pcount);
        if (rc)
        {
            LOGERR (klogFatal, rc, "Failure parsing force name");
            break;
        }
        if ((context.force = (pcount > 0)) != false)
            STSMSG (1, ("Using force option"));

        /* table path parameter */
        rc = ArgsOptionCount (args, OPTION_TABLE, &pcount);
        if (rc)
        {
            LOGERR (klogFatal, rc, "Failure parsing table name");
            break;
        }
        if (pcount < 1)
        {
            rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR (klogFatal, rc, "Missing table parameter");
            break;
        }
        if (pcount > 1)
        {
            rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR (klogFatal, rc, "Too many table parameters");
            break;
        }
        rc = ArgsOptionValue (args, OPTION_TABLE, 0, (const void **)&sargs.table);
        if (rc)
        {
            LOGERR (klogFatal, rc, "Failure retrieving table name");
            break;
        }

        /* file path parameter */
        rc = ArgsOptionCount (args, OPTION_FILE, &pcount);
        if (rc)
        {
            LOGERR (klogFatal, rc, "Failure parsing file name");
            break;
        }
        if (pcount < 1)
        {
            rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR (klogFatal, rc, "Missing file parameter");
            break;
        }
        if (pcount > 1)
        {
            rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR (klogFatal, rc, "Too many file parameters");
            break;
        }
        rc = ArgsOptionValue (args, OPTION_FILE, 0, (const void **)&context.file_path);
        if (rc)
        {
            LOGERR (klogFatal, rc, "Failure retrieving file name");
            break;
        }


        /* schema path parameter */
        rc = ArgsOptionCount (args, OPTION_SCHEMA, &pcount);
        if (rc)
        {
            LOGERR (klogFatal, rc, "Failure parsing schema name");
            break;
        }
        if (pcount < 1)
        {
            rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR (klogFatal, rc, "Missing schema parameter");
            break;
        }
        if (pcount > 1)
        {
            rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR (klogFatal, rc, "Too many schema parameters");
            break;
        }
        rc = ArgsOptionValue (args, OPTION_SCHEMA, 0, (const void **)&context.schema_path);
        if (rc)
        {
            LOGERR (klogFatal, rc, "Failure retrieving schema name");
            break;
        }

        /* now that we have parameter values: open the file and table
         * then run */
        rc = open_and_run (&context);

        KArgsRelease (args);

    } while (0);

    if (rc)
        LOGERR (klogErr, rc, "Exiting with an error");
    else
        STSMSG (1, ("Exiting %R\n", rc));
    return rc;
}

/* EOF */
