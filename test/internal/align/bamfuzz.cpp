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

#include "samextract.h"
#include <align/samextract-lib.h>
#include <ctype.h>
#include <kapp/args.h>
#include <kapp/main.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <klib/defs.h>
#include <klib/rc.h>
#include <klib/text.h>
#include <klib/vector.h>
#include <kproc/lock.h>
#include <kproc/queue.h>
#include <kproc/timeout.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

rc_t BAMGetHeaders(SAMExtractor* state);
rc_t BAMGetAlignments(SAMExtractor* state);

extern "C" {
ver_t CC KAppVersion(void) { return 0x1000000; }

rc_t CC UsageSummary(char const* name)
{
    fprintf(stderr, "Usage: %s fuzzbam\n", name);
    return 0;
}

rc_t CC Usage(Args const* args) { return 0; }

typedef enum BGZF_state { empty, compressed, uncompressed } BGZF_state;
typedef struct BGZF_s
{
    KLock* lock;
    Bytef in[READBUF_SZ + 1024];
    Bytef out[READBUF_SZ + 1024];
    uInt insize;
    uInt outsize;
    BGZF_state state;
} BGZF;

rc_t CC KMain(int argc, char* argv[])
{
    rc_t rc;
    if (argc == 1) {
        UsageSummary(argv[0]);
        return 0;
    }
    const char* fname = *(++argv);

    struct KDirectory* srcdir = NULL;
    const struct KFile* infile = NULL;
    rc = KDirectoryNativeDir(&srcdir);
    if (rc) return rc;

    rc = KDirectoryOpenFileRead(srcdir, &infile, fname);
    KDirectoryRelease(srcdir);
    if (rc) return rc;
    srcdir = NULL;

    String sfname;
    StringInitCString(&sfname, fname);
    SAMExtractor* extractor;
    rc = SAMExtractorMake(&extractor, infile, &sfname, -1);
    fprintf(stderr, "Made extractor for %s\n", fname);

    if (rc) return rc;
    BGZF* bgzf = (BGZF*)malloc(sizeof(BGZF));

    KLockMake(&bgzf->lock);
    KLockAcquire(bgzf->lock); // Not ready for parsing
    bgzf->state = uncompressed;
    bgzf->outsize = sizeof(bgzf->out);
    FILE* fin = fopen(fname, "r");
    if (!fin) {
        fprintf(stderr, "Couldn't read %s\n", fname);
        return 1;
    }
    size_t bsize = fread(bgzf->out, 1, READBUF_SZ, fin);
    fprintf(stderr, "Read in %zu from %s\n", bsize, fname);
    bgzf->outsize = bsize;
    fclose(fin);
    KLockUnlock(bgzf->lock); // OK to parse now

    struct timeout_t tm;
    TimeoutInit(&tm, 10);
    rc = KQueuePush(extractor->parsequeue, (void*)bgzf, &tm);
    KQueueSeal(extractor->parsequeue);

    extractor->file_type = BAM;
    rc = BAMGetHeaders(extractor);
    if (rc) {
        fprintf(stderr, "BAMGetHeaders rc=%d\n", rc);
    } else {
        rc = BAMGetAlignments(extractor);
        if (rc) {
            fprintf(stderr, "BAMGetAlignments rc=%d\n", rc);
        }
    }
    KFileRelease(infile);
    SAMExtractorRelease(extractor);
    infile = NULL;
    fprintf(stderr, "bamfuzz OK rc=%d\n", rc);
    return 0;
}
}
