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
#ifndef _tools_cg_load_writer_algn_h_
#define _tools_cg_load_writer_algn_h_

#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/manager.h>
#include <insdc/insdc.h>
#include <align/writer-alignment.h>
#include <align/writer-reference.h>

#include "writer-seq.h"

typedef struct TMappingsData_map_struct {
    bool saved;
    uint16_t flags;
    char chr[CG_CHROMOSOME_NAME];
    INSDC_coord_zero offset;
    int16_t gap[CG_READS_NGAPS];
    uint8_t weight;
    uint32_t mate;
} TMappingsData_map;

typedef struct TMappingsData_struct {
    uint16_t map_qty;
    TMappingsData_map map[CG_MAPPINGS_MAX];
} TMappingsData;

typedef struct CGWriterAlgn CGWriterAlgn;

rc_t CGWriterAlgn_Make(const CGWriterAlgn** cself, TMappingsData** data, VDatabase* db, const ReferenceMgr* rmgr,
                       uint32_t min_mapq, bool single_mate, uint32_t cluster_size);

rc_t CGWriterAlgn_Whack(const CGWriterAlgn* cself, bool commit, uint64_t* rows_1st, uint64_t* rows_2nd);

rc_t CGWriterAlgn_Write(const CGWriterAlgn* cself, TReadsData* read);

#endif /* _tools_cg_load_writer_algn_h_ */
