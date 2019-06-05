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

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>

#include <klib/rc.h>
#include <klib/log.h>

#include <kdb/btree.h>
#include <kdb/meta.h>
#include <kdb/manager.h>
#include <kdb/database.h>

#include <kapp/loader-meta.h>
#include <kapp/main.h>

#include <kfs/directory.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/vdb-priv.h>
#include <insdc/sra.h>

#include "common-reader.h"
#include "common-writer.h"
#include "sequence-writer.h"

#include "fastq-reader.h"

rc_t ArchiveFASTQ(CommonWriterSettings* G,
                VDBManager *mgr,
                VDatabase *db,
                unsigned seqFiles,
                char const *seqFile[],
                enum FASTQQualityFormat qualityFormat,
                const int8_t defaultReadNumbers[],
                bool ignoreSpotGroups)
{
    rc_t rc = 0;
    unsigned i;
    CommonWriter cw;

    KDirectory *dir;
    rc = KDirectoryNativeDir(&dir);
    if (rc != 0)
        return rc;

    rc = CommonWriterInit( &cw, mgr, db, G);
    if (rc != 0)
    {
        KDirectoryRelease(dir);
        return rc;
    }

    for (i = 0; i < seqFiles; ++i) {
        const ReaderFile *reader;
        if (G->platform == SRA_PLATFORM_PACBIO_SMRT)
            rc = FastqReaderFileMake(&reader, dir, seqFile[i], FASTQphred33, -1, ignoreSpotGroups);
        else
            rc = FastqReaderFileMake(&reader, dir, seqFile[i], qualityFormat, defaultReadNumbers[i], ignoreSpotGroups);

        if (rc == 0)
        {
            rc = CommonWriterArchive( &cw, reader );
            if (rc != 0)
                ReaderFileRelease(reader);
            else
                rc = ReaderFileRelease(reader);
        }
        if (rc != 0)
            break;
    }
    if (rc == 0) {
        bool const quitting = (Quitting() != 0);
        rc = CommonWriterComplete(&cw, quitting, 0);
    }
    else
        CommonWriterComplete( &cw, true, 0 );

    G->errCount = cw.err_count;

    if (rc == 0)
        rc = CommonWriterWhack( &cw );
    else
        CommonWriterWhack( &cw );

    if (rc == 0)
        rc = KDirectoryRelease(dir);
    else
        KDirectoryRelease(dir);

    if (rc == 0) {
        (void)LOGMSG(klogInfo, "Successfully loaded all files");
    }
    return rc;
}

rc_t WriteLoaderSignature(KMetadata *meta, char const progName[])
{
    KMDataNode *node;
    rc_t rc = KMetadataOpenNodeUpdate(meta, &node, "/");

    if (rc == 0) {
        rc = KLoaderMeta_Write(node, progName, __DATE__, "FASTQ", KAppVersion());
        KMDataNodeRelease(node);
    }
    if (rc) {
        (void)LOGERR(klogErr, rc, "Cannot update loader meta");
    }
    return rc;
}

rc_t OpenPath(char const path[], KDirectory **dir)
{
    KDirectory *p;
    rc_t rc = KDirectoryNativeDir(&p);

    if (rc == 0) {
        rc = KDirectoryOpenDirUpdate(p, dir, false, "%s", path);
        KDirectoryRelease(p);
    }
    return rc;
}

rc_t ConvertDatabaseToUnmapped(VDatabase* db)
{
    VTable* tbl;
    rc_t rc = VDatabaseOpenTableUpdate(db, &tbl, "SEQUENCE");
    if (rc == 0)
    {
        rc = VTableRenameColumn(tbl, false, "CMP_ALTREAD", "ALTREAD");
        if (rc == 0 || GetRCState(rc) == rcNotFound)
            rc = VTableRenameColumn(tbl, false, "CMP_READ", "READ");
/*        if (rc == 0 || GetRCState(rc) == rcNotFound)
            rc = VTableRenameColumn(tbl, false, "CMP_ALTCSREAD", "ALTCSREAD");
        if (rc == 0 || GetRCState(rc) == rcNotFound)
            rc = VTableRenameColumn(tbl, false, "CMP_CSREAD", "CSREAD");*/
        if (GetRCState(rc) == rcNotFound)
            rc = 0;
        rc = VTableRelease(tbl);
    }
    return rc;
}

rc_t run ( char const progName[],
           CommonWriterSettings* G,
           unsigned seqFiles,
           const char *seqFile[],
           uint8_t qualityOffset,
           const int8_t defaultReadNumbers[],
           bool ignoreSpotGroups )
{
    VDBManager *mgr;
    rc_t rc;
    rc_t rc2;
    char const *db_type = "NCBI:align:db:alignment_sorted";
/*    char const *db_type = "NCBI:align:db:unaligned"; */

    rc = VDBManagerMakeUpdate(&mgr, NULL);
    if (rc) {
        (void)LOGERR (klogErr, rc, "failed to create VDB Manager!");
    }
    else {
        VSchema *schema;

        rc = VDBManagerMakeSchema(mgr, &schema);
        if (rc) {
            (void)LOGERR (klogErr, rc, "failed to create schema");
        }
        else {
            (void)(rc = VSchemaAddIncludePath(schema, "%s", G->schemaIncludePath));
            rc = VSchemaParseFile(schema, "%s", G->schemaPath);
            if (rc) {
                (void)PLOGERR(klogErr, (klogErr, rc, "failed to parse schema file $(file)", "file=%s", G->schemaPath));
            }
            else {
                VDatabase *db;

                rc = VDBManagerCreateDB(mgr, &db, schema, db_type, kcmInit + kcmMD5, "%s", G->outpath);
                rc2 = VSchemaRelease(schema);
                if (rc2)
                    (void)LOGERR(klogWarn, rc2, "Failed to release schema");
                if (rc == 0)
                    rc = rc2;
                if (rc == 0) {
                    rc = ArchiveFASTQ(G, mgr, db, seqFiles, seqFile, qualityOffset, defaultReadNumbers, ignoreSpotGroups);
                }

                if (rc == 0) {
                    rc = ConvertDatabaseToUnmapped(db);
                }

                rc2 = VDatabaseRelease(db);
                if (rc == 0)
                {
                    if (rc2)
                        (void)LOGERR(klogWarn, rc2, "Failed to close database");
                    rc = rc2;
                }

                if (rc == 0) {
                    KMetadata *meta;
                    KDBManager *kmgr;

                    rc = VDBManagerOpenKDBManagerUpdate(mgr, &kmgr);
                    if (rc == 0) {
                        KDatabase *kdb;

                        rc = KDBManagerOpenDBUpdate(kmgr, &kdb, "%s", G->outpath);
                        if (rc == 0) {
                            rc = KDatabaseOpenMetadataUpdate(kdb, &meta);
                            KDatabaseRelease(kdb);
                        }
                        KDBManagerRelease(kmgr);
                    }
                    if (rc == 0) {
                        rc = WriteLoaderSignature(meta, progName);
                        KMetadataRelease(meta);
                    }
                }
            }
            rc2 = VDBManagerRelease(mgr);
            if (rc2)
                (void)LOGERR(klogWarn, rc2, "Failed to release VDB Manager");
            if (rc == 0)
                rc = rc2;
        }
    }
    return rc;
}
