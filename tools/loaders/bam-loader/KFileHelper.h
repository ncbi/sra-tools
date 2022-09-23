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

#ifndef BAM_LOAD_KFILEHELPER_H_
#define BAM_LOAD_KFILEHELPER_H_ 1
#include <klib/data-buffer.h>
#include <kfs/directory.h>
#include <kfs/file.h>

typedef struct BufferedKFile BufferedKFile;

#define KFILE_BUF_SIZE (4096)
struct BufferedKFile {
    uint64_t fpos;
    const KFile *kfp;
    unsigned inbuf;
    unsigned cur;
    char buf[KFILE_BUF_SIZE];
};

rc_t BufferedKFileGets(KDataBuffer *buf, unsigned offset, unsigned *nread, BufferedKFile *strm);
rc_t BufferedKFileOpen(const KDirectory *directory, BufferedKFile *rslt, const char *fileName, ...);
rc_t BufferedKFileClose(BufferedKFile *strm);
rc_t LoadFile(KDataBuffer *dst, uint64_t *lineCount, const KDirectory *dir, const char *fileName, ...);
#endif
