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

#include <loader/common-writer.h>
#include <loader/sequence-writer.h>
#include <loader/reference-writer.h>

#include "bam-reader.h"

static rc_t OpenBAM(const ReaderFile **bam, VDatabase *db, const CommonWriterSettings* G, const char bamFile[])
{
    rc_t rc = BamReaderFileMake(bam, G->headerText, bamFile);
    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open '$(file)'", "file=%s", bamFile));
    }
    else if (db) {
        KMetadata *dbmeta;
        
        rc = VDatabaseOpenMetadataUpdate(db, &dbmeta);
        if (rc == 0) {
            KMDataNode *node;
            
            rc = KMetadataOpenNodeUpdate(dbmeta, &node, "BAM_HEADER");
            KMetadataRelease(dbmeta);
            if (rc == 0) {
                char const *header;
                size_t size;
                
                rc = BAMFileGetHeaderText(ToBam(*bam), &header, &size);
                if (rc == 0) {
                    rc = KMDataNodeWrite(node, header, size);
                }
                KMDataNodeRelease(node);
            }
        }
    }

    return rc;
}

static rc_t VerifyReferences(ReferenceInfo const *bam, const CommonWriterSettings* G, Reference const *ref)
{
    rc_t rc = 0;
    uint32_t n;
    unsigned i;
    
    ReferenceInfoGetRefSeqCount(bam, &n);
    for (i = 0; i != n; ++i) {
        ReferenceSequence refSeq;
        
        ReferenceInfoGetRefSeq(bam, i, &refSeq);
        if (G->refFilter && strcmp(refSeq.name, G->refFilter) != 0)
            continue;
        
        rc = ReferenceVerify(ref, refSeq.name, refSeq.length, refSeq.checksum);
        if (rc) {
            if (GetRCObject(rc) == rcChecksum && GetRCState(rc) == rcUnequal) {
#if NCBI
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); checksums do not match", "name=%s,len=%u", refSeq.name, (unsigned)refSeq.length));
#endif
            }
            else
            if (GetRCObject(rc) == rcSize && GetRCState(rc) == rcUnequal) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); lengths do not match", "name=%s,len=%u", refSeq.name, (unsigned)refSeq.length));
            }
            else if (GetRCObject(rc) == rcSize && GetRCState(rc) == rcEmpty) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); fasta file is empty", "name=%s,len=%u", refSeq.name, (unsigned)refSeq.length));
            }
            else if (GetRCObject(rc) == rcId && GetRCState(rc) == rcNotFound) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Reference: '$(name)', Length: $(len); no match found", "name=%s,len=%u", refSeq.name, (unsigned)refSeq.length));
            }
            else {
                (void)PLOGERR(klogWarn, (klogWarn, rc, "Reference: '$(name)', Length: $(len); error", "name=%s,len=%u", refSeq.name, (unsigned)refSeq.length));
            }
        }
        else if (G->onlyVerifyReferences) {
            (void)PLOGMSG(klogInfo, (klogInfo, "Reference: '$(name)', Length: $(len); match found", "name=%s,len=%u", refSeq.name, (unsigned)refSeq.length));
        }
    }
    return 0;
}

rc_t ProcessReferences(const CommonWriterSettings* G, 
                       VDBManager *mgr, 
                       VDatabase *db,
                       unsigned bamFiles, 
                       char const *bamFile[])
{
    Reference ref;
    rc_t rc = ReferenceInit(&ref, mgr, db, G->expectUnsorted, G->acceptHardClip, G->refXRefPath, G->inpath, G->maxSeqLen, G->refFiles);
    if (rc)
        return rc;
    
    if (!G->noVerifyReferences) {
        unsigned i;
        for (i = 0; i < bamFiles; ++i) {
            const ReaderFile *bam;
            const ReferenceInfo* ri;
            
            rc = OpenBAM(&bam, db, G, bamFile[i]);
            if (rc) 
                break;
        
            rc = ReaderFileGetReferenceInfo(bam, &ri);
            if (rc == 0 && ri != NULL)
            {
                rc = VerifyReferences(ri, G, &ref);
            }
            ReferenceInfoRelease(ri);
            
            ReaderFileRelease(bam); 

            if (rc) 
                break;
        }
    }
    
    ReferenceWhack(&ref, false, G->maxSeqLen, Quitting);
    return rc;
}

rc_t ArchiveBAM(CommonWriterSettings* G, 
                VDBManager *mgr, 
                VDatabase *db,
                unsigned bamFiles, 
                char const *bamFile[],
                unsigned seqFiles, 
                char const *seqFile[],
                bool *has_alignments)
{
    rc_t rc = 0;
    unsigned i;
    CommonWriter cw;

    rc = CommonWriterInit( &cw, mgr, db, G);
    if (rc != 0)
        return rc;
    
    for (i = 0; i < bamFiles && rc == 0; ++i) {
        const ReaderFile *reader;
        rc = OpenBAM(&reader, db, G, bamFile[i]);
        if (rc == 0) 
        {
            rc = CommonWriterArchive( &cw, reader );
            if (rc != 0) 
                ReaderFileRelease(reader);
            else
                rc = ReaderFileRelease(reader);
        }
    }
    for (i = 0; i < seqFiles && rc == 0; ++i) {
        const ReaderFile *reader;
        rc = OpenBAM(&reader, db, G, seqFile[i]);
        if (rc == 0) 
        {
            rc = CommonWriterArchive( &cw, reader );
            if (rc != 0) 
                ReaderFileRelease(reader);
            else
                rc = ReaderFileRelease(reader);   
        }
    }
    if (rc == 0)
        rc = CommonWriterComplete( &cw, Quitting() != 0, G->maxMateDistance );
    else
        CommonWriterComplete( &cw, true, 0 );
        
    *has_alignments = cw.had_alignments;
    G->errCount = cw.err_count;
        
    if (rc == 0)
        rc = CommonWriterWhack( &cw );
    else
        CommonWriterWhack( &cw );
    
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
        rc = KLoaderMeta_Write(node, progName, __DATE__, "BAM", KAppVersion());
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

static
rc_t ConvertDatabaseToUnmapped(VDatabase *db)
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

rc_t run(char const progName[], CommonWriterSettings* G,
         unsigned bamFiles, char const *bamFile[],
         unsigned seqFiles, char const *seqFile[])
{
    VDBManager *mgr;
    rc_t rc;
    rc_t rc2;
    char const *db_type = G->expectUnsorted ? "NCBI:align:db:alignment_unsorted" : "NCBI:align:db:alignment_sorted";
    
    rc = VDBManagerMakeUpdate(&mgr, NULL);
    if (rc) {
        (void)LOGERR (klogErr, rc, "failed to create VDB Manager!");
    }
    else {
        bool has_alignments = false;
        
        rc = VDBManagerDisablePagemapThread(mgr);
        if (rc == 0)
        {
            if (G->onlyVerifyReferences) {
                rc = ProcessReferences(G, mgr, NULL, bamFiles, bamFile);
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
                        
                        rc = VDBManagerCreateDB(mgr, &db, schema, db_type,
                                                kcmInit + kcmMD5, "%s", G->outpath);
                        rc2 = VSchemaRelease(schema);
                        if (rc2)
                            (void)LOGERR(klogWarn, rc2, "Failed to release schema");
                        if (rc == 0)
                            rc = rc2;
                        if (rc == 0) {
                            rc = ProcessReferences(G, mgr, db, bamFiles, bamFile);
                            if (rc == 0)
                            {
                                rc = ArchiveBAM(G, mgr, db, bamFiles, bamFile, seqFiles, seqFile, &has_alignments);
                            }
                        }
                        
                        if (rc == 0 && !has_alignments) {
                            rc = ConvertDatabaseToUnmapped(db);
                        }
                        
                        rc2 = VDatabaseRelease(db);
                        if (rc2)
                            (void)LOGERR(klogWarn, rc2, "Failed to close database");
                        if (rc == 0)
                            rc = rc2;
                            
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
                }
            }
        }
        rc2 = VDBManagerRelease(mgr);
        if (rc2)
            (void)LOGERR(klogWarn, rc2, "Failed to release VDB Manager");
        if (rc == 0)
            rc = rc2;
    }
    return rc;
}
