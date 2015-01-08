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

#include "vcf-database.h"

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <align/writer-reference.h>

#include "vcf-reader.h"

static rc_t SaveVariants        ( const VcfReader* reader, const char configPath[], VDatabase* db, VDBManager* dbMgr );
static rc_t SaveVariantPhases   ( const VcfReader* reader, VDatabase* db, VDBManager* dbMgr );
static rc_t SaveAlignments      ( const VcfReader* reader, VDatabase* db, VDBManager* dbMgr );

rc_t VcfDatabaseSave ( const struct VcfReader* reader, const char configPath[], VDatabase* db )
{
    VDBManager* dbMgr;
    rc_t rc = VDatabaseOpenManagerUpdate(db, &dbMgr);
    if (rc == 0)
    {
        rc_t rc2;
        
        rc = SaveVariants(reader, configPath, db, dbMgr);
        if (rc == 0)
            rc = SaveVariantPhases(reader, db, dbMgr);
        if (rc == 0)
            rc = SaveAlignments(reader, db, dbMgr);
            
        rc2 = VDBManagerRelease(dbMgr);
        if (rc == 0)
            rc = rc2;
    }
    return rc;
}

rc_t SaveVariants( const VcfReader* reader, const char configPath[], VDatabase* db, VDBManager* dbMgr )
{
    VTable* tbl;
    rc_t rc = VDatabaseCreateTable(db, &tbl, "VARIANT", kcmCreate | kcmMD5, "VARIANT");
    if (rc == 0)
    {
        rc_t rc2;
        VCursor *cur;
        rc = VTableCreateCursorWrite( tbl, &cur, kcmInsert );
        if (rc == 0)
        {
            uint32_t ref_id_idx, position_idx, length_idx, sequence_idx;
            rc = VCursorAddColumn( cur, &ref_id_idx, "ref_id" );
            if (rc == 0) rc = VCursorAddColumn( cur, &position_idx, "position" );
            if (rc == 0) rc = VCursorAddColumn( cur, &length_idx, "length" );
            if (rc == 0) rc = VCursorAddColumn( cur, &sequence_idx, "sequence" );

            if (rc == 0)
            {
                rc = VCursorOpen( cur );
                if (rc == 0)
                {
                    uint32_t count;
                    rc_t rc = VcfReaderGetDataLineCount(reader, &count);
                    if (rc == 0)
                    {   
                        const ReferenceMgr* refMgr;
                        rc = ReferenceMgr_Make(&refMgr, db, dbMgr, 0, configPath, NULL, 0, 0, 0);
                        if (rc == 0)
                        {
                            uint32_t i;
                            for (i = 0; i < count; ++i)
                            {
                                const VcfDataLine* line;
                                rc = VcfReaderGetDataLine(reader, i, &line);            

                                if (rc == 0)
                                {
                                    const ReferenceSeq* seq;
                                    #define MAX_CHROMOSOME_NAME_LENGTH 1024
                                    char chromName[MAX_CHROMOSOME_NAME_LENGTH];
                                    bool shouldUnmap = false;
                                    string_copy(chromName, sizeof(chromName), line->chromosome.addr, line->chromosome.size);
                                    rc = (ReferenceMgr_GetSeq(refMgr, &seq, chromName, &shouldUnmap));
                                    if (rc == 0)
                                    {
                                        int64_t ref_id;
                                        INSDC_coord_zero ref_start;
                                        assert(shouldUnmap == false);
                                        rc = ReferenceSeq_TranslateOffset_int(seq, line->position, &ref_id, &ref_start, NULL);
                                        if (rc == 0)
                                        {
                                            rc = VCursorOpenRow( cur );
                                        
                                            if (rc == 0) 
                                                rc = VCursorWrite( cur, ref_id_idx,    sizeof(ref_id) * 8, &ref_id, 0, 1);
                                            if (rc == 0) 
                                                rc = VCursorWrite( cur, position_idx,  sizeof(ref_start) * 8, &ref_start, 0, 1);
                                            if (rc == 0) 
                                                rc = VCursorWrite( cur, length_idx,    sizeof(line->altBases.len) * 8,   &line->altBases.len,   0, 1);
                                            if (rc == 0) 
                                                rc = VCursorWrite( cur, sequence_idx,  line->altBases.len * 8,    line->altBases.addr,    0, 1);
                                        }
                                        rc2 = ReferenceSeq_Release(seq);
                                        if (rc == 0)
                                            rc = rc2;
                                    }    
                                    if (rc == 0) rc = VCursorCommitRow( cur );
                                    if (rc == 0) rc = VCursorCloseRow( cur );
                                }
                                if (rc != 0)
                                    break;
                            }
                            rc2 = ReferenceMgr_Release(refMgr, rc == 0, NULL, false, NULL);
                            if (rc == 0)
                                rc = rc2;
                        }
                    }
                    if (rc == 0)
                        rc = VCursorCommit( cur );
                }
            }
            rc2 = VCursorRelease(cur);
            if (rc == 0)
                rc = rc2;
        }
        
        rc2 = VTableRelease(tbl);
        if (rc == 0)
            rc = rc2;
    }
            
    return rc;
}

rc_t SaveVariantPhases( const VcfReader* reader, VDatabase* db, VDBManager* dbMgr )
{
    return 0;
}

rc_t SaveAlignments( const VcfReader* reader, VDatabase* db, VDBManager* dbMgr )
{
    return 0;
}

