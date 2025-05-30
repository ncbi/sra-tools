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
* =============================================================================$
*
*/

#include "sra-stat.h" /* VTableMakeSingleFileArchive_ */

#include <insdc/sra.h> /* SRA_READ_TYPE_BIOLOGICAL */

#include <kapp/log-xml.h> /* XMLLogger */
#include <kapp/main.h>

#include <kdb/database.h> /* KDatabase */
#include <kdb/kdb-priv.h>
#include <kdb/manager.h> /* KDBManager */
#include <kdb/meta.h> /* KMetadata */
#include <kdb/namelist.h> /* KMDataNodeListChild */
#include <kdb/table.h>

#include <kfg/config.h> /* KConfigSetNgcFile */

#include <kfs/directory.h> /* KDirectory */
#include <kfs/file.h> /* KFile */

#include <klib/checksum.h>
#include <klib/container.h>
#include <klib/debug.h> /* DBGMSG */
#include <klib/log.h> /* LOGERR */
#include <klib/namelist.h>
#include <klib/out.h>
#include <klib/printf.h>
#include <klib/rc.h>
#include <klib/sort.h> /* ksort */
#include <klib/text.h>

#include <sra/sraschema.h> /* VDBManagerMakeSRASchema */

#include <vdb/blob.h> /* VBlobCellData */
#include <vdb/cursor.h> /* VCursor */
#include <vdb/database.h> /* VDatabaseRelease */
#include <vdb/dependencies.h> /* VDBDependencies */
#include <vdb/manager.h> /* VDBManager */
#include <vdb/schema.h> /* VSchema */
#include <vdb/table.h> /* VTableRelease */
#include <vdb/vdb-priv.h> /* VDBManagerGetKDBManagerRead */

#include <vfs/manager.h> /* VFSManager */
#include <vfs/path.h> /* VPath */
#include <vfs/resolver.h> /* VResolver */

#include <os-native.h> /* strtok_r on Windows */

#include <loader/progressbar.h> /* KLoadProgressbar */

#include <assert.h>
#include <ctype.h> /* isprint */
#include <math.h> /* sqrt */
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DISP_RC2(rc, name, msg) (void)((rc == 0) ? 0 : \
    PLOGERR(klogInt, (klogInt, rc, \
        "$(name): $(msg)", "name=%s,msg=%s", name, msg)))

#define DISP_RC_Read(rc, name, spot, msg) (void)((rc == 0) ? 0 : \
    PLOGERR(klogInt, (klogInt, rc, "column $(name), spot $(spot): $(msg)", \
        "name=%s,spot=%lu,msg=%s", name, spot, msg)))

static size_t MAX_NREADS = 2500;

#define DEFAULT_CURSOR_CAPACITY (1024*1024*1024UL)

/********** _XMLLogger_Encode : copied from kapp/log-xml.c (-lload) ***********/

static
rc_t CC _XMLLogger_Encode(const char* src,
    char* aDst, size_t dst_sz, size_t *num_writ)
{
    rc_t rc = 0;
    size_t bytes;
    const char* p;
    char* dst = aDst;

    assert(src != NULL && num_writ != NULL);

    *num_writ = 0;
    if( *src == '\0' ) {
        *dst = *src;
    } else {
        do {
            switch(*src) {
                case '\'':
                    bytes = 6;
                    p = "&apos;";
                    break;
                case '"':
                    bytes = 6;
                    p = "&quot;";
                    break;
                case '&':
                    bytes = 5;
                    p = "&amp;";
                    break;
                case '<':
                    bytes = 4;
                    p = "&lt;";
                    break;
                case '>':
                    bytes = 4;
                    p = "&gt;";
                    break;
                default:
                    bytes = 1;
                    p = src;
                    break;
            }
            if( (*num_writ + bytes) > dst_sz ) {
                rc = RC(rcExe, rcLog, rcEncoding, rcBuffer, rcInsufficient);
            } else {
                memmove(dst, p, bytes);
                *num_writ = *num_writ + bytes;
                dst += bytes;
            }
        } while(rc == 0 && *++src != '\0');
        if (rc == 0 && *src == '\0' && ((*num_writ + bytes) <= dst_sz)) {
            *dst = *src;
        }
    }
    return rc;
}

/*********** XMLLogger_Encode : copied from kapp/log-xml.c (-lload) ***********/

typedef struct BAM_HEADER_RG { /* @RG: Read group. */
    char* ID; /* Read group identifier. */
    char* LB; /* Library. */
    char* SM; /* Sample. */
} BAM_HEADER_RG;
static
rc_t BAM_HEADER_RG_copy(BAM_HEADER_RG* dest, const BAM_HEADER_RG* src)
{
    assert(dest && !dest->ID && !dest->LB && !dest->SM
        && src  &&  src ->ID &&  src ->LB &&  src ->SM);

    dest->ID = string_dup_measure(src->ID, NULL);
    dest->LB = string_dup_measure(src->LB, NULL);
    dest->SM = string_dup_measure(src->SM, NULL);

    if (!dest->ID || !dest->LB || !dest->SM) {
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    }

    return 0;
}

static void BAM_HEADER_RG_free(BAM_HEADER_RG* rg) {
    assert(rg);

    free(rg->ID);
    free(rg->LB);
    free(rg->SM);

    rg->ID = rg->LB = rg->SM = NULL;
}

static void BAM_HEADER_RG_print_xml(const BAM_HEADER_RG* rg) {
    rc_t rc = 0;
    char library[1024] = "";
    char sample[1024] = "";
    char* l = library;
    char* s = sample;
    size_t num_writ = 0;

    assert(rg);

    if (rg->LB) {
        rc = _XMLLogger_Encode(rg->LB, library, sizeof library, &num_writ);
        if (rc != 0) {
            l = rg->LB;
            PLOGMSG(klogWarn, (klogWarn, "Library value is too long: "
                "XML-safe conversion was disabled for @RG\tID:$(ID)\tLB:$(LB)"
                "ID=%s,LB=%s", rg->ID, rg->LB));
        }

        OUTMSG((" library=\"%s\"", l ? l : ""));
    }

    if (rg->SM) {
        rc = _XMLLogger_Encode(rg->SM, sample, sizeof sample, &num_writ);
        if (rc != 0) {
            s = rg->SM;
            PLOGMSG(klogWarn, (klogWarn, "Sample value is too long: "
                "XML-safe conversion was disabled for @RG\tID:$(ID)\tSM:$(SM)"
                "ID=%s,SM=%s", rg->ID, rg->SM));
        }

        OUTMSG((" sample=\"%s\"" , s ? s : ""));
    }
}

typedef struct SraStats {
    BSTNode n;
    char     spot_group[1024]; /* SPOT_GROUP Column */
    uint64_t spot_count; /*** spot count ***/
    uint64_t spot_count_mates; /*** spots with mates ***/
    uint64_t bio_len; /** biological len (BIO_BASE_COUNT) **/
    uint64_t bio_len_mates; /** biological len when mates are present**/
    uint64_t total_len; /** total len (BASE_COUNT) **/
    uint64_t bad_spot_count; /** number of spots flagged with rd_filter=2 **/
    uint64_t bad_bio_len;        /** biological length of bad spots ***/
    uint64_t filtered_spot_count; /* number of spots flagged with rd_filter=2 */
    uint64_t filtered_bio_len;   /** biological length of filtered spots **/
    uint64_t total_cmp_len; /* CMP_READ : compressed */
    BAM_HEADER_RG BAM_HEADER;
} SraStats;
typedef struct Statistics {  /* READ_LEN columnn */
    /* average READ_LEN value */
    /* READ_LEN standard deviation. Is calculated just when requested. */
    /* running (continuous) standard deviation */

    int64_t n; /* number of values */
    double a; /* the mean value */
    double q; /* Qi = Q[i-1] + (Xi - A[i-1])(Xi - Ai) */

    bool variable; /* variable or fixed value */
    double prev_val;
} Statistics;
typedef struct Statistics2 {
    int n;
    double average;
    double diff_sq_sum;
} Statistics2;
typedef enum {
    ebtUNDEFINED,
    ebtREAD,
    ebtCSREAD,
    ebtRAW_READ,
} EBasesType;
typedef struct {
    uint64_t cnt[5];
    EBasesType basesType;

    const VCursor * cursSEQUENCE;
    uint32_t        idxSEQUENCE;
    uint32_t        idxSEQ_READ_LEN;
    uint32_t        idxSEQ_READ_TYPE;
    uint64_t        startSEQUENCE;
    uint64_t        stopSEQUENCE;

    const VCursor * cursALIGNMENT;
    uint32_t        idxALIGNMENT;
    uint32_t        idxALN_READ_LEN;
    uint32_t        idxALN_READ_TYPE;
    uint64_t        startALIGNMENT;
    uint64_t        stopALIGNMENT;

    bool finalized;

/* accessing PRIMARY_ALIGNMENT/RD_FILTER terribly slows down the reading
    uint32_t        idxALN_RD_FILTER;
   so we count filtered reads, as well
    uint32_t        idxSEQ_RD_FILTER; */
} Bases;
typedef struct SraStatsTotal {
    uint64_t spot_count;
    uint64_t spot_count_mates;
    uint64_t BIO_BASE_COUNT; /* bio_len */
    uint64_t bio_len_mates;
    uint64_t BASE_COUNT; /* total_len */
    uint64_t bad_spot_count;
    uint64_t bad_bio_len;
    uint64_t filtered_spot_count;
    uint64_t filtered_bio_len;
    uint64_t total_cmp_len; /* CMP_READ : compressed */

    bool variable_nreads;
    uint32_t nreads; /* if (nreads == 0) then (nreads is variable) */
    Statistics * stats ; /* nreads elements */
    Statistics2* stats2; /* nreads elements */

    Bases bases_count;
} SraStatsTotal;
typedef struct srastat_parms {
    SraStatsTotal total; /* is used in srastat_print */

    const char* table_path;
    const XMLLogger *logger;

    bool hasSPOT_GROUP;
    bool print_arcinfo;
    bool printMeta;
    bool progress;     /* show progress */
    bool quick; /* quick mode: stats from meta */
    bool skip_alignment; /* not to print alignment info */
    bool print_locinfo; /* print info about a local run */
    bool repair; /* generate instructions for metadata repair tool */
    bool report; /* report all fields examined for mismatch */
    bool skip_members; /* not to print spot_group statistics */
    bool statistics; /* calculate average and stdev */
    bool test; /* test stdev */
    bool variableReadLength;
    bool xml; /* output format (txt or xml) */

    int64_t  start, stop;
} srastat_parms;

static
void Statistics2Init(Statistics2* self, double sum, int64_t  count)
{
    assert(self);

    if (count) {
        self->average = sum / count;
    }
}

static void Statistics2Add(Statistics2* self, double value) {
    double diff = 0;

    assert(self);

    ++self->n;

    diff = value - self->average;
    self->diff_sq_sum += diff * diff;
}

static void Statistics2Print(const Statistics2* selfs,
    uint32_t nreads, const char* indent,

     /* the same as in <Statistics: just to make <Statistics> and <Statistics2>
        look the same */
    uint64_t spot_count)
{
    uint32_t i = 0;

    if (nreads) {
        assert(selfs && indent);
    }

    OUTMSG(("%s<Statistics2 nreads=\"%u\" nspots=\"%lu\">\n",
        indent, nreads, spot_count));

    for (i = 0; i < nreads; ++i) {
        double dev = 0;
        const Statistics2* stats = selfs + i;
        double avr = stats->average;
        if (stats->n > 0) {
            dev = sqrt(stats->diff_sq_sum / stats->n);
        }

        OUTMSG(("%s  "
            "<Read index=\"%d\" count=\"%ld\" average=\"%f\" stdev=\"%f\"/>\n",
            indent, i, stats->n, avr, dev));
    }

    OUTMSG(("%s</Statistics2>\n", indent));
}

static bool columnUndefined(rc_t rc) {
    return rc == SILENT_RC(rcVDB, rcCursor, rcOpening , rcColumn, rcUndefined)
        || rc == SILENT_RC(rcVDB, rcCursor, rcUpdating, rcColumn, rcNotFound );
}

static rc_t BasesInit(Bases *self, const Ctx *ctx, const VTable *vtbl,
                      const srastat_parms *pb)
{
    rc_t rc = 0;

    assert(self);
    memset(self, 0, sizeof *self);

    assert(ctx && pb);

    if (ctx->db != NULL && pb -> start == 0 && pb -> stop == 0) {
        const VTable * tbl = NULL;
        rc_t r2 = VDatabaseOpenTableRead(ctx->db, &tbl, "PRIMARY_ALIGNMENT");
        if (r2 == 0) {
            const VCursor *curs = NULL;
            rc = VTableCreateCachedCursorRead ( tbl, & curs,
                                                DEFAULT_CURSOR_CAPACITY);
            DISP_RC(rc,
                "Cannot VTableCreateCachedCursorRead(PRIMARY_ALIGNMENT)");
            if (rc == 0) {
                rc = VCursorPermitPostOpenAdd(curs);
                DISP_RC(rc, "Cannot VCursorPermitPostOpenAdd");
            }
            if (rc == 0) {
                rc = VCursorOpen(curs);
                DISP_RC(rc, "Cannot VCursorOpen");
            }
            if (rc == 0) {
                const char name [] = "(INSDC:4na:bin)RAW_READ";
                rc = VCursorAddColumn(curs, &self->idxALIGNMENT, name);
                DISP_RC2(rc, "Cannot VCursorAddColumn", name);
            }
            if (rc == 0) {
                const char name [] = "READ_LEN";
                rc = VCursorAddColumn(curs, &self->idxALN_READ_LEN, name);
                if (rc != 0)
                    PLOGERR(klogInt, (klogInt, rc, "Cannot VCursorAddColumn"
                        "(PRIMARY_ALIGNMENT:$(name))", "name=%s", name));
            }
            if (rc == 0) {
                const char name [] = "READ_TYPE";
                rc = VCursorAddColumn(curs, &self->idxALN_READ_TYPE, name);
                if (rc != 0)
                    PLOGERR(klogInt, (klogInt, rc, "Cannot VCursorAddColumn"
                        "(PRIMARY_ALIGNMENT:$(name))", "name=%s", name));
            }
            if (rc == 0) {
                int64_t first = 0;
                uint64_t count = 0;
                rc = VCursorIdRange(curs, 0, &first, &count);
                if ( rc == 0 ) {
                    self->startALIGNMENT = first;
                    self->stopALIGNMENT = first + count;
                }
            }
            if (rc != 0)
                VCursorRelease ( curs );
            else
                self -> cursALIGNMENT = curs;
            self->basesType = ebtRAW_READ;
        }
        RELEASE(VTable, tbl);
    }

    if (rc == 0 && self->cursSEQUENCE == NULL
                && self->basesType != ebtRAW_READ)
    {
        const char name[] = "CS_NATIVE";
        const void *base = NULL;

        const VCursor *curs = NULL;
        uint32_t idx = 0;

        self->basesType = ebtCSREAD;

        rc = VTableCreateCachedCursorRead(vtbl, &curs, DEFAULT_CURSOR_CAPACITY);
        DISP_RC(rc, "Cannot VTableCreateCachedCursorRead");

        if (rc == 0) {
            rc = VCursorPermitPostOpenAdd(curs);
            DISP_RC(rc, "Cannot VCursorPermitPostOpenAdd");
        }

        if (rc == 0) {
            rc = VCursorOpen(curs);
            DISP_RC(rc, "Cannot VCursorOpen");
        }

        if (rc == 0) {
            rc = VCursorAddColumn(curs, &idx, "%s", name);
            if (rc != 0) {
                if (columnUndefined(rc)) {
                    self->basesType = ebtREAD;
                    rc = 0;
                }
                else {
                    DISP_RC(rc, "Cannot VCursorAddColumn(CS_NATIVE)");
                }
            }
            else {
                bitsz_t boff = ~0;
                bitsz_t row_bits = ~0;

                uint32_t elem_bits = 0, elem_off = 0, elem_cnt = 0;
                rc = VCursorCellDataDirect(curs, 1, idx,
                    &elem_bits, &base, &elem_off, &elem_cnt);
                boff     = elem_off * elem_bits;
                row_bits = elem_cnt * elem_bits;

                if (boff != 0 || row_bits != 8) {
                    rc = RC(rcExe, rcColumn, rcReading, rcData, rcInvalid);
                    PLOGERR(klogInt, (klogErr, rc, "invalid boff or row_bits "
                        "while VCursorCellDataDirect($(name))",
                        "name=%s", name));
                }

                if (rc == 0) {
                    bool CS_NATIVE = *((bool*)base);
                    self -> basesType = CS_NATIVE ? ebtCSREAD : ebtREAD;
                }

            }
        }

        RELEASE(VCursor, curs);
    }

    if (self->cursSEQUENCE == NULL && rc == 0) {
        const char *name = self->basesType == ebtCSREAD
            ? "(INSDC:x2cs:bin)CSREAD" :
              self->basesType == ebtREAD
                  ? "(INSDC:x2na:bin)READ" : "(INSDC:x2na:bin)CMP_READ";
        rc = VTableCreateCachedCursorRead(vtbl, &self->cursSEQUENCE,
                                          DEFAULT_CURSOR_CAPACITY);
        DISP_RC(rc, "Cannot VTableCreateCachedCursorRead");
        if (rc == 0) {
            rc = VCursorAddColumn(self->cursSEQUENCE, &self->idxSEQUENCE, name);
            if (rc != 0)
                PLOGERR(klogInt, (klogInt, rc,
                    "Cannot VCursorAddColumn($(name))", "name=%s", name));
        }
        if (rc == 0) {
            const char name [] = "READ_LEN";
            rc = VCursorAddColumn(self->cursSEQUENCE,
                                  &self->idxSEQ_READ_LEN, name);
            if (rc != 0)
                PLOGERR(klogInt, (klogInt, rc,
                    "Cannot VCursorAddColumn($(name))", "name=%s", name));
        }
        if (rc == 0) {
            const char name [] = "READ_TYPE";
            rc = VCursorAddColumn(self->cursSEQUENCE,
                                  &self->idxSEQ_READ_TYPE, name);
            if (rc != 0)
                PLOGERR(klogInt, (klogInt, rc,
                    "Cannot VCursorAddColumn($(name))", "name=%s", name));
        }
        if (rc == 0) {
            rc = VCursorOpen(self->cursSEQUENCE);
            if (rc != 0)
                PLOGERR(klogInt, (klogInt, rc,
                    "Cannot VCursorOpen($(name))", "name=%s", name));
        }
    }

    if ( rc == 0 ) {
        int64_t first = 0;
        uint64_t count = 0;
        rc = VCursorIdRange(self->cursSEQUENCE, 0, &first, &count);
        if ( rc == 0 ) {
            if (pb->start > 0) {
                self->startSEQUENCE = pb->start;
                if (self->startSEQUENCE < first)
                    self->startSEQUENCE = first;
            }
            else
                self->startSEQUENCE = first;

            if (pb->stop > 0) {
                self->stopSEQUENCE = pb->stop;
                if ( ( uint64_t ) self->stopSEQUENCE > first + count)
                    self->stopSEQUENCE = first + count;
            }
            else
                self->stopSEQUENCE = first + count;
        }
    }

    return rc;
}

static void BasesFinalize(Bases *self) {
    assert(self);

    if (self->cursSEQUENCE == NULL) {
        LOGMSG(klogInfo, "Bases statistics will not be printed : "
            "READ cursor was not opened during BasesFinalize()");
        return;
    }

    self->finalized = true;
}

static rc_t BasesRelease(Bases *self) {
    rc_t rc = 0;

    assert(self);

    RELEASE(VCursor, self->cursSEQUENCE);
    RELEASE(VCursor, self->cursALIGNMENT);

    return rc;
}

static rc_t BasesAdd(Bases *self, int64_t spotid, bool alignment,
    uint32_t * dREAD_LEN, uint8_t * dREAD_TYPE)
{
    rc_t rc = 0;
    const void *base = NULL;
    bitsz_t row_bits = ~0;
    int64_t i = ~0;
    const unsigned char *bases = NULL;
    bitsz_t boff = 0;

    const VCursor * c = NULL;
    int64_t row_id = 0;

    int nreads = 0;

    int read = 0;
    uint32_t nxtRdStart = 0;

    assert(self);

    if (self->cursSEQUENCE == NULL) {
        return 0;
    }

    if ( alignment ) {
        c      = self -> cursALIGNMENT;
        row_id = self -> idxALIGNMENT;
    }
    else {
        c      = self -> cursSEQUENCE;
        row_id = self -> idxSEQUENCE;
    }
    if (rc == 0) {
        rc = VCursorColumnRead(c, spotid,
            alignment ? self->idxALN_READ_LEN : self->idxSEQ_READ_LEN,
            &base, &boff, &row_bits);
        DISP_RC_Read(rc, "READ_LEN", spotid, "while calling VCursorColumnRead");
        if (rc == 0) {
            if (boff & 7)
                rc = RC(rcExe, rcColumn, rcReading, rcOffset, rcInvalid);
            else if (row_bits & 7)
                rc = RC(rcExe, rcColumn, rcReading, rcSize, rcInvalid);
            else if ((row_bits >> 3) > MAX_NREADS * sizeof *dREAD_LEN)
                rc = RC(rcExe, rcColumn, rcReading, rcBuffer, rcInsufficient);
            DISP_RC_Read(rc, "READ_LEN", spotid,
                         "after calling VCursorColumnRead");
        }
        if (rc == 0) {
            nreads = (row_bits >> 3)  / sizeof(*dREAD_LEN);
            memmove(dREAD_LEN, ((const char*)base) + (boff >> 3),
                    ( size_t ) row_bits >> 3);
        }
    }

    if (rc == 0) {
        rc = VCursorColumnRead(c, spotid,
             alignment ? self->idxALN_READ_TYPE : self->idxSEQ_READ_TYPE,
             &base, &boff, &row_bits);
        DISP_RC_Read(rc, "READ_TYPE", spotid,
                     "while calling VCursorColumnRead");
        if (rc == 0) {
            if (boff & 7)
                rc = RC(rcExe, rcColumn, rcReading, rcOffset, rcInvalid);
            else if (row_bits & 7)
                rc = RC(rcExe, rcColumn, rcReading, rcSize, rcInvalid);
            else if ((row_bits >> 3) > MAX_NREADS * sizeof * dREAD_TYPE)
                rc = RC(rcExe, rcColumn, rcReading,
                    rcBuffer, rcInsufficient);
            else if (((row_bits >> 3) / sizeof(*dREAD_TYPE)) != nreads)
                rc = RC(rcExe, rcColumn, rcReading, rcData, rcIncorrect);
            DISP_RC_Read(rc, "READ_TYPE", spotid,
                "after calling VCursorColumnRead");
            if (rc == 0)
                memmove(dREAD_TYPE,
                    ((const char*)base) + (boff >> 3),
                    ( size_t ) row_bits >> 3);
        }
    }

    if ( rc == 0 ) {
        uint32_t elem_bits = 0, elem_off = 0, elem_cnt = 0;
        rc = VCursorCellDataDirect(c, spotid, row_id,
                                   &elem_bits, &base, &elem_off, &elem_cnt);
        if (rc != 0) {
            const char * name = self->basesType == ebtCSREAD ? "CSREAD"
                : self->basesType == ebtREAD ? "READ" : "RAW_READ";
            PLOGERR(klogInt, (klogErr, rc,
                "while VCursorCellDataDirect($(name))",
                "name=%s", name));
            BasesRelease(self);
            return rc;
        }

        row_bits = elem_cnt * elem_bits;
    }

    if ( rc != 0 )
        return rc;

    if ((row_bits % 8) != 0) {
        const char * name = self->basesType == ebtCSREAD ? "CSREAD"
            : self->basesType == ebtREAD ? "READ" : "RAW_READ";
        rc = RC(rcExe, rcColumn, rcReading, rcData, rcInvalid);
        PLOGERR(klogInt, (klogErr, rc, "Invalid row_bits '$(row_bits) "
            "while VCursorCellDataDirect($(name), spotid=$(spotid))",
            "row_bits=%lu,name=%s,spotid=%lu", row_bits, name, spotid));
        BasesRelease(self);
        return rc;
    }

    row_bits /= 8;
    bases = base;

    for (i = 0; i < row_bits; ++i) {
        unsigned char base = bases[i];
        if ( i == nxtRdStart) {
            if ( read > nreads )
                return RC(rcExe, rcNumeral, rcComparing, rcData, rcInvalid);
            nxtRdStart += dREAD_LEN [ read ++ ];
            assert ( read > 0 );
            if ( ( (dREAD_TYPE[read-1] & SRA_READ_TYPE_BIOLOGICAL) == 0 )
                    /* skip non-biological reads */
                 ||
                 ( dREAD_LEN [ read - 1 ] == 0 )
                    /* skip empty reads */
               )
            {
                if ( dREAD_LEN [ read - 1 ] > 0 )
                    i += dREAD_LEN [ read - 1 ];
                -- i; /* here i can become negative: it should be signed */
                continue;
            }
        }

        if ( alignment ) {
            const unsigned char x [16]
                = { 4, 0, 1, 4, 2, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, };
            /*      0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
                       A  C     G           T                    N  */
            if (base > 15) {
                rc = RC(rcExe, rcColumn, rcReading, rcData, rcInvalid);
                PLOGERR(klogInt, (klogErr, rc, "Invalid RAW_READ column "
                    "value '$(base)' while VCursorCellDataDirect"
                    "(spotid=$(spotid), index=$(i))",
                    "base=%d,spotid=%lu,i=%lu", base, spotid, i));
                BasesRelease(self);
                return rc;
            }
            assert ( base < sizeof x / sizeof x [ 0 ] );
            base = x [  base ];
        }

        if (base > 4) {
            const char * name = self->basesType == ebtCSREAD ? "CSREAD"
                : self->basesType == ebtREAD ? "READ" : "RAW_READ";
            rc = RC(rcExe, rcColumn, rcReading, rcData, rcInvalid);
            PLOGERR(klogInt, (klogErr, rc,
               "Invalid READ column value '$(base)' while VCursorCellDataDirect"
               "($(name), spotid=$(spotid), index=$(i))",
               "base=%d,name=%s,spotid=%lu,i=%lu",
               base, name, spotid, i));
            BasesRelease(self);
            return rc;
        }

        ++self->cnt[base];
    }

    return 0;
}

static rc_t BasesPrint(const Bases *self,
    uint64_t base_count, const char* indent)
{
    rc_t rc = 0;

    const char tag[] = "Bases";
    const char *name = NULL;
    int i = ~0;

    assert(self);

    if (!self->finalized) {
        LOGMSG(klogInfo, "Bases statistics will not be printed : "
            "Bases object was not finalized during BasesPrint()");
        return rc;
    }

    name = self->basesType == ebtCSREAD ? "0123." : "ACGTN";

    if (self->cnt[0] + self->cnt[1] + self->cnt[2] +
        self->cnt[3] + self->cnt[4] != base_count)
    {
        rc = RC(rcExe, rcNumeral, rcComparing, rcData, rcInvalid);
        LOGERR(klogErr, rc,
               "stored base count did not match observed base count");
    }

    if ( rc == 0 )
      OUTMSG(("%s<%s cs_native=\"%s\" count=\"%lu\">\n", indent,
        tag, self->basesType == ebtCSREAD ? "true" : "false", base_count));
    else
      OUTMSG(("%s<%s cs_native=\"%s\" count=\"%lu\" calculated=\"%lu\">\n",
        indent, tag, self->basesType == ebtCSREAD ? "true" : "false",
        base_count, self->cnt[0] + self->cnt[1] + self->cnt[2] +
                    self->cnt[3] + self->cnt[4]));

    for (i = 0; i < 5; ++i) {
        OUTMSG(("%s  <Base value=\"%c\" count=\"%lu\"/>\n",
            indent, name[i], self->cnt[i]));
    }

    OUTMSG(("%s</%s>\n", indent, tag));

    return rc;
}

static
rc_t SraStatsTotalMakeStatistics(SraStatsTotal* self, uint32_t nreads)
{
    assert(self);

    if (nreads == 0) {
        return 0;
    }

    assert(self->stats == NULL);
    self->stats = calloc(nreads, sizeof *self->stats);
    if (self->stats == NULL) {
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    }

    assert(self->stats2 == NULL);
    self->stats2 = calloc(nreads, sizeof *self->stats2);
    if (self->stats2 == NULL) {
        free(self->stats);
        self->stats = NULL;
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    }

    self->nreads = nreads;

    return 0;
}

static
void SraStatsTotalStatistics2Init(SraStatsTotal* self,
    int nreads, uint64_t* sums, uint64_t* count)
{
    int i = 0;
    assert(self);
    if (nreads) {
        assert(sums);
    }
    for (i = 0; i < nreads; ++i) {
        uint64_t sum = sums[i];
        Statistics2* stats = self->stats2 + i;
        assert(stats);
        Statistics2Init(stats, ( double ) sum, count[i]);
    }
}

static rc_t SraStatsTotalFree(SraStatsTotal* self) {
    assert(self);

    free(self->stats);
    self->stats = NULL;

    free(self->stats2);
    self->stats2 = NULL;

    BasesRelease(&self->bases_count);

    self->nreads = 0;

    return 0;
}

static void StatisticsAdd(Statistics* self, double value) {
    double a_1 = 0;

    /* http://en.wikipedia.org/wiki/Stdev#Rapid_calculation_methods */

    assert(self);

    a_1 = self->a;

    if (self->n++ == 0) {
        self->prev_val = value;
    }
    else if (self->prev_val != value) {
        self->variable = true;
    }

    self->a += (value - a_1) / self->n;
    self->q += ((double)value - a_1) * ((double)value - self->a);
}

static double StatisticsAverage(const Statistics* self) {
    assert(self);

    return self->a;
}

static double StatisticsStdev(const Statistics* self) {
    assert(self);

    if (self->n == 0) {
        return 0;
    }

    return sqrt(self->q / self->n);
}

static
void SraStatsTotalAdd(SraStatsTotal* self,
    uint32_t* values, uint32_t nreads)
{
    uint32_t i = 0;

    assert(self && values);

    if (self->nreads != nreads) {
        self->variable_nreads = true;
    }

    if (self->variable_nreads) {
        return;
    }

    for (i = 0; i < nreads; ++i) {
        uint32_t value = values[i];
        if (value > 0) {
            Statistics* stats = self->stats + i;
            assert(stats);

            StatisticsAdd(stats, value);
        }
    }
}

static
void SraStatsTotalAdd2(SraStatsTotal* self, uint32_t* values) {
    uint32_t i = 0;

    assert(self && values);

    for (i = 0; i < self->nreads; ++i) {
        uint32_t value = values[i];
        if (value > 0) {
            Statistics2* stats = self->stats2 + i;
            assert(stats);

            Statistics2Add(stats, value);
        }
    }
}

static double s_Round(double X) { return floor(X + 0.5); }

static
void print_double_or_int(const char* name, double val, bool variable) {
    double rnd = s_Round(val);
    assert(name);
    if (variable) {
        OUTMSG((" %s=\"%.2f\"", name, val));
    }
    else {
        OUTMSG((" %s=\"%d\"", name, (int)rnd));
    }
}

static void StatisticsPrint(const Statistics* selfs,
    uint32_t nreads, const char* indent)
{
    uint32_t i = 0;

    if (nreads) {
        assert(selfs && indent);
    }

    for (i = 0; i < nreads; ++i) {
        const Statistics* stats = selfs + i;
        double avr = StatisticsAverage(stats);
        double dev = StatisticsStdev(stats);

        OUTMSG(("%s  <Read index=\"%d\"", indent, i));
        OUTMSG((" count=\"%ld\"", stats->n, i));
        print_double_or_int("average", avr, stats->variable);
        print_double_or_int("stdev"  , dev, stats->variable);
        OUTMSG(("/>\n"));
    }
}

static rc_t StatisticsDiff(const Statistics* ss,
    const Statistics2* ss2, uint32_t nreads)
{
    rc_t rc = 0;

    uint32_t i = 0;

    assert(ss && ss2);

    for (i = 0; i < nreads; ++i) {
        const Statistics * s  = ss  + i;
        const Statistics2* s2 = ss2 + i;

        double d  = StatisticsStdev(s);
        double d2 = sqrt(s2->diff_sq_sum / s2->n);

        double diff = d - d2;
        if (diff < 0) {
            diff = -diff;
        }

        if (diff > 1e-6) {
              /* RC(rcExe, rcTest, rcExecuting, rcTest, rcFailed); */
            rc = RC(rcExe, rcNumeral, rcComparing, rcData, rcInvalid);
            DISP_RC(rc, "While comparing calculated standard deviations");
        }

        if (s->n != s2->n) {
            rc = RC(rcExe, rcNumeral, rcComparing, rcData, rcInvalid);
            DISP_RC(rc, "While comparing read statistics counts");
        }

        diff = StatisticsAverage(s) - s2->average;
        if (diff < 0) {
            diff = -diff;
        }
        if (diff > 1e-10) {
              /* RC(rcExe, rcTest, rcExecuting, rcTest, rcFailed); */
            rc = RC(rcExe, rcNumeral, rcComparing, rcData, rcInvalid);
            DISP_RC(rc, "While comparing calculated average");
        }
    }

    return rc;
}

static rc_t SraStatsTotalPrintStatistics(
    const SraStatsTotal* self, const char* indent, bool test)
{
    rc_t rc = 0;

    assert(self && indent);

    OUTMSG(("%s<Statistics nreads=\"", indent));

    if (self->variable_nreads) {
        OUTMSG(("variable\"/>\n"));
    }
    else {
        OUTMSG(("%u\"", self->nreads));
        OUTMSG((" nspots=\"%lu\">\n", self->spot_count));
        StatisticsPrint(self->stats, self->nreads, indent);
        OUTMSG(("%s</Statistics>\n", indent));

        if (test) {
            Statistics2Print(self->stats2, self->nreads, indent,
                self->spot_count);
            rc = StatisticsDiff(self->stats, self->stats2, self->nreads);
        }
    }

    return rc;
}

typedef struct SraStatsMeta {
    bool found;
    uint64_t BASE_COUNT;
    uint64_t BIO_BASE_COUNT;
    uint64_t CMP_BASE_COUNT; /* compressed base count when present */
    uint64_t spot_count;
    char* spot_group; /* optional */
    BAM_HEADER_RG BAM_HEADER;
} SraStatsMeta;
typedef struct MetaDataStats {
    bool found;
    SraStatsMeta table;
    uint32_t spotGroupN;
    SraStatsMeta *spotGroup;
} MetaDataStats;
typedef struct SraSizeStats { uint64_t size; } SraSizeStats;
typedef struct Formatter {
    char name[256];
    char vers[256];
} Formatter;
typedef struct Loader {
    char date[256];
    char name[256];
    char vers[256];
} Loader;
typedef struct SraMeta {
    bool isDb;
    uint32_t metaVersion;
    int32_t tblVersion;
    time_t loadTimestamp;
    Formatter formatter;
    Loader loader;
} SraMeta;

typedef struct {
    const char* tag;
    uint64_t size;
    char md5[32 + 1];
} TArcInfo;
typedef struct ArcInfo {
    KTime_t timestamp;
    TArcInfo i[3]; /* 0 - sra
                      1 - sralite
                      2 - current ( eArcInfoCrnt ) */
} ArcInfo;
#define eArcInfoCrnt 2

typedef struct Quality {
    uint32_t value;
    uint64_t count;
} Quality;

typedef struct Count {
    EMetaState state;
    uint64_t value;
} Count;

typedef struct Counts {
    char* tableName;
    EMetaState tableState;
    EMetaState metaState;
    Count BASE_COUNT;
    Count SPOT_COUNT;
} Counts;
typedef rc_t (CC * RG_callback)(const BAM_HEADER_RG* rg, const void* data);
static
rc_t CC meta_RG_callback(const BAM_HEADER_RG* rg, const void* data)
{
    rc_t rc = 0;

    const MetaDataStats* meta_stats = data;

    uint32_t i = 0;
    bool found = false;

    assert(rg && meta_stats && rg->ID);

    for (i = 0; i < meta_stats->spotGroupN && rc == 0; ++i) {
        SraStatsMeta* ss = &meta_stats->spotGroup[i];
        const char* spot_group = ss->spot_group;

        assert(spot_group);
        if (strcmp(spot_group, rg->ID) == 0) {
            rc = BAM_HEADER_RG_copy(&ss->BAM_HEADER, rg);
            found = true;
            break;
        }
    }

    if (rc == 0 && !found) {
        /* There could be Read groups that are present in the BAM_HEADER
        but missed in the csra. We was just ignore them. */
    }

    return rc;
}

static int64_t CC srastats_cmp(const void *item, const BSTNode *n) {
    const char *sg = item;
    const SraStats *ss = ( const SraStats* ) n;

    return strcmp(sg,ss->spot_group);
}

static
rc_t CC tree_RG_callback(const BAM_HEADER_RG* rg, const void* data)
{
    rc_t rc = 0;

    const BSTree* tr = data;
    SraStats* ss = NULL;

    assert(rg && tr && rg->ID);

    ss = (SraStats*) BSTreeFind(tr, rg->ID, srastats_cmp);
    if (ss == NULL) {
        /* There could be Read groups that are present in the BAM_HEADER
        but missed in the csra. We was just ignore them. */
    }
    else {
        rc = BAM_HEADER_RG_copy(&ss->BAM_HEADER, rg);
    }

    return rc;
}

static int64_t CC QualityCmp(const void* s1, const void* s2, void *data) {
    const Quality* q1 = s1;
    const Quality* q2 = s2;
    assert(q1 && q2);
    return (int64_t)q1->value - (int64_t)q2->value;
}

static int64_t CC CountsCmp(const void* v1, const void* v2, void *data) {
    const Counts* e1 = v1;
    const Counts* e2 = v2;
    assert(e1 && e2 && e1->tableName && e2->tableName);
    return strcmp(e1->tableName, e2->tableName);
}

static rc_t QualityParse(Quality* self,
    const KMDataNode* node, const char* name)
{
    rc_t rc = 0;
    const char start[] = "PHRED_";
    size_t i = strlen(start);

    assert(self && name);

    if (strlen(name) <= strlen(start)) {
        rc = RC(rcExe, rcXmlDoc, rcParsing, rcName, rcInvalid);
        PLOGERR(klogInt, (klogErr, rc,
            "Bad node name: STATS/QUALITY/$(name)", "name=%s", name));
        return rc;
    }

    self->value = 0;
    for (; i < strlen(name); ++i) {
        if (name[i] < '0' || name[i] > '9') {
            rc_t rc = RC(rcExe, rcXmlDoc, rcParsing, rcName, rcInvalid);
            PLOGERR(klogInt, (klogErr, rc,
                "Bad node name: STATS/QUALITY/$(name)", "name=%s", name));
            return rc;
        }
        self->value = self->value * 10 + name[i] - '0';
    }

    rc = KMDataNodeReadAsU64(node, &self->count);
    if (rc != 0) {
        PLOGERR(klogInt, (klogErr, rc,
            "while reading the value of STATS/QUALITY/$(name)",
            "name=%s", name));
    }

    return rc;
}

static rc_t QualityStatsRead1(QualityStats* self,
    const KMDataNode* parent, const char* name)
{
    const KMDataNode* node = NULL;

    rc_t rc = KMDataNodeOpenNodeRead(parent, &node, "%s", name);
    DISP_RC2(rc, name, "while calling KMDataNodeOpenNodeRead");

    if (rc == 0) {
        size_t size = 64;
        assert(self);
        if (self->allocated == 0) {
            assert(self->QUALITY == NULL && self->used == 0);
            self->QUALITY = malloc(size * sizeof *self->QUALITY);
            if (self->QUALITY == NULL) {
                return
                    RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
            }
            self->allocated = size;
        }
        else if (self->used >= self->allocated) {
            Quality* tmp = NULL;
            size = self->allocated * 2;
            tmp = realloc(self->QUALITY, size * sizeof *self->QUALITY);
            if (tmp == NULL) {
                return
                    RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
            }
            self->QUALITY = tmp;
            self->allocated = size;
        }
    }

    if (rc == 0) {
        assert((self->used < self->allocated) && self->QUALITY);
        rc = QualityParse(&self->QUALITY[self->used], node, name);
        if (rc == 0)
        {   ++self->used; }
    }

    RELEASE(KMDataNode, node);

    return rc;
}

static rc_t QualityStatsRead(QualityStats* self, const KMetadata* meta) {
    rc_t rc = 0;
    const char name[] = "STATS/QUALITY";
    const KMDataNode* node = NULL;

    assert(self && meta);

    memset(self, 0, sizeof *self);

    rc = KMetadataOpenNodeRead(meta, &node, "%s", name);

    if (rc != 0) {
        if (GetRCState(rc) == rcNotFound) {
            DBGMSG(DBG_APP,DBG_COND_1, ("%s: not found\n", name));
            return 0;
        }
        DISP_RC2(rc, name, "while calling KMetadataOpenNodeRead");
    }

    if (rc == 0 && node) {
        KNamelist* names = NULL;
        rc = KMDataNodeListChild(node, &names);
        DISP_RC2(rc, name, "while calling KMDataNodeListChild");
        if (rc == 0) {
            uint32_t count = 0;
            rc = KNamelistCount(names, &count);
            DISP_RC2(rc, name, "while calling KNamelistCount");
            if (rc == 0) {
                uint32_t i = ~0;
                for (i = 0; i < count && rc == 0; ++i) {
                    const char* child = NULL;
                    rc = KNamelistGet(names, i, &child);
                    if (rc != 0) {
                       PLOGERR(klogInt, (klogInt, rc,
                            "while calling STATS/QUALITY::KNamelistGet($(idx))",
                            "idx=%i", i));
                    }
                    else
                    {   rc = QualityStatsRead1(self, node, child); }
                }
            }
        }
        RELEASE(KNamelist, names);
    }

    RELEASE(KMDataNode, node);

    return 0;
}

static
void QualityStatsSort(QualityStats* self)
{
    assert(self);
    ksort(self->QUALITY, self->used, sizeof *self->QUALITY, QualityCmp, NULL);
}

static
void TableCountsSort(TableCounts* self)
{
    assert(self);
    ksort(self->count, self->used, sizeof *self->count, CountsCmp, NULL);
}
static
rc_t QualityStatsPrint(const QualityStats* self, const char* indent)
{
    size_t i = ~0;
    const char tag[] = "QualityCount";
    assert(self && indent);

    if (self->allocated == 0)
    {   return 0; }

    OUTMSG(("%s<%s>\n", indent, tag));
    for (i = 0; i < self->used; ++i) {
        Quality* q = &self->QUALITY[i];
        OUTMSG(("  %s<Quality value=\"%lu\" count=\"%lu\"/>\n",
            indent, q->value, q->count));
    }
    OUTMSG(("%s</%s>\n", indent, tag));

    return 0;
}

static
void QualityStatsRelease(QualityStats* self)
{
    assert(self);
    free(self->QUALITY);
    memset(self, 0, sizeof *self);
}

static
rc_t CountRead(Count* self, const char* name, const KMDataNode* parent)
{
    rc_t rc = 0;

    const KMDataNode* node = NULL;

    assert(self && parent && name);

    memset(self, 0, sizeof *self);

    rc = KMDataNodeOpenNodeRead(parent, &node, "%s", name);
    if (rc != 0) {
        if (GetRCState(rc) == rcNotFound) {
            DBGMSG(DBG_APP,DBG_COND_1, ("%s: not found\n", name));
            rc = 0;
        }
        DISP_RC2(rc, name, "while calling KMetadataOpenNodeRead");
    }
    else {
        rc = KMDataNodeReadAsU64(node, &self->value);
        DISP_RC2(rc, name, "while calling KMDataNodeReadAsU64");
        if (rc == 0)
        {   self->state = eMSFound; }
    }

    RELEASE(KMDataNode, node);

    return rc;
}

static
rc_t TableCountsRead1(TableCounts* self,
    const VDatabase* db, const char* tableName)
{
    const VTable* tbl = NULL;
    const KMetadata* meta = NULL;
    const char name[] = "STATS/TABLE";
    const KMDataNode* node = NULL;
    rc_t rc = VDatabaseOpenTableRead(db, &tbl, "%s", tableName);
    DISP_RC2(rc, tableName, "while calling VDatabaseOpenTableRead");

    if (rc == 0) {
        size_t size = 8;
        assert(self);
        if (self->allocated == 0) {
            assert(self->count == NULL && self->used == 0);
            self->count = malloc(size * sizeof *self->count);
            if (self->count == NULL) {
                return
                    RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
            }
            self->allocated = size;
        }
        else if (self->used >= self->allocated) {
            Counts* tmp = NULL;
            size = self->allocated * 2;
            tmp = realloc(self->count, size * sizeof *self->count);
            if (tmp == NULL) {
                return
                    RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
            }
            self->count = tmp;
            self->allocated = size;
        }
    }
    if (rc == 0) {
        assert(self->used < self->allocated && self->count);
    }
    if (rc == 0) {
        rc = VTableOpenMetadataRead(tbl, &meta);
        DISP_RC2(rc, name, "while calling VTableOpenMetadataRead");
    }
    if (rc == 0) {
        rc = KMetadataOpenNodeRead(meta, &node, "%s", name);
        if (rc != 0) {
            if (GetRCState(rc) == rcNotFound) {
                DBGMSG(DBG_APP,DBG_COND_1, ("%s: not found\n", name));
                rc = 0;
            }
            DISP_RC2(rc, name, "while calling KMetadataOpenNodeRead");
        }
        else
        {   assert(node); }
    }

    if (rc == 0 && node != NULL) {
        Counts* c = NULL;
        assert((self->used < self->allocated) && (self->count != NULL));
        c = &self->count[self->used];
        memset(c, 0, sizeof *c);
        c->tableName = string_dup_measure(tableName, NULL);
        if (c->tableName == NULL)
        {   return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted); }
        c->tableState = eMSFound;
        c->metaState = eMSFound;
        rc = CountRead(&c->BASE_COUNT, "BASE_COUNT", node);

        {
            rc_t rc2 = CountRead(&c->SPOT_COUNT, "SPOT_COUNT", node);
            if (rc2 != 0 && rc == 0)
            {   rc = rc2; }
        }

        if (rc == 0)
        {   ++self->used; }
    }

    RELEASE(KMDataNode, node);
    RELEASE(KMetadata, meta);
    RELEASE(VTable, tbl);

    return rc;
}

static
rc_t TableCountsRead(TableCounts* self, const VDatabase* db)
{
    rc_t rc = 0;
    KNamelist* names = NULL;

    assert(self);

    memset(self, 0, sizeof *self);

    if (db == NULL)
        return 0;

    rc = VDatabaseListTbl(db, &names);
    DISP_RC(rc, "while calling VDatabaseListTbl");
    if (rc == 0) {
        uint32_t count = 0;
        rc = KNamelistCount(names, &count);
        DISP_RC(rc, "while calling VDatabaseListTbl::KNamelistCount");
        if (rc == 0) {
            uint32_t i = ~0;
            for (i = 0; i < count && rc == 0; ++i) {
                const char* table = NULL;
                rc = KNamelistGet(names, i, &table);
                if (rc != 0) {
                   PLOGERR(klogInt, (klogInt, rc,
                        "while calling VDatabaseListTbl::KNamelistGet($(idx))",
                        "idx=%i", i));
                }
                else
                {   rc = TableCountsRead1(self, db, table); }
            }
        }
    }

    RELEASE(KNamelist, names);

    return rc;
}

static
rc_t CountPrint(const Count* self, const char* indent, const char* tag)
{
    assert(self && indent && tag);
    switch (self->state) {
        case eMSNotFound:
            OUTMSG(("  %s<%s state=\"not found\"/>\n", indent, tag));
            break;
        case eMSFound:
            OUTMSG(("  %s<%s count=\"%lu\"/>\n", indent, tag, self->value));
            break;
        default:
            assert(0);
            OUTMSG(("  %s<%s state=\"UNEXPECTED\"/>\n", indent, tag));
            break;
    }
    return 0;
}

static
rc_t TableCountsPrint(const TableCounts* self, const char* indent)
{
    rc_t rc = 0;
    const char tag[] = "Databases";

    assert(self && indent);

    if (self->allocated == 0)
    {   return 0; }

    OUTMSG(("%s<%s>\n", indent, tag));
    {
        size_t i = ~0;
        const char tag[] = "Database";
        OUTMSG(("  %s<%s>\n", indent, tag));
        for (i = 0; i < self->used; ++i) {
            const char tag[] = "Statistics";
            const Counts* p = &self->count[i];
            OUTMSG(("    %s<Table name=\"%s\">\n", indent, p->tableName));
            switch (p->metaState) {
            case eMSNotFound:
                OUTMSG(("      %s<%s source=\"meta\" state=\"not found\"/>\n",
                    indent, tag));
                break;
            case eMSFound: {
                char buf[512];
                OUTMSG(("      %s<%s source=\"meta\">\n", indent, tag));
                rc = string_printf(buf, sizeof buf, NULL, "      %s", indent);
                if (rc == 0) {
                    CountPrint(&p->SPOT_COUNT, buf, "Rows");
                    CountPrint(&p->BASE_COUNT, buf, "Elements");
                }
                OUTMSG(("      %s</%s>\n", indent, tag));
                break;
              }
            default:
                assert(0);
                OUTMSG(("      %s<%s source=\"meta\" state=\"UNEXPECTED\"/>\n",
                    indent, tag));
                break;
            }
            OUTMSG(("    %s</Table>\n", indent));
        }
        OUTMSG(("  %s</%s>\n", indent, tag));
    }
    OUTMSG(("%s</%s>\n", indent, tag));

    return rc;
}

static
void TableCountsRelease(TableCounts* self)
{
    size_t i = ~0;
    assert(self);
    for (i = 0; i < self->used; ++i) {
        assert(self->count);
        free(self->count[i].tableName);
        self->count[i].tableName = NULL;
    }
    free(self->count);
    memset(self, 0, sizeof *self);
}

static
rc_t parse_bam_header(const VDatabase* db,
    const RG_callback cb, const void* data)
{
    rc_t rc = 0;
    const char name[] = "BAM_HEADER";
    const KMetadata* meta = NULL;
    const KMDataNode* node = NULL;
    char* buffer = NULL;

    if (db == NULL) {
        return rc;
    }
    assert(cb);

    if (rc == 0) {
        rc = VDatabaseOpenMetadataRead(db, &meta);
        DISP_RC(rc, "while calling VDatabaseOpenMetadataRead");
    }

    if (rc == 0) {
        rc = KMetadataOpenNodeRead(meta, &node, "%s", name);
        if (rc != 0) {
            if (GetRCState(rc) == rcNotFound) {
                rc = 0;
                RELEASE ( KMetadata, meta );
                return rc;
            }
            DISP_RC2(rc, name, "while calling KMetadataOpenNodeRead");
        }
    }

    if (rc == 0) {
        size_t num_read = 0;
        size_t remaining = 0;
        rc = KMDataNodeRead(node, 0, NULL, 0, &num_read, &remaining);
        DISP_RC2(rc, name, "while calling KMDataNodeRead");
        assert(num_read == 0);
        if (rc == 0 && remaining > 0) {
            buffer = malloc(remaining + 1);
            if (buffer == NULL) {
                rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
            }
            else {
                size_t size = 0;
                rc = KMDataNodeReadCString(node, buffer, remaining + 1, &size);
                DISP_RC2(rc, name, "while calling KMDataNodeReadCString");
                assert(size == remaining);
            }
        }
    }

    if (rc == 0 && buffer) {
        const char* token = strtok(buffer, "\n");
        while (token) {
            const char RG[] = "@RG\t";
            if (strncmp(token, RG, sizeof RG - 1) == 0) {
                const char ID[] = "\tID:";
                const char LB[] = "\tLB:";
                const char SM[] = "\tSM:";
                char* id_end = NULL;
                char* lb_end = NULL;
                char* sm_end = NULL;
                BAM_HEADER_RG rg;
                memset(&rg, 0, sizeof rg);
                rg.ID = strstr(token, ID);
                rg.LB = strstr(token, LB);
                rg.SM = strstr(token, "\tSM:");
                if (rg.ID) {
                    rg.ID += sizeof ID - 1;
                }
                if (rg.ID) {
                    id_end = strchr(rg.ID, '\t');
                }
                if (rg.LB) {
                    rg.LB += sizeof LB - 1;
                }
                if (rg.LB) {
                    lb_end = strchr(rg.LB, '\t');
                }
                if (rg.SM) {
                    rg.SM += sizeof SM - 1;
                }
                if (rg.SM) {
                    sm_end = strchr(rg.SM, '\t');
                }
                if (id_end) {
                    *id_end = '\0';
                }
                if (lb_end) {
                    *lb_end = '\0';
                }
                if (sm_end) {
                    *sm_end = '\0';
                }
                if (rg.ID && rg.LB && rg.SM) {
                    rc = cb(&rg, data);
                }
                if (rc) {
                    break;
                }
            }
            token = strtok(NULL, "\n");
        }
    }
    free(buffer);
    buffer = NULL;
    RELEASE(KMDataNode, node);
    RELEASE(KMetadata, meta);
    return rc;
}

static
void SraStatsMetaDestroy(SraStatsMeta* self)
{
    if (self == NULL)
    {   return; }
    BAM_HEADER_RG_free(&self->BAM_HEADER);
    free(self->spot_group);
    self->spot_group = NULL;
}

static
rc_t CC fileSizeVisitor(const KDirectory* dir,
    uint32_t type, const char* name, void* data)
{
    rc_t rc = 0;

    SraSizeStats* sizes = (SraSizeStats*) data;

    if (type & kptAlias)
    {   return rc; }

    assert(sizes);

    switch (type) {
        case kptFile: {
            uint64_t size = 0;
            rc = KDirectoryFileSize(dir, &size, "%s", name);
            DISP_RC2(rc, name, "while calling KDirectoryFileSize");
            if (rc == 0) {
                sizes->size += size;
                DBGMSG(DBG_APP, DBG_COND_1,
                    ("File '%s', size %lu\n", name, size));
            }
            break;
        }
        case kptDir:
            DBGMSG(DBG_APP, DBG_COND_1, ("Dir '%s'\n", name));
            rc = KDirectoryVisit(dir, false, fileSizeVisitor, sizes, "%s", name);
            DISP_RC2(rc, name, "while calling KDirectoryVisit");
            break;
        default:
            rc = RC(rcExe, rcDirectory, rcVisiting, rcType, rcUnexpected);
            DISP_RC2(rc, name, "during KDirectoryVisit");
            break;
    }

    return rc;
}

static rc_t GetTableModDate(const srastat_parms * pb, const KDirectory * dir,
    const VDBManager *mgr, KTime_t *mtime, char * aPath, size_t sz)
{
    VFSManager *vfs = NULL;
    VResolver  *resolver = NULL;
    VPath *accession = NULL;
    const VPath *tblpath = NULL;
    const KDBManager *kmgr = NULL;
    const char *spec = NULL;
    const char *path = aPath;

    rc_t rc = VFSManagerMake(&vfs);
    DISP_RC(rc, "VFSManagerMake");

    assert(pb && mtime);

    *mtime = 0;
    spec = pb->table_path;

    if (rc == 0) {
        rc = VFSManagerGetResolver(vfs, &resolver);
        DISP_RC(rc, "VFSManagerGetResolver");
    }

    if (rc == 0) {
        rc = VFSManagerMakePath(vfs, &accession, spec);
        DISP_RC(rc, "VFSManagerMakePath");
    }

    if (rc == 0) {
        assert(accession);

        if (VPathIsAccessionOrOID(accession)) {
            rc = VResolverLocal(resolver, accession, &tblpath);
            if (rc != 0) {
                if (pb->print_arcinfo)
                    PLOGERR(klogErr, (klogErr, rc,
                        "'$(acc)' cannot be found locally", "acc=%s", spec));
            }
            else {
                rc = VPathReadPath(tblpath, aPath, sz, NULL);
                path = aPath;
            }
        }
        else {
            rc = KDirectoryResolvePath_v1(dir, true, aPath, sz, spec);
            if (rc == 0)
                path = aPath;
            else {
                path = spec;
                rc = 0;
            }
        }
    }

    if (rc == 0) {
        rc = VDBManagerGetKDBManagerRead(mgr, &kmgr);
        DISP_RC(rc, "VDBManagerGetKDBManagerRead");
    }

    if (rc == 0) {
        rc = KDBManagerGetTableModDate(kmgr, mtime, "%s", path);
    }

    RELEASE(KDBManager, kmgr);
    RELEASE(VPath, tblpath);
    RELEASE(VPath, accession);
    RELEASE(VResolver, resolver);
    RELEASE(VFSManager, vfs);

    return rc;
}

static rc_t sizeAndMd5(const KFile * kfile, TArcInfo * arc_info) {
    rc_t rc = 0;

    uint64_t pos = 0;
    uint8_t buffer[256 * 1024];
    size_t num_read = 0;

    MD5State md5;
    MD5StateInit(&md5);

    assert(arc_info);

    do {
        rc = KFileRead(kfile, pos, buffer, sizeof buffer, &num_read);
        if (rc == 0) {
            MD5StateAppend(&md5, buffer, num_read);
            pos += num_read;
        }
        rc = Quitting();
        if (rc != 0)
            LOGMSG(klogWarn, "Interrupted");
    } while (rc == 0 && num_read != 0);

    if (rc == 0)
        rc = KFileSize(kfile, &arc_info->size);

    if (rc == 0) {
        size_t x = 0;
        uint8_t digest[16];
        MD5StateFinish(&md5, digest);
        for (pos = 0, x = 0; rc == 0 && pos < sizeof(digest); pos++) {
            rc = string_printf(&arc_info->md5[x],
                sizeof arc_info->md5 - x, &num_read, "%02x", digest[pos]);
            x += num_read;
        }
    }

    return rc;
}

static rc_t get_arc_info_crnt(const KDirectory * dir,
    const char * path, TArcInfo * arc_info)
{
    rc_t rc = 0;

    assert(arc_info);

    if ((KDirectoryPathType(dir, "%s", path) & ~kptAlias) == kptFile) {
        const KFile * f = NULL;
        rc = KDirectoryOpenFileRead(dir, &f, "%s", path);

        if (rc == 0)
            rc = sizeAndMd5(f, arc_info);

        RELEASE(KFile, f);
    }

    return rc;
}

static rc_t get_arc_info(const srastat_parms * pb, ArcInfo *arc_info,
    const VDBManager *vmgr, const VTable *vtbl)
{
    rc_t rc = 0;

    static char aPath[4096] = "";
    const char *path = aPath;

    KDirectory * dir = NULL;

    assert(pb && arc_info);

    memset(arc_info, 0, sizeof(*arc_info));

    if (!pb->print_arcinfo && !pb->print_locinfo)
        return rc;

    rc = KDirectoryNativeDir(&dir);

    if (rc == 0)
        rc = GetTableModDate(pb, dir,
            vmgr, &arc_info->timestamp, aPath, sizeof aPath);

    if (rc == 0) {
        if (aPath[0] == '\0')
            path = arc_info->i[eArcInfoCrnt].tag = pb->table_path;
        else
            path = arc_info->i[eArcInfoCrnt].tag = aPath;
        rc = get_arc_info_crnt(dir, path, arc_info->i + eArcInfoCrnt);
    }
    else if (!pb->print_arcinfo)
        rc = 0;

    RELEASE(KDirectory, dir);

    if (!pb->print_arcinfo)
        return rc;

    if (rc == 0 ) {
        uint32_t i;
        for (i = 0; i < 2; ++i) {
            const KFile* kfile;
            arc_info->i[i].tag = i == 0 ? "sra" : "lite.sra";
            if ((rc = VTableMakeSingleFileArchive(vtbl, &kfile, i == 1)) == 0) {
                rc = sizeAndMd5(kfile, arc_info->i + i);
                KFileRelease(kfile);
            }
        }
    }
    return rc;
}

static rc_t get_size( SraSizeStats* sizes, const VTable *vtbl) {
    rc_t rc = 0;

    const KDatabase* kDb = NULL;
    const KTable* kTbl = NULL;
    const KDirectory* dir = NULL;

    assert(vtbl && sizes);
    rc = VTableOpenKTableRead(vtbl, &kTbl);
    DISP_RC(rc, "while calling VTableOpenKTableRead");

    if (rc == 0) {
        rc = KTableOpenParentRead(kTbl, &kDb);
        DISP_RC(rc, "while calling KTableOpenParentRead");
    }

    if (rc == 0 && kDb) {
        while (rc == 0) {
            const KDatabase* par = NULL;
            rc = KDatabaseOpenParentRead(kDb, &par);
            DISP_RC(rc, "while calling KDatabaseOpenParentRead");
            if (par == NULL)
            {   break; }
            else {
                RELEASE(KDatabase, kDb);
                kDb = par;
            }
        }
        if (rc == 0) {
            rc = KDatabaseOpenDirectoryRead(kDb, &dir);
            DISP_RC(rc, "while calling KDatabaseOpenDirectoryRead");
        }
    }

    if (rc == 0 && dir == NULL) {
        rc = KTableOpenDirectoryRead(kTbl, &dir);
        DISP_RC(rc, "while calling KTableOpenDirectoryRead");
    }

    memset(sizes, 0, sizeof *sizes);

    if (rc == 0) {
        rc = KDirectoryVisit(dir, false, fileSizeVisitor, sizes, NULL);
        DISP_RC(rc, "while calling KDirectoryVisit");
    }

    RELEASE(KDirectory, dir);
    RELEASE(KTable, kTbl);
    RELEASE(KDatabase, kDb);

    return rc;
}

static
rc_t readTxtAttr(const KMDataNode* self, const char* name, const char* attrName,
    char* buf, size_t buf_size)
{
    rc_t rc = 0;
    size_t num_read = 0;

    if (self == NULL) {
        rc = RC(rcExe, rcMetadata, rcReading, rcSelf, rcNull);
        return rc;
    }

    assert(self && name && attrName && buf && buf_size);

    rc = KMDataNodeReadAttr(self, attrName, buf, buf_size, &num_read);
    if (rc != 0) {
        PLOGERR(klogErr, (klogErr, rc, "Cannot read '$(name)@$(attr)'",
            "name=%s,attr=%s", name, attrName));
    }
    else { DBGMSG(DBG_APP, DBG_COND_1, ("%s@%s = %s\n", name, attrName, buf)); }

    return rc;
}

static
rc_t get_load_info(const KMetadata* dbMeta,
    const KMetadata* tblMeta, SraMeta* info)
{
    rc_t rc = 0;
    const KMDataNode* node = NULL;
    const KMetadata* meta = dbMeta != NULL ? dbMeta : tblMeta;
    assert(info);
    memset(info, 0, sizeof *info);
    info->metaVersion = 99;
    info->tblVersion = -1;
    info->isDb = dbMeta != NULL;

    if (rc == 0) {
        rc = KMetadataVersion(meta, &info->metaVersion);
        DISP_RC(rc, "While calling KMetadataVersion");
        if (rc == 0) {
            DBGMSG(DBG_APP, DBG_COND_1,
                ("KMetadataVersion=%d\n",info->metaVersion));
            switch (info->metaVersion) {
                case 1:
                    info->tblVersion = 0;
                    break;
                case 2: {
                    const char name[] = "SOFTWARE/loader";
                    rc = KMetadataOpenNodeRead(meta, &node, "%s", name);
                    if (rc != 0) {
                        if (GetRCState(rc) == rcNotFound) {
                            DBGMSG(DBG_APP,DBG_COND_1,("%s: not found\n",name));
                            rc = 0;
                        }
                        DISP_RC2(rc, name,
                            "while calling KMetadataOpenNodeRead");
                    }
                    else {
                        rc = readTxtAttr(node, name, "vers",
                            info->loader.vers, sizeof info->loader.vers);
                        if (rc == 0) {
                            if (strncmp("2.", info->loader.vers, 2) == 0)
                            {      info->tblVersion = 2; }
                            else { info->tblVersion = 1; }
                        }
                        else if (GetRCState(rc) == rcNotFound) {
                            DBGMSG(DBG_APP,DBG_COND_1,
                                ("%s/@%s: not found\n", name, "vers"));
                            rc = 0;
                        }
                        if (rc == 0) {
                            rc = readTxtAttr(node, name, "date",
                                info->loader.date, sizeof info->loader.date);
                            if (GetRCState(rc) == rcNotFound) {
                                DBGMSG(DBG_APP,DBG_COND_1,
                                    ("%s/@%s: not found\n", name, "date"));
                                rc = 0;
                            }
                        }
                        if (rc == 0) {
                            rc = readTxtAttr(node, name, "name",
                                info->loader.name, sizeof info->loader.name);
                            if (GetRCState(rc) == rcNotFound) {
                                DBGMSG(DBG_APP,DBG_COND_1,
                                    ("%s/@%s: not found\n", name, "name"));
                                rc = 0;
                            }
                        }
                    }
                    break;
                }
                default:
                    rc = RC(rcExe, rcMetadata, rcReading, rcData, rcUnexpected);
                    PLOGERR(klogErr, (klogErr, rc,
                        "Unexpected MetadataVersion: $(version)",
                        "version=%d", info->metaVersion));
                    break;
            }
        }
    }

    if (rc == 0) {
        const char name[] = "SOFTWARE/formatter";
        RELEASE(KMDataNode, node);
        rc = KMetadataOpenNodeRead(meta, &node, "%s", name);
        if (rc != 0) {
            if (GetRCState(rc) == rcNotFound ) {
                DBGMSG(DBG_APP,DBG_COND_1,("%s: not found\n",name));
                rc = 0;
            }
            DISP_RC2(rc, name, "while calling KMetadataOpenNodeRead");
        }
        else {
            if (rc == 0) {
                rc = readTxtAttr(node, name, "vers",
                    info->formatter.vers, sizeof info->formatter.vers);
                if (GetRCState(rc) == rcNotFound) {
                    DBGMSG(DBG_APP,DBG_COND_1,
                        ("%s/@%s: not found\n", name, "vers"));
                    rc = 0;
                }
            }
            if (rc == 0) {
                rc = readTxtAttr(node, name, "name",
                    info->formatter.name, sizeof info->formatter.name);
                if (GetRCState(rc) == rcNotFound) {
                    DBGMSG(DBG_APP,DBG_COND_1,
                        ("%s/@%s: not found\n", name, "name"));
                    rc = 0;
                }
            }
        }
    }

    if (rc == 0) {
        const char name[] = "LOAD/timestamp";
        RELEASE(KMDataNode, node);
        rc = KMetadataOpenNodeRead(meta, &node, "%s", name);
        if (rc != 0) {
            if (GetRCState(rc) == rcNotFound ) {
                DBGMSG(DBG_APP,DBG_COND_1,("%s: not found\n",name));
                rc = 0;
            }
            DISP_RC2(rc, name, "while calling KMetadataOpenNodeRead");
        }
        else {
            size_t num_read = 0;
            size_t remaining = 0;
            rc = KMDataNodeRead(node, 0,
                &info->loadTimestamp, sizeof info->loadTimestamp,
                &num_read, &remaining);
            if (rc == 0) {
                if (remaining || num_read != sizeof info->loadTimestamp) {
                    rc = RC(rcExe, rcMetadata, rcReading, rcData, rcIncorrect);
                }
            }
            else {
                if (GetRCState(rc) == rcNotFound) {
                    DBGMSG(DBG_APP, DBG_COND_1, ("%s: not found\n", name));
                    rc = 0;
                }
            }
            DISP_RC2(rc, name, "while calling KMetadataOpenNodeRead");
        }
    }

    RELEASE(KMDataNode, node);

    return rc;
}

static
rc_t readStatsMetaNode(const KMDataNode* parent, const char* parentName,
    const char* name, uint64_t* result, bool quick, bool optional)
{
    rc_t rc = 0;

    const KMDataNode* node = NULL;

    assert(parent && parentName && name && result);

    rc = KMDataNodeOpenNodeRead(parent, &node, "%s", name);
    if (rc != 0)
    {
        if (GetRCState(rc) == rcNotFound && optional)
        {
            *result = 0;
            rc = 0;
        }
        else if ( quick )
        {
            PLOGERR(klogInt, (klogInt, rc, "while opening $(parent)/$(child)",
                              "parent=%s,child=%s", parentName, name));
        }
    }
    else {
        rc = KMDataNodeReadAsU64(node, result);
        if (GetRCObject(rc) == rcTransfer && GetRCState(rc) == rcIncomplete) {
            *result = 0;
            rc = 0;
        }
        if ( rc != 0 && quick )
        {
            PLOGERR(klogInt, (klogInt, rc, "while reading $(parent)/$(child)",
                              "parent=%s,child=%s", parentName, name));
        }
    }

    RELEASE(KMDataNode, node);

    return rc;
}

static
rc_t readStatsMetaAttr(const KMDataNode* parent, const char* parentName,
    const char* name, char ** result, bool quick, bool optional)
{
    rc_t rc = 0;
    char temp[4096];
    size_t actsize;

    assert(parent && parentName && name && result);

    rc = KMDataNodeReadAttr(parent, name, temp, sizeof(temp), &actsize);
    if (GetRCState(rc) == rcInsufficient)
    {
        *result = malloc(actsize + 1);
        if (*result)
            rc = KMDataNodeReadAttr(parent, name, *result, actsize, &actsize);
    }
    else if (rc == 0)
    {
        *result = string_dup_measure(temp, NULL);
    }
    if (rc == 0 && *result == NULL)
    {
        rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    }
    else if (rc != 0)
    {
        if (GetRCState(rc) == rcNotFound && optional)
        {
            *result = 0;
            rc = 0;
        }
        else
        {
            PLOGERR(klogInt, (klogInt, rc, "while opening $(parent)[$(child)]",
                "parent=%s,child=%s", parentName, name));
        }
    }

    return rc;
}

static
rc_t readStatsMetaNodes(const KMDataNode* parent, const char* name,
                        SraStatsMeta* stats, bool quick)
{
    rc_t rc = 0;
    const char *const parentName = name ? name : "STATS/TABLE";

    assert(parent && stats);

    if (name) {
        char *tempname;

        rc = readStatsMetaAttr(parent, parentName, "name", &tempname, quick, true);
        if (rc == 0) {
            if (tempname)
                stats->spot_group = tempname;
            else {
                stats->spot_group = string_dup_measure(name, NULL);
                if (stats->spot_group == NULL)
                {   rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted); }
            }
        }
    }

    if (rc == 0) {
        rc = readStatsMetaNode(parent,
            parentName, "BASE_COUNT", &stats->BASE_COUNT, quick, false);
    }
    if (rc == 0) {
        rc = readStatsMetaNode(parent,
            parentName, "BIO_BASE_COUNT", &stats->BIO_BASE_COUNT, quick, false);
    }
    if (rc == 0) {
        rc = readStatsMetaNode(parent,
            parentName, "SPOT_COUNT", &stats->spot_count, quick, false);
    }
    if (rc == 0) {
        rc = readStatsMetaNode(parent,
            parentName, "CMP_BASE_COUNT", &stats->CMP_BASE_COUNT, quick, true);
    }

    if (rc == 0)
    {   stats->found = true; }
    else { SraStatsMetaDestroy(stats); }

    return rc;
}

static
rc_t get_stats_meta(const KMetadata* meta,
    MetaDataStats* stats, bool quick)
{
    rc_t rc = 0;
    assert(meta && stats);
    memset(stats, 0, sizeof *stats);

    if (rc == 0) {
        const char name[] = "STATS/TABLE";
        const KMDataNode* node = NULL;
        rc = KMetadataOpenNodeRead(meta, &node, "%s", name);
        if (rc != 0) {
            if (GetRCState(rc) == rcNotFound) {
                DBGMSG(DBG_APP,DBG_COND_1, ("%s: not found\n", name));
                rc = 0;
            }
            DISP_RC2(rc, name, "while calling KMetadataOpenNodeRead");
        }
        if (rc == 0 && node)
        {   rc = readStatsMetaNodes(node, NULL, &stats->table, quick); }
        RELEASE(KMDataNode, node);
    }

    if (rc == 0) {
        const char name[] = "STATS/SPOT_GROUP";
        const KMDataNode* parent = NULL;
        rc = KMetadataOpenNodeRead(meta, &parent, "%s", name);
        if (rc != 0) {
            if (GetRCState(rc) == rcNotFound) {
                DBGMSG(DBG_APP,DBG_COND_1, ("%s: not found\n", name));
                rc = 0;
            }
            DISP_RC2(rc, name, "while calling KMetadataOpenNodeRead");
        }

        if (rc == 0 && parent) {
            uint32_t count = 0, i = 0;
            KNamelist* names = NULL;
            rc = KMDataNodeListChild(parent, &names);
            DISP_RC2(rc, name, "while calling KMDataNodeListChild");
            if (rc == 0) {
                rc = KNamelistCount(names, &count);
                DISP_RC2(rc, name, "while calling KNamelistCount");
                if (rc == 0) {
                    stats->spotGroupN = count;
                    stats->spotGroup = calloc(count, sizeof *stats->spotGroup);
                    if (stats->spotGroup == NULL) {
                        rc = RC(rcExe,
                            rcStorage, rcAllocating, rcMemory, rcExhausted);
                    }
                }
            }

            for (i = 0; i < count && rc == 0; ++i) {
                const KMDataNode* node = NULL;
                const char* child = NULL;
                rc = KNamelistGet(names, i, &child);
                if (rc != 0) {
                    PLOGERR(klogInt, (klogInt, rc,
                        "while calling STATS/SPOT_GROUP::KNamelistGet($(idx))",
                        "idx=%i", i));
                }
                else {
                    rc = KMDataNodeOpenNodeRead(parent, &node, "%s", child);
                    DISP_RC2(rc, child, "while calling KMDataNodeOpenNodeRead");
                }
                if (rc == 0) {
                    rc = readStatsMetaNodes
                        (node, child, &stats->spotGroup[i], quick);
                }
                RELEASE(KMDataNode, node);
            }
            RELEASE(KNamelist, names);
        }
        RELEASE(KMDataNode, parent);
    }

    if (rc == 0) {
        uint32_t i = 0;
        bool found = stats->table.found;
        for (i = 0; i < stats->spotGroupN && found; ++i)
        {   found = stats->spotGroup[i].found; }
        stats->found = found;
    }

    return rc;
}

static bool needEscaping ( const char * c ) {
    return string_chr ( c, string_measure ( c, NULL), '"' ) != NULL
        || string_chr ( c, string_measure ( c, NULL), '\'' ) != NULL
        || string_chr ( c, string_measure ( c, NULL), '&' ) != NULL
        || string_chr ( c, string_measure ( c, NULL), '<' ) != NULL
        || string_chr ( c, string_measure ( c, NULL), '>' ) != NULL;
}

static rc_t printXmlEscaped ( const char * c ) {
    rc_t rc = 0;
    rc_t r2 = 0;
    while ( * c ) {
        switch  ( * c ) {
            case '"':
                r2 = OUTMSG ( ( "&quot;" ) );
                break;
            case '\'':
                r2 = OUTMSG ( ( "&apos;" ) );
                break;
            case '&':
                r2 = OUTMSG ( ( "&amp;" ) );
                break;
            case '<':
                r2 = OUTMSG ( ( "&lt;" ) );
                break;
            case '>':
                r2 = OUTMSG ( ( "&gt;" ) );
                break;
            default:
                r2 = OUTMSG ( ( "%c", *c ) );
                break;
        }
        if ( rc == 0 )
            rc = r2;
        ++ c;;
    }
    return rc;
}

static
void srastatmeta_print(const MetaDataStats* meta_stats, srastat_parms *pb)
{
    uint32_t i = 0;
    assert(pb && meta_stats);

    if (meta_stats->spotGroupN) {
        for (i = 0; i < meta_stats->spotGroupN; ++i) {
            const SraStatsMeta* ss = &meta_stats->spotGroup[i];
            if (!pb->skip_members) {
                const char* spot_group = "";
                if (strcmp("default", ss->spot_group) != 0)
                {   spot_group = ss->spot_group; }
                if ((spot_group == NULL || spot_group[0] == '\0')
                    && meta_stats->spotGroupN > 1)
                {
                    /* skip an empty "extra" 'default' spot_group */
                    if (ss->BASE_COUNT == 0     && ss->BIO_BASE_COUNT == 0 &&
                        ss->CMP_BASE_COUNT == 0 && ss->spot_count == 0)
                    {   continue; }
                }
                if (pb->xml) {
                    if (pb->hasSPOT_GROUP) {
                       if (needEscaping(ss->spot_group)) {
                           OUTMSG(("  <Member member_name=\""));
                           printXmlEscaped(                 ss->spot_group);
                           OUTMSG((                                      "\""));
                       }
                       else
                            OUTMSG(("  <Member member_name=\"%s\"",
                                    ss->spot_group));
                       OUTMSG((" spot_count=\"%ld\" base_count=\"%ld\"",
                            ss->spot_count, ss->BASE_COUNT));
                       OUTMSG((" base_count_bio=\"%ld\"", ss->BIO_BASE_COUNT));
                       if (ss->CMP_BASE_COUNT > 0) {
                           OUTMSG((" cmp_base_count=\"%ld\"",
                               ss->CMP_BASE_COUNT));
                       }
                       BAM_HEADER_RG_print_xml(&ss->BAM_HEADER);
                       OUTMSG(("/>\n"));
                    }
                }
                else {
                    OUTMSG(("%s|%s|%ld:%ld:%ld|:|:|:\n",
                        pb->table_path, spot_group, ss->spot_count,
                        ss->BASE_COUNT, ss->BIO_BASE_COUNT));
                }
            }

            pb->total.spot_count += ss->spot_count;      /* eTOTAL_SPOT_COUNT */
            pb->total.BASE_COUNT += ss->BASE_COUNT;

            /* eTOTAL_BIO_BASE_COUNT */
            pb->total.BIO_BASE_COUNT += ss->BIO_BASE_COUNT;

            /* eTOTAL_CMP_BASE_COUNT */
            pb->total.total_cmp_len += ss->CMP_BASE_COUNT;
        }
    }
    else {
        if (!pb->xml && !(pb->quick && pb->skip_members)) {
                const char* spot_group = "";
                OUTMSG(("%s|%s|%ld:%ld:%ld|:|:|:\n",
                        pb->table_path, spot_group, meta_stats->table.spot_count,
                        meta_stats->table.BASE_COUNT, meta_stats->table.BIO_BASE_COUNT));
        }
        pb->total.spot_count = meta_stats->table.spot_count;
        pb->total.BASE_COUNT = meta_stats->table.BASE_COUNT;
        pb->total.BIO_BASE_COUNT = meta_stats->table.BIO_BASE_COUNT;
        pb->total.total_cmp_len = meta_stats->table.CMP_BASE_COUNT;
    }
}

static
void CC srastat_print( BSTNode* n, void* data )
{
    srastat_parms *pb = (srastat_parms*) data;
    const SraStats *ss = (const SraStats*) n;
    assert(pb && ss);
    if (!pb->skip_members) {
        if (pb->xml) {
            if (pb->hasSPOT_GROUP) {
                OUTMSG(("  <Member member_name=\"%s\"", ss->spot_group));
                OUTMSG((" spot_count=\"%ld\" base_count=\"%ld\"", ss->spot_count, ss->total_len));
                OUTMSG((" base_count_bio=\"%ld\"", ss->bio_len));
                OUTMSG((" spot_count_mates=\"%ld\" base_count_bio_mates=\"%ld\"", ss->spot_count_mates, ss->bio_len_mates));
                OUTMSG((" spot_count_bad=\"%ld\" base_count_bio_bad=\"%ld\"", ss->bad_spot_count, ss->bad_bio_len));
                OUTMSG((" spot_count_filtered=\"%ld\" base_count_bio_filtered=\"%ld\"",
                    ss->filtered_spot_count, ss->filtered_bio_len));
                if (ss->total_cmp_len > 0)
                {   OUTMSG((" cmp_base_count=\"%ld\"", ss->total_cmp_len)); }
                BAM_HEADER_RG_print_xml(&ss->BAM_HEADER);
                OUTMSG(("/>\n"));
            }
        }
        else {
            OUTMSG(("%s|%s|%ld:%ld:%ld|%ld:%ld|%ld:%ld|%ld:%ld\n",
                pb->table_path,ss->spot_group,ss->spot_count,ss->total_len,ss->bio_len,ss->spot_count_mates,ss->bio_len_mates,
                ss->bad_spot_count,ss->bad_bio_len,ss->filtered_spot_count,ss->filtered_bio_len));
        }
    }
    pb->total.spot_count += ss->spot_count;
    pb->total.spot_count_mates += ss->spot_count_mates;
    pb->total.BIO_BASE_COUNT += ss->bio_len;
    pb->total.bio_len_mates += ss->bio_len_mates;

    /* eTOTAL_BASE_COUNT SRR1215779 */
    pb->total.BASE_COUNT += ss->total_len;

    pb->total.bad_spot_count += ss->bad_spot_count;
    pb->total.bad_bio_len += ss->bad_bio_len;
    pb->total.filtered_spot_count += ss->filtered_spot_count;
    pb->total.filtered_bio_len += ss->filtered_bio_len;
    pb->total.total_cmp_len += ss->total_cmp_len;
}

static
rc_t process_align_info(const char* indent, const Ctx* ctx)
{
    rc_t rc = 0;
    uint32_t count = 0;
    uint32_t i = 0;
    const VDBDependencies* dep = NULL;

    assert(indent && ctx);

    if (ctx->db == NULL) {
        return rc;
    }

    rc = VDatabaseListDependencies(ctx->db, &dep, false);
    DISP_RC2(rc, ctx->pb->table_path,
        "while calling VDatabaseListDependencies");

    rc = VDBDependenciesCount(dep, &count);
    if (rc) {
        DISP_RC(rc, "while calling VDBDependenciesCount");
        return rc;
    }


    OUTMSG(("%s<AlignInfo>\n", indent));

    for (i = 0; i < count && rc == 0; ++i) {
        bool circular = false;
        const char* name = NULL;
        const char* path = NULL;
        bool local = false;
        const char* seqId = NULL;

        rc = VDBDependenciesCircular(dep, &circular, i);
        if (rc != 0) {
            DISP_RC(rc, "while calling VDBDependenciesCircular");
            break;
        }
        rc = VDBDependenciesName(dep, &name, i);
        if (rc != 0) {
            DISP_RC(rc, "while calling VDBDependenciesName");
            break;
        }

        rc = VDBDependenciesPath(dep, &path, i);
        if (rc != 0) {
            DISP_RC(rc, "while calling VDBDependenciesPath");
            break;
        }
        if ( path == NULL ) {
            rc = VDBDependenciesPathRemote(dep, &path, i);
            if (rc != 0) {
                DISP_RC(rc, "while calling VDBDependenciesPath");
                break;
            }
        }

        rc = VDBDependenciesLocal(dep, &local, i);
        if (rc != 0) {
            DISP_RC(rc, "while calling VDBDependenciesLocal");
            break;
        }
        rc = VDBDependenciesSeqId(dep, &seqId, i);
        if (rc != 0) {
            DISP_RC(rc, "while calling VDBDependenciesSeqId");
            break;
        }

        OUTMSG((
            "%s  <Ref seqId=\"%s\" name=\"%s\" circular=\"%s\" local=\"%s\"",
            indent, seqId, name, (circular ? "true" : "false"),
            (local ? "local" : "remote")));
        if (path && path[0]) {
            OUTMSG((" path=\"%s\"", path));
        }
        OUTMSG(("/>\n"));
    }

    OUTMSG(("%s</AlignInfo>\n", indent));

    RELEASE(VDBDependencies, dep);

    return rc;
}

/******************************************************************************/

static
void printChangeValue ( const char * value,
                        size_t size )
{
    assert ( value );

    switch ( size ) {
        case 1:
            OUTMSG ( ( "%hu", ( ( const uint8_t * ) value ) [ 0 ] ) );
            break;
        case 2:
            OUTMSG ( ( "%u", ( ( const uint16_t * ) value ) [ 0 ] ) );
            break;
        case 4:
            OUTMSG ( ( "%u", ( ( const uint32_t * ) value ) [ 0 ] ) );
            break;
        case 8:
            OUTMSG ( ( "%lu", ( ( const uint64_t * ) value ) [ 0 ] ) );
            break;
        default: {
            size_t i = 0;
            for ( i = 0; i < size; ++ i ) {
                char c = value [ i ];
                if ( isprint ( c ) )
                    OUTMSG ( ( "%c", c ) );
                else
                    OUTMSG ( ( "\\x%02X", ( ( const uint8_t* ) value ) [ i ] ));
            }
            break;
        }
    }
}

static
rc_t KMDataNodePrintAttr ( const KMDataNode * self,
                           const KNamelist * attrs,
                           uint32_t idx )
{
    rc_t rc = 0;

    const char * attr = NULL;
    char value [ 512 ] = "";
    size_t size = 0;

    rc = KNamelistGet ( attrs, idx, & attr );

    if ( rc == 0 )
        rc = KMDataNodeReadAttr ( self, attr, value, sizeof value, & size );

    if ( rc == 0 )
        rc = OUTMSG ( ( " %s=\"%s\"", attr, value ) );

    if ( rc != 0 )
        OUTMSG ( ( " CANNOT_READ_ATTRIBULE=\"%R\"", rc ) );

    return rc;
}

static
rc_t KMDataNodePrintChange ( const KMDataNode * self,
                            const char * name,
                            const char * indent )
{
    rc_t rc = 0;

    const KMDataNode * node = NULL;

    rc = KMDataNodeOpenNodeRead ( self, & node, name );
    DISP_RC2 ( rc, name,
        "while calling KMDataNodeOpenNodeRead" );

    if ( rc == 0 ) {
        const char tag [] = "Change";
        char value [] = "sra-stat: ERROR WHILE READING CHANGES CHILD VALUE";
        size_t size = 0;

        KNamelist * names = NULL;
        uint32_t count = 0;

        KMDataNodeRead ( node, 0, value, sizeof value, & size, NULL );
        if ( size == 0 )
            size = sizeof value;

        rc = KMDataNodeListAttr ( node, & names );
        DISP_RC2 ( rc, name,
            "while calling KMDataNodeListAttr" );

        if ( rc == 0 )
            rc = KNamelistCount ( names, & count );

        if ( rc == 0 ) {
            uint32_t i = 0;

            OUTMSG ( ( "  %s" "<%s", indent, tag ) );
            for ( i = 0; i < count; ++i )
                KMDataNodePrintAttr ( node, names, i );
            OUTMSG ( ( ">" ) );

            printChangeValue ( value, size );

            OUTMSG ( ( "</%s>\n", tag ) );
        }

        RELEASE ( KNamelist, names );
    }

    RELEASE ( KMDataNode, node );

    return rc;
}

static rc_t CtxPrintCHANGES ( const Ctx * self, const char * indent ) {
    rc_t rc = 0;

    const KMetadata * meta = NULL;
    const KMDataNode * parent = NULL;
    bool toReleaseMeta = false;

    const char meta_tag [] = "CHANGES";
    const char tag [] = "Changes";

    assert ( self && indent );

    meta = self -> meta;
    if ( meta == NULL ) {
        rc = VDatabaseOpenMetadataRead ( self -> db, & meta );
        if ( rc != 0 )
            return rc;
        toReleaseMeta = true;
    }

    KMetadataOpenNodeRead ( meta, & parent, meta_tag );

    if ( parent != NULL ) {
        KNamelist * names = NULL;
        rc = KMDataNodeListChild ( parent, & names );
        DISP_RC2 ( rc, meta_tag, "while calling KMDataNodeListChild" );

        OUTMSG ( ( "%s" "<%s>"  "\n", indent, tag ) );

        if ( rc == 0 ) {
            uint32_t count = 0;
            rc = KNamelistCount ( names, & count );
            DISP_RC2 ( rc, meta_tag, "while calling KNamelistCount" );

            if ( rc == 0 ) {
                uint32_t i = 0;
                for ( i = 0; i < count || rc != 0; ++i ) {
                    const char * child = NULL;
                    rc = KNamelistGet ( names, i, & child );
                    if ( rc != 0 )
                       PLOGERR ( klogInt, ( klogInt, rc,
                            "while calling $(n)::KNamelistGet($(idx))",
                            "n=%s,idx=%i", meta_tag, i ) );
                    else
                        rc = KMDataNodePrintChange ( parent, child, indent );
                }
            }
        }

        OUTMSG ( ( "%s" "</%s>" "\n", indent, tag ) );

        RELEASE ( KNamelist, names );
    }

    if ( toReleaseMeta )
        RELEASE ( KMetadata, meta );

    RELEASE ( KMDataNode, parent );

    return rc;
}

typedef struct {
    uint32_t spotGroupN;
    const SraStats* sg;
} SSGMatcher;

static bool SSGMatcherMismatch(const SSGMatcher* self, const SraStatsTotal* t) {
    const SraStats* sg = NULL;
    assert(self && t);
    if (self->spotGroupN != 1)
        return true;
    sg = self->sg;
    assert(sg);
    return sg->spot_group[0] != '\0' || sg->spot_count != t->spot_count
        || sg->spot_count_mates != t->spot_count_mates
        || sg->bio_len != t->BIO_BASE_COUNT
        || sg->bio_len_mates != t->bio_len_mates
        || sg->total_len != t->BASE_COUNT
        || sg->bad_spot_count != t->bad_spot_count
        || sg->bad_bio_len != t->bad_bio_len
        || sg->filtered_spot_count != t->filtered_spot_count
        || sg->filtered_bio_len != t->filtered_bio_len
        || sg->total_cmp_len != t->total_cmp_len;
}

static void CC SSGMatcherIncrement(BSTNode* n, void* data) {
    SSGMatcher* i = (SSGMatcher*) data;
    assert(i);
    if (++(i->spotGroupN) == 1)
        i->sg = (SraStats*)n;
}

static uint32_t _NumberOfObservedSpotGroups(const BSTree* t, SSGMatcher* sg) {
    BSTreeForEach(t, false, SSGMatcherIncrement, sg);
    assert(sg);
    return sg->spotGroupN;
}

static rc_t make_time(KTime_t timestamp, char * t, size_t sz, size_t * out) {
    time_t ts = timestamp;
    struct tm * tm = localtime(&ts);
    if (tm == NULL)
        return RC(rcExe, rcData, rcReading, rcParam, rcInvalid);
    else {
        assert(out);
        *out = strftime(t, sz, "%Y-%m-%dT%H:%M:%S", tm);
        return 0;
    }
}

/// Equivalent to POSIX `strftime("%a %Y-%m-%d %H:%M:%S %Z", gmtime(tv))`.
/// - Parameters:
///   - tv: the time to format.
///   - buf: the result buffer.
///   - sz: `sizeof(buf)`.
///   - out: actual bytes used.
/// - Returns: 0 or non-zero RC if error.
///
/// Timezone is "UTC" not "GMT" because POSIX says `gmtime` converts to UTC time.
static rc_t formatLoadTime(time_t const tv, char *const buf, size_t const sz, size_t *const out) {
    struct tm *const tm = gmtime(&tv);
    if (tm) {
        static char const *const weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

        assert(0 <= tm->tm_wday && tm->tm_wday <= 6);
        return string_printf(buf, sz, out
                             , "%s %02u-%02u-%02u %02u:%02u:%02u UTC"
                             , (0 <= tm->tm_wday && tm->tm_wday <= 6) ? weekdays[tm->tm_wday] : "???"
                             , tm->tm_year + 1900
                             , tm->tm_mon + 1
                             , tm->tm_mday
                             , tm->tm_hour
                             , tm->tm_min
                             , tm->tm_sec);
    }
    else
        return RC(rcExe, rcData, rcReading, rcParam, rcInvalid);
}

static
rc_t print_results(const Ctx* ctx)
{
    rc_t rc = 0;
    rc_t rc2 = 0;

    uint64_t mismatch = 0;
    enum {
        eBASE_COUNT           =      1,
        eBIO_BASE_COUNT       =      2,
        eCMP_BASE_COUNT       =      4,
        eSPOT_GROUP_N_MISMATCH=      8,
        eNO_SPOT_GROUP        =   0x10,
        eSG_BASE_COUNT        =   0x20,
        eSG_BIO_BASE_COUNT    =   0x40,
        eSG_CMP_BASE_COUNT    =   0x80,
        eDfltSG_CMP_BASE_COUNT=  0x100,
        eSG_SPOT_COUNT        =  0x200,
        eSPOT_COUNT           =  0x400,
        eTOTAL_BASE_COUNT     =  0x800,
        eTOTAL_BIO_BASE_COUNT = 0x1000,
        eTOTAL_CMP_BASE_COUNT = 0x2000,
        eTOTAL_SPOT_COUNT     = 0x4000,
    };

    /*
    mismatchCMP_BASE_COUNT is ignored.
    It is detected in runs
    that are DBs with a single SEQUENCE but no no references, e.g. SRR6336685.
    The currect schema used during load
    is recording CMP_BASE_COUNT == BIO_BASE_COUNT
    even though the run does not have any reference
    and CMP_BASE_COUNT doesn't make any sense and should be equal to 0.
    */
    bool mismatchCMP_BASE_COUNT = false;

    SraStats* ssDfl = NULL;
    SraStats* ssNxt = NULL;
    const SraStatsMeta* mDfl = NULL;
    const SraStatsMeta* mNxt = NULL;
    const char* spot_group = "";
    uint32_t spotGroupN = 0;

    assert(ctx && ctx->pb
        && ctx->tr && ctx->sizes && ctx->info && ctx->meta_stats && ctx->total);

    if (ctx->pb->quick) {
        if ( ! ctx->meta_stats -> found )
            LOGMSG(klogWarn, "Statistics metadata not found");
        else
        {
            ctx->pb->hasSPOT_GROUP = ctx->meta_stats->spotGroupN > 1 ||
                (ctx->meta_stats->spotGroupN == 1
                 && strcmp("default", ctx->meta_stats->spotGroup[0].spot_group) != 0);
        }
    }

    if (ctx->meta_stats->found && ! ctx->pb->quick) {
        ssDfl = (SraStats*)BSTreeFind(ctx->tr, "", srastats_cmp);
        mDfl = &ctx->meta_stats->table;
        if (ctx->total->BASE_COUNT != mDfl->BASE_COUNT)
            mismatch |= eBASE_COUNT;
        if (ctx->total->BIO_BASE_COUNT != mDfl->BIO_BASE_COUNT)
            mismatch |= eBIO_BASE_COUNT;
        if (ctx->total->spot_count != mDfl->spot_count)
            mismatch |= eSPOT_COUNT;

        if (ctx->total->total_cmp_len != mDfl->CMP_BASE_COUNT) {
            /* DB without references and without CMP reads */
            bool noRefDbWithoutCmpReads =
                ctx->total->total_cmp_len == 0 &&
                mDfl->CMP_BASE_COUNT == ctx->total->BASE_COUNT &&
                ctx->db != NULL;

            /* DB without references and with CMP reads.
               A database with a single SEQUENCE table (without references)
               and all reads compressed.
               In this case CMP_BASE_COUNT == 0 or absent is ignored. */
            bool noRefDbWithCmpReads =
                ctx->total->total_cmp_len == ctx->total->BASE_COUNT &&
                mDfl->CMP_BASE_COUNT == 0 &&
                ctx->singleTblDb;

            if (noRefDbWithoutCmpReads || noRefDbWithCmpReads)
                /* DB without references. */
                /* See comment about noRefDbWithoutCmpReads below. */
                mismatchCMP_BASE_COUNT = true;
            else
                mismatch |= eCMP_BASE_COUNT;
        }

        if (ssDfl != NULL) {
            uint32_t i = 0;
            SSGMatcher sg;
            memset(&sg, 0, sizeof sg);
            spotGroupN = _NumberOfObservedSpotGroups(ctx->tr, &sg);
            if (ctx->meta_stats->spotGroupN != spotGroupN &&
                ctx->meta_stats->spotGroupN == 0 &&
                SSGMatcherMismatch(&sg, ctx->total))
            {
                mismatch |= eSPOT_GROUP_N_MISMATCH;
            }
            for (i = 0; i < ctx->meta_stats->spotGroupN; ++i) {
                bool isDefault = false;
                spot_group = "";
                mNxt = &ctx->meta_stats->spotGroup[i];
                assert(mNxt);
                if (strcmp("default", mNxt->spot_group) != 0)
                    spot_group = mNxt->spot_group;
                else
                    isDefault = true;
                ssNxt = (SraStats*)
                    BSTreeFind(ctx->tr, spot_group, srastats_cmp);
                if (ssNxt == NULL) {
                    mismatch |= eNO_SPOT_GROUP;
                    break;
                }
                if (ssNxt->total_cmp_len != mNxt->CMP_BASE_COUNT) {
                    bool noRefDbWithoutCmpReads =
                        ctx->total->total_cmp_len == 0 &&
                        mNxt->CMP_BASE_COUNT == ssNxt->total_len &&
                        ctx->db != NULL;
                    bool noRefDbWithCmpReads =
                        ctx->total->total_cmp_len == ctx->total->BASE_COUNT &&
                        mNxt->CMP_BASE_COUNT == 0 &&
                        mDfl->CMP_BASE_COUNT == 0 &&
                        ctx->singleTblDb;
                    if (noRefDbWithoutCmpReads || noRefDbWithCmpReads)
                         /* noRefDbWithoutCmpReads:
                     for a database where total basecont == total cmp basecount:
                     CMP_BASE_COUNT does not make sence.
                     It is a dababase without references.
                     So if it is not recorded at all - it's OK.
                     We ignore this. */
                        mismatchCMP_BASE_COUNT = true;
                    else {
                      if (ctx->pb->repair && spotGroupN == 1 && isDefault) {
                        PLOGMSG(klogInfo, (klogInfo,
                          "{MISMATCH} Name:$(N), Expected:$(E), Actual:$(A).",
                          "N=%s,E=%lu,A=%lu",
                          "STATS/SPOT_GROUP/default/CMP_BASE_COUNT",
                          ssNxt->total_cmp_len, mNxt->CMP_BASE_COUNT));
                      if (ctx->pb->report)
                            PLOGMSG(klogInfo, (klogInfo,
                              "Examined STATS/SPOT_GROUP/$(N)/CMP_BASE_COUNT - "
                              "mismatch: Expected:$(E), Actual:$(A).",
                              "N=%s,E=%lu,A=%lu", mNxt->spot_group,
                              ssNxt->total_cmp_len, mNxt->CMP_BASE_COUNT));

                        mismatch |= eDfltSG_CMP_BASE_COUNT;
                      }
                      else
                        mismatch |= eSG_CMP_BASE_COUNT;
                      break;
                    }
                }
                else if (ctx->pb->report)
                   PLOGMSG(klogInfo, (klogInfo,
                    "Examined STATS/SPOT_GROUP/$(N)/CMP_BASE_COUNT - match: "
                    "Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", mNxt->spot_group,
                    ssNxt->total_cmp_len, mNxt->CMP_BASE_COUNT));
                if (ssNxt->total_len != mNxt->BASE_COUNT) {
                    mismatch |= eSG_BASE_COUNT;
                    break;
                }
                if (ssNxt->bio_len != mNxt->BIO_BASE_COUNT) {
                    mismatch |= eSG_BIO_BASE_COUNT;
                    break;
                }
                if (ssNxt->spot_count != mNxt->spot_count) {
                    mismatch |= eSG_SPOT_COUNT;
                    break;
                }
            }
        }
    }

    if (ctx->pb->xml) {
        assert(ctx->arc_info);

        OUTMSG(("<Run accession=\"%s\"", ctx->pb->table_path));
        if (ctx->pb->print_locinfo)
            if (ctx->arc_info->i[eArcInfoCrnt].tag != NULL)
                OUTMSG((" path=\"%s\"", ctx->arc_info->i[eArcInfoCrnt].tag));
        if (!ctx->pb->quick || ! ctx->meta_stats->found) {
            OUTMSG((" read_length=\"%s\"",
                ctx->pb->variableReadLength ? "variable" : "fixed"));
        }

        if (ctx->pb->quick) {
            OUTMSG((" spot_count=\"%ld\" base_count=\"%ld\"",
                ctx->meta_stats->table.spot_count, ctx->meta_stats->table.BASE_COUNT));
            OUTMSG((" base_count_bio=\"%ld\"",
                ctx->meta_stats->table.BIO_BASE_COUNT));
            if (ctx->meta_stats->table.CMP_BASE_COUNT > 0) {
                OUTMSG((" cmp_base_count=\"%ld\"",
                    ctx->meta_stats->table.CMP_BASE_COUNT));
            }
        }
        else {
            OUTMSG((" spot_count=\"%ld\" base_count=\"%ld\"",
                ctx->total->spot_count, ctx->total->BASE_COUNT));
            OUTMSG((" base_count_bio=\"%ld\"", ctx->total->BIO_BASE_COUNT));
            OUTMSG((" spot_count_mates=\"%ld\" base_count_bio_mates=\"%ld\"",
                ctx->total->spot_count_mates, ctx->total->bio_len_mates));
            OUTMSG((" spot_count_bad=\"%ld\" base_count_bio_bad=\"%ld\"",
                ctx->total->bad_spot_count, ctx->total->bad_bio_len));
            OUTMSG((
                " spot_count_filtered=\"%ld\" base_count_bio_filtered=\"%ld\"",
                ctx->total->filtered_spot_count, ctx->total->filtered_bio_len));
            if (ctx->total->total_cmp_len > 0) {
                OUTMSG((" cmp_base_count=\"%ld\"", ctx->total->total_cmp_len));
            }

            if (rc == 0) {
                const ArcInfo* a = ctx->arc_info;
                if (a->timestamp != 0) {
                    char b[1024] = "";
                    size_t k = 0;
                    rc = make_time(a->timestamp, b, sizeof b, &k);
                    if (rc == 0 && ctx->pb->print_locinfo) {
                        OUTMSG((" date=\"%.*s\"", (int)k, b));
                        if (a->i[eArcInfoCrnt].size > 0)
                            OUTMSG((" size=\"%lu\" md5=\"%s\"",
                                a->i[eArcInfoCrnt].size,
                                a->i[eArcInfoCrnt].md5));
                    }
                }
            }
        }

        OUTMSG((">\n"));
    }

    else if (ctx->pb->skip_members) {
        if (ctx->pb->quick) {
            OUTMSG(("%s||%ld:%ld:%ld|:|:|:\n",
                ctx->pb->table_path, ctx->meta_stats->table.spot_count,
                ctx->meta_stats->table.BASE_COUNT, ctx->meta_stats->table.BIO_BASE_COUNT));
        }
        else {
            OUTMSG(("%s||%ld:%ld:%ld|%ld:%ld|%ld:%ld|%ld:%ld\n",
                ctx->pb->table_path,
                ctx->total->spot_count, ctx->total->BASE_COUNT,
                ctx->total->BIO_BASE_COUNT,
                ctx->total->spot_count_mates, ctx->total->bio_len_mates,
                ctx->total->bad_spot_count, ctx->total->bad_bio_len,
                ctx->total->filtered_spot_count, ctx->total->filtered_bio_len));
        }
    }

    if (ctx->pb->quick && ctx->meta_stats->found) {
        memset(&ctx->pb->total, 0, sizeof ctx->pb->total);
        rc = parse_bam_header(ctx->db, meta_RG_callback, ctx->meta_stats);
        srastatmeta_print(ctx->meta_stats, ctx->pb);
        if (ctx->pb->total.spot_count != ctx->meta_stats->table.spot_count ||
            ctx->pb->total.BIO_BASE_COUNT != ctx->meta_stats->table.BIO_BASE_COUNT ||
            ctx->pb->total.BASE_COUNT != ctx->meta_stats->table.BASE_COUNT ||
            ctx->pb->total.total_cmp_len != ctx->meta_stats->table.CMP_BASE_COUNT)
        {
            rc = RC(rcExe, rcData, rcValidating, rcData, rcUnequal);
            assert(rc == 0);
        }
    }
    else {
        memset(&ctx->pb->total, 0, sizeof ctx->pb->total);
        rc = parse_bam_header(ctx->db, tree_RG_callback, ctx->tr);
        BSTreeForEach(ctx->tr, false, srastat_print, ctx->pb);
        if (ctx->meta_stats->found) {
            const SraStatsMeta* m = &ctx->meta_stats->table;
            if (ctx->pb->total.BASE_COUNT != m->BASE_COUNT)
                mismatch |= eTOTAL_BASE_COUNT;
            if (ctx->pb->total.BIO_BASE_COUNT != m->BIO_BASE_COUNT)
                mismatch |= eTOTAL_BIO_BASE_COUNT;
            if (ctx->pb->total.spot_count != m->spot_count)
                mismatch |= eTOTAL_SPOT_COUNT;
            if (ctx->pb->total.total_cmp_len != m->CMP_BASE_COUNT) {
                bool noRefDbWithoutCmpReads =
                    ctx->pb->total.total_cmp_len == 0 &&
                    m->CMP_BASE_COUNT == ctx->pb->total.BASE_COUNT &&
                    ctx->db != NULL;
                bool noRefDbWithCmpReads =
                    ctx->pb->total.total_cmp_len == ctx->pb->total.BASE_COUNT &&
                    m->CMP_BASE_COUNT == 0 &&
                    ctx->singleTblDb;
                if (noRefDbWithoutCmpReads || noRefDbWithCmpReads)
                    mismatchCMP_BASE_COUNT = true;
                else
                    mismatch |= eTOTAL_CMP_BASE_COUNT;
            }
        }
/*      if (ctx->pb->total.spot_count != ctx->total->spot_count ||
            ctx->pb->total.spot_count_mates != ctx->total->spot_count_mates ||
            ctx->pb->total.BIO_BASE_COUNT != ctx->total->BIO_BASE_COUNT ||
            ctx->pb->total.bio_len_mates != ctx->total->bio_len_mates ||
            ctx->pb->total.BASE_COUNT != ctx->total->BASE_COUNT ||
            ctx->pb->total.bad_spot_count != ctx->total->bad_spot_count ||
            ctx->pb->total.bad_bio_len != ctx->total->bad_bio_len ||
            ctx->pb->total.filtered_spot_count
                != ctx->total->filtered_spot_count ||
            ctx->pb->total.filtered_bio_len != ctx->total->filtered_bio_len ||
            ctx->pb->total.total_cmp_len != ctx->total->total_cmp_len
            || ( mDfl &&
                (  ctx->total->BASE_COUNT != mDfl->BASE_COUNT ||
                   ctx->total->BIO_BASE_COUNT != mDfl->BIO_BASE_COUNT ||
                   ctx->total->spot_count != mDfl->spot_count ||
                    mismatch & eCMP_BASE_COUNT)
            || mismatch ))
        {
            rc = RC(rcExe, rcData, rcValidating, rcData, rcUnequal);
        } */
    }

    if (ctx->pb->xml) {
        if (ctx->sizes) {
            OUTMSG(("  <Size value=\"%lu\" units=\"bytes\"/>\n",
                ctx->sizes->size));
        }
        if (ctx->pb->printMeta && ctx->info->tblVersion >= 0) {
            OUTMSG(("  <%s vers=\"%d\">\n    <Meta vers=\"%d\">\n",
                ctx->info->isDb ? "Database" : "Table",
                ctx->info->tblVersion, ctx->info->metaVersion));
            if (ctx->info->formatter.name[0] || ctx->info->formatter.vers[0] ||
                ctx->info->loader.date[0] || ctx->info->loader.name[0] ||
                ctx->info->loader.vers[0])
            {
                OUTMSG(("      <SOFTWARE>\n"));
                if (ctx->info->formatter.name[0] ||
                    ctx->info->formatter.vers[0])
                {
                    OUTMSG(("        <formatter"));
                    if (ctx->info->formatter.name[0])
                    {   OUTMSG((" name=\"%s\"", ctx->info->formatter.name)); }
                    if (ctx->info->formatter.vers[0])
                    {   OUTMSG((" vers=\"%s\"", ctx->info->formatter.vers)); }
                    OUTMSG(("/>\n"));
                }
                if (ctx->info->loader.date[0] || ctx->info->loader.name[0] ||
                    ctx->info->loader.vers[0])
                {
                    OUTMSG(("        <loader"));
                    if (ctx->info->loader.date[0])
                    {   OUTMSG((" date=\"%s\"", ctx->info->loader.date)); }
                    if (ctx->info->loader.name[0])
                    {   OUTMSG((" name=\"%s\"", ctx->info->loader.name)); }
                    if (ctx->info->loader.vers[0])
                    {   OUTMSG((" vers=\"%s\"", ctx->info->loader.vers)); }
                    OUTMSG(("/>\n"));
                }
                OUTMSG(("      </SOFTWARE>\n"));
            }
            if (ctx->info->loadTimestamp) {
                char       buf[80];
                size_t sz = 0;
                rc_t rc_fmt = formatLoadTime(ctx->info->loadTimestamp, buf, sizeof(buf), &sz);
                assert(rc_fmt == 0);
                OUTMSG(("      <LOAD timestamp=\"%lX\">%s</LOAD>\n",
                    ctx->info->loadTimestamp, buf));
            }
            OUTMSG(("    </Meta>\n  </%s>\n",
                ctx->info->isDb ? "Database" : "Table"));
        }
        if (rc == 0 && !ctx->pb->quick) {
            rc2 = BasesPrint(&ctx->total->bases_count,
                             ctx->total->BIO_BASE_COUNT, "  ");
        }
        if (rc == 0 && !ctx->pb->skip_alignment) {
            rc = process_align_info("  ", ctx);
        }
        if (rc == 0 && ctx->pb->statistics) {
            rc = SraStatsTotalPrintStatistics(ctx->total, "  ", ctx->pb->test);
        }

        if (rc == 0 && ctx->pb->print_arcinfo) {
            const ArcInfo* a = ctx->arc_info;
            uint32_t i;
            char b[1024] = "";
            size_t k = 0;
            rc = make_time(a->timestamp, b, sizeof b, &k);
            if (rc == 0) {
                OUTMSG(("  <Archive timestamp=\"%.*s\"", ( int ) k, b));
                for (i = 0; i < 2; ++i) {
                    OUTMSG((" size.%s=\"%lu\" md5.%s=\"%s\"",
                        a->i[i].tag, a->i[i].size, a->i[i].tag, a->i[i].md5));
                }
                OUTMSG(("/>\n"));
            }
        }

        if (rc == 0)
        {   rc = QualityStatsPrint(&ctx->quality, "  "); }

        if (rc == 0)
        {   rc = TableCountsPrint(&ctx->tables, "  "); }

        if ( rc == 0 )
            rc = CtxPrintCHANGES ( ctx, "  " );

        if ( rc == 0 && ctx -> n90 > 0 )
            OUTMSG ( ("  <AssemblyStatistics "
                "n50=\"%lu\" l50=\"%lu\" n90=\"%lu\" l90=\"%lu\" "
                "n=\"%lu\" l=\"%lu\"/>\n",
                ctx -> n50, ctx -> l50, ctx -> n90, ctx -> l90,\
                ctx -> n, ctx -> l ) );
        OUTMSG(("</Run>\n"));
    }

    if (mismatchCMP_BASE_COUNT != 0)
        /* ignore it */;

    assert(ctx->pb);
    if (!mismatch && ctx->pb->start == 0 && ctx->pb->stop == 0 &&
        ctx->pb->report)
    {
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BIO_BASE_COUNT",
                    ctx->total->BIO_BASE_COUNT, mDfl->BIO_BASE_COUNT));
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BASE_COUNT",
                    ctx->total->BASE_COUNT, mDfl->BASE_COUNT));
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BASE_COUNT",
                    ctx->pb->total.BASE_COUNT,
                    ctx->meta_stats->table.BASE_COUNT));
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/CMP_BASE_COUNT",
                    ctx->total->total_cmp_len, mDfl->CMP_BASE_COUNT));
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/CMP_BASE_COUNT",
                    ctx->total->total_cmp_len,
                    ctx->meta_stats->table.CMP_BASE_COUNT));
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/SPOT_COUNT",
                    ctx->total->spot_count, mDfl->spot_count));
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BIO_BASE_COUNT",
                    ctx->pb->total.BIO_BASE_COUNT,
                    ctx->meta_stats->table.BIO_BASE_COUNT));
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/SPOT_COUNT",
                    ctx->pb->total.spot_count,
                    ctx->meta_stats->table.spot_count));
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined number of spot groups - match: "
                    "Expected:$(E), Actual:$(A).",
                    "E=%u,A=%u", spotGroupN, ctx->meta_stats->spotGroupN));
                LOGMSG(klogInfo,
                    "Examined CMP_BASE_COUNT in spot groups - match");
    }
    else if (mismatch && ctx->pb->start == 0 && ctx->pb->stop == 0) {
        /* check mismatch just when no --start, --stop specified */

        assert(mDfl);

        if (mismatch & eBIO_BASE_COUNT) {
            PLOGMSG(klogWarn, (klogWarn,
                "Mismatch between calculated and recorded statistics: "
                "sum{READ_LEN}[SRA_READ_TYPE_BIOLOGICAL]($(C)) != "
                "STATS/TABLE/BIO_BASE_COUNT($(R))",
                "C=%lu,R=%lu",
                ctx->total->BIO_BASE_COUNT, mDfl->BIO_BASE_COUNT));
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eBIO_BASE_COUNT"));
            if (ctx->pb->report)
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - mismatch: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BIO_BASE_COUNT",
                    ctx->total->BIO_BASE_COUNT, mDfl->BIO_BASE_COUNT));
        }
        else if (ctx->pb->report)
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BIO_BASE_COUNT",
                    ctx->total->BIO_BASE_COUNT, mDfl->BIO_BASE_COUNT));

        if (mismatch & (eBASE_COUNT | eTOTAL_BASE_COUNT)) {
            bool printed = false;

            if (mismatch & eBASE_COUNT && mismatch & eTOTAL_BASE_COUNT)
            {
                if (ctx->total->BASE_COUNT != ctx->pb->total.BASE_COUNT ||
                          mDfl->BASE_COUNT != ctx->meta_stats->table.BASE_COUNT
                   )
                {
                    LOGMSG(klogErr, "BASE_COUNT != TOTAL_BASE_COUNT");
                    printed = true;
                }
            }

            if (mismatch & eBASE_COUNT) {
                PLOGMSG(klogWarn, (klogWarn,
                    "Mismatch between calculated and recorded statistics: "
                          "sum{READ_LEN}($(C)) != STATS/TABLE/BASE_COUNT($(R))",
                    "C=%lu,R=%lu", ctx->total->BASE_COUNT, mDfl->BASE_COUNT));
                if (ctx->pb->repair && !printed) {
                    PLOGMSG(klogInfo, (klogInfo,
                        "{MISMATCH} Name:$(N), Expected:$(E), Actual:$(A).",
                        "N=%s,E=%lu,A=%lu", "STATS/TABLE/BASE_COUNT",
                        ctx->total->BASE_COUNT, mDfl->BASE_COUNT));
                    printed = true;
                }
                if (ctx->pb->report)
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - mismatch: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BASE_COUNT",
                    ctx->total->BASE_COUNT, mDfl->BASE_COUNT));
            }
            else if (ctx->pb->report)
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BASE_COUNT",
                    ctx->total->BASE_COUNT, mDfl->BASE_COUNT));

            if (mismatch & eTOTAL_BASE_COUNT) {
                PLOGMSG(klogWarn, (klogWarn,
                    "Mismatch between calculated and recorded statistics: "
                          "foreach SPOT_GROUP sum{READ_LEN}($(C))"
                              " != STATS/TABLE/BASE_COUNT($(R))", "C=%lu,R=%lu",
                    ctx->pb->total.BASE_COUNT,
                    ctx->meta_stats->table.BASE_COUNT));
                if (ctx->pb->repair && !printed) {
                    PLOGMSG(klogInfo, (klogInfo,
                        "{MISMATCH} Name:$(N), Expected:$(E), Actual:$(A).",
                        "N=%s,E=%lu,A=%lu", "STATS/TABLE/BASE_COUNT",
                        ctx->pb->total.BASE_COUNT,
                        ctx->meta_stats->table.BASE_COUNT));
                    printed = true;
                }
                if (ctx->pb->report)
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - mismatch: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BASE_COUNT",
                    ctx->pb->total.BASE_COUNT,
                    ctx->meta_stats->table.BASE_COUNT));
            }
            else if (ctx->pb->report)
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BASE_COUNT",
                    ctx->pb->total.BASE_COUNT,
                    ctx->meta_stats->table.BASE_COUNT));
        }

        if (mismatch & (eCMP_BASE_COUNT | eTOTAL_CMP_BASE_COUNT)) {
            bool printed = false;

            if (mismatch & eCMP_BASE_COUNT && mismatch & eTOTAL_CMP_BASE_COUNT)
            {
                if (ctx->total->total_cmp_len != ctx->pb->total.total_cmp_len ||
                   mDfl->CMP_BASE_COUNT != ctx->meta_stats->table.CMP_BASE_COUNT
                    )
                {
                    LOGMSG(klogErr, "CMP_BASE_COUNT != TOTAL_CMP_BASE_COUNT");
                    printed = true;
                }
            }

            if (mismatch & eCMP_BASE_COUNT) {
                PLOGMSG(klogWarn, (klogWarn,
                    "Mismatch between calculated and recorded statistics: "
                          "sum{READ_LEN}[CMP]($(C)) != "
                                             "STATS/TABLE/CMP_BASE_COUNT($(R))",
                    "C=%lu,R=%lu",
                    ctx->total->total_cmp_len, mDfl->CMP_BASE_COUNT));
                if (ctx->pb->repair && !printed) {
                    PLOGMSG(klogInfo, (klogInfo,
                        "{MISMATCH} Name:$(N), Expected:$(E), Actual:$(A).",
                        "N=%s,E=%lu,A=%lu", "STATS/TABLE/CMP_BASE_COUNT",
                        ctx->total->total_cmp_len, mDfl->CMP_BASE_COUNT));
                    printed = true;
                }
                if (ctx->pb->report)
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - mismatch: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/CMP_BASE_COUNT",
                    ctx->total->total_cmp_len, mDfl->CMP_BASE_COUNT));
            }
            else if (ctx->pb->report)
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/CMP_BASE_COUNT",
                    ctx->total->total_cmp_len, mDfl->CMP_BASE_COUNT));

            if (mismatch & eTOTAL_CMP_BASE_COUNT) {
                PLOGMSG(klogWarn, (klogWarn,
                    "Mismatch between calculated and recorded statistics: "
                    "foreach SPOT_GROUP sum{READ_LEN}[CMP]($(C))"
                                   " != STATS/TABLE/CMP_BASE_COUNT($(R))",
                    "C=%lu,R=%lu", ctx->pb->total.total_cmp_len,
                    ctx->meta_stats->table.CMP_BASE_COUNT));
                if (ctx->pb->repair && !printed)
                    PLOGMSG(klogInfo, (klogInfo,
                        "{MISMATCH} Name:$(N), Expected:$(E), Actual:$(A).",
                        "N=%s,E=%lu,A=%lu", "STATS/TABLE/CMP_BASE_COUNT",
                        ctx->pb->total.total_cmp_len,
                        ctx->meta_stats->table.CMP_BASE_COUNT));
                if (ctx->pb->report)
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - mismatch: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/CMP_BASE_COUNT",
                    ctx->total->total_cmp_len,
                    ctx->meta_stats->table.CMP_BASE_COUNT));
            }
            else if (ctx->pb->report)
                  PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/CMP_BASE_COUNT",
                    ctx->total->total_cmp_len,
                    ctx->meta_stats->table.CMP_BASE_COUNT));
        }

        if (mismatch & eSPOT_COUNT) {
            PLOGMSG(klogWarn, (klogWarn,
                "Mismatch between calculated and recorded statistics: "
                "number-of-spots($(C)) != "
                "STATS/TABLE/SPOT_COUNT($(R))",
                "C=%lu,R=%lu",
                ctx->total->spot_count, mDfl->spot_count));
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eSPOT_COUNT"));
            if (ctx->pb->report)
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - mismatch: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/SPOT_COUNT",
                    ctx->total->spot_count, mDfl->spot_count));
        }
        else if (ctx->pb->report)
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/SPOT_COUNT",
                    ctx->total->spot_count, mDfl->spot_count));

        if (mismatch & eSPOT_GROUP_N_MISMATCH) {
            PLOGMSG(klogWarn, (klogWarn,
                "Mismatch between calculated and recorded statistics: "
                "number-of-spot-groups($(C) != $(R))",
                "C=%u,R=%u", spotGroupN, ctx->meta_stats->spotGroupN));
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eSPOT_GROUP_N_MISMATCH"));
            if (ctx->pb->report)
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined number of spot groups - mismatch: "
                    "Expected:$(E), Actual:$(A).",
                    "E=%u,A=%u", spotGroupN, ctx->meta_stats->spotGroupN));
        }
        else if (ctx->pb->report)
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined number of spot groups - match: "
                    "Expected:$(E), Actual:$(A).",
                    "E=%u,A=%u", spotGroupN, ctx->meta_stats->spotGroupN));

        if (mismatch & eSG_CMP_BASE_COUNT) {
            LOGMSG(klogWarn,
                "Mismatch between calculated and recorded statistics: "
                "CMP_BASE_COUNT in spot groups");
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eSG_CMP_BASE_COUNT"));
            if (ctx->pb->report)
                LOGMSG(klogInfo,
                    "Examined CMP_BASE_COUNT in spot groups - mismatch");
        }
        else if (ctx->pb->report)
                LOGMSG(klogInfo,
                    "Examined CMP_BASE_COUNT in spot groups - match");

        if (mismatch & eNO_SPOT_GROUP) {
            LOGMSG(klogWarn, "Mismatch between calculated and recorded "
                "statistics: spot_group not found");
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eNO_SPOT_GROUP"));
        }

        if (mismatch & eSG_BASE_COUNT) {
            LOGMSG(klogWarn, "Mismatch between calculated and recorded "
                "statistics: BASE_COUNT in spot_group");
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eSG_BASE_COUNT"));
        }

        if (mismatch & eSG_BIO_BASE_COUNT) {
            LOGMSG(klogWarn, "Mismatch between calculated and recorded "
                "statistics: BIO_BASE_COUNT in spot_group");
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eSG_BIO_BASE_COUNT"));
        }

        if (mismatch & eSG_CMP_BASE_COUNT) {
            LOGMSG(klogWarn, "Mismatch between calculated and recorded "
                "statistics: CMP_BASE_COUNT in spot_group");
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eSG_CMP_BASE_COUNT"));
        }

        if (mismatch & eSG_SPOT_COUNT) {
            LOGMSG(klogWarn, "Mismatch between calculated and recorded "
                "statistics: SPOT_COUNT in spot_group");
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eSG_SPOT_COUNT"));
        }

        if (mismatch & eTOTAL_BIO_BASE_COUNT) {
            PLOGMSG(klogWarn, (klogWarn,
                "Mismatch between calculated and recorded statistics: "
                "foreach SPOT_GROUP sum{READ_LEN}[SRA_READ_TYPE_BIOLOGICAL]"
                "($(C)) != STATS/TABLE/BIO_BASE_COUNT($(R))",
                "C=%lu,R=%lu", ctx->pb->total.BIO_BASE_COUNT,
                ctx->meta_stats->table.BIO_BASE_COUNT));
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eTOTAL_BIO_BASE_COUNT"));
            if (ctx->pb->report)
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - mismatch: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BIO_BASE_COUNT",
                    ctx->pb->total.BIO_BASE_COUNT,
                    ctx->meta_stats->table.BIO_BASE_COUNT));
        }
        else if (ctx->pb->report)
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/BIO_BASE_COUNT",
                    ctx->pb->total.BIO_BASE_COUNT,
                    ctx->meta_stats->table.BIO_BASE_COUNT));

        if (mismatch & eTOTAL_SPOT_COUNT) {
            PLOGMSG(klogWarn, (klogWarn,
                "Mismatch between calculated and recorded statistics: "
                "foreach SPOT_GROUP number-of-spots($(C))"
                              " != STATS/TABLE/SPOT_COUNT($(R))", "C=%lu,R=%lu",
                ctx->pb->total.spot_count, ctx->meta_stats->table.spot_count));
            if (ctx->pb->repair)
                PLOGMSG(klogInfo, (klogInfo,
                    "{MISMATCH} Case:$(C).", "C=%s", "eTOTAL_SPOT_COUNT"));
            if (ctx->pb->report)
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - mismatch: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/SPOT_COUNT",
                    ctx->pb->total.spot_count,
                    ctx->meta_stats->table.spot_count));
        }
        else if (ctx->pb->report)
                PLOGMSG(klogInfo, (klogInfo,
                    "Examined $(N) - match: Expected:$(E), Actual:$(A).",
                    "N=%s,E=%lu,A=%lu", "STATS/TABLE/SPOT_COUNT",
                    ctx->pb->total.spot_count,
                    ctx->meta_stats->table.spot_count));

        if (rc == 0)
            rc = RC(rcExe, rcData, rcValidating, rcData, rcUnequal);
    }

    if (rc == 0 && rc2 != 0)
        rc = rc2;

    return rc;
}

static
void CC bst_whack_free ( BSTNode *n, void *ignore )
{
    SraStats* ss = (SraStats*)n;
    BAM_HEADER_RG_free(&ss->BAM_HEADER);
    free(ss);
}

static
int64_t CC srastats_sort ( const BSTNode *item, const BSTNode *n )
{
    const SraStats *ss = ( const SraStats* ) item;
    return srastats_cmp(ss->spot_group,n);
}

static rc_t sra_stat(srastat_parms* pb, BSTree* tr,
    SraStatsTotal* total, const Ctx * ctx, const VTable *vtbl)
{
    rc_t rc = 0;

    const VCursor *curs = NULL;

    const char PRIMARY_ALIGNMENT_ID[] = "PRIMARY_ALIGNMENT_ID";
    const char RD_FILTER [] = "RD_FILTER";
    const char READ_LEN  [] = "READ_LEN";
    const char READ_TYPE [] = "READ_TYPE";
    const char SPOT_GROUP[] = "SPOT_GROUP";

    uint32_t idxPRIMARY_ALIGNMENT_ID = 0;
    uint32_t idxRD_FILTER = 0;
    uint32_t idxREAD_LEN = 0;
    uint32_t idxREAD_TYPE = 0;
    uint32_t idxSPOT_GROUP = 0;

    int g_nreads = 0;
    int64_t  n_spots = 0;
    int64_t start = 0;
    int64_t stop  = 0;

    /* filled with dREAD_LEN[i] for (spotid == start);
       used to check fixedReadLength */
    uint64_t * g_totalREAD_LEN
        = calloc ( MAX_NREADS, sizeof * g_totalREAD_LEN );
    uint64_t * g_nonZeroLenReads
        = calloc ( MAX_NREADS, sizeof * g_nonZeroLenReads );
    uint32_t * g_dREAD_LEN = calloc ( MAX_NREADS, sizeof * g_dREAD_LEN );
    if ( g_totalREAD_LEN == NULL || g_nonZeroLenReads == NULL ||
         g_dREAD_LEN == NULL )
    {
        rc = RC ( rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted );
        DBGMSG ( DBG_APP, DBG_COND_1,
            ( "Failed to allocate buffers for %zu READS\n", MAX_NREADS ) );
    }
    else
        DBGMSG ( DBG_APP, DBG_COND_1,
            ( "Allocated buffers for %zu READS\n", MAX_NREADS ) );

    rc = VTableCreateCachedCursorRead(vtbl, &curs, DEFAULT_CURSOR_CAPACITY);
    DISP_RC(rc, "Cannot VTableCreateCachedCursorRead");

    if (rc == 0) {
        rc = VCursorPermitPostOpenAdd(curs);
        DISP_RC(rc, "Cannot VCursorPermitPostOpenAdd");
    }

    if (rc == 0) {
        rc = VCursorOpen(curs);
        DISP_RC(rc, "Cannot VCursorOpen");
    }

    assert(pb && vtbl && tr && total);

    if (rc == 0) {
        const char* name = READ_LEN;
        rc = VCursorAddColumn(curs, &idxREAD_LEN, "%s", name);
        DISP_RC2(rc, name, "while calling VCursorAddColumn");
    }
    if (rc == 0) {
        const char* name = READ_TYPE;
        rc = VCursorAddColumn(curs, &idxREAD_TYPE, "%s", name);
        DISP_RC2(rc, name, "while calling VCursorAddColumn");
    }

    if (rc == 0) {
        {
            const char* name = SPOT_GROUP;
            rc = VCursorAddColumn(curs, &idxSPOT_GROUP, "%s", name);
            if (columnUndefined(rc)) {
                idxSPOT_GROUP = 0;
                rc = 0;
            }
            DISP_RC2(rc, name, "while calling VCursorAddColumn");
        }
        if (rc == 0) {
            if (rc == 0) {
                const char* name = RD_FILTER;
                rc = VCursorAddColumn(curs, &idxRD_FILTER, "%s", name);
                if (columnUndefined(rc)) {
                    idxRD_FILTER = 0;
                    rc = 0;
                }
                DISP_RC2(rc, name, "while calling VCursorAddColumn");
            }
/*          if (rc == 0) {
                const char* name = CMP_READ;
                rc = SRATableOpenColumnRead
                    (tbl, &cCMP_READ, name, "INSDC:dna:text");
                if (GetRCState(rc) == rcNotFound)
                {   rc = 0; }
                DISP_RC2(rc, name, "while calling SRATableOpenColumnRead");
            } */
            if (rc == 0) {
                const char* name = PRIMARY_ALIGNMENT_ID;
                rc = VCursorAddColumn(curs, &idxPRIMARY_ALIGNMENT_ID,
                    "%s", name);
                if (columnUndefined(rc)) {
                    idxPRIMARY_ALIGNMENT_ID = 0;
                    rc = 0;
                }
                DISP_RC2(rc, name, "while calling VCursorAddColumn");
            }
            if (rc == 0) {
                int64_t first = 0;
                uint64_t count = 0;
                int64_t spotid;
                pb->hasSPOT_GROUP = 0;
                rc = VCursorIdRange(curs, 0, &first, &count);
                DISP_RC(rc, "VCursorIdRange() failed");
                if (rc == 0) {
                    rc = BasesInit(&total->bases_count, ctx, vtbl, pb);
                }
                if (rc == 0) {
                    const KLoadProgressbar *pr = NULL;
                    bool bad_read_filter = false;
                    bool fixedNReads = true;
                    bool fixedReadLength = true;

                    uint32_t * dREAD_LEN
                        = calloc ( MAX_NREADS, sizeof * dREAD_LEN );
                    uint8_t * dREAD_TYPE
                        = calloc ( MAX_NREADS, sizeof * dREAD_TYPE );
                    uint8_t * dRD_FILTER
                        = calloc ( MAX_NREADS, sizeof * dRD_FILTER );
                    size_t MAX_SPOT_GROUP = 1000;
                    char * dSPOT_GROUP
                        = calloc ( MAX_SPOT_GROUP, sizeof * dSPOT_GROUP );
                    if ( dREAD_LEN  == NULL || dREAD_TYPE  == NULL ||
                         dRD_FILTER == NULL || dSPOT_GROUP == NULL )
                    {
                        rc = RC ( rcExe, rcStorage,
                                  rcAllocating, rcMemory, rcExhausted );
                        DBGMSG ( DBG_APP, DBG_COND_1, ( "Failed to allocate "
                            "cursor buffers for READ_LEN %zu\n", MAX_NREADS ) );
                    }
                    else {
                        DBGMSG ( DBG_APP, DBG_COND_1, ( "Allocated cursor "
                                "buffers for %zu READS\n", MAX_NREADS ) );
                        DBGMSG ( DBG_APP, DBG_COND_1, ( "Allocated "
                                "buffer for SPOT_GROUP[%zu]\n",
                                MAX_SPOT_GROUP ) );
                        string_copy_measure ( dSPOT_GROUP, MAX_SPOT_GROUP,
                                              "NULL" );
                    }

                    if (pb->start > 0) {
                        start = pb->start;
                        if (start < first) {
                            start = first;
                        }
                    }
                    else {
                        start = first;
                    }

                    if (pb->stop > 0) {
                        stop = pb->stop;
                        if ( ( uint64_t ) stop > first + count) {
                            stop = first + count;
                        }
                    }
                    else {
                        stop = first + count;
                    }

                    for (spotid = start; spotid < stop && rc == 0; ++spotid) {
                        SraStats* ss;

                        const void* base;
                        bitsz_t boff, row_bits;
                        int nreads;

                        rc = Quitting();
                        if (rc != 0) {
                            LOGMSG(klogWarn, "Interrupted");
                        }

                        if (rc == 0 && pb->progress && pr == NULL) {
                            uint64_t b = total->bases_count.stopSEQUENCE + 1
                                       - total->bases_count.startSEQUENCE;
                            if ( total->bases_count.stopALIGNMENT > 0 )
                                b +=  total->bases_count.stopALIGNMENT + 1
                                    - total->bases_count.startALIGNMENT;
                            rc = KLoadProgressbar_Make(&pr,
                                                       stop + 1 - start + b);
                            if (rc != 0) {
                                DISP_RC(rc, "cannot initialize progress bar");
                                rc = 0;
                                pr = NULL;
                            }
                            else if (stop - start > 99) {
                                KLoadProgressbar_Process(pr, 0, true);
                            }
                        }

                        if (rc == 0) {
                            rc = VCursorColumnRead(curs, spotid,
                                idxREAD_LEN, &base, &boff, &row_bits);
                            DISP_RC_Read(rc, READ_LEN, spotid,
                                "while calling VCursorColumnRead");
                        }
                        if (rc == 0) {
                            if (boff & 7) {
                                rc = RC(rcExe, rcColumn, rcReading,
                                    rcOffset, rcInvalid);
                            }
                            else if (row_bits & 7) {
                                rc = RC(rcExe, rcColumn, rcReading,
                                    rcSize, rcInvalid);
                            }
                            else if ( ( row_bits >> 3 )
                                 > MAX_NREADS * sizeof * dREAD_LEN )
                            {
                                size_t oldMAX_NREADS = MAX_NREADS;
                                MAX_NREADS =
                                    ( row_bits >> 3 ) / sizeof * dREAD_LEN
                                    + 1000;
                                if ( rc == 0 ) {
                                    uint32_t * tmp = realloc ( dREAD_LEN,
                                        MAX_NREADS * sizeof * dREAD_LEN );
                                    if ( tmp == NULL ) {
                                        rc = RC ( rcExe, rcStorage,
                                          rcAllocating, rcMemory, rcExhausted );
                                    }
                                    else
                                        dREAD_LEN = tmp;
                                }
                                if ( rc == 0 ) {
                                    uint8_t * tmp = realloc ( dREAD_TYPE,
                                        MAX_NREADS * sizeof * dREAD_TYPE );
                                    if ( tmp == NULL )
                                        rc = RC ( rcExe, rcStorage,
                                          rcAllocating, rcMemory, rcExhausted );
                                    else
                                        dREAD_TYPE = tmp;
                                }
                                if ( rc == 0 ) {
                                    uint8_t * tmp = realloc ( dRD_FILTER,
                                        MAX_NREADS * sizeof * dRD_FILTER );
                                    if ( tmp == NULL )
                                        rc = RC ( rcExe, rcStorage,
                                          rcAllocating, rcMemory, rcExhausted );
                                    else
                                        dRD_FILTER = tmp;
                                }
                                if ( rc == 0 ) {
                                    uint64_t * tmp = realloc ( g_totalREAD_LEN,
                                        MAX_NREADS * sizeof * g_totalREAD_LEN );
                                    if ( tmp == NULL )
                                        rc = RC ( rcExe, rcStorage,
                                          rcAllocating, rcMemory, rcExhausted );
                                    else {
                                        g_totalREAD_LEN = tmp;
                                        memset (
                                            g_totalREAD_LEN + oldMAX_NREADS, 0,
                                            ( MAX_NREADS - oldMAX_NREADS ) * sizeof * g_totalREAD_LEN
                                        );
                                    }
                                }
                                if ( rc == 0 ) {
                                    uint64_t * tmp = realloc ( g_totalREAD_LEN,
                                        MAX_NREADS * sizeof * g_totalREAD_LEN );
                                    if ( tmp == NULL )
                                        rc = RC ( rcExe, rcStorage,
                                          rcAllocating, rcMemory, rcExhausted );
                                    else {
                                        g_totalREAD_LEN = tmp;
                                        memset (
                                            g_totalREAD_LEN + oldMAX_NREADS, 0,
                                            ( MAX_NREADS - oldMAX_NREADS ) * sizeof * g_totalREAD_LEN
                                        );
                                    }
                                }
                                if ( rc == 0 ) {
                                    uint64_t * tmp = realloc ( g_nonZeroLenReads,
                                        MAX_NREADS * sizeof * g_nonZeroLenReads );
                                    if ( tmp == NULL )
                                        rc = RC ( rcExe, rcStorage,
                                          rcAllocating, rcMemory, rcExhausted );
                                    else {
                                        g_nonZeroLenReads = tmp;
                                        memset (
                                            g_nonZeroLenReads + oldMAX_NREADS, 0,
                                            ( MAX_NREADS - oldMAX_NREADS ) * sizeof * g_nonZeroLenReads
                                        );
                                    }
                                }
                                if ( rc == 0 ) {
                                    uint32_t * tmp = realloc ( g_dREAD_LEN,
                                        MAX_NREADS * sizeof * g_dREAD_LEN );
                                    if ( tmp == NULL )
                                        rc = RC ( rcExe, rcStorage,
                                          rcAllocating, rcMemory, rcExhausted );
                                    else {
                                        g_dREAD_LEN = tmp;
                                        memset (
                                            g_dREAD_LEN + oldMAX_NREADS, 0,
                                            ( MAX_NREADS - oldMAX_NREADS ) * sizeof * g_dREAD_LEN
                                        );
                                    }
                                }
                                if ( rc == 0 )
                                    DBGMSG ( DBG_APP, DBG_COND_1, (
                                        "Reallocated buffers "
                                        "for %zu READS\n", MAX_NREADS ) );
                                else
                                    DBGMSG ( DBG_APP, DBG_COND_1, ( "Failed to "
                                        "reallocate buffers for %zu READS\n",
                                        MAX_NREADS ) );
                            }
                            DISP_RC_Read(rc, READ_LEN, spotid,
                                "after calling VCursorColumnRead");
                        }
                        if (rc == 0) {
                            int i, bio_len, bio_count, bad_cnt, filt_cnt;
                            memmove(dREAD_LEN, ((const char*)base) + (boff>>3),
                                    ( size_t ) row_bits >> 3);
                            nreads
                                = (int) ((row_bits >> 3) / sizeof(*dREAD_LEN));
                            if (spotid == start) {
                                g_nreads = nreads;
                                if (pb->statistics) {
                                    rc = SraStatsTotalMakeStatistics
                                        (total, g_nreads);
                                }
                            }
                            else if (g_nreads != nreads) {
                                fixedNReads = false;
                            }

                            if (rc == 0) {
                                rc = VCursorColumnRead(curs, spotid,
                                    idxREAD_TYPE, &base, &boff, &row_bits);
                                DISP_RC_Read(rc, READ_TYPE, spotid,
                                    "while calling VCursorColumnRead");
                                if (rc == 0) {
                                    if (boff & 7) {
                                        rc = RC(rcExe, rcColumn, rcReading,
                                            rcOffset, rcInvalid);
                                    }
                                    else if (row_bits & 7) {
                                        rc = RC(rcExe, rcColumn, rcReading,
                                            rcSize, rcInvalid);
                                    }
                                    else if ((row_bits >> 3) >
                                        MAX_NREADS * sizeof * dREAD_TYPE)
                                    {
                                        rc = RC(rcExe, rcColumn, rcReading,
                                            rcBuffer, rcInsufficient);
                                    }
                                    else if ((row_bits >> 3) !=  nreads) {
                                        rc = RC(rcExe, rcColumn, rcReading,
                                            rcData, rcIncorrect);
                                    }
                                    DISP_RC_Read(rc, READ_TYPE, spotid,
                                        "after calling VCursorColumnRead");
                                }
                            }
                            if (rc == 0) {
                                memmove(dREAD_TYPE,
                                    ((const char*)base) + (boff >> 3),
                                    ( size_t ) row_bits >> 3);
                                if (idxSPOT_GROUP != 0) {
                                    rc = VCursorColumnRead(curs, spotid,
                                        idxSPOT_GROUP, &base, &boff, &row_bits);
                                    DISP_RC_Read(rc, SPOT_GROUP, spotid,
                                        "while calling VCursorColumnRead");
                                    if (rc == 0) {
                                        if (row_bits > 0) {
                                            int n = row_bits >> 3;
                                            if (boff & 7) {
                                                rc = RC(rcExe, rcColumn,
                                                    rcReading,
                                                    rcOffset, rcInvalid);
                                            }
                                            else if (row_bits & 7) {
                                                rc = RC(rcExe, rcColumn,
                                                    rcReading,
                                                    rcSize, rcInvalid); }
                                            else if ( n  > MAX_SPOT_GROUP ) {
                                                char * tmp = NULL;
                                                MAX_SPOT_GROUP = n + 1000;
                                                tmp = realloc ( dSPOT_GROUP,
                                                    MAX_SPOT_GROUP );
                                                if ( tmp == NULL ) {
                                                    rc = RC ( rcExe, rcStorage,
                                                     rcAllocating, rcMemory, rcExhausted );
                                                    DBGMSG ( DBG_APP, DBG_COND_1,
                                                        ( "Failed to reallocate "
                                                        "buffer for SPOT_GROUP[%zu]\n",
                                                        MAX_SPOT_GROUP ) );
                                                }
                                                else {
                                                    DBGMSG ( DBG_APP, DBG_COND_1,
                                                        ( "Reallocated "
                                                        "buffer for SPOT_GROUP[%zu]\n",
                                                        MAX_SPOT_GROUP ) );
                                                    dSPOT_GROUP = tmp;
                                                }
                                            }
                                            DISP_RC_Read(rc, SPOT_GROUP, spotid,
                                               "after calling VCursorColumnRead"
                                               );
                                            if (rc == 0) {
                                                bitsz_t n = row_bits >> 3;
                                                memmove(dSPOT_GROUP,
                                                  ((const char*)base)+(boff>>3),
                                                  ( size_t ) row_bits>>3);
                                                dSPOT_GROUP[n]='\0';
                                                if (n > 1 ||
                                                    (n == 1 && dSPOT_GROUP[0]))
                                                {
                                                    pb -> hasSPOT_GROUP = 1;
                                                }
                                            }
                                        }
                                        else {
                                            dSPOT_GROUP[0]='\0';
                                        }
                                    }
                                    else {
                                        break;
                                    }
                                }
                            }
                            if (rc == 0) {
                                uint64_t cmp_len = 0; /* CMP_READ */
                                if (idxRD_FILTER != 0) {
                                    rc = VCursorColumnRead(curs, spotid,
                                        idxRD_FILTER, &base, &boff, &row_bits);
                                    DISP_RC_Read(rc, RD_FILTER, spotid,
                                        "while calling VCursorColumnRead");
                                    if (rc == 0) {
                                        bitsz_t size = row_bits >> 3;
                                        if (boff & 7) {
                                            rc = RC(rcExe, rcColumn, rcReading,
                                                rcOffset, rcInvalid); }
                                        else if (row_bits & 7) {
                                            rc = RC(rcExe, rcColumn, rcReading,
                                                rcSize, rcInvalid);
                                        }
                                        else if (size >
                                            MAX_NREADS * sizeof * dRD_FILTER)
                                        {
                                            rc = RC(rcExe, rcColumn, rcReading,
                                                rcBuffer, rcInsufficient);
                                        }
                                        DISP_RC_Read(rc, RD_FILTER, spotid,
                                            "after calling VCursorColumnRead");
                                        if (rc == 0) {
                                            memmove(dRD_FILTER,
                                                ((const char*)base) + (boff>>3),
                                                ( size_t ) size);
                                            if (size < nreads) {
                             /* RD_FILTER is expected to have nreads elements */
                                                if (size == 1) {
                             /* fill all RD_FILTER elements with RD_FILTER[0] */
                                                    int i = 0;
                                                    for (i = 1; i < nreads;
                                                        ++i)
                                                    {
                                                        memmove(dRD_FILTER + i,
                                                  ((const char*)base)+(boff>>3),
                                                  1);
                                                    }
                                                    if
                                                     (!bad_read_filter)
                                                    {
                                                        bad_read_filter = true;
                                                        PLOGMSG(klogWarn,
                                                            (klogWarn,
             "RD_FILTER column size is 1 but it is expected to be $(n)",
                                                            "n=%d", nreads));
                                                    }
                                                }
                                                else {
                                  /* something really bad with RD_FILTER column:
                                     let's pretend it does not exist */
                                                    idxRD_FILTER = 0;
                                                    bad_read_filter = true;
                                                    PLOGMSG(klogWarn,
                                                        (klogWarn,
             "RD_FILTER column size is $(real) but it is expected to be $(exp)",
                                                        "real=%d,exp=%d",
                                                        size, nreads));
                                                }
                                            }
                                        }
                                    }
                                    else {
                                        break;
                                    }
                                }
                                if (idxPRIMARY_ALIGNMENT_ID != 0) {
                                    rc = VCursorColumnRead(curs, spotid,
                                        idxPRIMARY_ALIGNMENT_ID,
                                        &base, &boff, &row_bits);
                                    DISP_RC_Read(rc, PRIMARY_ALIGNMENT_ID,
                                        spotid,
                                        "while calling VCursorColumnRead");
                                    if (boff & 7) {
                                        rc = RC(rcExe, rcColumn, rcReading,
                                            rcOffset, rcInvalid); }
                                    else if (row_bits & 7) {
                                        rc = RC(rcExe, rcColumn, rcReading,
                                            rcSize, rcInvalid);
                                    }
                                    DISP_RC_Read(rc, PRIMARY_ALIGNMENT_ID,
                                       spotid,
                                       "after calling calling VCursorColumnRead"
                                       );
                                    if (rc == 0) {
                                        int i = 0;
                                        const int64_t* pii = base;
                                        assert(nreads);
                                        for (i = 0; i < nreads; ++i) {
                                            if (pii[i] == 0)
/* eCMP_BASE_COUNT SRR12544267 */               cmp_len += dREAD_LEN[i];
                                        }
                                    }
                                }

                                ss = (SraStats*)BSTreeFind
                                    (tr, dSPOT_GROUP, srastats_cmp);
                                if (ss == NULL) {
                                    ss = calloc(1, sizeof(*ss));
                                    if (ss == NULL) {
                                        rc = RC(rcExe, rcStorage, rcAllocating,
                                            rcMemory, rcExhausted);
                                        break;
                                    }
                                    else {
                                        strcpy(ss->spot_group, dSPOT_GROUP);
                                        BSTreeInsert
                                            (tr, (BSTNode*)ss, srastats_sort);
                                    }
                                }
     /* eSG_SPOT_COUNT */       ++ss->spot_count;
     /* eSPOT_COUNT */          ++total->spot_count;

     /* eSG_CMP_BASE_COUNT */   ss->total_cmp_len += cmp_len;
                                total->total_cmp_len += cmp_len;

                                if (pb->statistics) {
                                    SraStatsTotalAdd(total, dREAD_LEN, nreads);
                                }
                                for (bio_len = bio_count = i = bad_cnt
                                        = filt_cnt = 0;
                                    (i < nreads) && (rc == 0); i++)
                                {
                                    if ( i >= MAX_NREADS ) {
                                        rc = RC ( rcExe, rcData, rcProcessing,
                                                  rcBuffer, rcInsufficient );
                                        break;
                                    }
                                    if (dREAD_LEN[i] > 0) {
                                        g_totalREAD_LEN[i] += dREAD_LEN[i];
                                        ++g_nonZeroLenReads[i];
                                    }
                                    if (spotid == start) {
                                        g_dREAD_LEN[i] = dREAD_LEN[i];
                                    }
                                    else if (g_dREAD_LEN[i] != dREAD_LEN[i]) {
                                        fixedReadLength = false;
                                    }

                                    if (dREAD_LEN[i] > 0) {
                                        bool biological = false;
             /* eSG_BASE_COUNT */       ss->total_len += dREAD_LEN[i];
             /* eBASE_COUNT */          total->BASE_COUNT += dREAD_LEN[i];
                                        if ((dREAD_TYPE[i]
                                            & SRA_READ_TYPE_BIOLOGICAL) != 0)
                                        {
                                            biological = true;
                                            bio_len += dREAD_LEN[i];
                                            bio_count++;
                                        }
                                        if (idxRD_FILTER != 0) {
                                            switch (dRD_FILTER[i]) {
                                                case SRA_READ_FILTER_PASS:
                                                    break;
                                                case SRA_READ_FILTER_REJECT:
                                                case SRA_READ_FILTER_CRITERIA:
                                                    if (biological) {
                                                        ss->bad_bio_len
                                                            += dREAD_LEN[i];
                                                        total->bad_bio_len
                                                            += dREAD_LEN[i];
                                                    }
                                                    bad_cnt++;
                                                    break;
                                                case SRA_READ_FILTER_REDACTED:
                                                    if (biological) {
                                                        ss->filtered_bio_len
                                                            += dREAD_LEN[i];
                                                        total->filtered_bio_len
                                                            += dREAD_LEN[i];
                                                    }
                                                    filt_cnt++;
                                                    break;
                                                default:
                                                    rc = RC(rcExe, rcColumn,
                                                        rcReading,
                                                        rcData, rcUnexpected);
                                                    PLOGERR(klogInt,
                                                        (klogInt, rc,
    "spot=$(spot), read=$(read), READ_FILTER=$(val)", "spot=%lu,read=%d,val=%d",
                                                        spotid, i,
                                                        dRD_FILTER[i]));
                                                    break;
                                            }
                                        }
                                    }
                                }
     /* eSG_BIO_BASE_COUNT */   ss->bio_len += bio_len;
     /* eBIO_BASE_COUNT */      total->BIO_BASE_COUNT += bio_len;
                                if (bio_count > 1) {
                                    ++ss->spot_count_mates;
                                    ++total->spot_count_mates;
                                    ss->bio_len_mates += bio_len;
                                    total->bio_len_mates += bio_len;
                                }
                                if (bad_cnt) {
                                    ss->bad_spot_count++;
                                    total->bad_spot_count++;
                                }
                                if (filt_cnt) {
                                    ss->filtered_spot_count++;
                                    total->filtered_spot_count++;
                                }
                            }

                            if (rc == 0 && pb->progress) {
                                KLoadProgressbar_Process(pr, 1, false);
                            }
                        }
                    } /* for (spotid = start; spotid <= stop && rc == 0;
                              ++spotid) */

                    for (spotid = total->bases_count.startALIGNMENT;
                         !pb->quick &&
                           spotid < total->bases_count.stopALIGNMENT && rc == 0;
                         ++spotid)
                    {
                        rc = BasesAdd(&total->bases_count, spotid, true,
                            dREAD_LEN, dREAD_TYPE);
                        if ( rc == 0 && pb->progress )
                            KLoadProgressbar_Process ( pr, 1, false );
                        rc = Quitting();
                        if (rc != 0)
                            LOGMSG(klogWarn, "Interrupted");
                    }

                    for (spotid = total->bases_count.startSEQUENCE;
                         !pb->quick &&
                           spotid < total->bases_count.stopSEQUENCE && rc == 0;
                         ++spotid)
                    {
                        rc = BasesAdd(&total->bases_count, spotid, false,
                            dREAD_LEN, dREAD_TYPE);
                        if ( rc == 0 && pb->progress )
                            KLoadProgressbar_Process ( pr, 1, false );
                        rc = Quitting();
                        if (rc != 0)
                            LOGMSG(klogWarn, "Interrupted");
                    }

                    if (rc == 0) {
                        BasesFinalize(&total->bases_count);
                        pb->variableReadLength = !fixedReadLength;

              /* --- g_totalREAD_LEN[i] is sum(READ_LEN[i]) for all spots --- */
                        if (fixedNReads) {
                            int i = 0;
                            if (stop >= start) {
                                n_spots = stop - start;
                            }
                            if (n_spots > 0) {
                                for (i = 0; i < g_nreads && rc == 0; ++i) {
                                    if (fixedReadLength) {
                                        assert(g_totalREAD_LEN[i] / n_spots
                                            == g_dREAD_LEN[i]);
                                    }
                                }
                            }
                        }
                    }
                    if (rc == 0) {
                        KLoadProgressbar_Release(pr, true);
                        pr = NULL;
                    }

                    free ( dREAD_LEN );
                    free ( dREAD_TYPE );
                    free ( dRD_FILTER );
                    free ( dSPOT_GROUP );
                }
            }
        }
    }

    RELEASE(VCursor, curs);

    if (pb->test && rc == 0) {
        uint32_t idx = 0;
        int i = 0;
        int64_t spotid = 0;

        double   * average   = calloc ( MAX_NREADS, sizeof * average   );
        double   * diff_sq   = calloc ( MAX_NREADS, sizeof * diff_sq   );
        uint32_t * dREAD_LEN = calloc ( MAX_NREADS, sizeof * dREAD_LEN );
        if ( average == NULL || diff_sq == NULL || dREAD_LEN == NULL )
            rc = RC ( rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted );
        SraStatsTotalStatistics2Init(total,
            g_nreads, g_totalREAD_LEN, g_nonZeroLenReads);
        for (i = 0; i < g_nreads; ++i) {
            average[i] = (double)g_totalREAD_LEN[i] / n_spots;
        }

        if ( rc == 0 ) {
            rc = VTableCreateCachedCursorRead(vtbl, &curs, DEFAULT_CURSOR_CAPACITY);
            DISP_RC(rc, "Cannot VTableCreateCachedCursorRead");
        }

        if (rc == 0) {
            const char* name = READ_LEN;
            rc = VCursorAddColumn(curs, &idx, "%s", name);
            DISP_RC(rc, "Cannot VCursorAddColumn(READ_LEN)");
            if (rc == 0) {
                rc = VCursorOpen(curs);
                if (rc != 0) {
                    PLOGERR(klogInt, (klogInt,
                        rc, "Cannot VCursorOpen($(name))", "name=%s", name));
                }
            }
        }

        for (spotid = start; spotid < stop && rc == 0; ++spotid) {
            const void* base;
            bitsz_t boff, row_bits;
            if (rc == 0) {
                rc = VCursorColumnRead(curs, spotid,
                    idx, &base, &boff, &row_bits);
                DISP_RC_Read(rc, READ_LEN, spotid,
                    "while calling VCursorColumnRead");
                if ( ( row_bits >> 3 ) > sizeof dREAD_LEN )
                    rc = RC ( rcExe, rcColumn, rcReading,
                              rcBuffer, rcInsufficient);
            }
            if (rc == 0) {
                memmove(dREAD_LEN, ((const char*)base) + (boff>>3),
                        ( size_t ) row_bits>>3);
            }
            for (i = 0; i < g_nreads; ++i) {
                diff_sq[i] +=
                    (dREAD_LEN[i] - average[i]) * (dREAD_LEN[i] - average[i]);
            }
            SraStatsTotalAdd2(total, dREAD_LEN);
        }
        RELEASE(VCursor, curs);

        free ( average );
        free ( diff_sq );
        free ( dREAD_LEN );
    }

    free ( g_totalREAD_LEN );
    free ( g_nonZeroLenReads );
    free ( g_dREAD_LEN );

    return rc;
}

static
void CtxRelease(Ctx* ctx)
{
    assert(ctx);

    QualityStatsRelease(&ctx->quality);
    TableCountsRelease(&ctx->tables);

    memset(ctx, 0, sizeof *ctx);
}

static bool VDatabaseIsSingleTblDb(const VDatabase * self) {
    bool isSingleTblDb = false;

    uint32_t count = 0;

    KNamelist *names = NULL;
    rc_t rc = VDatabaseListTbl(self, &names);
    if (rc != 0)
        return false;

    rc = KNamelistCount(names, &count);
    if (rc == 0 && count == 1) {
        const char *name = NULL;
        rc = KNamelistGet(names, 0, &name);

        if (rc == 0) {
            const char SEQUENCE[] = "SEQUENCE";
            isSingleTblDb = strcmp(SEQUENCE, name) == 0;
        }
    }

    KNamelistRelease(names);

    return isSingleTblDb;
}

static
rc_t run(srastat_parms* pb)
{
    rc_t rc = 0;
    const VDBManager* vmgr = NULL;

    assert(pb && pb->table_path);

    rc = VDBManagerMakeRead(&vmgr, NULL);
    if (rc != 0)
        LOGERR(klogInt, rc, "failed to open VDBManager");
    else {
        SraSizeStats sizes;
        ArcInfo arc_info;
        SraMeta info;

        const VDatabase * db = NULL;  /* sra-srat argument is a DB */
        const VTable    * tbl = NULL; /* sra-srat argument is a table:
                                         don't release it*/
        const VTable* vtbl = NULL;

        VSchema *schema = NULL;

        rc = VDBManagerMakeSRASchema(vmgr, &schema);
        if (rc != 0) {
            LOGERR(klogInt, rc, "cannot VDBManagerMakeSRASchema");
        }
        if (rc == 0) {
            rc = VDBManagerOpenTableRead(vmgr, &vtbl,
                schema, "%s", pb->table_path);
            if (rc == 0 )
                tbl = vtbl; /* sra-srat argument is a table */
            else if ( GetRCObject(rc) == (enum RCObject)rcTable
                   && GetRCState (rc) == rcIncorrect)
            {
                const char altname[] = "SEQUENCE";
                rc_t rc2 = VDBManagerOpenDBRead(vmgr,
                    &db, schema, pb->table_path);
                if (rc2 == 0) {
                    rc2 = VDatabaseOpenTableRead(db, &vtbl, "%s", altname);
                    if (rc2 == 0) {
                        rc = 0;
                    }
                }
            }
            if (rc != 0) {
                PLOGERR(klogInt, (klogInt, rc,
                    "'$(spec)'", "spec=%s", pb->table_path));
            }
        }
        if (rc == 0) {
            MetaDataStats stats;
            SraStatsTotal total;
            const KTable* ktbl = NULL;
            const KMetadata* meta = NULL;
            const KMetadata* dbMeta = NULL;

            BSTree tr;
            Ctx ctx;

            BSTreeInit(&tr);

            memset(&ctx, 0, sizeof ctx);
            ctx . db  = db;
            ctx . tbl = tbl;
            ctx.singleTblDb = VDatabaseIsSingleTblDb(ctx.db);

            memset(&total, 0, sizeof total);

            rc = VTableOpenKTableRead(vtbl, &ktbl);
            DISP_RC(rc, "While calling VTableOpenKTableRead");
            if (rc == 0) {
                rc = KTableOpenMetadataRead(ktbl, &meta);
                DISP_RC(rc, "While calling KTableOpenMetadataRead");
            }
            if (db != NULL && rc == 0) {
                rc = VDatabaseOpenMetadataRead(db, &dbMeta);
                DISP_RC(rc, "While calling VDatabaseOpenMetadataRead");
            }
            if (rc == 0) {
                rc = get_stats_meta(meta, &stats, pb->quick);
                if (rc == 0) {
                    if (pb->quick && !stats.found) {
                        LOGMSG(klogWarn, "Statistics metadata not found: "
                            "performing full table scan");
                        pb->quick = false;
                    }
                }
                rc = 0;
            }

            if (rc == 0) {
                rc = get_size(&sizes, vtbl);
            }

            if (rc == 0 && pb->printMeta)
                rc = get_load_info(dbMeta, meta, &info);

            if (rc == 0 && !pb->quick) {
                rc = sra_stat(pb, &tr, &total, &ctx, vtbl);
            }

            if (rc == 0) {
                rc = get_arc_info(pb, &arc_info, vmgr, vtbl);
            }

            if (rc == 0) {
                rc = QualityStatsRead(&ctx.quality, meta);
                if (rc == 0) {
                    QualityStatsSort(&ctx.quality);
                }
            }

            if (rc == 0) {
                rc = TableCountsRead(&ctx.tables, db);
                if (rc == 0) {
                    TableCountsSort(&ctx.tables);
                }
            }

            if ( rc == 0 )
                rc = CalculateNL ( db, & ctx );

            if (rc == 0) {
                if ( db == NULL )
                    ctx . meta = meta;
                ctx.info = &info;
                ctx.meta_stats = &stats;
                ctx.pb = pb;
                ctx.sizes = &sizes;
                ctx.total = &total;
                ctx.arc_info = &arc_info;
                ctx.tr = &tr;
                rc = print_results(&ctx);
            }
            BSTreeWhack(&tr, bst_whack_free, NULL);
            SraStatsTotalFree(&total);
            RELEASE(KTable, ktbl);
            {
                uint32_t i;
                for (i = 0; i < stats.spotGroupN; ++i) {
                    SraStatsMetaDestroy(&stats.spotGroup[i]);
                }

                SraStatsMetaDestroy(&stats.table);

                free(stats.spotGroup);
                stats.spotGroup = NULL;
            }
            CtxRelease(&ctx);
            RELEASE(KMetadata, dbMeta);
            RELEASE(KMetadata, meta);
        }
        RELEASE(VTable, vtbl);
        RELEASE(VDatabase, db);
        RELEASE(VSchema, schema);
    }

    RELEASE(VDBManager, vmgr);

    return rc;
}


/* Usage */
#define ALIAS_ALIGN    "a"
#define OPTION_ALIGN   "alignment"
static const char * align_usage[] = { "Print alignment info, default is on."
                                                                   , NULL };

#define ALIAS_ARCINFO  NULL
#define OPTION_ARCINFO "archive-info"
static const char * arcinfo_usage[] = { "Output archive info, default is off."
                                                                    , NULL };

#define ALIAS_LOCINFO    "l"
#define OPTION_LOCINFO   "local-info"
static const char * locinfo_usage[] = {
    "Print the date, path, size and md5 of local run." , NULL };

#define ALIAS_MEMBR    NULL
#define OPTION_MEMBR   "member-stats"
static const char * membr_usage[] = { "Print member stats, default is on."
                                                          , NULL };

#define ALIAS_META     "m"
#define OPTION_META    "meta"
static const char * meta_usage[] = { "Print load metadata.", NULL };

#define ALIAS_NGC    NULL
#define OPTION_NGC     "ngc"
static const char * ngc_usage[] = { "Path to ngc file.", NULL };

#define ALIAS_QUICK    "q"
#define OPTION_QUICK   "quick"
static const char * quick_usage[] = {
   "Quick mode: get statistics from metadata;", "do not scan the table.",
   NULL };

#define ALIAS_REPAIR   NULL
#define OPTION_REPAIR "repair-data"
static const char *repair_usage[] = { "Generate data for repair tool.", NULL };

#define ALIAS_PROGRESS "p"
#define OPTION_PROGRESS "show_progress"
static const char *progress_usage[] = { "Show the percentage of completion."
                                                          , NULL };

#define ALIAS_INFO NULL
#define OPTION_INFO "info"
static const char *info_usage[] = { "Print report for "
"all fields examined for mismatch even if the old value is correct.", NULL };

#define ALIAS_SPT_D    "d"
#define OPTION_SPT_D   "spot-desc"
static const char * spt_d_usage[] = { "Print table spot descriptor.", NULL };

#define ALIAS_START    "b"
#define OPTION_START   "start"
static const char * start_usage[] = { "Starting spot id, default is 1.", NULL };

#define ALIAS_STATS    "s"
#define OPTION_STATS   "statistics"
static const char * stats_usage[] = {
       "Calculate READ_LEN average and standard deviation.", NULL };

#define ALIAS_STOP     "e"
#define OPTION_STOP    "stop"
static const char * stop_usage[] = { "Ending spot id, default is max.", NULL };

#define ALIAS_TEST     "t"
#define OPTION_TEST    "test"
static const char * test_usage[] = {
   "Test READ_LEN average and standard deviation calculation.", NULL };

#define ALIAS_XML      "x"
#define OPTION_XML     "xml"
static const char * xml_usage[] = { "Output as XML, default is text.", NULL };

OptDef Options[] = { /*                            maxCount needValue required*/
      { OPTION_ALIGN   , ALIAS_ALIGN   , NULL, align_usage   , 1, true , false }
    , { OPTION_ARCINFO , ALIAS_ARCINFO , NULL, arcinfo_usage , 0, false, false }
    , { OPTION_INFO    , ALIAS_INFO    , NULL, info_usage    , 1, false, false }
    , { OPTION_LOCINFO , ALIAS_LOCINFO , NULL, locinfo_usage , 1, false, false }
    , { OPTION_MEMBR   , ALIAS_MEMBR   , NULL, membr_usage   , 1, true , false }
    , { OPTION_META    , ALIAS_META    , NULL, meta_usage    , 1, false, false }
    , { OPTION_NGC     , ALIAS_NGC     , NULL, ngc_usage     , 1, true , false }
    , { OPTION_PROGRESS, ALIAS_PROGRESS, NULL, progress_usage, 1, false, false }
    , { OPTION_QUICK   , ALIAS_QUICK   , NULL, quick_usage   , 1, false, false }
    , { OPTION_REPAIR  , ALIAS_REPAIR  , NULL, repair_usage  , 1, false, false }
    , { OPTION_SPT_D   , ALIAS_SPT_D   , NULL, spt_d_usage   , 1, false, false }
    , { OPTION_START   , ALIAS_START   , NULL, start_usage   , 1, true,  false }
    , { OPTION_STATS   , ALIAS_STATS   , NULL, stats_usage   , 1, false, false }
    , { OPTION_STOP    , ALIAS_STOP    , NULL, stop_usage    , 1, true,  false }
    , { OPTION_TEST    , ALIAS_TEST    , NULL, test_usage    , 1, false, false }
    , { OPTION_XML     , ALIAS_XML     , NULL, xml_usage     , 1, false, false }
};

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s [options] table\n"
        "\n"
        "Summary:\n"
        "  Display table statistics\n"
        "\n", progname);
}

const char UsageDefaultName[] = "sra-stat";
rc_t CC Usage (const Args * args)
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

    HelpOptionLine(ALIAS_XML     , OPTION_XML     , NULL      , xml_usage);
    HelpOptionLine(ALIAS_START   , OPTION_START   , "row-id"  , start_usage);
    HelpOptionLine(ALIAS_STOP    , OPTION_STOP    , "row-id"  , stop_usage);
    HelpOptionLine(ALIAS_META    , OPTION_META    , NULL      , meta_usage);
    HelpOptionLine(ALIAS_QUICK   , OPTION_QUICK   , NULL      , quick_usage);
    HelpOptionLine(ALIAS_MEMBR   , OPTION_MEMBR   , "on | off", membr_usage);
    HelpOptionLine(ALIAS_ARCINFO , OPTION_ARCINFO , NULL      , arcinfo_usage);
    HelpOptionLine(ALIAS_STATS   , OPTION_STATS   , NULL      , stats_usage);
    HelpOptionLine(ALIAS_ALIGN   , OPTION_ALIGN   , "on | off", align_usage);
    HelpOptionLine(ALIAS_LOCINFO , OPTION_LOCINFO , NULL      , locinfo_usage);
    HelpOptionLine(ALIAS_PROGRESS, OPTION_PROGRESS, NULL      , progress_usage);
    HelpOptionLine(ALIAS_NGC     , OPTION_NGC     , "path"    , ngc_usage);
    XMLLogger_Usage();
    HelpOptionLine(ALIAS_REPAIR  , OPTION_REPAIR  , NULL      , repair_usage);
    HelpOptionLine(ALIAS_INFO    , OPTION_INFO    , NULL      , info_usage);

    KOutMsg ("\n");

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}


MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args* args = NULL;
    rc_t rc = 0;

    srastat_parms pb;
    memset(&pb, 0, sizeof pb);


    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc = ArgsMakeAndHandle(&args, argc, argv, 2, Options,
        sizeof Options / sizeof(OptDef), XMLLogger_Args, XMLLogger_ArgsQty);
    if (rc == 0) {
        do {
            uint32_t pcount = 0;
            const char* pc = NULL;

            {
                rc = ArgsOptionCount (args, OPTION_START, &pcount);
                if (rc != 0) {
                    break;
                }

                if (pcount == 1) {
                    rc = ArgsOptionValue (args, OPTION_START, 0, (const void **)&pc);
                    if (rc != 0) {
                        break;
                    }

                    pb.start = AsciiToU32 (pc, NULL, NULL);
                }


                rc = ArgsOptionCount (args, OPTION_STOP, &pcount);
                if (rc != 0) {
                    break;
                }


                if (pcount == 1) {
                    rc = ArgsOptionValue (args, OPTION_STOP, 0, (const void **)&pc);
                    if (rc != 0) {
                        break;
                    }

                    pb.stop = AsciiToU32 (pc, NULL, NULL);
                }


                rc = ArgsOptionCount (args, OPTION_XML, &pcount);
                if (rc != 0) {
                    break;
                }

                if (pcount > 0) {
                    pb.xml = true;
                }
            }

            {
                rc = ArgsOptionCount (args, OPTION_QUICK, &pcount);
                if (rc != 0) {
                    break;
                }

                if (pcount > 0) {
                    pb.quick = true;
                }


                rc = ArgsOptionCount (args, OPTION_META, &pcount);
                if (rc != 0) {
                    break;
                }

                if (pcount > 0) {
                    pb.printMeta = true;
                }


                rc = ArgsOptionCount (args, OPTION_MEMBR, &pcount);
                if (rc != 0) {
                    break;
                }

                if (pcount > 0) {
                    const char* v = NULL;
                    rc = ArgsOptionValue (args, OPTION_MEMBR, 0, (const void **)&v);
                    if (rc != 0) {
                        break;
                    }
                    if (!strcmp(v, "off")) {
                        pb.skip_members = true;
                    }
                }
            }

/* OPTION_REPAIR */
            {
                rc = ArgsOptionCount (args, OPTION_REPAIR, &pcount);
                if (rc != 0)
                    break;
                if (pcount > 0)
                    pb.repair = true;
            }

/* OPTION_INFO */
            {
                rc = ArgsOptionCount (args, OPTION_INFO, &pcount);
                if (rc != 0)
                    break;
                if (pcount > 0)
                    pb.report = true;
            }

/* OPTION_PROGRESS */
            {
                rc = ArgsOptionCount(args, OPTION_PROGRESS, &pcount);
                if (rc != 0) {
                    break;
                }
                if (pcount > 0) {
                    KLogLevel l = KLogLevelGet();
                    pb.progress = true;
                    rc = ArgsOptionCount(args, OPTION_LOG_LEVEL, &pcount);
                    if (rc == 0) {
                        if (pcount == 0) {
                            if (l < klogInfo) {
                                KLogLevelSet(klogInfo);
                            }
                        }
                        else if (l < klogInfo) {
                            LOGMSG(klogWarn, "log-level was set: "
                                "progress will not be shown");
                        }
                    }
                }


                rc = ArgsOptionCount (args, OPTION_ARCINFO, &pcount);
                if (rc != 0) {
                    break;
                }

                pb.print_arcinfo = pcount > 0;
            }

            const char * v = NULL;

            {
                rc = ArgsOptionCount (args, OPTION_ALIGN, &pcount);
                if (rc != 0) {
                    break;
                }

                if (pcount > 0) {
                    rc = ArgsOptionValue (args, OPTION_ALIGN, 0,
                        (const void **)&v);
                    if (rc != 0) {
                        break;
                    }
                    if (!strcmp(v, "off")) {
                        pb.skip_alignment = true;
                    }
                }
            }
            {
                rc = ArgsOptionCount (args, OPTION_LOCINFO, &pcount);
                if (rc != 0)
                    break;

                if (pcount > 0)
                    pb.print_locinfo = true;
            }
            {
                rc = ArgsOptionCount (args, OPTION_STATS, &pcount);
                if (rc != 0) {
                    break;
                }

                if (pcount > 0) {
                    pb.statistics = true;
                }


                rc = ArgsOptionCount (args, OPTION_TEST, &pcount);
                if (rc != 0) {
                    break;
                }

                if (pcount > 0) {
                    pb.test = pb.statistics = true;
                }


                rc = ArgsOptionCount(args, OPTION_NGC, &pcount);
                if (rc != 0)
                    break;
                if (pcount > 0) {
                    rc = ArgsOptionValue(args, OPTION_NGC, 0,
                        (const void **)&v);
                    if (rc != 0)
                        break;
                    KConfigSetNgcFile(v);
                }
            }

            {
                rc = ArgsParamCount (args, &pcount);
                if (rc != 0) {
                    break;
                }

                if (pcount == 0) {
                    if (pb.repair) /* meta-repair checks that sra-stat
                                      supports --repair-data by calling
                                      'sra-stat --repair-data' without argument.
                                      Just exit with success here. */
                        exit(0);

                    MiniUsage (args);
                    exit(1);
                }


                rc = ArgsParamValue (args, 0, (const void **)&pb.table_path);
                if (rc != 0) {
                    break;
                }

                if (pb.statistics && (pb.quick || ! pb.xml)) {
                    KOutMsg("\n--" OPTION_STATS
                        " option can be used just in XML NON-QUICK mode\n");
                    MiniUsage (args);
                    exit(1);
                }

                rc = XMLLogger_Make(&pb.logger, NULL, args);
                if (rc != 0) {
                    DISP_RC(rc, "cannot initialize XMLLogger");
                    rc = 0;
                }
            }
        } while (0);
    }

    if (rc == 0) {
        rc = run(&pb);
    }
    else {
        DISP_RC(rc, "while processing command line");
    }

    XMLLogger_Release(pb.logger);

    {
        rc_t rc2 = ArgsWhack(args);
        if (rc == 0) {
            rc = rc2;
        }
    }

    return VDB_TERMINATE( rc );
}
