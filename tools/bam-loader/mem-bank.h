/* ===========================================================================
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

typedef struct MemBank MemBank;

rc_t MemBankMake(MemBank **rslt, struct KDirectory *dir, int pid, size_t const climits[2]);

void MemBankRelease(MemBank *self);

rc_t MemBankAlloc(MemBank *self, uint32_t *id, size_t size, bool clear, bool longlived);

rc_t MemBankWrite(MemBank *self, uint32_t id, uint64_t pos, void const *buffer, size_t size, size_t *num_writ);

rc_t MemBankSize(MemBank const *self, uint32_t id, size_t *rslt);

rc_t MemBankRead(MemBank const *self, uint32_t id, uint64_t pos, void *buffer, size_t bsize, size_t *num_read);

rc_t MemBankFree(MemBank *self, uint32_t id);
