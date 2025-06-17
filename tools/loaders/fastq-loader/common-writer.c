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

#if _POSIX_C_SOURCE < 200809L
#define _POSIX_C_SOURCE 200809L /* pwrite */
#endif

#include "common-writer.h"

#include <stdio.h>

#include <klib/log.h>
#include <klib/rc.h>
#include <klib/printf.h>

#include <kfs/file.h>

#include <kapp/main.h>

#include <kproc/queue.h>
#include <kproc/thread.h>
#include <kproc/timeout.h>

#include <loader/progressbar.h>

#include "common-reader-priv.h"
#include "spot-assembler.h"
#include "sequence-writer.h"

#define USE_READER_THREAD (1)

void EditAlignedQualities(const CommonWriterSettings* settings, uint8_t qual[], bool const hasMismatch[], unsigned readlen) /* generic */
{
    unsigned i;

    for (i = 0; i < readlen; ++i) {
        uint8_t const q = hasMismatch[i] ? settings->alignedQualValue : qual[i];

        qual[i] = q;
    }
}

void EditUnalignedQualities(uint8_t qual[], bool const hasMismatch[], unsigned readlen) /* generic */
{
    unsigned i;

    for (i = 0; i < readlen; ++i) {
        uint8_t const q = (qual[i] & 0x7F) | (hasMismatch[i] ? 0x80 : 0);

        qual[i] = q;
    }
}

rc_t CheckLimitAndLogError(CommonWriterSettings* settings)
{
    ++settings->errCount;
    if (settings->maxErrCount > 0 && settings->errCount > settings->maxErrCount) {
        (void)PLOGERR(klogErr, (klogErr, RC(rcAlign, rcFile, rcReading, rcError, rcExcessive),
                                "Number of errors $(cnt) exceeds limit of $(max): Exiting", "cnt=%lu,max=%lu",
                                settings->errCount, settings->maxErrCount));
        return RC(rcAlign, rcFile, rcReading, rcError, rcExcessive);
    }
    return 0;
}

void RecordNoMatch(const CommonWriterSettings* settings, char const readName[], char const refName[], uint32_t const refPos)
{
    if (settings->noMatchLog) {
        static uint64_t lpos = 0;
        char logbuf[256];
        size_t len;

        if (string_printf(logbuf, sizeof(logbuf), &len, "%s\t%s\t%u\n", readName, refName, refPos) == 0) {
            KFileWrite(settings->noMatchLog, lpos, logbuf, len, NULL);
            lpos += len;
        }
    }
}

rc_t LogNoMatch(CommonWriterSettings* settings, char const readName[], char const refName[], unsigned rpos, unsigned matches)
{
    rc_t const rc = CheckLimitAndLogError(settings);
    static unsigned count = 0;

    ++count;
    if (rc) {
        (void)PLOGMSG(klogInfo, (klogInfo, "This is the last warning; this class of warning occurred $(occurred) times",
                                 "occurred=%u", count));
        (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos)",
                                 "name=%s,ref=%s,pos=%u,count=%u", readName, refName, rpos, matches));
    }
    else if (settings->maxWarnCount_NoMatch == 0 || count < settings->maxWarnCount_NoMatch)
        (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' contains too few ($(count)) matching bases to reference '$(ref)' at $(pos)",
                                 "name=%s,ref=%s,pos=%u,count=%u", readName, refName, rpos, matches));
    return rc;
}

rc_t LogDupConflict(CommonWriterSettings* settings, char const readName[])
{
    rc_t const rc = CheckLimitAndLogError(settings);
    static unsigned count = 0;

    ++count;
    if (rc) {
        (void)PLOGMSG(klogInfo, (klogInfo, "This is the last warning; this class of warning occurred $(occurred) times",
                                 "occurred=%u", count));
        (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                 "Spot '$(name)' is both a duplicate and NOT a duplicate!",
                                 "name=%s", readName));
    }
    else if (settings->maxWarnCount_DupConflict == 0 || count < settings->maxWarnCount_DupConflict)
        (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                 "Spot '$(name)' is both a duplicate and NOT a duplicate!",
                                 "name=%s", readName));
    return rc;
}

void COPY_QUAL(uint8_t D[], uint8_t const S[], unsigned const L, bool const R)
{
    if (R) {
        unsigned i;
        unsigned j;

        for (i = 0, j = L - 1; i != L; ++i, --j)
            D[i] = S[j];
    }
    else
        memmove(D, S, L);
}

void COPY_READ(INSDC_dna_text D[], INSDC_dna_text const S[], unsigned const L, bool const R)
{
    static INSDC_dna_text const compl[] = {
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 , '.',  0 ,
        '0', '1', '2', '3',  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C',
        'D',  0 ,  0 , 'M',  0 , 'K', 'N',  0 ,
         0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W',
         0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C',
        'D',  0 ,  0 , 'M',  0 , 'K', 'N',  0 ,
         0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W',
         0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0
    };
    if (R) {
        unsigned i;
        unsigned j;

        for (i = 0, j = L - 1; i != L; ++i, --j)
            D[i] = compl[((uint8_t const *)S)[j]];
    }
    else
        memmove(D, S, L);
}

/*--------------------------------------------------------------------------
 * ArchiveFile
 */

void ParseSpotName(char const name[], size_t* namelen)
{ /* remove trailing #... */
    const char* hash = string_chr(name, *namelen, '#');
    if (hash)
        *namelen = hash - name;
}

static int8_t LogOddsToPhred1(int8_t logOdds)
{ /* conversion table copied from interface/ncbi/seq.vschema */
    static int8_t const toPhred[] = {
                  0, 1, 1, 2, 2, 3, 3,
         4, 4, 5, 5, 6, 7, 8, 9,10,10,
        11,12,13,14,15,16,17,18,19,20,
        21,22,23,24,25,26,27,28,29,30,
        31,32,33,34,35,36,37,38,39,40
    };
    if (logOdds < -6)
        return 0;
    if (logOdds > 40)
        return 40;
    return toPhred[logOdds + 6];
}

static void LogOddsToPhred(unsigned const readLen, uint8_t dst[], int8_t const src[], int offset)
{
    unsigned i;

    for (i = 0; i < readLen; ++i)
        dst[i] = LogOddsToPhred1(src[i] - offset);
}

static void PhredToPhred(unsigned const readLen, uint8_t dst[], int8_t const src[], int offset)
{
    unsigned i;

    for (i = 0; i < readLen; ++i)
        dst[i] = src[i] - offset;
}

struct ReadResult {
    float progress;
    uint64_t recordNo;
    enum {
        rr_undefined = 0,
        rr_sequence,
        rr_rejected,
        rr_fileDone,
        rr_done,
        rr_error
    } type;
    union {
        struct sequence {
            char *name;
            char *spotGroup;
            char *seqDNA;
            unsigned char *quality;
            uint64_t id;
            unsigned readLen;
            unsigned readNo;
            int mated;
            int orientation;
            int bad;
            int inserted;
            int colorspace;
            char cskey;
        } sequence;
        struct reject {
            char *message;
            uint64_t line;
            unsigned column;
            int fatal;
        } reject;
        struct error {
            char const *message;
            rc_t rc;
        } error;
    } u;
};

static char *getSpotGroup(Sequence const *const sequence)
{
    char *rslt = NULL;
    char const *tmp = NULL;
    size_t len = 0;

    SequenceGetSpotGroup(sequence, &tmp, &len);
    if (tmp) {
        rslt = malloc(len + 1);
        if (rslt) {
            memmove(rslt, tmp, len);
            rslt[len] = '\0';
        }
    }
    else {
        rslt = malloc(1);
        if (rslt) {
            rslt[0] = '\0';
        }
    }
    return rslt;
}

static char *getName(Sequence const *const sequence, bool const parseSpotName)
{
    char *rslt = NULL;
    char const *tmp = NULL;
    size_t len = 0;

    SequenceGetSpotName(sequence, &tmp, &len);
    if (parseSpotName)
        ParseSpotName(tmp, &len);
    rslt = malloc(len + 1);
    if (rslt) {
        memmove(rslt, tmp, len);
        rslt[len] = '\0';
    }
    return rslt;
}

static char const kReaderFileGetRecord[] = "ReaderFileGetRecord";
static char const kRecordGetSequence[] = "RecordGetSequence";
static char const kRecordGetRejected[] = "RecordGetRejected";
static char const kRejectedGetError[] = "RejectedGetError";
static char const kSequenceGetQuality[] = "SequenceGetQuality";
static char const kGetKeyID[] = "GetKeyID";
static char const kSequenceGetRead[] = "SequenceGetRead";
static char const kQuitting[] = "Quitting";

static void readSequence(CommonWriterSettings *const G, SpotAssembler *const ctx, Sequence const *const sequence, struct ReadResult *const rslt)
{
    char *seqDNA = NULL;
    uint8_t *qual = NULL;
    char *name = NULL;
    char *spotGroup = NULL;
    uint32_t readLen = 0;
    unsigned readNo = 0;
    int mated = 0;
    int orientation = 0;
    int colorspace = 0;
    char cskey[2];
    uint64_t keyId = 0;
    bool wasInserted = 0;
    bool bad = 0;
    rc_t rc = 0;

    memset(cskey, 0, sizeof(cskey));
    colorspace = SequenceIsColorSpace(sequence);
    orientation = SequenceGetOrientationSelf(sequence);
    bad = SequenceIsLowQuality(sequence);
    if (colorspace)
        rc = SequenceGetCSReadLength(sequence, &readLen);
    else
        rc = SequenceGetReadLength(sequence, &readLen);
    assert(rc == 0);
    seqDNA = malloc(readLen);
    qual = malloc(readLen);
    name = getName(sequence, G->parseSpotName);
    spotGroup = getSpotGroup(sequence);

    if (   seqDNA == NULL
        || qual == NULL
        || name == NULL
        || spotGroup == NULL
        )
    {
        fprintf(stderr, "OUT OF MEMORY!!!");
        abort();
    }

    if (!colorspace) {
        rc = SequenceGetRead(sequence, seqDNA);
    }
    else {
        SequenceGetCSKey(sequence, cskey);
        rc = SequenceGetCSRead(sequence, seqDNA);
    }
    if (rc != 0) {
        (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(name)': failed to get sequence", "%s", name));
        rslt->type = rr_error;
        rslt->u.error.rc = rc;
        rslt->u.error.message = kSequenceGetRead;
        goto CLEANUP;
    }
    {
        int8_t const *squal = NULL;
        uint8_t qoffset = 0;
        int qualType = 0;

        if (!colorspace || G->useQUAL)
            rc = SequenceGetQuality(sequence, &squal, &qoffset, &qualType);
        else {
            rc = SequenceGetCSQuality(sequence, &squal, &qoffset, &qualType);
            qualType = QT_Phred;
        }
        if (rc != 0) {
            (void)PLOGERR(klogErr, (klogErr, rc, "Spot '$(name)': length of original quality does not match sequence", "name=%s", name));
            rslt->type = rr_error;
            rslt->u.error.rc = rc;
            rslt->u.error.message = kSequenceGetQuality;
            goto CLEANUP;
        }
        if (squal) {
            if (qualType == QT_Phred)
                PhredToPhred(readLen, qual, squal, qoffset);
            else if (qualType == QT_LogOdds)
                LogOddsToPhred(readLen, qual, squal, qoffset);
            else
                memmove(qual, squal, readLen);
        }
        else if (!colorspace)
            memset(qual, 30, readLen);
        else
            memset(qual, 0, readLen);
    }
    if (SequenceWasPaired(sequence)) {
        if (SequenceIsFirst(sequence))
            readNo |= 1;
        if (SequenceIsSecond(sequence))
            readNo |= 2;
        if (readNo == 1 || readNo == 2)
            mated = 1;
    }
    if (!mated)
        readNo = 1;

    rc = SpotAssemblerGetKeyID(ctx,
                               &keyId,
                               &wasInserted,
                               spotGroup,
                               name,
                               strlen(name));
    if (rc != 0) {
        rslt->type = rr_error;
        rslt->u.error.rc = rc;
        rslt->u.error.message = kGetKeyID;
    }
CLEANUP:
    if (rslt->type == rr_error) {
        free(seqDNA);
        free(qual);
        free(name);
        free(spotGroup);
    }
    else {
        rslt->type = rr_sequence;
        rslt->u.sequence.name = name;
        rslt->u.sequence.spotGroup = spotGroup;
        rslt->u.sequence.seqDNA = seqDNA;
        rslt->u.sequence.quality = qual;
        rslt->u.sequence.id = keyId;
        rslt->u.sequence.readLen = readLen;
        rslt->u.sequence.readNo = readNo;
        rslt->u.sequence.mated = mated;
        rslt->u.sequence.orientation = orientation;
        rslt->u.sequence.bad = bad;
        rslt->u.sequence.inserted = wasInserted;
        rslt->u.sequence.colorspace = colorspace;
        rslt->u.sequence.cskey = cskey[0];
    }
    return;
}

static void freeReadResultSequence(struct ReadResult const *const rslt)
{
    free(rslt->u.sequence.name);
    free(rslt->u.sequence.spotGroup);
    free(rslt->u.sequence.seqDNA);
    free(rslt->u.sequence.quality);
}

static void readRejected(Rejected const *const reject, struct ReadResult *const rslt)
{
    char const *message;
    uint64_t line = 0;
    uint64_t col = 0;
    bool fatal = 0;
    rc_t const rc = RejectedGetError(reject, &message, &line, &col, &fatal);

    if (rc == 0) {
        rslt->type = rr_rejected;
        rslt->u.reject.message = string_dup_measure(message, NULL);
        rslt->u.reject.line = line;
        rslt->u.reject.column = col;
        rslt->u.reject.fatal = fatal;
    }
    else {
        rslt->type = rr_error;
        rslt->u.error.rc = rc;
        rslt->u.error.message = kRejectedGetError;
    }
}

static void freeReadResultRejected(struct ReadResult const *const rslt)
{
    free(rslt->u.reject.message);
}

static void readRecord(CommonWriterSettings *const G, SpotAssembler *const ctx, Record const *const record, struct ReadResult *const rslt)
{
    rc_t rc = 0;
    Rejected const *rej = NULL;

    rc = RecordGetRejected(record, &rej);
    if (rc == 0 && rej == NULL) {
        Sequence const *sequence = NULL;
        rc = RecordGetSequence(record, &sequence);
        if (rc == 0) {
            assert(sequence != NULL);
            readSequence(G, ctx, sequence, rslt);
            SequenceRelease(sequence);
            return;
        }
        rslt->type = rr_error;
        rslt->u.error.rc = rc;
        rslt->u.error.message = kRecordGetSequence;
        return;
    }
    if (rc != 0) {
        rslt->type = rr_error;
        rslt->u.error.rc = rc;
        rslt->u.error.message = kRecordGetRejected;
        return;
    }
    assert(rej != NULL);
    readRejected(rej, rslt);
    RejectedRelease(rej);
}

static struct ReadResult *threadGetNextRecord(
    CommonWriterSettings *const G,
    SpotAssembler *const ctx,
    struct ReaderFile const *reader,
    uint64_t *reccount)
{
    rc_t rc = 0;
    Record const *record = NULL;
    struct ReadResult *const rslt = calloc(1, sizeof(*rslt));

    assert(rslt != NULL);
    if (rslt != NULL)
    {
        rslt->progress = ReaderFileGetProportionalPosition(reader);
        rslt->recordNo = ++*reccount;
        if (G->maxAlignCount > 0 && rslt->recordNo > G->maxAlignCount) {
            (void)PLOGMSG(klogDebug, (klogDebug, "reached limit of $(max) records read", "max=%u", (unsigned)G->maxAlignCount));
            rslt->type = rr_fileDone;
            return rslt;
        }
        rc = ReaderFileGetRecord(reader, &record);
        if (rc != 0) {
            rslt->type = rr_error;
            rslt->u.error.rc = rc;
            rslt->u.error.message = kReaderFileGetRecord;
            return rslt;
        }
        if (record != NULL) {
            readRecord(G, ctx, record, rslt);
            RecordRelease(record);
            return rslt;
        }
        else {
            rslt->type = rr_fileDone;
            return rslt;
        }
    }
    abort();
}

static void freeReadResultError(struct ReadResult const *const rslt)
{
    (void)0;
}

static void freeReadResult(struct ReadResult const *const rslt)
{
    if (rslt->type == rr_sequence)
        freeReadResultSequence(rslt);
    else if (rslt->type == rr_rejected)
        freeReadResultRejected(rslt);
    else if (rslt->type == rr_error)
        freeReadResultError(rslt);
}

struct ReadThreadContext {
    KThread *th;
    KQueue *que;
    CommonWriterSettings *settings;
    SpotAssembler *ctx;
    ReaderFile const *reader1;
    ReaderFile const *reader2;
    ReaderFile const *cur_reader;
    bool reader1_active;
    bool reader2_active;
    uint64_t reccount;
};

static rc_t readThread(KThread const *const th, void *const ctx)
{
    struct ReadThreadContext *const self = ctx;
    rc_t rc = 0;

    self->reader1_active = self -> reader1 != NULL;
    self->reader2_active = self -> reader2 != NULL;

    if ( self -> reader1_active )
    {
        self->cur_reader = self -> reader1;
    }
    else
    {
        self->cur_reader = self -> reader2;
    }
    assert( self->cur_reader != NULL );

    while ( rc == 0 && Quitting() == 0 )
    {
        (void)LOGMSG(klogDebug, "threadGetNextRecord from ");

        (void)PLOGMSG(klogDebug, (klogDebug, "threadGetNextRecord($(r))", "r=%s",
                                self->cur_reader == self -> reader1 ? "1" : "2" ) );

        struct ReadResult *const rr =
            threadGetNextRecord(
                self->settings,
                self->ctx,
                self->cur_reader,
                &self->reccount
            );
        bool fileDone = rr->type == rr_fileDone;

        while ( Quitting() == 0  && ! fileDone )
        {
            timeout_t tm;
            TimeoutInit(&tm, 10000);
            rc = KQueuePush(self->que, rr, &tm);
            if (rc == 0)
            {
                break;
            }
            if ((int)GetRCObject(rc) == rcTimeout)
            {
                continue;
            }
            break;
        }

        if ( rc != 0 )
        {   // KQueuePush failed
            free(rr);
            if ((int)GetRCState(rc) == rcReadonly && (int)GetRCObject(rc) == rcQueue)
            {
                (void)LOGMSG(klogDebug, "readThread: consumer closed queue");
                rc = 0;
            }
            else
            {
                (void)LOGERR(klogErr, rc, "readThread: failed to push next record into queue");
                break;
            }
        }
        else if (fileDone)
        {
            /* normal exit from an end of file */
            (void)LOGMSG(klogDebug, "readThread: end of file");
            /* do not use this reader anymore */
            if ( self->cur_reader == self -> reader1 )
            {
                self->reader1_active = false;
            }
            else
            {
                assert( self->cur_reader == self -> reader2 );
                self->reader2_active = false;
            }
            free(rr);
        }

        // switch to the other reader if necessary
        if ( self->cur_reader == self -> reader1 )
        {
            if ( self->reader2_active )
            {
                self->cur_reader = self -> reader2;
            }
        }
        else
        {
            if ( self->reader1_active )
            {
                self->cur_reader = self -> reader1;
            }
        }

        // if no more readers left, we are done
        if ( fileDone && ! self->reader1_active && ! self->reader2_active )
        {   
            break;
        }
    }
    KQueueSeal(self->que);
    return rc;
}

static struct ReadResult getNextRecord(struct ReadThreadContext *const self)
{
    struct ReadResult rslt;

    memset(&rslt, 0, sizeof(rslt));
#if USE_READER_THREAD
    if (self->th == NULL) {
        rslt.u.error.rc = KQueueMake(&self->que, 1024);
        if (rslt.u.error.rc) {
            rslt.type = rr_error;
            rslt.u.error.message = "KQueueMake";
            return rslt;
        }
        rslt.u.error.rc = KThreadMake(&self->th, readThread, (void *)self);
        if (rslt.u.error.rc) {
            rslt.type = rr_error;
            rslt.u.error.message = "KThreadMake";
            return rslt;
        }
    }
    while ((rslt.u.error.rc = Quitting()) == 0) {
        timeout_t tm;
        void *rr = NULL;

        TimeoutInit(&tm, 10000);
        rslt.u.error.rc = KQueuePop(self->que, &rr, &tm);
        if (rslt.u.error.rc == 0) {
            memmove(&rslt, rr, sizeof(rslt));
            free(rr);
            if (rslt.type == rr_done)
                goto DONE;
            return rslt;
        }
        if ((int)GetRCObject(rslt.u.error.rc) == rcTimeout) {
            (void)LOGMSG(klogDebug, "KQueuePop timed out");
        }
        else {
            if ( Quitting() == 0 )
            {
                if ( (int)GetRCObject(rslt.u.error.rc) != rcData ||
                     (int)GetRCState(rslt.u.error.rc) != rcDone  ) // normal exit from a canceled wait on a queue
                    (void)LOGERR(klogErr, rslt.u.error.rc, "KQueuePop failed");
            }
            rslt.type = rr_done;
            goto DONE;
        }
    }
    rslt.type = rr_error;
    rslt.u.error.message = kQuitting;
DONE:
    {
        rc_t rc = 0;
        KThreadWait(self->th, &rc);
        KThreadRelease(self->th);
        KQueueRelease(self->que);
        self->que = NULL;
        self->th = NULL;
    }
#else
    if ((rslt.u.error.rc = Quitting()) == 0) {
        struct ReadResult *const rr = threadGetNextRecord(self->settings, self->ctx, self->reader1, &self->reccount);
        rslt = *rr;
        free(rr);
    }
    else {
        rslt.type = rr_error;
        rslt.u.error.message = kQuitting;
    }
#endif
    return rslt;
}

rc_t
HandleSequence( const struct ReadResult * rr,
                const char * fileName,
                CommonWriterSettings * G,
                struct SpotAssembler * ctx,
                bool * had_sequences,
                uint64_t * fragmentsAdded,
                uint64_t * fragmentsEvicted,
                uint64_t * spotsCompleted,
                KDataBuffer * fragBuf,
                SequenceRecord * srec,
                struct SequenceWriter * seq
            )
{
    bool removeMe = false; //TODO: remove all code related to colorspace
    bool * isColorSpace = & removeMe;
    rc_t rc = 0;
    if ( rr != NULL && rr -> type == rr_sequence)
    {
        uint64_t const keyId = rr -> u.sequence.id;
        bool const wasInserted = !!rr -> u.sequence.inserted;
        bool const mated = !!rr -> u.sequence.mated;
        unsigned const readNo = rr -> u.sequence.readNo;
        char const *const seqDNA = rr -> u.sequence.seqDNA;
        uint8_t const *const qual = rr -> u.sequence.quality;
        unsigned const readlen = rr -> u.sequence.readLen;
        int const readOrientation = !!rr -> u.sequence.orientation;
        bool const reverse = * isColorSpace ? false : (readOrientation == ReadOrientationReverse);
        char const cskey = rr -> u.sequence.cskey;
        bool const bad = !!rr -> u.sequence.bad;
        char const *const spotGroup = rr -> u.sequence.spotGroup;
        char const *const name = rr -> u.sequence.name;
        int const namelen = strlen(name);

        ctx_value_t *value;
        value = SpotAssemblerGetCtxValue(ctx, &rc, keyId);
        if (value == NULL)
        {
            (void)PLOGERR(klogErr, (klogErr, rc, "MMArrayGet: failed on id '$(id)'", "id=%u", keyId));
            return rc;
        }

        if (wasInserted) {
            memset(value, 0, sizeof(*value));
            value->fragmentOffset = -1;
            value->unmated = !mated;
        }
        else {
            if ( ! G->allowDuplicateReadNames )
            {   // VDB-4524
                if ( ! mated && value -> unmated )
                {   // same read name, no frag# in either
                    rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
                    PLOGERR(klogErr, (klogErr, rc,
                        "Duplicate read name '$(name)'",
                        "name=%s", name));
                    return rc;
                }
            }

            if (mated && value->unmated) {
                (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                            "Spot '$(name)', which was first seen without mate info, now has mate info",
                                            "name=%s", name));
                rc = CheckLimitAndLogError(G);
                return rc;
            }
        }

        if (mated) {
            if (value->written) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' has already been assigned a spot id", "name=%s", name));
            }
            else if (!value->has_a_read) {
                /* new mated fragment - do spot assembly */
                unsigned sz;
                FragmentInfo fi;
                void *myData = NULL;
                FragmentEntry * frag = SpotAssemblerGetFragmentEntry(ctx, keyId);
                int64_t const victimId = frag -> id;
                void *const victimData = frag -> data;

                ++ (*fragmentsAdded);
                value->seqHash[readNo - 1] = SeqHashKey(seqDNA, readlen);

                memset(&fi, 0, sizeof(fi));
                fi.orientation = readOrientation;
                fi.otherReadNo = readNo;
                fi.sglen   = strlen(spotGroup);
                fi.readlen = readlen;
                fi.cskey = cskey;
                fi.is_bad = bad;
                sz = sizeof(fi) + 2*fi.readlen + fi.sglen;
                myData = malloc(sz);
                if (myData == NULL) {
                    (void)LOGERR(klogErr, RC(rcExe, rcFile, rcReading, rcMemory, rcExhausted), "OUT OF MEMORY!");
                    abort();
                    return rc;
                }
                {{
                    uint8_t *dst = (uint8_t*)myData;

                    memmove(dst,&fi,sizeof(fi));
                    dst += sizeof(fi);
                    COPY_READ((char *)dst, seqDNA, fi.readlen, reverse);
                    dst += fi.readlen;
                    COPY_QUAL(dst, qual, fi.readlen, reverse);
                    dst += fi.readlen;
                    memmove(dst,spotGroup,fi.sglen);
                }}

                frag->id = keyId;
                frag->data = myData;

                value->has_a_read = 1;
                value->fragmentSize = sz;
                *had_sequences = true;

                if (victimData != NULL) {
                    ctx_value_t *const victim = SpotAssemblerGetCtxValue(ctx, &rc, victimId);
                    if (victim == NULL) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "MMArrayGet: failed on id '$(id)'", "id=%u", victimId));
                        abort();
                        return rc;
                    }
                    if (victim->fragmentOffset < 0) {
                        int64_t const pos = ctx->nextFragment; //AB
                        int64_t const nwrit = pwrite(ctx->fragmentFd, victimData, victim->fragmentSize, pos); //AB

                        if (nwrit == victim->fragmentSize) {
                            ctx->nextFragment += victim->fragmentSize; //AB
                            victim->fragmentOffset = pos;
                            free(victimData);
                            ++ (*fragmentsEvicted);
                        }
                        else {
                            (void)LOGMSG(klogFatal, "Failed to write fragment data");
                            abort();
                            return rc;
                        }
                    }
                    else {
                        (void)LOGMSG(klogFatal, "PROGRAMMER ERROR!!");
                        abort();
                        return rc;
                    }
                }

                /* save the read, to be used when mate shows up, or in SpotAssemblerWriteSoloFragments() */
                rc = Id2Name_Add ( & ctx->id2name, keyId, name );
            }
            else {
                /* might be second fragment */
                uint64_t const sz = value->fragmentSize;
                bool freeFip = false;
                FragmentInfo *fip = NULL;
                FragmentEntry * frag = SpotAssemblerGetFragmentEntry(ctx, keyId);

                if (frag->id == keyId)
                {
                    fip = frag->data;
                    freeFip = true;
                }
                else {
                    int64_t nread = 0;
                    rc = KDataBufferResize(fragBuf, (size_t)sz);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to resize fragment buffer", ""));
                        abort();
                        return rc;
                    }
                    nread = pread(ctx->fragmentFd, fragBuf->base, sz, value->fragmentOffset); //AB
                    if (nread == sz) {
                        fip = (FragmentInfo *) fragBuf->base;
                    }
                    else {
                        (void)PLOGMSG(klogFatal, (klogFatal, "Failed to read fragment data; spot:'$(name)'; "
                            "size: $(size); pos: $(pos); read: $(read)", "name=%s,size=%lu,pos=%lu,read=%lu",
                            name, sz, value->fragmentOffset, nread));
                        abort();
                        return rc;
                    }
                }
                if (readNo == fip->otherReadNo)
                {   // VDB-4524
                    if ( ! G->allowDuplicateReadNames )
                    {
                        rc = RC(rcApp, rcFile, rcReading, rcData, rcInconsistent);
                        PLOGERR(klogErr, (klogErr, rc,
                            "Duplicate read name '$(name)'",
                            "name=%s", name));
                        if (freeFip) {
                            free(fip);
                            frag->data = NULL;
                        }                            
                        return rc;
                    }
                }
                else
                {   /* mate found */
                    unsigned readLen[2];
                    unsigned read1 = 0;
                    unsigned read2 = 1;
                    uint8_t  *src  = (uint8_t*) fip + sizeof(*fip);

                    ++ (*spotsCompleted);
                    value->seqHash[readNo - 1] = SeqHashKey(seqDNA, readlen);

                    if (readNo < fip->otherReadNo) {
                        read1 = 1;
                        read2 = 0;
                    }
                    readLen[read1] = fip->readlen;
                    readLen[read2] = readlen;
                    rc = SequenceRecordInit(srec, 2, readLen);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "Failed resizing sequence record buffer", ""));
                        return rc;
                    }
                    srec->is_bad[read1] = fip->is_bad;
                    srec->orientation[read1] = fip->orientation;
                    srec->cskey[read1] = fip->cskey;
                    memmove(srec->seq + srec->readStart[read1], src, fip->readlen);
                    src += fip->readlen;
                    memmove(srec->qual + srec->readStart[read1], src, fip->readlen);
                    src += fip->readlen;

                    srec->orientation[read2] = readOrientation;
                    COPY_READ(srec->seq + srec->readStart[read2],
                                seqDNA,
                                srec->readLen[read2],
                                reverse);
                    COPY_QUAL(srec->qual + srec->readStart[read2],
                                qual,
                                srec->readLen[read2],
                                reverse);

                    srec->keyId = keyId;
                    srec->is_bad[read2] = bad;
                    srec->cskey[read2] = cskey;

                    rc = Id2Name_Get ( & ctx->id2name, keyId, (const char**) & srec->spotName );
                    if (rc)
                    {
                        (void)LOGERR(klogErr, rc, "Is2Name_Get failed");
                        return rc;
                    }
                    srec->spotNameLen = strlen(srec->spotName);

                    srec->spotGroup = (char *)spotGroup;
                    srec->spotGroupLen = strlen(spotGroup);
                    rc = SequenceWriteRecord(seq, srec, *isColorSpace, false, G->platform,
                                                G->keepMismatchQual, G->no_real_output, G->hasTI, G->QualQuantizer, G->dropReadnames);
                    if (freeFip) {
                        free(fip);
                        frag->data = NULL;
                    }
                    if (rc) {
                        (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                        return rc;
                    }
                    CTX_VALUE_SET_S_ID(*value, ++ctx->spotId); //AB
                    value->written = 1;
                }
            }
        }
        else if (value->written) {
            (void)PLOGMSG(klogWarn, (klogWarn, "Spot '$(name)' has already been assigned a spot id", "name=%s", name));
        }
        else if (value->has_a_read)
        {
            FragmentEntry * frag = SpotAssemblerGetFragmentEntry(ctx, keyId);
            if (frag -> id == keyId)
            {
                free ( frag -> data );
                frag -> data = NULL;
            }
            (void)PLOGERR(klogWarn, (klogWarn, RC(rcApp, rcFile, rcReading, rcData, rcInconsistent),
                                        "Spot '$(name)', which was first seen with mate info, now has no mate info",
                                        "name=%s", name));
            rc = CheckLimitAndLogError(G);
        }
        else {
            /* new unmated fragment - no spot assembly */
            unsigned readLen[1];

            value->seqHash[0] = SeqHashKey(seqDNA, readlen);

            readLen[0] = readlen;
            rc = SequenceRecordInit(srec, 1, readLen);
            if (rc) {
                (void)LOGERR(klogErr, rc, "Failed resizing sequence record buffer");
                return rc;
            }
            srec->is_bad[0] = bad;
            srec->orientation[0] = readOrientation;
            srec->cskey[0] = cskey;
            COPY_READ(srec->seq  + srec->readStart[0], seqDNA, readlen, false);
            COPY_QUAL(srec->qual + srec->readStart[0], qual, readlen, false);

            srec->keyId = keyId;

            srec->spotGroup = (char *)spotGroup;
            srec->spotGroupLen = strlen(spotGroup);

            srec->spotName = (char *)name;
            srec->spotNameLen = namelen;

            rc = SequenceWriteRecord(seq, srec, *isColorSpace, false, G->platform,
                                        G->keepMismatchQual, G->no_real_output, G->hasTI, G->QualQuantizer, G->dropReadnames);
            if (rc) {
                (void)LOGERR(klogErr, rc, "SequenceWriteRecord failed");
                return rc;
            }
            CTX_VALUE_SET_S_ID(*value, ++ctx->spotId); //AB
            value->written = 1;
            *had_sequences = true;
        }
    }
    else
        abort();
    return rc;
}

rc_t ArchiveFile(const struct ReaderFile *const reader1,
                 const struct ReaderFile *const reader2,
                 CommonWriterSettings *const G,
                 struct SpotAssembler *const ctx,
                 struct SequenceWriter *const seq,
                 bool *const had_sequences,
                 bool *const isColorSpace,
                 const struct KLoadProgressbar *progress_bar)
{
    KDataBuffer fragBuf;
    rc_t rc;
    SequenceRecord srec;
    unsigned progress = 0;
    uint64_t recordsProcessed = 0;
    uint64_t filterFlagConflictRecords=0; /*** counts number of conflicts between flags 'duplicate' and 'lowQuality' ***/
#define MAX_WARNINGS_FLAG_CONFLICT 10000 /*** maximum errors to report ***/

    char const *const fileName = ReaderFileGetPathname(reader1); //AB
    struct ReadThreadContext threadCtx;
    uint64_t fragmentsAdded = 0;
    uint64_t spotsCompleted = 0;
    uint64_t fragmentsEvicted = 0;

    memset(&srec, 0, sizeof(srec));
    memset(&threadCtx, 0, sizeof(threadCtx));
    threadCtx.settings = G;
    threadCtx.ctx = ctx;
    threadCtx.reader1 = reader1;
    threadCtx.reader2 = reader2;

    rc = KDataBufferMake(&fragBuf, 8, 4096);
    if (rc)
        return rc;

    if (rc == 0) {
        (void)PLOGMSG(klogInfo, (klogInfo, "Loading '$(file)'", "file=%s", fileName));
    }

    *had_sequences = false;

    while (rc == 0) {
        struct ReadResult const rr = getNextRecord(&threadCtx);

        if ((unsigned)(rr.progress * 100.0) > progress) {
            unsigned new_value = rr.progress * 100.0;
            KLoadProgressbar_Process(progress_bar, new_value - progress, false);
            progress = new_value;
        }
        if (rr.type == rr_done)
            break;
        if ( rr.type == rr_fileDone )
            continue;
        if (rr.type == rr_error) {
            rc = rr.u.error.rc;
            if (rr.u.error.message == kQuitting) {
                (void)LOGMSG(klogInfo, "Exiting read loop");
                break;
            }
            if (rr.u.error.message == kReaderFileGetRecord) {
                if (GetRCObject(rc) == rcRow && (GetRCState(rc) == rcInvalid || GetRCState(rc) == rcEmpty)) {
                    (void)PLOGERR(klogWarn, (klogWarn, rc, "ArchiveFile: '$(file)' - row $(row)", "file=%s,row=%lu", fileName, rr.recordNo));
                    rc = CheckLimitAndLogError(G);
                }
                /* else fail */
            }
            else {
                (void)PLOGERR(klogErr, (klogErr, rc, "ArchiveFile: $(func) failed", "func=%s", rr.u.error.message));
                rc = CheckLimitAndLogError(G);
            }
        }
        else if (rr.type == rr_rejected) {
            char const *const message = rr.u.reject.message;
            uint64_t const line = rr.u.reject.line;
            uint64_t const col = rr.u.reject.column;
            bool const fatal = rr.u.reject.fatal;

            (void)PLOGMSG(fatal ? klogErr : klogWarn, (fatal ? klogErr : klogWarn,
                                                       "$(file):$(l):$(c):$(msg)", "file=%s,l=%lu,c=%lu,msg=%s",
                                                       fileName, line, col, message));
            rc = CheckLimitAndLogError(G);
            if (fatal)
                rc = RC(rcExe, rcFile, rcParsing, rcFormat, rcUnsupported);
        }
        else
        {
            rc = HandleSequence( & rr, fileName, G, ctx, had_sequences, & fragmentsAdded, & fragmentsEvicted, & spotsCompleted, & fragBuf, & srec, seq );
            ++recordsProcessed;
        }


        freeReadResult(&rr);
    }

    if (threadCtx.que != NULL && threadCtx.th != NULL) {
        /* this means the exit was triggered in here, so the producer thread
         * needs to be notified and allowed to exit
         *
         * if the exit were triggered by the context setup, then
         * only one of que or th would be NULL
         *
         * it the exit were triggered by the getNextRecord, then both
         * que and th would be NULL
         */
        KQueueSeal(threadCtx.que);
        for ( ; ; ) {
            timeout_t tm;
            void *rr = NULL;
            rc_t rc;

            TimeoutInit(&tm, 1000);
            rc = KQueuePop(threadCtx.que, &rr, &tm);
            if (rc == 0)
                free(rr);
            else
                break;
        }
        KThreadWait(threadCtx.th, NULL);
    }
    KThreadRelease(threadCtx.th);
    KQueueRelease(threadCtx.que);

    if (filterFlagConflictRecords > 0) {
        (void)PLOGMSG(klogWarn, (klogWarn, "$(cnt1) out of $(cnt2) records contained warning : both 'duplicate' and 'lowQuality' flag bits set, only 'duplicate' will be saved", "cnt1=%lu,cnt2=%lu", filterFlagConflictRecords,recordsProcessed));
    }
    if (rc == 0 && recordsProcessed == 0) {
        (void)LOGMSG(klogWarn, (G->limit2config || G->refFilter != NULL) ?
                     "All records from the file were filtered out" :
                     "The file contained no records that were processed.");
        rc = RC(rcAlign, rcFile, rcReading, rcData, rcEmpty);
    }
    if (rc == 0 && threadCtx.reccount > 0) {
        uint64_t const reccount = threadCtx.reccount - 1;
        double const percentage = ((double)G->errCount) / reccount;
        double const allowed = G->maxErrPct/ 100.0;
        if (percentage > allowed) {
            rc = RC(rcExe, rcTable, rcClosing, rcData, rcInvalid);
            (void)PLOGERR(klogErr,
                            (klogErr, rc,
                             "Too many bad records: "
                                 "records: $(records), bad records: $(bad_records), "
                                 "bad records percentage: $(percentage), "
                                 "allowed percentage: $(allowed)",
                             "records=%lu,bad_records=%lu,percentage=%.2f,allowed=%.2f",
                             reccount, G->errCount, percentage, allowed));
        }
    }
    (void)PLOGMSG(klogDebug, (klogDebug, "Fragments added to spot assembler: $(added). Fragments evicted to disk: $(evicted). Spots completed: $(completed)",
        "added=%lu,evicted=%lu,completed=%lu", fragmentsAdded, fragmentsEvicted, spotsCompleted));

    KDataBufferWhack(&fragBuf);
    KDataBufferWhack(&srec.storage);
    return rc;
}

/*--------------------------------------------------------------------------
 * CommonWriter
 */

rc_t CommonWriterInit(CommonWriter* self, struct VDBManager *mgr, struct VDatabase *db, const CommonWriterSettings* G)
{
    rc_t rc = 0;
    assert(self);
    assert(mgr);
    assert(db);

    memset(self, 0, sizeof(*self));
    if (G)
        self->settings = *G;

    self->seq = malloc(sizeof(*self->seq));
    if (self->seq == 0)
    {
        return RC(rcAlign, rcArc, rcAllocating, rcMemory, rcExhausted);
    }
    SequenceWriterInit(self->seq, db);

    if (self->settings.tmpfs == NULL)
        self->settings.tmpfs = "/tmp";

    rc = KLoadProgressbar_Make(&self->progress, 0);
    if (rc) return rc;

    KLoadProgressbar_Append(self->progress, 100 * self->settings.numfiles);

    rc = SpotAssemblerMake ( & self->ctx, self->settings.cache_size, self->settings.tmpfs, self->settings.pid);

    self->commit = true;

    return rc;
}

rc_t CommonWriterArchive(CommonWriter *const self,
                         const struct ReaderFile *const reader1,
                         const struct ReaderFile *const reader2)
{
    rc_t rc;
    bool has_sequences = false;

    assert(self);
    rc = ArchiveFile(reader1,
                     reader2,
                     &self->settings,
                     self->ctx,
                     self->seq,
                     &has_sequences,
                     &self->isColorSpace,
                     self->progress);
    if (rc)
        self->commit = false;
    else
        self->had_sequences |= has_sequences;

    self->err_count += self->settings.errCount;
    return rc;
}

rc_t CommonWriterComplete(CommonWriter* self, bool quitting, uint64_t maxDistance)
{
    rc_t rc=0;
    if (self->had_sequences)
    {
        if (!quitting)
        {
            (void)LOGMSG(klogInfo, "Writing unpaired sequences");
            rc = SpotAssemblerWriteSoloFragments(self->ctx,
                                                 self->isColorSpace,
                                                 self->settings.platform,
                                                 self->settings.keepMismatchQual,
                                                 self->settings.no_real_output,
                                                 self->settings.hasTI,
                                                 self->settings.QualQuantizer,
                                                 self->settings.dropReadnames,
                                                 self->seq,
                                                 self->progress);
            SpotAssemblerReleaseMemBank(self->ctx);
            if (rc == 0)
            {
                rc = SequenceDoneWriting(self->seq);
            }
        }
        else
        {
            SpotAssemblerReleaseMemBank(self->ctx);
        }
    }

    return rc;
}

rc_t CommonWriterWhack(CommonWriter* self)
{
    rc_t rc = 0;
    assert(self);

    SpotAssemblerRelease(self->ctx);

    KLoadProgressbar_Release(self->progress, true);

    if (self->seq)
    {
        SequenceWhack(self->seq, self->commit);
        free(self->seq);
    }

    return rc;
}

