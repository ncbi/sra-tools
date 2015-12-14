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

#ifndef BAM_LOAD_SEQUENCE_WRITER_H_
#define BAM_LOAD_SEQUENCE_WRITER_H_ 1

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <klib/text.h>
#include <klib/data-buffer.h>

struct TableWriterSeq;

typedef struct s_sequence_record {
    char *seq;
    uint8_t *qual;
    uint32_t *readStart;
    uint32_t *readLen;
    uint8_t *orientation;
    uint8_t *is_bad;
    uint8_t *alignmentCount;
    char *spotGroup;
    bool *aligned;
    char *cskey;
    uint64_t *ti;
    uint64_t keyId;
    unsigned spotGroupLen;
    KDataBuffer storage;
    uint8_t numreads;
} SequenceRecord;


rc_t SequenceRecordInit(SequenceRecord *self,
                        unsigned numreads, unsigned readLen[]);

rc_t SequenceRecordAppend(SequenceRecord *self,
                          const SequenceRecord *other);

typedef struct s_sequence {
    VDatabase *db;
    struct TableWriterSeq const *tbl;
} Sequence;

Sequence *SequenceInit(Sequence *self, VDatabase *db);

rc_t SequenceWriteRecord(Sequence *self, SequenceRecord const *rec,
                         bool color, bool isDup, INSDC_SRA_platform_id platform);

rc_t SequenceDoneWriting(Sequence *self);
rc_t SequenceReadKey(const Sequence *self, int64_t row, uint64_t *key);
rc_t SequenceUpdateAlignData(Sequence *self, int64_t row, unsigned nreads,
                             const int64_t primeId[/* nreads */],
                             const uint8_t alignCount[/* nreads */]);

void SequenceWhack(Sequence *self, bool commit);


#endif /* ndef BAM_LOAD_SEQUENCE_WRITER_H_ */
