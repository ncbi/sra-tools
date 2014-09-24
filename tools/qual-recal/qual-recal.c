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
 *==============================================================================
 */

#include <vdb/manager.h> /* VDBManager */
#include <vdb/database.h> /* VDatabase */
#include <vdb/table.h> /* VTable */
#include <vdb/schema.h> /* VSchema */
#include <vdb/cursor.h> /* VCursor */
#include <insdc/insdc.h>

#include <kdb/meta.h>

#include <kapp/main.h>

#include <kfs/file.h>
#include <klib/out.h> /* OUTMSG */
#include <klib/log.h> /* (void)LOGERR */
#include <klib/debug.h> /* DBGMSG */
#include <klib/rc.h> /* RC */

#include <assert.h>
#include <stdlib.h> /* free */
#include <string.h> /* strcmp */
#include <stdio.h>
#include <math.h>

#define MIN_HITS (10000)

struct Params {
    char const *statPath;
    char const *dbPath;
} static Params;

typedef struct stats_t {
    unsigned hits;
    unsigned miss;
} stats_t;

typedef struct {
    stats_t total;
    struct {
        stats_t total;
        struct {
            stats_t total;
            struct {
                stats_t hp_r[8];
                stats_t total;
            } gc_c[8];
        } dmer[25];
        unsigned qual[41];
        unsigned rcal[41];
        unsigned diff[82];
    } read[2];
} read_node_t;

unsigned qurc[41][41];

typedef struct {
    char *spot_group;
    unsigned positions;
    unsigned reads;
    read_node_t position[1];
} top_node_t;

static unsigned spot_groups;
static top_node_t **stats;

static top_node_t *alloc_top_node_t(unsigned positions)
{
    top_node_t *self = calloc(1, sizeof(*self) - sizeof(self->position) + positions * sizeof(self->position[0]));
    return self;
}

static top_node_t *new_top_node_t(unsigned positions, unsigned reads, char const sgnm[])
{
    top_node_t *self = alloc_top_node_t(positions);
    if (self) {
        self->spot_group = strdup(sgnm);
        self->positions = positions;
        self->reads = reads;
    }
    return self;
}

unsigned base2number(int base)
{
    switch (base) {
    case 'A':
        return 0;
    case 'C':
        return 1;
    case 'G':
        return 2;
    case 'T':
        return 3;
    default:
        return 4;
    }
}

static rc_t LoadStats(KFile const *kf)
{
    char buf[16 * 4096];
    unsigned bsz = 0;
    uint64_t fpos = 0;
    bool inheader = true;
    unsigned reads;
    unsigned positions;
    unsigned nsgrp;
    unsigned line = 1;

    for ( ; ; ) {
        unsigned cur = 0;
        unsigned len;

        {
            size_t nread;
            rc_t rc = KFileRead(kf, fpos, &buf[bsz], sizeof(buf) - bsz, &nread);
            
            if (rc) return rc;
            if (nread == 0)
                return 0;
            bsz += nread;
            fpos += nread;
        }
        do {
            for (len = 0; len + cur < bsz; ++len) {
                if (buf[cur + len] == '\n') {
                    buf[cur + len] = '\0';
                    ++len;
                    ++line;
                    goto PROCESS_LINE;
                }
            }
            memmove(buf, &buf[cur], bsz -= cur);
            break;
        PROCESS_LINE:
            if (inheader) {
                unsigned val;

                if (buf[cur] == '\0')
                    inheader = false;
                else if (sscanf(buf + cur, "MAX_POS: %u", &val) == 1)
                    positions = val + 1;
                else if (sscanf(buf + cur, "SPOTGROUPS: %u", &val) == 1) {
                    spot_groups = val;
                    stats = malloc(spot_groups * sizeof(stats[0]));
                    if (stats == NULL)
                        return RC(rcApp, rcFile, rcReading, rcMemory, rcExhausted);
                    nsgrp = 0;
                }
                else if (sscanf(buf + cur, "MAX_READ: %u", &val) == 1)
                    reads = val + 1;
                else
                    return RC(rcApp, rcFile, rcReading, rcData, rcInvalid);
            }
            else {
                unsigned hits;
                unsigned miss;
                unsigned bpos;
                unsigned read;
                unsigned hp_r;
                unsigned gc_c;
                unsigned dmer;
                char dimer[2];
                char sgnm[256];
                int i;
                
                i = sscanf(buf + cur, "%256s %u %u %2s %u %u %*u %*u %u %u",
                           sgnm,
                           &bpos,
                           &read,
                           dimer,
                           &gc_c,
                           &hp_r,
                           &hits,
                           &miss
                           );
                
                if (i != 8) {
                    fprintf(stderr, "error at line %u: %s\n", line, buf + cur);
                    return RC(rcApp, rcFile, rcReading, rcData, rcInvalid);
                }
                dmer = base2number(dimer[0]) * 5 + base2number(dimer[1]);
                if (nsgrp == 0) {
                    fprintf(stderr, "spot group: %s\n", sgnm);
                    stats[0] = new_top_node_t(positions, reads, sgnm);
                    if (stats[0] == NULL)
                        return RC(rcApp, rcFile, rcReading, rcMemory, rcExhausted);
                    ++nsgrp;
                }
                else {
                    if (strcmp(stats[nsgrp-1]->spot_group, sgnm) != 0) {
                        fprintf(stderr, "spot group: %s\n", sgnm);
                        if (nsgrp == spot_groups)
                            return RC(rcApp, rcFile, rcReading, rcData, rcInvalid);
                        stats[nsgrp] = new_top_node_t(positions, reads, sgnm);
                        if (stats[nsgrp] == NULL)
                            return RC(rcApp, rcFile, rcReading, rcMemory, rcExhausted);
                        ++nsgrp;
                    }
                }
                assert(bpos < positions);
                assert(read < reads);
                assert(gc_c < 8);
                assert(hp_r < 8);

                stats[nsgrp-1]->position[bpos].total.hits += hits;
                stats[nsgrp-1]->position[bpos].total.miss += miss;
                stats[nsgrp-1]->position[bpos].read[read].total.hits += hits;
                stats[nsgrp-1]->position[bpos].read[read].total.miss += miss;
                stats[nsgrp-1]->position[bpos].read[read].dmer[dmer].total.hits += hits;
                stats[nsgrp-1]->position[bpos].read[read].dmer[dmer].total.miss += miss;
                stats[nsgrp-1]->position[bpos].read[read].dmer[dmer].gc_c[gc_c].total.hits += hits;
                stats[nsgrp-1]->position[bpos].read[read].dmer[dmer].gc_c[gc_c].total.miss += miss;
                stats[nsgrp-1]->position[bpos].read[read].dmer[dmer].gc_c[gc_c].hp_r[hp_r].hits += hits;
                stats[nsgrp-1]->position[bpos].read[read].dmer[dmer].gc_c[gc_c].hp_r[hp_r].miss += miss;
            }
            cur += len;
        } while (1);
    }
}

static rc_t compute(char const sgnm[], char const seqbin[], uint8_t const qual[], unsigned read, unsigned len)
{
    unsigned last_base = 4;
    unsigned i;
    unsigned hp_r = 0;
    unsigned gc_c = 0;
    static unsigned sgrp = 0;

    if (strcmp(stats[sgrp]->spot_group, sgnm) != 0) {
        for (i = 1; i != spot_groups; ++i) {
            if (strcmp(stats[(i + sgrp)%spot_groups]->spot_group, sgnm) == 0) {
                sgrp = (i + sgrp)%spot_groups;
                break;
            }
        }
        if (sgrp == spot_groups)
            return RC(rcApp, rcTable, rcReading, rcData, rcInvalid);
    }
    assert(len <= stats[sgrp]->positions);
    assert(read < stats[sgrp]->reads);

    for (i = 0; i != len; ++i) {
        unsigned base = base2number(seqbin[i]);
        unsigned dmer = last_base * 5 + base;
        unsigned hits = stats[sgrp]->position[i].read[read].total.hits;
        unsigned miss = stats[sgrp]->position[i].read[read].total.miss;
        unsigned recalQ;
        int diff;

#if 1
        if (last_base == base)
            ++hp_r;
        else
            hp_r = 0;

        if (i >= 8 && (seqbin[i - 8] == 'G' || seqbin[i - 8] == 'C')) {
            assert(gc_c > 0);
            --gc_c;
        }
        assert(gc_c < 8);

        if (stats[sgrp]->position[i].read[read].dmer[dmer].total.hits >= MIN_HITS) {
            hits = stats[sgrp]->position[i].read[read].dmer[dmer].total.hits;
            miss = stats[sgrp]->position[i].read[read].dmer[dmer].total.miss;

            if (stats[sgrp]->position[i].read[read].dmer[dmer].gc_c[gc_c].total.hits >= MIN_HITS) {
                unsigned const hpr = hp_r > 7 ? 7 : hp_r;

                hits = stats[sgrp]->position[i].read[read].dmer[dmer].gc_c[gc_c].total.hits;
                miss = stats[sgrp]->position[i].read[read].dmer[dmer].gc_c[gc_c].total.miss;

                if (stats[sgrp]->position[i].read[read].dmer[dmer].gc_c[gc_c].hp_r[hpr].hits >= MIN_HITS) {
                    hits = stats[sgrp]->position[i].read[read].dmer[dmer].gc_c[gc_c].hp_r[hpr].hits;
                    miss = stats[sgrp]->position[i].read[read].dmer[dmer].gc_c[gc_c].hp_r[hpr].miss;
                }
            }
        }
        if (seqbin[i] == 'G' || seqbin[i] == 'C')
            ++gc_c;
        last_base = base;
#endif
        recalQ = floor(-10.0*log10(miss/(double)hits));

        diff = recalQ - qual[i];
        if (qual[i] == 2)
            diff = 41;
        if (diff < -40)
            diff = -40;
        else if (diff > 40)
            diff = 40;

        if (recalQ > 40)
            recalQ = 40;

        ++stats[sgrp]->position[i].read[read].qual[qual[i]];
        ++stats[sgrp]->position[i].read[read].rcal[recalQ];
        ++stats[sgrp]->position[i].read[read].diff[diff + 40];
        ++qurc[qual[i]][recalQ];
    }
    return 0;
}

static void print(void)
{
    FILE *Q = fopen("qual.histo.txt", "w");
    if (Q) {
        FILE *R = fopen("rcal.histo.txt", "w");
        if (R) {
            FILE *D = fopen("diff.histo.txt", "w");
            if (D) {
                unsigned sg;

                for (sg = 0; sg < spot_groups; ++sg) {
                    unsigned read;

                    for (read = 0; read < stats[sg]->reads; ++read) {
                        unsigned pos;

                        for (pos = 0; pos < stats[sg]->positions; ++pos) {
                            unsigned i;

                            for (i = 0; i < 41; ++i) {
                                if (stats[sg]->position[pos].read[read].qual[i])
                                    fprintf(Q, "%s\t%u\t%u\t%u\t%u\n", stats[sg]->spot_group, read, pos, i, stats[sg]->position[pos].read[read].qual[i]);
                                if (stats[sg]->position[pos].read[read].rcal[i])
                                    fprintf(R, "%s\t%u\t%u\t%u\t%u\n", stats[sg]->spot_group, read, pos, i, stats[sg]->position[pos].read[read].rcal[i]);
                            }
                            for (i = 0; i < 81; ++i) {
                                if (stats[sg]->position[pos].read[read].diff[i])
                                    fprintf(D, "%s\t%u\t%u\t%i\t%u\n", stats[sg]->spot_group, read, pos, (int)i - 40, stats[sg]->position[pos].read[read].diff[i]);
                            }
                            if (stats[sg]->position[pos].read[read].diff[81])
                                fprintf(D, "%s\t%u\t%u\t-128\t%u\n", stats[sg]->spot_group, read, pos, stats[sg]->position[pos].read[read].diff[81]);
                        }
                    }
                }
                fclose(D);
            }
            fclose(R);
        }
        fclose(Q);
    }
    Q = fopen("qurc.histo.txt", "w");
    if (Q) {
        unsigned q, r;

        for (q = 0; q != 41; ++q) {
            for (r = 0; r != 41; ++r) {
                if (qurc[q][r])
                    fprintf(Q, "%u\t%u\t%u\n", q, r, qurc[q][r]);
            }
        }
        fclose(Q);
    }
}

static rc_t process(VTable *dst, VTable const *src)
{
    VCursor const *curs;
    rc_t rc = VTableCreateCursorRead(src, &curs);

    if (rc == 0) {
        uint32_t idx[5];
        
        while (1) {
            rc = VCursorAddColumn(curs, idx + 0, "SPOT_GROUP"); if (rc) break;
            rc = VCursorAddColumn(curs, idx + 1, "(INSDC:dna:text)READ"); if (rc) break;
            rc = VCursorAddColumn(curs, idx + 2, "QUALITY"); if (rc) break;
            rc = VCursorAddColumn(curs, idx + 3, "(INSDC:coord:len)READ_LEN"); if (rc) break;
            rc = VCursorAddColumn(curs, idx + 4, "(INSDC:coord:zero)READ_START"); if (rc) break;
            rc = VCursorOpen(curs);
            break;
        }
        while (rc == 0) {
            rc = VCursorOpenRow(curs);
            while (rc == 0) {
                char sgnm[256];
                char const *read;
                unsigned readlen;
                unsigned nreads;
                unsigned i;
                uint8_t const *qual;
                INSDC_coord_len const *read_len;
                INSDC_coord_zero const *read_start;
                uint32_t bits;
                uint32_t boff;
                uint32_t rlen;

                rc = VCursorRead(curs, idx[0], 8, sgnm, 256, &rlen); if (rc) break;
                sgnm[rlen] = '\0';

                rc = VCursorCellData(curs, idx[1], &bits, (void const **)&read, &boff, &rlen); if (rc) break;
                readlen = rlen;
                assert(bits == 8);
                assert(boff == 0);

                rc = VCursorCellData(curs, idx[2], &bits, (void const **)&qual, &boff, &rlen); if (rc) break;
                assert(readlen == rlen);
                assert(bits == 8);
                assert(boff == 0);

                rc = VCursorCellData(curs, idx[3], &bits, (void const **)&read_len, &boff, &rlen); if (rc) break;
                nreads = rlen;
                assert(bits == 32);
                assert(boff == 0);

                rc = VCursorCellData(curs, idx[4], &bits, (void const **)&read_start, &boff, &rlen); if (rc) break;
                assert(nreads == rlen);
                assert(bits == 32);
                assert(boff == 0);

                for (i = 0; i != nreads && rc == 0; ++i) {
                    rc = compute(sgnm, read + read_start[i], qual + read_start[i], i, read_len[i]);
                }
                break;
            }
            if (GetRCState(rc) == rcNotFound && GetRCObject(rc) == rcRow) {
                print();
                rc = 0;
                break;
            }
            rc = VCursorCloseRow(curs);
        }
        VCursorRelease(curs);
    }
    return rc;
}

static rc_t ModifySchema(VSchema *schema)
{
    VSchemaRuntimeTable *tschema;
    rc_t rc = VSchemaMakeRuntimeTable(schema, &tschema, "NCBI:align:tbl:seq:qc", NULL);
    
    if (rc == 0) {
        rc = VSchemaRuntimeTableAddIntegerColumn(tschema, 16, true, "DELTA_Q");
        if (rc == 0) {
            rc = VSchemaRuntimeTableCommit(tschema);
        }
        VSchemaRuntimeTableClose(tschema);
    }
    return rc;
}

static rc_t OpenDatabaseAndTable(VTable **tbl, VDatabase const **db)
{
    VDBManager *vdb;
    rc_t rc = VDBManagerMakeUpdate(&vdb, NULL);

    if (rc == 0) {
        rc = VDBManagerOpenDBRead(vdb, db, NULL, "%s", Params.dbPath);
        if (rc == 0) {
#if 0
            VSchema *schema;
            
            rc = VDBManagerMakeSchema(vdb, &schema);
            if (rc == 0) {
                rc = VSchemaParseFile(schema, "align/align.vschema");
                if (rc == 0) {
                    rc = ModifySchema(schema);
                    if (rc == 0) {
                        rc = VDBManagerCreateTable(vdb, tbl, schema, "NCBI:align:tbl:seq:qc", kcmInit + kcmMD5, "%s/%s", Params.dbPath, ".TEMP");
                        if (rc == 0)
                            rc = VTableColumnCreateParams(*tbl, kcmInit, kcmMD5, 0);
                        if (rc)
                            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to create output '$(outname)'", "outname=%s/%s", Params.dbPath, ".TEMP"));
                    }
                }
                else
                    (void)PLOGERR(klogErr, (klogErr, rc, "Failed to load schema", ""));
                VSchemaRelease(schema);
            }
            else
                (void)PLOGERR(klogErr, (klogErr, rc, "Failed to create schema", ""));
#endif
        }
        else
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open input '$(inname)'", "inname=%s", Params.dbPath));
        VDBManagerRelease(vdb);
    }
    else
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to create manager", ""));
    return rc;
}

rc_t KFileOpenRead(KFile const **kfp, char const fname[])
{
    KDirectory *dir;
    rc_t rc = KDirectoryNativeDir(&dir);
    
    if (rc == 0) {
        rc = KDirectoryOpenFileRead(dir, kfp, "%s", fname);
        KDirectoryRelease(dir);
    }
    return rc;
}

static rc_t run(void)
{
    KFile const *kf;
    rc_t rc = KFileOpenRead(&kf, Params.statPath);

    if (rc == 0) {
        rc = LoadStats(kf);
        KFileRelease(kf);
        if (rc == 0) {
            VTable *dst = NULL;
            VDatabase const *src = NULL;
            rc_t rc = OpenDatabaseAndTable(&dst, &src);

            if (rc == 0) {
                VTable const *seq;

                rc = VDatabaseOpenTableRead(src, &seq, "SEQUENCE");
                if (rc == 0) {
                    rc = process(dst, seq);
                    VTableRelease(seq);
                }
            }
            VTableRelease(dst);
            VDatabaseRelease(src);
        }
    }
    return rc;
}
 
static const char* param_usage[] = { "Path to the database" };
static const char* stats_usage[] = { "Path to the statistics file" };

static OptDef const Options[] =
{
    { "stats", "s", NULL, stats_usage, 1, true, true }
};

rc_t CC UsageSummary (const char * progname) {
    return KOutMsg (
"Usage:\n"
"  %s <db-path>\n"
, progname);
 }

rc_t CC Usage(const Args* args) { 
    rc_t rc = 0 ;

    const char* progname = UsageDefaultName;
    const char* fullpath = UsageDefaultName;

    if (args == NULL)
    {    rc = RC(rcApp, rcArgv, rcAccessing, rcSelf, rcNull); }
    else
    {    rc = ArgsProgram(args, &fullpath, &progname); }

    UsageSummary(progname);

    KOutMsg("Parameters:\n");

    HelpParamLine ("db-path", param_usage);

    KOutMsg ("\nOptions:\n");

    HelpOptionLine ("s", "stats", NULL, stats_usage);

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}

const char UsageDefaultName[] = "qual-recal";

ver_t CC KAppVersion(void) { return 0x1000000; }

static rc_t ArgsRelease(Args* self) { return ArgsWhack(self); }

rc_t CC KMain(int argc, char* argv[]) {
    rc_t rc = 0;
    Args* args = NULL;

    do {
        uint32_t pcount = 0;

        rc = ArgsMakeAndHandle(&args, argc, argv, 1,
            Options, sizeof Options / sizeof (OptDef));
        if (rc) {
            (void)LOGERR(klogErr, rc, "While calling ArgsMakeAndHandle");
            break;
        }
        rc = ArgsParamCount(args, &pcount);
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failure parsing database name");
            break;
        }
        if (pcount < 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            MiniUsage(args);
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            (void)LOGERR(klogErr, rc, "Too many database parameters");
            break;
        }
        rc = ArgsParamValue(args, 0, &Params.dbPath);
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failure retrieving database name");
            break;
        }
        
        rc = ArgsOptionCount (args, "stats", &pcount);
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failure to get 'stats' argument");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            (void)LOGERR(klogErr, rc, "Too many output parameters");
            break;
        }
        rc = ArgsOptionValue(args, "stats", 0, &Params.statPath);
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failure retrieving stats file name");
            break;
        }
    } while (false);

    rc = rc ? rc : run();
    ArgsRelease(args);
    return rc;
}
