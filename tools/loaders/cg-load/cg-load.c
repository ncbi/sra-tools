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


#include "debug.h" /* DEBUG_MSG */
#include "file.h" /* FGroupMAP_LoadReads */
#include "formats.h" /* get_cg_reads_ngaps */

#include <kapp/log-xml.h> /* XMLLogger */
#include <kapp/main.h> /* KAppVersion */

#include <kdb/manager.h> /* kptDatabase */
#include <kdb/meta.h> /* KMetadata */

#include <kfs/arc.h> /* KDirectoryOpenArcDirRead */
#include <kfs/tar.h> /* KArcParseTAR */

#include <klib/out.h> /* OUTMSG */
#include <klib/printf.h> /* string_printf */
#include <klib/status.h> /* STSMSG */

#include <vdb/schema.h> /* VDBManagerMakeSchema */

#include <loader/loader-meta.h> /* KLoaderMeta_Write */

#include <sysalloc.h> /* malloc */

#include <assert.h>
#include <errno.h>

typedef struct SParam_struct
{
    const char* argv0;

    KDirectory* input_dir;
    const KDirectory *map_dir;
    const KDirectory *asm_dir;
    KDirectory* output_dir;

    const char* map_path;
    const char* asm_path;
    const char* schema;
    const char* refseqcfg;
    const char* refseqpath;
    const char* library;
    char const** refFiles; /* NULL-terminated array pointing to argv */
    /* mostly for debugging */
    uint32_t refseq_chunk;
    const char* out;
    uint32_t force;
    uint32_t force_refw;
    uint32_t force_readw;
    uint32_t no_read_ahead;
    const char* qual_quant;
    uint32_t no_spot_group;
    uint32_t min_mapq;
    uint32_t single_mate;
    uint32_t cluster_size;
    uint32_t load_other_evidence;

    uint32_t read_len;
} SParam;

typedef struct DB_Handle_struct {
    VDBManager* mgr;
    VSchema* schema;
    VDatabase* db;
    const ReferenceMgr* rmgr;
    const CGWriterSeq* wseq;
    TReadsData* reads;
    const CGWriterAlgn* walgn;
    TMappingsData* mappings;
    const CGWriterEvdInt* wev_int;
    TEvidenceIntervalsData* ev_int;
    const CGWriterEvdDnbs* wev_dnb;
    TEvidenceDnbsData* ev_dnb;
} DB_Handle;

static
rc_t DB_Init(const SParam* p, DB_Handle* h)
{
    rc_t rc;

    if( (rc = VDBManagerMakeUpdate(&h->mgr, NULL)) != 0 ) {
        LOGERR(klogErr, rc, "failed to create VDB Manager");

    }
    else if( (rc = VDBManagerMakeSchema(h->mgr, &h->schema)) != 0 ) {
        LOGERR(klogErr, rc, "failed to create schema");

    }
    else if( (rc = VSchemaParseFile(h->schema, "%s", p->schema)) != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to parse schema file '$(schema)'", PLOG_S(schema), p->schema));

    }
    else if( (rc = VDBManagerCreateDB(h->mgr, &h->db, h->schema, "NCBI:align:db:alignment_evidence",
                                        p->force ? kcmInit : kcmCreate, p->out)) != 0 ) {
        PLOGERR(klogErr, (klogErr, rc, "failed to create database at '$(path)'", PLOG_S(path), p->out));

    }
    else if( (rc = ReferenceMgr_Make(&h->rmgr, h->db, h->mgr, (p->force_refw ? ewrefmgr_co_allREADs : 0),
                        p->refseqcfg, p->refseqpath, p->refseq_chunk, 350 * 1024 * 1024, 0)) != 0 ) {
        LOGERR(klogErr, rc, "failed to create reference manager");

    }
    else if( (rc = CGWriterAlgn_Make(&h->walgn, &h->mappings, h->db, h->rmgr, p->min_mapq, p->single_mate, p->cluster_size)) != 0 ) {
        LOGERR(klogErr, rc, "failed to create alignment writer");

    }
    else if ((rc = CGWriterSeq_Make(&h->wseq, &h->reads, h->db,
            (p->force_readw ? ewseq_co_SaveRead : 0) |
                (p->no_spot_group ? 0 : ewseq_co_SpotGroup), p->qual_quant,
            p->read_len))
        != 0)
    {
        LOGERR(klogErr, rc, "failed to create sequence writer");

    } else if( p->asm_path && (rc = CGWriterEvdInt_Make(&h->wev_int, &h->ev_int, h->db, h->rmgr, 0)) != 0 ) {
        LOGERR(klogErr, rc, "failed to create evidence intervals writer");
    }
    else if (p->asm_path
        && (rc = CGWriterEvdDnbs_Make
            (&h->wev_dnb, &h->ev_dnb, h->db, h->rmgr, 0, p->read_len)) != 0)
    {
        LOGERR(klogErr, rc, "failed to create evidence dnbs writer");
    }
    else {
        const char** r = p->refFiles;
        while( rc == 0 && *r != NULL ) {
            if( (rc = ReferenceMgr_FastaPath(h->rmgr, *r++)) != 0 ) {
                PLOGERR(klogInfo, (klogInfo, rc, "fasta file '$(file)'", "file=%s", r[-1]));
            }
        }
    }
    return rc;
}

static
rc_t DB_Fini(const SParam* p, DB_Handle* h, bool drop)
{
    rc_t rc = 0, rc2;

    /* THIS FUNCTION MAKES NO ATTEMPT TO PRESERVE INITIAL ERROR CODES
       EACH SUCCESSIVE ERROR OVERWRITES THE PREVIOUS CODE */
    if( h != NULL ) {
        PLOGMSG(klogInfo, (klogInfo, "DB_Fini was called with drop=$(drop)",
            "drop=%s", drop ? "true" : "false"));

        PLOGMSG(klogInfo, (klogInfo, "Fini SEQUENCE", "severity=status"));
        if( (rc2 = CGWriterSeq_Whack(h->wseq, !drop, NULL)) != 0) {
            LOGERR(klogErr, rc2, "Failed SEQUENCE release");
            if (!drop) {
                drop = true;
                rc = rc2;
            }
        }
        h->wseq = NULL;
        h->reads = NULL;
        PLOGMSG(klogInfo, (klogInfo, "Fini (PRI&SEC)_ALIGNMENT", "severity=status"));
        if( (rc2 = CGWriterAlgn_Whack(h->walgn, !drop, NULL, NULL)) != 0) {
            LOGERR(klogErr, rc2, "Failed (PRI&SEC)_ALIGNMENT release");
            if (!drop) {
                drop = true;
                rc = rc2;
            }
        }
        h->walgn = NULL;
        h->mappings = NULL;
        PLOGMSG(klogInfo, (klogInfo, "Fini EVIDENCE_INTERVAL", "severity=status"));
        if( (rc2 = CGWriterEvdInt_Whack(h->wev_int, !drop, NULL)) != 0) {
            LOGERR(klogErr, rc2, "Failed EVIDENCE_INTERVAL release");
            if (!drop) {
                drop = true;
                rc = rc2;
            }
        }
        h->wev_int = NULL;
        h->ev_int = NULL;
        PLOGMSG(klogInfo, (klogInfo, "Fini EVIDENCE_ALIGNMENT", "severity=status"));
        if( (rc2 = CGWriterEvdDnbs_Whack(h->wev_dnb, !drop, NULL)) != 0) {
            LOGERR(klogErr, rc2, "Failed EVIDENCE_ALIGNMENT release");
            if (!drop) {
                drop = true;
                rc = rc2;
            }
        }
        h->wev_dnb = NULL;
        h->ev_dnb = NULL;
        PLOGMSG(klogInfo, (klogInfo, "Fini calculating reference coverage", "severity=status"));
        if( (rc2 = ReferenceMgr_Release
            (h->rmgr, !drop, NULL, drop ? false : true, Quitting)) != 0)
        {
            LOGERR(klogErr, rc2, "Failed calculating reference coverage");
            if (!drop) {
                drop = true;
                rc = rc2;
            }
        }
        h->rmgr = NULL;
        if( rc == 0 )
        {
            KMetadata* meta;
            if( (rc = VDatabaseOpenMetadataUpdate(h->db, &meta)) == 0 ) {
                KMDataNode *node;
                if( (rc = KMetadataOpenNodeUpdate(meta, &node, "/")) == 0 ) {
                    if( (rc = KLoaderMeta_Write(node, p->argv0, __DATE__, "Complete Genomics", KAppVersion())) != 0 ) {
                        LOGERR(klogErr, rc, "Cannot update loader meta");
                    }
                    KMDataNodeRelease(node);
                }
                KMetadataRelease(meta);
            }
        }
        PLOGERR(klogInfo, (klogInfo, rc, "Fini VDatabaseRelease", "severity=status"));
        VDatabaseRelease(h->db);
        h->db = NULL;
        VSchemaRelease(h->schema);
        h->schema = NULL;
        if( drop || rc != 0 ) {
            rc2 = VDBManagerDrop(h->mgr, kptDatabase, "%s", p->out);
            /*if( GetRCState(rc2) == rcNotFound ) {
                // WHAT WOULD BE THE POINT OF RESETTING "rc" TO ZERO?
                rc = 0;
            } else*/ if( rc2 != 0 ) {
                if ( rc == 0 )
                    rc = rc2;
                PLOGERR(klogErr, (klogErr, rc2, "cannot drop db at '$(path)'", PLOG_S(path), p->out));
            }
        }
        VDBManagerRelease(h->mgr);
        h->mgr = NULL;
    }
    return rc;
}

typedef struct FGroupKey_struct {
    CG_EFileType type;
    const CGFIELD_ASSEMBLY_ID_TYPE* assembly_id;
    union {
        struct {
            const CGFIELD_SLIDE_TYPE* slide;
            const CGFIELD_LANE_TYPE* lane;
            const CGFIELD_BATCH_FILE_NUMBER_TYPE* batch_file_number;
        } map;
        struct {
            const CGFIELD_SAMPLE_TYPE* sample;
            const CGFIELD_CHROMOSOME_TYPE* chromosome;
        } asmb;
    } u;
} FGroupKey;

static
rc_t CC FGroupKey_Make(FGroupKey* key,
    const CGLoaderFile* file, const SParam* param)
{
    rc_t rc = 0;
    CG_EFileType ftype;

    if( (rc = CGLoaderFile_GetType(file, &ftype)) == 0 ) {
        switch(ftype) {
            case cg_eFileType_READS:
            case cg_eFileType_MAPPINGS:
            case cg_eFileType_TAG_LFR:
                key->type = cg_eFileType_READS;
                if ((rc = CGLoaderFile_GetAssemblyId(file, &key->assembly_id)) == 0 &&
                    (rc = CGLoaderFile_GetSlide(file, &key->u.map.slide)) == 0 &&
                    (rc = CGLoaderFile_GetLane(file, &key->u.map.lane)) == 0)
                {
                    rc = CGLoaderFile_GetBatchFileNumber(file,
                        &key->u.map.batch_file_number);
                }
                break;
            case cg_eFileType_EVIDENCE_INTERVALS:
            case cg_eFileType_EVIDENCE_DNBS:
                if( param->asm_path != NULL ) {
                    /* do not pick up ASM files if not requested */
                    key->type = cg_eFileType_EVIDENCE_INTERVALS;
                    if( (rc = CGLoaderFile_GetAssemblyId(file, &key->assembly_id)) == 0 &&
                        (rc = CGLoaderFile_GetSample(file, &key->u.asmb.sample)) == 0 ) {
                        rc = CGLoaderFile_GetChromosome(file, &key->u.asmb.chromosome);
                    }
                    break;
                }
            default:
                rc = RC(rcExe, rcQueue, rcInserting, rcItem, rcIgnored);
                break;
        }
    }
    return rc;
}
static
rc_t CC FGroupKey_Validate(const FGroupKey* key)
{
    rc_t rc = 0;
    DEBUG_MSG(5, ("KEY: "));
    switch( key->type ) {
        case cg_eFileType_READS:
            DEBUG_MSG(5, ("%s:%s:%s:%04u", key->assembly_id, key->u.map.slide, key->u.map.lane, key->u.map.batch_file_number));
            if( !key->assembly_id || !key->u.map.slide || !key->u.map.lane || !key->u.map.batch_file_number ) {
                rc = RC(rcExe, rcQueue, rcValidating, rcId, rcIncomplete);
            }
            break;
        case cg_eFileType_EVIDENCE_INTERVALS:
            DEBUG_MSG(5, ("%s:%s:%s", key->assembly_id, key->u.asmb.sample, key->u.asmb.chromosome));
            if( !key->assembly_id || !key->u.asmb.sample || !key->u.asmb.chromosome ) {
                rc = RC(rcExe, rcQueue, rcValidating, rcId, rcIncomplete);
            }
            break;
        default:
            DEBUG_MSG(5, ("CORRUPT!!"));
            rc = RC(rcExe, rcQueue, rcValidating, rcId, rcWrongType);
            break;
    }
    return rc;
}

typedef struct FGroupMAP_struct {
    BSTNode dad;
    FGroupKey key;
    const CGLoaderFile* seq;
    const CGLoaderFile* align;
    const CGLoaderFile* tagLfr;
} FGroupMAP;

static
void FGroupMAP_CloseFiles(FGroupMAP *g)
{
    CGLoaderFile_Close(g->seq);
    CGLoaderFile_Close(g->align);
    CGLoaderFile_Close(g->tagLfr);
}

static void CC FGroupMAP_Whack(BSTNode *node, void *data) {
    FGroupMAP* n = (FGroupMAP*)node;

    CGLoaderFile_Release(n->seq   , false);
    CGLoaderFile_Release(n->align , false);
    CGLoaderFile_Release(n->tagLfr, false);

    free(node);
}

static
int64_t CC FGroupMAP_Cmp( const void *item, const BSTNode *node )
{
    const FGroupKey* i = (const FGroupKey*)item;
    const FGroupMAP* n = (const FGroupMAP*)node;

    int64_t r = strcmp(i->assembly_id, n->key.assembly_id);
    if( r == 0 ) {
        if( i->type == n->key.type ) {
            switch( i->type ) {
                case cg_eFileType_READS:
                    if( (r = strcmp(i->u.map.slide, n->key.u.map.slide)) == 0 ) {
                        if( (r = strcmp(i->u.map.lane, n->key.u.map.lane)) == 0 ) {
                            r = (int64_t)(*(i->u.map.batch_file_number)) - (int64_t)(*(n->key.u.map.batch_file_number));
                        }
                    }
                    break;
                case cg_eFileType_EVIDENCE_INTERVALS:
                    if( (r = strcmp(i->u.asmb.sample, n->key.u.asmb.sample)) == 0 ) {
                        r = strcmp(i->u.asmb.chromosome, n->key.u.asmb.chromosome);
                    }
                    break;
                default:
                    r = -1;
                    break;
            }
        } else {
            r = -1;
        }
    }
    return r;
}

static
int64_t CC FGroupMAP_Sort( const BSTNode *item, const BSTNode *n )
{
    return FGroupMAP_Cmp(&((const FGroupMAP*)item)->key, n);
}

typedef struct FGroupMAP_FindData_struct {
    FGroupKey key;
    int64_t rowid;
} FGroupMAP_FindData;

static
bool CC FGroupMAP_FindRowId( BSTNode *node, void *data )
{
    FGroupMAP_FindData* d = (FGroupMAP_FindData*)data;
    const FGroupMAP* n = (const FGroupMAP*)node;

    if( FGroupMAP_Cmp(&d->key, node) == 0 ) {
        if( CGLoaderFile_GetStartRow(n->seq, &d->rowid) == 0 ) {
            return true;
        }
    }
    return false;
}

static
rc_t CC FGroupMAP_Set(FGroupMAP* g, const CGLoaderFile* file)
{
    rc_t rc = 0;
    CG_EFileType ftype;

    if( (rc = CGLoaderFile_GetType(file, &ftype)) == 0 ) {
        if( g->key.type == cg_eFileType_READS ) {
            if( ftype == cg_eFileType_READS ) {
                if( g->seq == NULL ) {
                    g->seq = file;
                } else {
                    rc = RC(rcExe, rcQueue, rcInserting, rcItem, rcDuplicate);
                }
            } else if( ftype == cg_eFileType_MAPPINGS ) {
                if( g->align == NULL ) {
                    g->align = file;
                } else {
                    rc = RC(rcExe, rcQueue, rcInserting, rcItem, rcDuplicate);
                }
            }
            else if (ftype == cg_eFileType_TAG_LFR) {
                if (g->tagLfr == NULL) {
                    g->tagLfr = file;
                } else {
                    rc = RC(rcExe, rcQueue, rcInserting, rcItem, rcDuplicate);
                }
            } else {
                rc = RC(rcExe, rcQueue, rcInserting, rcItem, rcWrongType);
            }
        } else if( g->key.type == cg_eFileType_EVIDENCE_INTERVALS ) {
            if( ftype == cg_eFileType_EVIDENCE_INTERVALS ) {
                if( g->seq != NULL ) {
                    rc = RC(rcExe, rcQueue, rcInserting, rcItem, rcDuplicate);
                } else {
                    g->seq = file;
                }
            } else if( ftype == cg_eFileType_EVIDENCE_DNBS ) {
                if( g->align == NULL ) {
                    g->align = file;
                } else {
                    rc = RC(rcExe, rcQueue, rcInserting, rcItem, rcDuplicate);
                }
            } else {
                rc = RC(rcExe, rcQueue, rcInserting, rcItem, rcWrongType);
            }
        } else {
            rc = RC(rcExe, rcQueue, rcInserting, rcType, rcUnexpected);
        }
    }
    return rc;
}

static
void CC FGroupMAP_Validate( BSTNode *n, void *data )
{
    const FGroupMAP* g = (const FGroupMAP*)n;
    rc_t rc = 0, *rc_out = (rc_t*)data;
    const char* rnm = NULL, *mnm = NULL;

    rc = FGroupKey_Validate(&g->key);
    if( g->seq != NULL ) {
        CGLoaderFile_Filename(g->seq, &rnm);
        rnm = rnm ? strrchr(rnm, '/') : rnm;
        DEBUG_MSG(5, (" READS(%s)", rnm));
    }
    if( g->align ) {
        CGLoaderFile_Filename(g->align, &mnm);
        mnm = mnm ? strrchr(mnm, '/') : mnm;
        DEBUG_MSG(5, (" MAPPINGS(%s)", mnm));
    }
    if (g->tagLfr != NULL) {
        CGLoaderFile_Filename(g->tagLfr, &mnm);
        mnm = mnm ? strrchr(mnm, '/') : mnm;
        DEBUG_MSG(5, (" TAG_LFR(%s)", mnm));
    }
    DEBUG_MSG(5, ("\n"));
    if( rc == 0 && g->seq == NULL ) {
        rc = RC(rcExe, rcQueue, rcValidating, rcItem, rcIncomplete);
    }

    /* THIS USED TO WIPE OUT THE "rc" ON EACH ENTRY */
    if( rc != 0)  {
        PLOGERR(klogErr, (klogErr, rc,  "file pair $(f1)[mandatory], $(f2)[optional]", PLOG_2(PLOG_S(f1),PLOG_S(f2)), rnm, mnm));
        if ( * rc_out == 0 )
            *rc_out = rc;
#if 0
    } else {
        *rc_out = RC(0, 0, 0, 0, 0);
#endif
    }
}

typedef struct DirVisit_Data_Struct {
    const SParam* param;
    BSTree* tree;
    const KDirectory* dir;
    uint32_t format_version;
} DirVisit_Data;

/* you cannot addref to this dir object cause it's created on stack silently */
static
rc_t CC DirVisitor(const KDirectory *dir, uint32_t type, const char *name, void *data)
{
    rc_t rc = 0;
    DirVisit_Data* d = (DirVisit_Data*)data;

    assert(d);

    if( (type & ~kptAlias) == kptFile ) {
        if (strcmp(&name[strlen(name) - 4], ".tsv") == 0 ||
            strcmp(&name[strlen(name) - 8], ".tsv.bz2") == 0 ||
            strcmp(&name[strlen(name) - 7], ".tsv.gz") == 0)
        {
            char buf[4096] = "";
            const CGLoaderFile* file = NULL;
            FGroupKey key;
            if( (rc = KDirectoryResolvePath(dir, true, buf, sizeof(buf), "%s", name)) == 0 &&
                (rc = CGLoaderFile_Make(&file, d->dir, buf, NULL, !d->param->no_read_ahead)) == 0 &&
                (rc = FGroupKey_Make(&key, file, d->param)) == 0 )
            {
                FGroupMAP* found = (FGroupMAP*)BSTreeFind(d->tree, &key, FGroupMAP_Cmp);

                assert(file && file->cg_file);
                if (file->cg_file->type == cg_eFileType_READS ||
                    file->cg_file->type == cg_eFileType_MAPPINGS)
                {
                    if (d->format_version == 0) {
                        d->format_version = file->cg_file->format_version;
                    }
                    else if (d->format_version != file->cg_file->format_version)
                    {
                        rc = RC(rcExe, rcFile, rcInserting,
                            rcData, rcInconsistent);
                        PLOGERR(klogErr, (klogErr, rc, "READS or MAPPINGS "
                            "format_version mismatch: $(o) vs. $(n)",
                            "o=%d,n=%d",
                            d->format_version, file->cg_file->format_version));
                    }
                }

                if (rc == 0) {
                    if (d->param != NULL && ! d->param->load_other_evidence &&
                        strstr(buf, "/EVIDENCE") != NULL &&
                        strstr(buf, "/EVIDENCE/") == NULL)
                    {
                        DEBUG_MSG(5, ("file %s recognized as %s: ignored\n",
                            name, buf));
                        rc = CGLoaderFile_Release(file, true);
                        file = NULL;
                    }
                    else {
                        DEBUG_MSG(5, ("file %s recognized as %s\n", name, buf));
                        if (found != NULL) {
                            rc = FGroupMAP_Set(found, file);
                        }
                        else {
                            FGroupMAP* x = calloc(1, sizeof(*x));
                            if (x == NULL) {
                                rc = RC(rcExe,
                                    rcFile, rcInserting, rcMemory, rcExhausted);
                            }
                            else {
                                memmove(&x->key, &key, sizeof(key));
                                if ((rc = FGroupMAP_Set(x, file)) == 0) {
                                    rc = BSTreeInsertUnique(d->tree,
                                        &x->dad, NULL, FGroupMAP_Sort);
                                }
                            }
                        }
                    }
                }
            } else if( GetRCObject(rc) == rcItem && GetRCState(rc) == rcIgnored ) {
                DEBUG_MSG(5, ("file %s ignored\n", name));
                rc = CGLoaderFile_Release(file, true);
                file = NULL;
            }
            if( rc != 0 && file != NULL ) {
                CGLoaderFile_LOG(file, klogErr, rc, NULL, NULL);
                CGLoaderFile_Release(file, true);
            }
        } else if( strcmp(&name[strlen(name) - 4], ".tar") == 0 ) {
            const KDirectory* tmp = d->dir;
            if( (rc = KDirectoryOpenArcDirRead(dir, &d->dir, true, name, tocKFile, KArcParseTAR, NULL, NULL)) == 0 ) {
                rc = KDirectoryVisit(d->dir, true, DirVisitor, d, ".");
                KDirectoryRelease(d->dir);
            }
            d->dir = tmp;
        }
    }
    return rc;
}

typedef struct FGroupMAP_LoadData_struct {
    rc_t rc;
    const SParam* param;
    DB_Handle db;
    const BSTree* reads;
} FGroupMAP_LoadData;

typedef enum {
    eCtxRead,
    eCtxLfr,
    eCtxMapping
} TCtx;
static bool _FGroupMAPDone(FGroupMAP *self, TCtx ctx, FGroupMAP_LoadData* d) {
    /* (rcData rcDone) is always set on reads file EOF */
    bool eofLfr = true;
    bool eofMapping = true;
    assert(self && d);
    if (d->rc == 0 ||
        GetRCState(d->rc) != rcDone || GetRCObject(d->rc) != (enum RCObject)rcData)
    {
        return false;
    }
    d->rc = 0;
    if (d->rc == 0 && self->tagLfr != NULL) {
        d->rc = CGLoaderFile_IsEof(self->tagLfr, &eofLfr);
    }
    if (d->rc == 0 && self->align != NULL) {
        d->rc = CGLoaderFile_IsEof(self->align, &eofMapping);
    }
    if (d->rc == 0) {
        switch (ctx) {
            case eCtxRead:
                if (!eofLfr) {
                    /* not EOF */
                    d->rc = RC(rcExe, rcFile, rcReading, rcData, rcUnexpected);
                    CGLoaderFile_LOG(self->align, klogErr, d->rc,
                        "extra tag LFRs, possible that corresponding "
                        "reads file is truncated", NULL);
                }
                else if (!eofMapping) {
                    /* not EOF */
                    d->rc = RC(rcExe, rcFile, rcReading, rcData, rcUnexpected);
                    CGLoaderFile_LOG(self->align, klogErr, d->rc,
                        "extra mappings, possible that corresponding "
                        "reads file is truncated", NULL);
                }
                break;
            case eCtxLfr:
            case eCtxMapping:
                d->rc = RC(rcExe, rcFile, rcReading, rcCondition, rcInvalid);
                break;
            default:
                assert(0);
                break;
        }
    }
    if (d->rc == 0) {
        /* mappings and lfr file EOF detected ok */
        DEBUG_MSG(5, (" done\n", FGroupKey_Validate(&self->key)));
    }
    return true;
}

bool CC FGroupMAP_LoadReads( BSTNode *node, void *data )
{
    TCtx ctx = eCtxRead;
    FGroupMAP* n = (FGroupMAP*)node;
    FGroupMAP_LoadData* d = (FGroupMAP_LoadData*)data;

    bool done = false;

    DEBUG_MSG(5, (" started\n", FGroupKey_Validate(&n->key)));
    while (!done && d->rc == 0) {
        ctx = eCtxRead;
        d->rc = CGLoaderFile_GetRead(n->seq, d->db.reads);
        if (d->rc == 0 && n->tagLfr != NULL) {
            ctx = eCtxLfr;
            d->rc = CGLoaderFile_GetTagLfr(n->tagLfr, d->db.reads);
        }
        if (d->rc == 0) {
            if ((d->db.reads->flags
                   & (cg_eLeftHalfDnbNoMatches | cg_eLeftHalfDnbMapOverflow))
                &&
                (d->db.reads->flags
                   & (cg_eRightHalfDnbNoMatches | cg_eRightHalfDnbMapOverflow)))
            {
                d->db.mappings->map_qty = 0;
            } else {
                ctx = eCtxMapping;
                d->rc = CGLoaderFile_GetMapping(n->align, d->db.mappings);
            }
/* alignment written 1st than sequence -> primary_alignment_id must be set!! */
            if (d->rc == 0 &&
                (d->rc = CGWriterAlgn_Write(d->db.walgn, d->db.reads)) == 0)
            {
                d->rc = CGWriterSeq_Write(d->db.wseq);
            }
        }
        done = _FGroupMAPDone(n, ctx, d);
        d->rc = d->rc ? d->rc : Quitting();
    }
    if( d->rc != 0 ) {
        CGLoaderFile_LOG(n->seq, klogErr, d->rc, NULL, NULL);
        CGLoaderFile_LOG(n->align, klogErr, d->rc, NULL, NULL);
    }

    FGroupMAP_CloseFiles(n);
    return d->rc != 0;
}

bool CC FGroupMAP_LoadEvidence( BSTNode *node, void *data )
{
    FGroupMAP* n = (FGroupMAP*)node;
    FGroupMAP_LoadData* d = (FGroupMAP_LoadData*)data;

    DEBUG_MSG(5, ("' started\n", FGroupKey_Validate(&n->key)));
     while( d->rc == 0 ) {
        if( (d->rc = CGLoaderFile_GetEvidenceIntervals(n->seq, d->db.ev_int)) == 0 ) {
            int64_t evint_rowid;
            if( n->align != NULL ) {
                d->rc = CGLoaderFile_GetEvidenceDnbs(n->align, d->db.ev_int->interval_id, d->db.ev_dnb);
            } else {
		d->db.ev_dnb->qty = 0; /***weird, but the easiest place to fix ***/
	    }
            /* interval written 1st than dnbs which uses interval as reference */
            if( d->rc == 0 ) {
                d->rc = CGWriterEvdInt_Write(d->db.wev_int, d->db.ev_dnb, &evint_rowid);
            }
            if( d->rc == 0 && n->align != NULL ) {
                /* attach dnbs to reads */
                uint16_t i;
                FGroupMAP_FindData found;

                found.key.type = cg_eFileType_READS;
                d->rc = CGLoaderFile_GetAssemblyId(n->align, &found.key.assembly_id);
                for(i = 0; d->rc == 0 && i < d->db.ev_dnb->qty; i++) {
                    found.key.u.map.slide = d->db.ev_dnb->dnbs[i].slide;
                    found.key.u.map.lane = d->db.ev_dnb->dnbs[i].lane;
                    found.key.u.map.batch_file_number = &d->db.ev_dnb->dnbs[i].file_num_in_lane;
                    if( BSTreeDoUntil(d->reads, false, FGroupMAP_FindRowId, &found) ) {
                        d->rc = CGWriterEvdDnbs_SetSEQ(d->db.wev_dnb, i, found.rowid);
                    } else {
                        d->rc = RC(rcExe, rcFile, rcWriting, rcData, rcInconsistent);
                    }
                }
            }
            if( d->rc == 0 && n->align != NULL ) {
                d->rc = CGWriterEvdDnbs_Write(d->db.wev_dnb, d->db.ev_int, evint_rowid);
            }
        }
        if( GetRCState(d->rc) == rcDone && GetRCObject(d->rc) == (enum RCObject)rcData ) {
            bool eof = false;
            d->rc = 0;
            if( n->align == NULL || ((d->rc = CGLoaderFile_IsEof(n->align, &eof)) == 0 && eof) ) {
                /* dnbs file EOF detected ok */
                DEBUG_MSG(5, ("' done\n", FGroupKey_Validate(&n->key)));
                break;
            } else if( d->rc == 0 ) {
                /* not EOF */
                d->rc = RC(rcExe, rcFile, rcReading, rcData, rcUnexpected);
                CGLoaderFile_LOG(n->align, klogErr, d->rc,
                    "extra dnbs, possible that corresponding intervals file is truncated", NULL);
            }
        }
        d->rc = d->rc ? d->rc : Quitting();
    }
    if( d->rc != 0 ) {
        CGLoaderFile_LOG(n->seq, klogErr, d->rc, NULL, NULL);
        CGLoaderFile_LOG(n->align, klogErr, d->rc, NULL, NULL);
    }
    FGroupMAP_CloseFiles(n);
    return d->rc != 0;
}


static const char * lib_dst = "extra/library";

static rc_t copy_library( const KDirectory * src_dir, KDirectory * dst_dir,
                          const char * in_path, const char * out_path )
{
    rc_t rc;
    size_t l = string_size( out_path ) + string_size( lib_dst ) + 2;
    char * dst = malloc( l );
    if ( dst == NULL )
        rc = RC( rcExe, rcFile, rcCopying, rcMemory, rcExhausted );
    else
    {
        size_t written;
        rc = string_printf( dst, l, &written, "%s/%s", out_path, lib_dst );
        if ( rc == 0 )
            rc = KDirectoryCopy( src_dir, dst_dir, true, in_path, dst );
        free( dst );
    }
    return rc;
}


static rc_t open_dir_or_tar( const KDirectory *dir, const KDirectory **sub, const char * name )
{
    rc_t rc1, rc2;
    rc1 = KDirectoryOpenDirRead( dir, sub, true, "%s", name );
    if ( rc1 != 0 )
    {
        rc2 = KDirectoryOpenTarArchiveRead( dir, sub, true, "%s", name );
        if ( rc2 == 0 )
            rc1 = rc2;
    }
    return rc1;
}

static rc_t Load( SParam* param )
{
    rc_t rc = 0, rc1 = 0;
    BSTree slides, evidence;


    param->map_dir = NULL;
    param->asm_dir = NULL;
    param->output_dir = ( KDirectory * )param->input_dir;
    BSTreeInit( &slides );
    BSTreeInit( &evidence );

    rc = open_dir_or_tar( param->input_dir, &param->map_dir, param->map_path );
    if ( rc == 0 )
    {
        DirVisit_Data dv;

        dv.param = param;
        dv.tree = &slides;
        dv.dir = param->map_dir;
        dv.format_version = 0;
        rc = KDirectoryVisit( param->map_dir, true, DirVisitor, &dv, NULL );
        if ( rc == 0 )
        {
            if ( param->asm_path != NULL )
            {
                rc_t rc2 = open_dir_or_tar( param->input_dir, &param->asm_dir, param->asm_path );
                if ( rc2 == 0 )
                {
                    dv.tree = &evidence;
                    dv.dir = param->asm_dir;
                    rc = KDirectoryVisit( param->asm_dir, true, DirVisitor, &dv, NULL );
                }
            }

            if (rc == 0) {
                param->read_len = get_cg_read_len(dv.format_version);
            }

            if ( rc == 0 )
            {
                /* SHOULD HAVE A BSTreeEmpty FUNCTION OR SOMETHING...
                   MAKE ONE HERE - WITH KNOWLEDGE THAT TREE IS NOT NULL: */
#ifndef BSTreeEmpty
#define BSTreeEmpty( bst ) \
    ( ( bst ) -> root == NULL )
#endif
                if ( BSTreeEmpty ( & slides ) && BSTreeEmpty ( & evidence ) )
                    rc = RC( rcExe, rcFile, rcReading, rcData, rcInsufficient );
                else
                {
                    /* CORRECTED SETTING OF "rc" IN "FGroupMAP_Validate" */
                    assert ( rc == 0 );
                    BSTreeForEach( &slides, false, FGroupMAP_Validate, &rc );
                    BSTreeForEach( &evidence, false, FGroupMAP_Validate, &rc );
                }

                if ( rc == 0 )
                {
                    FGroupMAP_LoadData data;

                    PLOGMSG( klogInfo, ( klogInfo, "file set validation complete", "severity=status" ) );
                    memset( &data, 0, sizeof( data ) );
                    data.rc = 0;
                    data.param = param;
                    data.reads = &slides;
                    rc = DB_Init( param, &data.db );
                    if ( rc == 0 )
                    {
                        BSTreeDoUntil( &slides, false, FGroupMAP_LoadReads, &data );
                        rc = data.rc;
                        if ( rc == 0 )
                        {
                            PLOGMSG( klogInfo, ( klogInfo, "MAP loaded", "severity=status" ) );
                            BSTreeDoUntil( &evidence, false, FGroupMAP_LoadEvidence, &data );
                            rc = data.rc;
                            if ( rc == 0 )
                                PLOGMSG( klogInfo, ( klogInfo, "ASM loaded", "severity=status" ) );
                        }
                    }
                    rc1 = DB_Fini( param, &data.db, rc != 0 );
                    if ( rc == 0 )
                        rc = rc1;
                }
            }
        }

        /* copy the extra library ( file or recursive directory ) */
        if ( rc == 0 && param->library != NULL )
        {
            const KDirectory *lib_src;
            rc = open_dir_or_tar( param->input_dir, &lib_src, param->library );
            if ( rc == 0 )
            {
                rc = copy_library( param->input_dir, param->output_dir,
                                   param->library, param->out );
                if ( rc == 0 )
                    STSMSG( 0, ( "extra lib copied" ) );
                else
                    LOGERR( klogErr, rc, "failed to copy extra library" );
                KDirectoryRelease( lib_src );
            }
/*
            else
            {
                rc = copy_library( param->input_dir, param->output_dir,
                                   ".", param->out );
                if ( rc == 0 )
                    STSMSG( 0, ( "extra lib copied" ) );
                else
                    LOGERR( klogErr, rc, "failed to copy extra library" );
            }
*/
        }
        KDirectoryRelease( param->map_dir );
        KDirectoryRelease( param->asm_dir );
    }
    BSTreeWhack( &slides, FGroupMAP_Whack, NULL );
    BSTreeWhack( &evidence, FGroupMAP_Whack, NULL );
    return rc;
}

const char* map_usage[] = {"MAP input directory path containing files", NULL};
const char* asm_usage[] = {"ASM input directory path containing files", NULL};
const char* load_extra_evidence_usage[]
    = { "load extra evidence files", NULL};
const char* schema_usage[] = {"database schema file name", NULL};
const char* output_usage[] = {"output database path", NULL};
const char* force_usage[] = {"force output overwrite", NULL};
const char* refseqcfg_usage[] = {"path to a file with reference-to-accession list", NULL};
const char* refseqpath_usage[] = {"path to a directory with reference sequences in fasta", NULL};
const char* refseqchunk_usage[] = {NULL, "hidden param to override REFERENCE table seq chunk size for testing", NULL};
const char* reffile_usage[] = {"path to fasta file with references", NULL};
const char* forceref_usage[] = {"force reference sequence write", NULL};
const char* forceread_usage[] = {"force reads write", NULL};
const char* qualquant_usage[] = {"quality scores quantization level, can be a number (0: none, 1: 2bit, 2: 1bit), or a string like '1:10,10:20,20:30,30:-' (which is equivalent to 1)", NULL};
const char* nospotgroup_usage[] = {"do not write source file key to SPOT_GROUP column", NULL};
const char* min_mapq_usage[] = {"filter secondary mappings by minimum weight (phred)", NULL};
const char* no_secondary_usage[] = {"preserve only one mapping per half-DNB based on weight", NULL};
const char* single_mate_usage[] = {"if secondary mates have duplicates preserve only one in each pair based on weight", NULL};
const char* cluster_size_usage[] = {"defines cluster window on the reference, records only 1 placement from given cluster size; default is zero which means ignore", NULL};
const char* no_read_ahead_usage[] = {"disable input files threaded caching", NULL};
const char* library_usage[] = {"copy extra file/directory into output", NULL};

/* this enum must have same order as MainArgs array below */
enum OptDefIndex {
    eopt_MapInput = 0,
    eopt_AsmInput,
    eopt_LoadExtraEvidence,
    eopt_Schema,
    eopt_RefSeqConfig,
    eopt_RefSeqPath,
    eopt_RefSeqChunk,
    eopt_RefFile,
    eopt_Output,
    eopt_Force,
    eopt_ForceRef,
    eopt_ForceRead,
    eopt_QualQuantization,
    eopt_NoSpotGroup,
    eopt_MinMapQ,
    eopt_No2nd_ary,
    eopt_SingleMate,
    eopt_ClusterSize,
    eopt_noReadAhead,
    eopt_Library
};

OptDef MainArgs[] =
{
    /* if you change order in this array, rearrange enum above accordingly! */
    { "map",              "m",  NULL, map_usage,            1, true,  true  },
    { "asm",              "a",  NULL, asm_usage,            1, true,  false },
    { "load-extra-evidence", NULL, NULL,
                                 load_extra_evidence_usage, 1, false, false },
    { "schema",           "s",  NULL, schema_usage,         1, true,  false },
    { "refseq-config",    "k",  NULL, refseqcfg_usage,      1, true,  false },
    { "refseq-path",      "i",  NULL, refseqpath_usage,     1, true,  false },
    { "refseq-chunk",     "C",  NULL, refseqchunk_usage,    1, true,  false },
    { "ref-file",         "r",  NULL, reffile_usage,        1, true,  false },
    { "output",           "o",  NULL, output_usage,         1, true,  true  },
    { "force",            "f",  NULL, force_usage,          1, false, false },
    { "write-reference",  "g",  NULL, forceref_usage,       1, false, false },
    { "write-read",       "w",  NULL, forceread_usage,      1, false, false },
    { "qual-quant",       "Q",  NULL, qualquant_usage,      1, true,  false },
    { "no-spotgroup",     "G",  NULL, nospotgroup_usage,    1, false, false },
    { "min-mapq",         "q",  NULL, min_mapq_usage,       1, true,  false },
    { "no-secondary",     "P",  NULL, no_secondary_usage,   1, false, false },
    { "single-mate",      NULL, NULL, single_mate_usage,    1, false, false },
    { "cluster-size",     NULL, NULL, cluster_size_usage,   1, true,  false },
    { "input-no-threads", "t",  NULL, no_read_ahead_usage,  1, false, false },
    { "library",          "l",  NULL, library_usage,        1, true,  false }
};
const size_t MainArgsQty = sizeof(MainArgs) / sizeof(MainArgs[0]);

rc_t UsageSummary (char const * progname)
{
    OUTMSG((
        "Usage:\n"
        "\t%s [options] -m map-dir -o path-to-run\n"
        "\n"
        "Summary:\n"
        "\tLoad a Complete Genomics formatted data files\n"
        "\n"
        "Example:\n"
        "\t%s -m build36/MAP -o /tmp/SRZ123456\n"
        "\n"
        ,progname, progname));
    return 0;
}

char const UsageDefaultName[] = "cg-load";

rc_t CC Usage( const Args* args )
{
    rc_t rc;
    int i;
    const char* progname = UsageDefaultName;
    const char* fullname = UsageDefaultName;

    rc = ArgsProgram(args, &fullname, &progname);

    UsageSummary(progname);

    for(i = 0; i < MainArgsQty; i++ ) {
        if( MainArgs[i].required && MainArgs[i].help[0] != NULL ) {
            HelpOptionLine(MainArgs[i].aliases, MainArgs[i].name, NULL, MainArgs[i].help);
        }
    }
    OUTMSG(("\nOptions:\n"));
    for(i = 0; i < MainArgsQty; i++ ) {
        if( !MainArgs[i].required && MainArgs[i].help[0] != NULL ) {
            HelpOptionLine(MainArgs[i].aliases, MainArgs[i].name, NULL, MainArgs[i].help);
        }
    }
    XMLLogger_Usage();
    OUTMSG(("\n"));
    HelpOptionsStandard();
    HelpVersion(fullname, KAppVersion());
    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    rc_t rc = 0;
    Args* args = NULL;
    const char* errmsg = NULL, *refseq_chunk = NULL, *min_mapq = NULL, *cluster_size = NULL;
    const XMLLogger* xml_logger = NULL;
    SParam params;
    memset(&params, 0, sizeof(params));
    params.schema = "align/align.vschema";

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    params.argv0 = argv[0];

    if( (rc = ArgsMakeAndHandle(&args, argc, argv, 2, MainArgs, MainArgsQty, XMLLogger_Args, XMLLogger_ArgsQty)) == 0 ) {
        uint32_t count;
        if( (rc = ArgsParamCount (args, &count)) != 0 || count != 0 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            ArgsParamValue(args, 0, (const void **)&errmsg);

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_Output].name, &count)) != 0 || count != 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, count ? rcExcessive : rcInsufficient);
            errmsg = MainArgs[eopt_Output].name;
        } else if( (rc = ArgsOptionValue(args, MainArgs[eopt_Output].name, 0, (const void **)&params.out)) != 0 ) {
            errmsg = MainArgs[eopt_Output].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_MapInput].name, &count)) != 0 || count != 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, count ? rcExcessive : rcInsufficient);
            errmsg = MainArgs[eopt_MapInput].name;
        } else if( (rc = ArgsOptionValue(args, MainArgs[eopt_MapInput].name, 0, (const void **)&params.map_path)) != 0 ) {
            errmsg = MainArgs[eopt_MapInput].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_Schema].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, count ? rcExcessive : rcInsufficient);
            errmsg = MainArgs[eopt_Schema].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_Schema].name, 0, (const void **)&params.schema)) != 0 ) {
            errmsg = MainArgs[eopt_Schema].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_AsmInput].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            errmsg = MainArgs[eopt_AsmInput].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_AsmInput].name, 0, (const void **)&params.asm_path)) != 0 ) {
            errmsg = MainArgs[eopt_AsmInput].name;

        }
        else if ((rc = ArgsOptionCount(args,
            MainArgs[eopt_LoadExtraEvidence].name,
            &params.load_other_evidence)) != 0 )
        {
            errmsg = MainArgs[eopt_LoadExtraEvidence].name;

        }
        else if( (rc = ArgsOptionCount(args, MainArgs[eopt_RefSeqConfig].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            errmsg = MainArgs[eopt_RefSeqConfig].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_RefSeqConfig].name, 0, (const void **)&params.refseqcfg)) != 0 ) {
            errmsg = MainArgs[eopt_RefSeqConfig].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_RefSeqPath].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            errmsg = MainArgs[eopt_RefSeqPath].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_RefSeqPath].name, 0, (const void **)&params.refseqpath)) != 0 ) {
            errmsg = MainArgs[eopt_RefSeqPath].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_Library].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            errmsg = MainArgs[eopt_Library].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_Library].name, 0, (const void **)&params.library)) != 0 ) {
            errmsg = MainArgs[eopt_Library].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_RefSeqChunk].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            errmsg = MainArgs[eopt_RefSeqChunk].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_RefSeqChunk].name, 0, (const void **)&refseq_chunk)) != 0 ) {
            errmsg = MainArgs[eopt_RefSeqChunk].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_Force].name, &params.force)) != 0 ) {
            errmsg = MainArgs[eopt_Force].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_ForceRef].name, &params.force_refw)) != 0 ) {
            errmsg = MainArgs[eopt_ForceRef].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_ForceRead].name, &params.force_readw)) != 0 ) {
            errmsg = MainArgs[eopt_ForceRead].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_noReadAhead].name, &params.no_read_ahead)) != 0 ) {
            errmsg = MainArgs[eopt_noReadAhead].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_NoSpotGroup].name, &params.no_spot_group)) != 0 ) {
            errmsg = MainArgs[eopt_NoSpotGroup].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_No2nd_ary].name, &params.min_mapq)) != 0 ) {
            errmsg = MainArgs[eopt_No2nd_ary].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_QualQuantization].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            errmsg = MainArgs[eopt_QualQuantization].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_QualQuantization].name, 0, (const void **)&params.qual_quant)) != 0 ) {
            errmsg = MainArgs[eopt_QualQuantization].name;

        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_MinMapQ].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            errmsg = MainArgs[eopt_MinMapQ].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_MinMapQ].name, 0, (const void **)&min_mapq)) != 0 ) {
            errmsg = MainArgs[eopt_MinMapQ].name;
        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_ClusterSize].name, &count)) != 0 || count > 1 ) {
            rc = rc ? rc : RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
            errmsg = MainArgs[eopt_ClusterSize].name;
        } else if( count > 0 && (rc = ArgsOptionValue(args, MainArgs[eopt_ClusterSize].name, 0, (const void **)&cluster_size)) != 0 ) {
            errmsg = MainArgs[eopt_ClusterSize].name;
        } else if( (rc = ArgsOptionCount(args, MainArgs[eopt_SingleMate].name, &params.single_mate)) != 0 ) {
            errmsg = MainArgs[eopt_SingleMate].name;

        } else {
            do {
                long val = 0;
                char* end = NULL;
                uint32_t count;

                if( (rc = ArgsOptionCount(args, MainArgs[eopt_RefFile].name, &count)) != 0 ) {
                    break;
                } else {
                    params.refFiles = calloc(count + 1, sizeof(*(params.refFiles)));
                    if( params.refFiles == NULL ) {
                        rc = RC(rcApp, rcArgv, rcReading, rcMemory, rcExhausted);
                        break;
                    }
                    while(rc == 0 && count-- > 0) {
                        rc = ArgsOptionValue(args, MainArgs[eopt_RefFile].name, count, (const void **)&params.refFiles[count]);
                    }
                }

                if( refseq_chunk != NULL ) {
                    errno = 0;
                    val = strtol(refseq_chunk, &end, 10);
                    if( errno != 0 || refseq_chunk == end || *end != '\0' || val < 0 || val > UINT32_MAX ) {
                        rc = RC(rcExe, rcArgv, rcReading, rcParam, rcInvalid);
                        break;
                    }
                    params.refseq_chunk = val;
                }
                if( params.min_mapq > 0 ) {
                    params.min_mapq = ~0;
                } else if( min_mapq != NULL ) {
                    errno = 0;
                    val = strtol(min_mapq, &end, 10);
                    if( errno != 0 || min_mapq == end || *end != '\0' || val < 0 || val > 255 ) {
                        rc = RC(rcExe, rcArgv, rcReading, rcParam, rcInvalid);
                        break;
                    }
                    params.min_mapq = val;
                }

                if ( cluster_size )
                    params.cluster_size = atoi( cluster_size );
                else
                    params.cluster_size = 0;

                rc = KDirectoryNativeDir( &params.input_dir );
                if ( rc != 0 )
                    errmsg = "current directory";
                else
                {
                    rc = XMLLogger_Make( &xml_logger, params.input_dir, args );
                    if ( rc != 0 )
                        errmsg = "XML logging";
                    else
                        rc = Load( &params );
                }
            } while( false );
            KDirectoryRelease( params.input_dir );

            free(params.refFiles);
            params.refFiles = NULL;
        }
    }
    /* find accession as last part of path for internal XML logging */
    refseq_chunk = params.out ? strrchr(params.out, '/') : "/???";
    if( refseq_chunk ++ == NULL )
        refseq_chunk = params.out;

    if( argc < 2 )
        MiniUsage(args);
    else if( rc != 0 )
    {
        if( errmsg )
        {
            MiniUsage(args);
            LOGERR(klogErr, rc, errmsg);
        }
        else
        {
            PLOGERR(klogErr, (klogErr, rc, "load failed: $(accession)",
                   "severity=total,status=failure,accession=%s", refseq_chunk));
        }
    }
    else
    {
        PLOGMSG(klogInfo, (klogInfo, "loaded",
                "severity=total,status=success,accession=%s", refseq_chunk));
    }
    ArgsWhack(args);
    XMLLogger_Release(xml_logger);
    return VDB_TERMINATE( rc );
}
