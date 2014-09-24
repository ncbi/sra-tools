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

#include "csra-trim.vers.h"

#include <vdb/manager.h> /* VDBManager */
#include <vdb/database.h> /* VDatabase */
#include <vdb/table.h> /* VTable */
#include <vdb/schema.h> /* VSchema */
#include <vdb/cursor.h> /* VCursor */

#include <kdb/meta.h>

#include <kapp/main.h>

#include <klib/out.h> /* OUTMSG */
#include <klib/log.h> /* (void)LOGERR */
#include <klib/debug.h> /* DBGMSG */
#include <klib/rc.h> /* RC */

#include <assert.h>
#include <stdlib.h> /* free */
#include <string.h> /* strcmp */

struct Params {
    char const *dbPath;
    char const *output;
    bool exclude_secondary;
} static Params;

typedef struct ColumnInfo_s {
    char const *col;
    char const *src;
    int group;
} ColumnInfo_t;

static ColumnInfo_t const priCol[] = {
    /* these columns are independent and are physical copies */
    { "SEQ_READ_ID"     , NULL, 0 },
    { "GLOBAL_REF_START", NULL, 0 },
    { "REF_ORIENTATION" , NULL, 0 },
    { "MAPQ"            , NULL, 0 },

    /* these columns are pulled in from SEQUENCE */
    { "SEQ_SPOT_ID"  , NULL, 1 },
    { "SPOT_GROUP"   , NULL, 1 },
    { "MATE_ALIGN_ID", NULL, 1 },
    { "READ_FILTER"  , NULL, 1 },

    /* these columns all rely on clipping */
    { "(bool)HAS_REF_OFFSET", "(bool)CLIPPED_HAS_REF_OFFSET", 2 },
    { "(bool)HAS_MISMATCH"  , "(bool)CLIPPED_HAS_MISMATCH"  , 2 },
    { "REF_OFFSET"          , "CLIPPED_REF_OFFSET"          , 2 },
    { "MISMATCH"            , "CLIPPED_MISMATCH"            , 2 },
    { "CMP_QUALITY"         , "CMP_QUALITY"                 , 2 },
    
    { NULL, NULL, 0 }
};

static ColumnInfo_t const secCol[] = {
    /* these columns are independent and are physical copies */
    { "SEQ_READ_ID"         , NULL, 0 },
    { "GLOBAL_REF_START"    , NULL, 0 },
    { "REF_ORIENTATION"     , NULL, 0 },
    { "MAPQ"                , NULL, 0 },
    { "PRIMARY_ALIGNMENT_ID", NULL, 0 },
    
    /* these columns are pulled in from SEQUENCE */
    { "SEQ_SPOT_ID"  , NULL, 1 },
    { "SPOT_GROUP"   , NULL, 1 },
    { "MATE_ALIGN_ID", NULL, 1 },
    { "READ_FILTER"  , NULL, 1 },
    
    /* these columns all rely on clipping */
    { "(bool)HAS_REF_OFFSET", "(bool)CLIPPED_HAS_REF_OFFSET", 2 },
    { "(bool)HAS_MISMATCH"  , "(bool)CLIPPED_HAS_MISMATCH"  , 2 },
    { "REF_OFFSET"          , "CLIPPED_REF_OFFSET"          , 2 },
    { "MISMATCH"            , "CLIPPED_MISMATCH"            , 2 },
    { "CMP_QUALITY"         , "CMP_QUALITY"                 , 2 },
    
    { NULL, NULL, 0 }
};


static ColumnInfo_t const refCol[] = {
    /* these columns are independent and are physical copies */
    { "MAX_SEQ_LEN"             , NULL, 0 },
    { "NAME"                    , NULL, 0 },
    { "(INSDC:dna:text)CS_KEY"  , NULL, 0 },
    { "(INSDC:dna:text)CMP_READ", NULL, 0 },
    { "SEQ_ID"                  , NULL, 0 },
    { "SEQ_START"               , NULL, 0 },
    { "SEQ_LEN"                 , NULL, 0 },
    { "CIRCULAR"                , NULL, 0 },
    { "CGRAPH_HIGH"             , NULL, 0 },
    { "CGRAPH_LOW"              , NULL, 0 },
    { "CGRAPH_MISMATCHES"       , NULL, 0 },
    { "CGRAPH_INDELS"           , NULL, 0 },
    { "PRIMARY_ALIGNMENT_IDS"   , NULL, 0 },
    { "SECONDARY_ALIGNMENT_IDS" , NULL, 0 },
    { "(bool)PRESERVE_QUAL"     , NULL, 0 },
    { NULL, NULL, 0 },
};    

static rc_t CopyCursor(VCursor *dst, uint32_t const dcid[],
                       VCursor const *src, uint32_t const scid[], unsigned n)
{
    rc_t rc;
    bool done = false;
    
    do {
        rc_t rc2;
        
        rc = VCursorOpenRow(dst);
        while (rc == 0) {
            unsigned i;
            int64_t row;
            
            rc = VCursorRowId(dst, &row); if (rc) break;
            for (i = 0; i != n && rc == 0; ++i) {
                uint32_t elem_bits;
                void const *data;
                uint32_t offset;
                uint32_t rowlen;
                
                rc = VCursorCellDataDirect(src, row, scid[i], &elem_bits, &data, &offset, &rowlen);
                if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound && i == 0) {
                    rc = 0;
                    done = true;
                    break;
                }
                if (rc) break;
                rc = VCursorWrite(dst, dcid[i], elem_bits, data, offset, rowlen); if (rc) break;
            }
            break;
        }
        if (rc == 0 && !done) {
            rc = VCursorCommitRow(dst);
        }
        rc2 = VCursorCloseRow(dst); if (rc == 0) rc = rc2;
    } while (!done && rc == 0 && (rc = Quitting()) == 0);
    if (rc == 0)
        rc = VCursorCommit(dst);
    return rc;
}

static rc_t CopyTable(VDatabase *dst, VDatabase const *src,
                      char const tblName[], ColumnInfo_t const col[],
                      bool required)
{
    uint32_t scid[64];
    uint32_t dcid[64];
    VTable const *stbl;
    rc_t rc = VDatabaseOpenTableRead(src, &stbl, "%s", tblName);
    
    if (rc == 0) {
        VTable *dtbl;
        
        rc = VDatabaseCreateTable(dst, &dtbl, tblName, kcmCreate, "%s", tblName);
        if (rc == 0) {
            unsigned i = 0;
            bool done = false;
            
            rc = VTableColumnCreateParams(dtbl, kcmCreate, kcsCRC32, 0);
            while (rc == 0 && !done && col[i].col) {
                unsigned n = 1;
                unsigned k;
                int group = col[i].group;
                VCursor const *scurs;
                
                if (group != 0) while (col[i + n].col && col[i + n].group == group) ++n;
                rc = VTableCreateCursorRead(stbl, &scurs);
                if (rc) {
                    (void)PLOGERR(klogErr, (klogErr, rc, "Failed to create read cursor", ""));
                    done = true;
                }
                else {
                    VCursor *dcurs;
                    
                    rc = VTableCreateCursorWrite(dtbl, &dcurs, kcmInsert);
                    if (rc) {
                        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to create write cursor", ""));
                        done = true;
                    }
                    else {
                        for (k = 0; k != n; ++k) {
                            char const *dcol = col[i + k].col;
                            char const *scol = col[i + k].src;
                            
                            if (scol == NULL) scol = dcol;
                            (void)PLOGMSG(klogInfo, (klogInfo, "Copying column '$(col)' of table '$(tbl)'", "col=%s,tbl=%s", dcol, tblName));
                            rc = VCursorAddColumn(scurs, &scid[k], "%s", scol);
                            if (rc) {
                                (void)PLOGERR(klogErr, (klogErr, rc, "Failed to add column to read cursor", ""));
                                done = true;
                                break;
                            }
                            rc = VCursorAddColumn(dcurs, &dcid[k], "%s", dcol);
                            if (rc) {
                                (void)PLOGERR(klogErr, (klogErr, rc, "Failed to add column to write cursor", ""));
                                done = true;
                                break;
                            }
                        }
                        if (rc == 0) {
                            rc = VCursorOpen(scurs);
                            if (rc) {
                                (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open read cursor", ""));
                                done = true;
                            }
                            else {
                                rc = VCursorOpen(dcurs);
                                if (rc) {
                                    (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open write cursor", ""));
                                    done = true;
                                }
                                else
                                    rc = CopyCursor(dcurs, dcid, scurs, scid, n);
                            }
                        }
                    }
                    VCursorRelease(dcurs);
                }
                VCursorRelease(scurs);
                i += n;
            }
            if (rc == 0)
                VTableReindex(dtbl);
            VTableRelease(dtbl);
        }
        VTableRelease(stbl);
    }
    else if (!required) {
        /*
        (void)PLOGERR(klogInfo, (klogInfo, rc, "Failed to open optional table '$(tbl)'", "tbl=%s", tblName));
         */
        rc = 0;
    }
    else {
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open required table '$(tbl)'", "tbl=%s", tblName));
    }

    return rc;
}

static rc_t OpenDatabases(VDatabase **dst, VDatabase const **src)
{
    VDBManager *vdb;
    rc_t rc = VDBManagerMakeUpdate(&vdb, NULL);

    if (rc == 0) {
        VSchema *schema;
        
        rc = VDBManagerMakeSchema(vdb, &schema);
        if (rc == 0) {
            rc = VSchemaParseFile(schema, "align/align.vschema");
            if (rc == 0) {
                rc = VDBManagerOpenDBRead(vdb, src, NULL, "%s", Params.dbPath);
                if (rc == 0) {
                    rc = VDBManagerCreateDB(vdb, dst, schema,
                                            "NCBI:align:db:alignment_sorted",
                                            kcmInit + kcmMD5, "%s", Params.output);
                    if (rc == 0)
                        rc = VDatabaseColumnCreateParams(*dst, kcmInit, kcmMD5, 0);
                    if (rc)
                        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to create output '$(outname)'", "outname=%s", Params.output));
                }
                else
                    (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open input '$(inname)'", "inname=%s", Params.dbPath));
            }
            else
                (void)PLOGERR(klogErr, (klogErr, rc, "Failed to load schema", ""));
            VSchemaRelease(schema);
        }
        else
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to create schema", ""));
        VDBManagerRelease(vdb);
    }
    else
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to create manager", ""));
    return rc;
}

static rc_t ReadMeta(VDatabase const *src, char const name[], void **value, size_t *size)
{
    KMetadata const *meta;
    rc_t rc = VDatabaseOpenMetadataRead(src, &meta);

    *value = NULL;
    *size = 0;
    if (rc == 0) {
        KMDataNode const *node;
        
        rc = KMetadataOpenNodeRead(meta, &node, "%s", name);
        KMetadataRelease(meta);
        if (rc == 0) {
            char dummy;
            size_t remain;
            void *buf;
            size_t bsize;
            
            KMDataNodeRead(node, 0, &dummy, 0, &bsize, &remain);
            if (remain == 0) {
                (void)LOGMSG(klogErr, "Failed to read metadata");
                rc = RC(rcApp, rcMetadata, rcReading, rcData, rcNotFound);
            }
            else {
                buf = malloc(remain);
                if (buf) {
                    rc = KMDataNodeRead(node, 0, buf, remain, &bsize, &remain);
                    if (rc == 0) {
                        *value = buf;
                        *size = bsize;
                    }
                    else
                        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to read metadata", ""));
                }
                else {
                    rc = RC(rcApp, rcMetadata, rcReading, rcMemory, rcExhausted);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Failed to read metadata", ""));
                }
            }
            KMDataNodeRelease(node);
        }
        else
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to load metadata", ""));
    }
    else
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to load metadata", ""));
    return rc;
}

static rc_t WriteMeta(VDatabase *dst, char const name[], void *value, size_t size)
{
    KMetadata *meta;
    rc_t rc = VDatabaseOpenMetadataUpdate(dst, &meta);
    
    if (rc == 0) {
        KMDataNode *node;
        
        rc = KMetadataOpenNodeUpdate(meta, &node, "%s", name);
        KMetadataRelease(meta);
        if (rc == 0) {
            rc = KMDataNodeWrite(node, value, size);
            KMDataNodeRelease(node);
        }
    }
    if (rc)
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to write metadata", ""));
    return rc;
}

static rc_t CopyBAMHeader(VDatabase *dst, VDatabase const *src)
{
    void *header;
    size_t hsize;
    rc_t rc = ReadMeta(src, "BAM_HEADER", &header, &hsize);
    
    if (rc == 0)
        rc = WriteMeta(dst, "BAM_HEADER", header, hsize);
    if (header) free(header);
    return rc;
}

static rc_t run(void)
{
    VDatabase *dst = NULL;
    VDatabase const *src = NULL;
    rc_t rc = OpenDatabases(&dst, &src);

    while (rc == 0) {
        rc = CopyBAMHeader(dst, src); if (rc) break;
        rc = CopyTable(dst, src, "REFERENCE", refCol, true); if (rc) break;
        rc = CopyTable(dst, src, "PRIMARY_ALIGNMENT", priCol, true); if (rc) break;
        if (!Params.exclude_secondary)
            rc = CopyTable(dst, src, "SECONDARY_ALIGNMENT", secCol, false); break;
        break;
    }
    VDatabaseRelease(dst);
    VDatabaseRelease(src);
    return rc;
}

static const char* usage_output[] = { "path of output database", NULL };
static const char* usage_exclude[] = { "exclude secondary alignments", NULL };

static const char* param_usage[] = { "Path to the database" };

OptDef Options[] =
{
    { "output", "o", NULL, usage_output, 1, true, true },
    { "exclude-secondary", "x", NULL, usage_exclude, 1, false, false }
};

rc_t CC UsageSummary (const char * progname) {
    return KOutMsg (
"Usage:\n"
"  %s [options] <db-path>\n"
"\n"
"Summary:\n"
"  Creates an analysis alignment database from an archive alignment database\n"
"  by flattening and removing sequence data.\n"
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

    HelpOptionLine ("o", "output", NULL, usage_output);

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}

const char UsageDefaultName[] = "csra-trim";

ver_t CC KAppVersion(void) { return CSRA_TRIM_VERS; }

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

        rc = ArgsOptionCount (args, "output", &pcount);
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failure to get 'output' argument");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            (void)LOGERR(klogErr, rc, "Too many output parameters");
            break;
        }
        rc = ArgsOptionValue(args, "output", 0, &Params.output);
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failure retrieving output name");
            break;
        }
        
        rc = ArgsOptionCount (args, "exclude-secondary", &pcount);
        if (rc) {
            (void)LOGERR(klogErr, rc, "Failure to get 'exclude-secondary' argument");
            break;
        }
        Params.exclude_secondary = pcount > 0;
    } while (false);

    rc = rc ? rc : run();
    ArgsRelease(args);
    return rc;
}
