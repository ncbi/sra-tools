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
 * ============================================================================$
 */

#include <kapp/main.h>

#include <sra/wsradb.h> /* spotid_t */
#include <sra/sradb-priv.h> /* SRASchemaMake */

#include <vdb/cursor.h> /* VCursor */
#include <vdb/database.h> /* VDatabase */
#include <vdb/manager.h> /* VDBManager */
#include <vdb/schema.h> /* VSchemaRelease */
#include <vdb/table.h> /* VDBTable */
#include <vdb/vdb-priv.h> /* VTableDropColumn */

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

#include <os-native.h>

#include <assert.h>
#include <ctype.h> /* isblank */
#include <stdio.h> /* sscanf */
#include <stdlib.h> /* exit */
#include <string.h> /* memset */

#define DISP_RC(rc,msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)

static KDirectory* __SpotIteratorDirectory = NULL;

typedef struct CmdLine {
    const char* table;
    const char* file;
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
typedef struct Db {
    const char* table;

    VDBManager* mgr;
    const VDatabase* rDb;
    VTable* wTbl;   /* table to redact */
    VTable* newTbl; /* temporary table */

    const VCursor* rCursor; /* read cursor */
    uint32_t rFilterIdx;    /* column read index */

    VCursor* wCursor;       /* write cursor */
    uint32_t wIdx;          /* column write index */

    KMetadata* meta;        /* original Table/DB metadata to record HISTORY */
    const KMetadata* dMeta; /* original DB metadata */

    bool locked;            /* original run will be locked at the end? */

    spotid_t nSpots;        /* total number of copied spots */
    spotid_t redactedSpots; /* number of redacted spots */
    
    bool VCursorOpenFAILED; /* copy in place failed: copy to temp will be done*/
} Db;
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
            int i = 0;
            for (i = 0; i < self->inBuffer; ++i) {
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

            sscanf(self->buffer, "%ld", &spot);

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
            else if (spot > self->maxSpotId) {
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

    it->file = NULL;
    it->inBuffer = 0;
    it->hasCh = false;

    {
        rc_t rc2 = KDirectoryRelease(__SpotIteratorDirectory);
        if (rc == 0)
        {   rc = rc2; }
        __SpotIteratorDirectory = NULL;
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

    if (self->crnSpotId <= self->maxSpotId) {
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

static const char s_Column[] = "READ_FILTER";
    
static rc_t DbInit(rc_t rc, const CmdLine* args, Db* db)
{
    const VTable* rTbl = NULL;

    assert(args && db);

    memset(db, 0, sizeof *db);

    if (rc != 0)
    {   return rc; }

    db->table = args->table;

    if (rc == 0) {
        rc = VDBManagerMakeUpdate(&db->mgr, NULL);
        DISP_RC(rc, "while calling VDBManagerMakeUpdate");
    }

    if (rc == 0) {
        rc = VDBManagerWritable(db->mgr, args->table);
        if (rc != 0) {
            if (GetRCState(rc) == rcLocked)
            {
                rc = VDBManagerUnlock(db->mgr, args->table);
                if (rc != 0) {
                    PLOGERR(klogErr, (klogErr, rc,
                        "while calling VDBManagerUnlock('$(table)')",
                        "table=%s", args->table));
                }
                db->locked = true;
            }
            else {
                PLOGERR(klogErr, (klogErr, rc,
                    "while calling VDBManagerWritable('$(table)')",
                    "table=%s", args->table));
                if (rc == RC(rcDB, rcPath, rcAccessing, rcPath, rcReadonly)) {
                    PLOGERR(klogErr, (klogErr, rc,
                        "N.B. It is possible '$(table)' was not locked properly"
                        , "table=%s", args->table));
                }
            }
        }
    }

    if (rc == 0) {
/*      bool isDb = false; */

        db->locked = true; /* has to be locked in production mode */

        rc = VDBManagerOpenTableUpdate (db->mgr, &db->wTbl, NULL, args->table);
        if( rc == 0) {
            rc = VTableOpenMetadataUpdate ( db->wTbl, & db->meta );
            DISP_RC(rc, "while Opening Table Metadata");
        }
        else {
            VDatabase* wDb = NULL;
            rc_t rc2 = VDBManagerOpenDBUpdate ( db->mgr, &wDb, NULL,
                                                args->table );
            DISP_RC(rc2, "while VDBManagerOpenDBUpdate");

            if( rc2 == 0) {
                rc2 = VDatabaseOpenTableUpdate( wDb, &db->wTbl, "SEQUENCE" );
                DISP_RC(rc2, "while OpenTableUpdate");
                if (rc2 == 0) {
                    rc = 0;
/*                  isDb = true; */
		}
            }

            if (rc == 0) {	    
                rc = VDBManagerOpenDBRead( db->mgr, &db->rDb, NULL,
                                                args->table );
                DISP_RC(rc, "while VDBManagerOpenDBRead");
                if( rc == 0) {
                    rc = VDatabaseOpenTableRead(db->rDb, &rTbl, "SEQUENCE");
                    DISP_RC(rc, "while OpenTableRead");
                }

                if( rc == 0) {
                    rc = VDatabaseOpenMetadataRead(db->rDb, &db->dMeta);
                    DISP_RC(rc, "while Opening DB Metadata");
                }
            }

            if (rc == 0) {
                rc = VDatabaseOpenMetadataUpdate(wDb, &db->meta);
                DISP_RC(rc, "while Opening Db Metadata");
            }

	    RELEASE(VDatabase, wDb);
        }

        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "while opening VTable '$(table)'", "table=%s", args->table));
        }
    } 

    if( rc == 0) {
        VTable* t = rTbl ? (VTable*)rTbl : db->wTbl;
        rc = VTableCreateCursorRead(t, &db->rCursor);
        DISP_RC(rc, "while creating read cursor");
        if (rc == 0) {
            rc = VCursorAddColumn(db->rCursor, &db->rFilterIdx, s_Column);
            if (rc != 0) {
                PLOGERR(klogErr, (klogErr, rc,
                    "while adding $(name) to read cursor", "name=%s", s_Column));
            }
        }
        if (rc == 0) {
            rc = VCursorOpen(db->rCursor);
            DISP_RC(rc, "while opening read cursor");
        }
    }

    if (rc == 0) {
        rc = VTableCreateCursorWrite(db->wTbl, &db->wCursor, kcmInsert);
        DISP_RC(rc, "while creating write cursor");
        if (rc == 0) {
            rc = VCursorAddColumn(db->wCursor, &db->wIdx, s_Column);
            if (rc != 0) {
                PLOGERR(klogErr, (klogErr, rc,
                   "while adding $(name) to write cursor", "name=%s", s_Column));
            }
        }
        if (rc == 0) {
            rc = VCursorOpen(db->wCursor);
            if (rc != 0) {
                db->VCursorOpenFAILED = true;
                RELEASE(VCursor, db->wCursor);
                db->wIdx = 0;
	    }
        }
    }

    RELEASE(VTable, rTbl);

    return rc;
}

static const char s_TmpPath[] = "tmp";

static rc_t DbInit2(Db* db) {
    rc_t rc = 0;

    const KMDataNode* node = NULL;
    VSchema* schema = NULL;
    char buffer[99999] = "";
    size_t size = 0;

    assert(db);

    if (db->dMeta != NULL) {
        VDatabase* newDb = NULL;
        char typespec[ 128 ] = "";

        rc = VDatabaseTypespec ( db->rDb, typespec, sizeof typespec );
        DISP_RC( rc, "VDatabaseTypespec( db ) failed" );

        if (rc == 0) {
            rc = KMetadataOpenNodeRead(db->dMeta, &node, "schema");
            DISP_RC( rc, "while KMetadataOpenNodeRead(db)" );
        }
        if (rc == 0) {
            rc = KMDataNodeReadCString(node, buffer, sizeof buffer, &size);
            DISP_RC( rc, "while KMDataNodeRead(db)" );
        }

        if (rc == 0) {
            rc = VDBManagerMakeSchema(db->mgr, &schema);
            DISP_RC( rc, "while VDBManagerMakeSchema(db)" );
        }
        if (rc == 0) {
            rc = VSchemaParseText(schema, NULL, buffer, size);
            DISP_RC( rc, "while VSchemaParseText(db)" );
        }

        if (rc == 0) {
            rc = VDBManagerCreateDB
                (db->mgr, &newDb, schema, typespec, 0, s_TmpPath);
            DISP_RC( rc, "while VDBManagerCreateDB" );
        }

        if (rc == 0) {
            const char name[] = "SEQUENCE";
            rc = VDatabaseCreateTable(newDb, &db->newTbl, name, 0, name);
            DISP_RC( rc, "while VDatabaseCreateTable" );
        }

        if (rc == 0) {
            rc = VTableCreateCursorWrite(db->newTbl, &db->wCursor, kcmInsert);
            DISP_RC( rc, "while VTableCreateCursorWrite(new table)" );
            if (rc == 0) {
                rc = VCursorAddColumn(db->wCursor, &db->wIdx, s_Column);
                if (rc != 0) {
                  PLOGERR(klogErr, (klogErr, rc,
                   "while adding $(name) to write cursor", "name=%s", s_Column));
                }
                else {
                    rc = VCursorOpen(db->wCursor);
                    DISP_RC( rc, "while VCursorOpen(new table)" );
                }
            }
        }

        RELEASE(VDatabase, newDb);
    }

    RELEASE(VSchema, schema);
    RELEASE(KMDataNode, node);

    return rc;
}

static rc_t DbDestroy(Db* db)
{
    rc_t rc = 0;

    assert(db);

    {
        rc_t rc2 = KMetadataRelease(db->meta);
        db->meta = NULL;
        if (rc == 0)
        {   rc = rc2; }
    }

    {
        rc_t rc2 = KMetadataRelease(db->dMeta);
        db->dMeta = NULL;
        if (rc == 0)
            rc = rc2;
    }

    {
        rc_t rc2 = VCursorRelease(db->rCursor);
        db->rCursor = NULL;
        if (rc == 0)
        {   rc = rc2; }
    }

    {
        rc_t rc2 = VCursorRelease(db->wCursor);
        db->wCursor = NULL;
        if (rc == 0)
        {   rc = rc2; }
    }

    if (db->VCursorOpenFAILED) {
        rc_t r2 = VTableDropColumn(db->wTbl, s_Column);
        if (r2 == SILENT_RC(rcDB, rcDirectory, rcRemoving, rcPath, rcNotFound))
            r2 = 0;
        DISP_RC(r2, "during VTableDropColumn()");
        if (rc == 0 && r2 != 0)
            rc = r2;

        if (r2 == 0) {
            r2 = VTableCopyColumn(db->wTbl, true, db->newTbl, "RD_FILTER");
            DISP_RC(r2, "during TableCopyColumn()");
            if (rc == 0 && r2 != 0)
                rc = r2;
        }
    }

    {
        rc_t rc2 = VTableRelease(db->wTbl);
        db->wTbl = NULL;
        if (rc == 0)
        {   rc = rc2; }
    }

    {
        rc_t rc2 = VTableRelease(db->newTbl);
        db->newTbl = NULL;
        if (rc == 0)
        {   rc = rc2; }
    }

    {
        rc_t rc2 = VDatabaseRelease(db->rDb);
        db->rDb = NULL;
        if (rc == 0)
            rc = rc2;
    }

    if (db->locked) {
        rc_t rc2 = VDBManagerLock(db->mgr, "%s", db->table);
        if (rc == 0)
        {   rc = rc2; }
    }

    {
        rc_t rc2 = VDBManagerRelease(db->mgr);
        db->mgr = NULL;
        if (rc == 0)
        {   rc = rc2; }
    }

    if (db->VCursorOpenFAILED) {
        KDirectory* d = NULL;
        rc_t r2 = KDirectoryNativeDir(&d);
        if (r2 != 0) {
            if (rc == 0)
                rc = r2;
        } else {
            r2 = KDirectoryRemove(d, true, s_TmpPath);
            DISP_RC(r2, "during KDirectoryRemove");
            if (r2 != 0&& rc == 0)
                rc = r2;
        }
        RELEASE(KDirectory, d);
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

    uint8_t filter[64];
    memset(filter, SRA_READ_FILTER_REDACTED, sizeof filter);

    assert(it);

    while (rc == 0 && SpotIteratorNext(it, &rc, &row_id, &toRedact)) {
        uint8_t nreads = 0;
        char bufferIn[64];
        void* buffer = NULL;
        uint32_t row_len = 0;

        rc = Quitting();

        ++nSpots;

        if (rc == 0) {
            uint32_t elem_bits = 8;
            rc = VCursorReadDirect(db->rCursor, row_id, db->rFilterIdx,
                elem_bits, bufferIn, sizeof bufferIn, &row_len);
            DISP_RC(rc, "while reading READ_FILTER");
            nreads = row_len;
        }
        if (toRedact) {
            buffer = filter;
            ++redactedSpots;
            DBGMSG(DBG_APP,DBG_COND_1,
                ("Redacting spot %d: %d reads\n",row_id,nreads));
        }
        else {
            buffer = bufferIn;
        }
        if (rc == 0) {
            rc = VCursorOpenRow(db->wCursor);
            DISP_RC(rc, "while opening row to write");
            if (rc == 0) {
                rc = VCursorWrite
                    (db->wCursor, db->wIdx, 8 * nreads, buffer, 0, 1);
                DISP_RC(rc, "while writing READ_FILTER");
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
    rc = string_printf ( s, size, NULL, "%lT", &tr );

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

    rc = string_printf ( buff, sizeof( buff ), NULL, "%.3V", KAppVersion() );
    assert ( rc == 0 );
    rc = KMDataNodeWriteAttr ( node, key, buff );
    DISP_RC( rc, "enter_version:KMDataNodeWriteAttr() failed" );
    return rc;
}

static rc_t enter_date_name_vers( KMDataNode *node )
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
    return rc;
}

static rc_t update_history ( KMetadata *dst_meta ) {
    rc_t rc = 0;
    rc_t r2 = 0;
    char event_name[ 32 ] = "";

    KMDataNode *hist_node = NULL;
    rc = KMetadataOpenNodeUpdate ( dst_meta, &hist_node, "HISTORY" );
    DISP_RC(rc, "while Opening HISTORY Metadata");
    if ( rc == 0 ) {
        uint32_t index = get_child_count( hist_node );

        if ( index > 0 ) { /* make sure EVENT_[i-1] exists */
            const KMDataNode *node = NULL;
            rc_t r = KMDataNodeOpenNodeRead ( hist_node, & node, "EVENT_%u",
                                                                        index );
            if ( r != 0 )
                PLOGERR(klogErr, (klogErr, r,
                    "while opening metanode HISTORY/EVENT_'$(n)'",
                    "n=%u", index));
            else
                KMDataNodeRelease ( node );
        }

        ++ index;

        { /* make sure EVENT_[i] does not exist */
            const KMDataNode *node = NULL;
            rc_t r = KMDataNodeOpenNodeRead ( hist_node, & node, "EVENT_%u",
                                                                        index );
            if ( r == 0 ) {
                uint32_t mx = index * 2;
                KMDataNodeRelease ( node );
                r = RC ( rcExe, rcMetadata, rcReading, rcNode, rcExists );
                PLOGERR(klogErr, (klogErr, r,
                    "while opening metanode HISTORY/EVENT_'$(n)'",
                    "n=%u", index));
                r = 0;
                for ( ; index < mx; ++ index ) {
                    r = KMDataNodeOpenNodeRead ( hist_node, & node, "EVENT_%u",
                                                                        index);
                    if ( r == 0 ) {
                        KMDataNodeRelease ( node );
                        r = RC ( rcExe, rcMetadata, rcReading,
                                        rcNode, rcExists );
                        PLOGERR(klogErr, (klogErr, r,
                            "while opening metanode HISTORY/EVENT_'$(n)'",
                            "n=%u", index));
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

        rc = string_printf ( event_name, sizeof( event_name ), NULL, "EVENT_%u",
                                                                     index );
        DISP_RC( rc, "update_history:string_printf(EVENT_NR) failed" );

        if ( rc == 0 )
        {
            KMDataNode *event_node;
            rc = KMDataNodeOpenNodeUpdate ( hist_node, &event_node,
                                            event_name );
            DISP_RC( rc,
                "update_history:KMDataNodeOpenNodeUpdate('EVENT_NR') failed" );
            if ( rc == 0 )
            {
                rc = enter_date_name_vers( event_node );
                KMDataNodeRelease ( event_node );
            }
        }
    }

    r2 = KMDataNodeRelease ( hist_node );
    if ( r2 != 0 && rc == 0 )
        rc = r2;
    return rc;
}

static rc_t Run(const CmdLine* args)
{
    rc_t rc = 0;

    Db db;

    SpotIterator it;
    memset(&it, 0, sizeof it);

    assert(args);

    if (string_chr(args->table, string_measure(args->table, NULL), '/') != NULL)
    {
        rc = RC(rcExe, rcArgv, rcParsing, rcPath, rcUnsupported);
        PLOGERR(klogErr, (klogErr, rc,
            "table is expected to be a directory in the current directory. "
            "Don't use absolute or relative path: $(path)",
            "path=%s", args->table));
        return rc;
    } 

    if (!SpotIteratorFileExists(args->file)) {
        rc = RC(rcExe, rcFile, rcOpening, rcFile, rcNotFound);
        PLOGERR(klogErr,
            (klogErr, rc, "Cannot find '$(path)'", "path=%s", args->file));
    }
    else if (!SpotIteratorFileExists(args->table)) {
        rc = RC(rcExe, rcTable, rcOpening, rcTable, rcNotFound);
        PLOGERR(klogErr,
            (klogErr, rc, "Cannot find '$(path)'", "path=%s", args->table));
    }

    {
        rc_t rc2 = DbInit(rc, args, &db);
        if (rc2 != 0 && db.VCursorOpenFAILED)
            rc2 = DbInit2(&db);
        if (rc == 0 && rc2 != 0)
            rc = rc2;
    }

    if (rc == 0) {
        rc = SpotIteratorInit(args->file, &db, &it);
    }

    if (rc == 0) {
        rc = Work(&db, &it);
    }

    if (rc == 0)
        rc = update_history(db.meta);

    if (rc == 0) {
        PLOGMSG(klogInfo, (klogInfo,
            "Success: redacted $(redacted) spots out of $(all)",
            "redacted=%d,all=%d", db.redactedSpots, db.nSpots));
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

#define OPTION_FILE  "file"
#define ALIAS_FILE   "F"

static const char* file_usage []
    = { "File containing SpotId-s to redact" , NULL };

OptDef Options[] =  {
     { OPTION_FILE , ALIAS_FILE , NULL, file_usage,  1, true , true }
};

rc_t CC UsageSummary (const char* progname)
{
    return KOutMsg (
        "Usage:\n"
        "  %s [Options] -" ALIAS_FILE " <file> <run>\n", progname);
}

const char UsageDefaultName[] = "rd-filter-redact";

rc_t CC Usage(const Args* args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    KOutMsg(
        "\n"
        "  N.B. <run> is expected to be a directory in the current directory.\n"
        "  Don't use relative or absolute path. \n"
        "\n"
        "Options:\n");

    HelpOptionLine(ALIAS_FILE, OPTION_FILE, "file", file_usage);

    HelpOptionsStandard();

    HelpVersion(fullpath, KAppVersion());

    return rc;
}


static rc_t CmdLineInit(const Args* args, CmdLine* cmdArgs)
{
    rc_t rc = 0;

    assert(args && cmdArgs);

    memset(cmdArgs, 0, sizeof *cmdArgs);

    while (rc == 0) {
        uint32_t pcount = 0;

        /* file path parameter */
        rc = ArgsOptionCount(args, OPTION_FILE, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing file name");
            break;
        }
        if (pcount < 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR(klogErr, rc, "Missing file parameter");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many file parameters");
            break;
        }
        rc = ArgsOptionValue (args, OPTION_FILE, 0, (const void **)&cmdArgs->file);
        if (rc) {
            LOGERR(klogErr, rc, "Failure retrieving file name");
            break;
        }

        /* table path parameter */
        rc = ArgsParamCount(args, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing table name");
            break;
        }
        if (pcount < 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR(klogErr, rc, "Missing table parameter");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many table parameters");
            break;
        }
        rc = ArgsParamValue(args, 0, (const void **)&cmdArgs->table);
        if (rc) {
            LOGERR(klogErr, rc, "Failure retrieving table name");
            break;
        }

        break;
    }

    if (rc != 0) {
        MiniUsage (args);
    }

    return rc;
}

rc_t CC KMain(int argc, char* argv[])
{
    rc_t rc = 0;
    Args* args = NULL;

    CmdLine cmdArgs;

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

    return rc;
}

/************************************ EOF ****************** ******************/
