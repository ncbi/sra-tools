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
#ifndef _sra_load_sra_writer_
#define _sra_load_sra_writer_

#include <sra/sradb.h>

typedef struct SRAWriter SRAWriter;

/* Make SRAWriter from configuration
 */
rc_t SRAWriter_Make( SRAWriter** self, const SRALoaderConfig* config);

rc_t SRAWriter_CreateTable(SRAWriter* self, const char* schema);

rc_t SRAWriter_OpenColumnWrite(SRAWriter* self, uint32_t *idx, const char *name, const char *datatype);

#define COL_OPEN(colid, name, type) \
    if( (rc = SRAWriter_OpenColumnWrite(self->base, &colid, name, type)) != 0 ) { \
        if( GetRCState(rc) != rcExists ) { \
            LOGERR(klogInt, rc, "failed to open column " name " of type " type); \
            return rc; \
        } \
        rc = 0; \
    }

rc_t SRAWriter_NewSpot(SRAWriter* self, spotid_t *id);

rc_t SRAWriter_WriteIdxColumn(SRAWriter* self, uint32_t idx, const void *base, uint64_t bytes, bool as_dflt);

#define COL_WRITE(colid, name, pstring) \
    if( colid != 0 ) { \
        if( (rc = SRAWriter_WriteIdxColumn(cself->base, colid, pstring.data, pstring.len, false)) != 0 ) { \
            LOGERR(klogInt, rc, "failed to write column " name); \
            return rc; \
        } \
    }

#define COL_WRITE_DEFAULT(colid, name, pstring) \
    if( colid != 0 ) { \
        if( (rc = SRAWriter_WriteIdxColumn(cself->base, colid, pstring.data, pstring.len, true)) != 0 ) { \
            LOGERR(klogInt, rc, "failed to write default to column " name); \
            return rc; \
        } \
    }

rc_t SRAWriter_CloseSpot(SRAWriter* self);

/* Writes default values ONLY ONCE to common SRATable columns
 * bases_per_spot [IN, OPTIONAL] - 0 ignored, otherwise used in default READ_SEG calculations
 */
rc_t SRAWriter_WriteDefaults(SRAWriter* self);

/* WriteSpotSegment
 *  Will determine SPOT_GROUP and READ_SEG values and write them to table to current spot
 *
 *  file [IN] - current file
 *  member_name [IN,NULL] - value extracted from a spotname if present
 *  bases [IN] - 2na FASTA string
 *  nbases [IN] - bases number (length)
 *  read_seg [OUT,OPTIONAL] - calculated READ_SEG values
 */
rc_t SRAWriter_WriteSpotSegment(SRAWriter* self, const SRALoaderFile* file, const char* file_member_name, uint32_t nbases, const char* bases);

rc_t SRAWriter_WriteSpotSegmentSimple(SRAWriter* self, const SRALoaderFile* file, const char* file_member_name, const SRASegment* read_seg);

void SRAWriter_Whack(SRAWriter* self, SRATable** table);

#endif /* _sra_load_sra_writer_ */
