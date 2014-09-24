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
#include <klib/report.h>
#include <klib/container.h>
#include <klib/log.h>
#include <klib/debug.h>
#include <klib/out.h>
#include <klib/status.h>
#include <kfs/file.h>
#include <kfs/directory.h>
#include <kfs/buffile.h>
#include <kfs/gzip.h>
#include <kfs/bzip.h>
#include <kapp/main.h>
#include <kapp/args.h>
#include <os-native.h>
#include <sysalloc.h>

#include "cmdline_cmn.h"
#include "writer.h"

struct {
    KWrtWriter writer;
    void* data;
    KFile* kfile;
    uint64_t pos;
} g_out_writer = {NULL};

static
rc_t CC BufferedWriter(void* self, const char* buffer, size_t bufsize, size_t* num_writ)
{
    rc_t rc = 0;

    assert(buffer != NULL);
    assert(num_writ != NULL);

    do {
        if( (rc = KFileWrite(g_out_writer.kfile, g_out_writer.pos, buffer, bufsize, num_writ)) == 0 ) {
            buffer += *num_writ;
            bufsize -= *num_writ;
            g_out_writer.pos += *num_writ;
        }
    } while(rc == 0 && bufsize > 0);
    return rc;
}

rc_t OUTSTR_(const char* buf, size_t buf_sz)
{
    size_t nm;
    return BufferedWriter(NULL, buf, buf_sz, &nm);
}

rc_t BufferedWriterMake(const common_options* opt)
{
    rc_t rc = 0;

    if( opt == NULL || (opt->gzip_output && opt->bzip_output) ) {
        rc = RC(rcApp, rcFile, rcConstructing, rcParam, rcInvalid);
    } else if( g_out_writer.writer != NULL ) {
        rc = RC(rcApp, rcFile, rcConstructing, rcParam, rcAmbiguous);
    }
    if( opt->output_file != NULL ) {
        KDirectory *dir;
        if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
            rc = KDirectoryCreateFile(dir, &g_out_writer.kfile, false, 0664, kcmInit, "%s", opt->output_file);
            KDirectoryRelease(dir);
        }
    } else {
        KOutHandlerSetStdOut();
        KStsHandlerSetStdErr();
        KLogHandlerSetStdErr();
        KDbgHandlerSetStdErr();
        rc = KFileMakeStdOut(&g_out_writer.kfile);
    }
    if( rc == 0 ) {
        g_out_writer.pos = 0;
        if( opt->gzip_output ) {
            KFile* gz;
            if( (rc = KFileMakeGzipForWrite(&gz, g_out_writer.kfile)) == 0 ) {
                KFileRelease(g_out_writer.kfile);
                g_out_writer.kfile = gz;
            }
        } else if( opt->bzip_output ) {
            KFile* bz;
            if( (rc = KFileMakeBzip2ForWrite(&bz, g_out_writer.kfile)) == 0 ) {
                KFileRelease(g_out_writer.kfile);
                g_out_writer.kfile = bz;
            }
        }
        if( rc == 0 ) {
            KFile* buf;
            if( (rc = KBufFileMakeWrite(&buf, g_out_writer.kfile, false, 128 * 1024)) == 0 ) {
                KFileRelease(g_out_writer.kfile);
                g_out_writer.kfile = buf;
                g_out_writer.writer = KOutWriterGet();
                g_out_writer.data = KOutDataGet();
                rc = KOutHandlerSet(BufferedWriter, &g_out_writer);
            }
        }
    }
    return rc;
}

void BufferedWriterRelease( bool flush )
{
    if( flush ) {
        /* avoid flushing buffered data after failure */
        KFileRelease(g_out_writer.kfile);
    }
    if( g_out_writer.writer != NULL ) {
        KOutHandlerSet(g_out_writer.writer, g_out_writer.data);
    }
    g_out_writer.writer = NULL;
}
