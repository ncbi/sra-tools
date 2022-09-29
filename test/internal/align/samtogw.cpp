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

#include <align/samextract-lib.h>
#include <ctype.h>
#include <general-writer.hpp>
#include <kapp/args.h>
#include <kapp/main.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <klib/defs.h>
#include <klib/rc.h>
#include <klib/text.h>
#include <klib/vector.h>
#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char tool_name[] = "samtogw";

namespace ncbi
{
static rc_t process(const char* fname)
{
    rc_t rc;
    struct KDirectory* srcdir = NULL;
    const struct KFile* infile = NULL;
    rc = KDirectoryNativeDir(&srcdir);
    if (rc) return rc;

    rc = KDirectoryOpenFileRead(srcdir, &infile, fname);
    KDirectoryRelease(srcdir);
    if (rc) {
        fprintf(stderr, "Couldn't open file %s:%d\n", fname, rc);
        return rc;
    }
    srcdir = NULL;

    String sfname;
    StringInitCString(&sfname, fname);
    struct timespec stime, etime;
    clock_gettime(CLOCK_REALTIME, &stime);
    SAMExtractor* extractor;
    rc = SAMExtractorMake(&extractor, infile, &sfname, -1);
    if (rc) {
        fprintf(stderr, "Error %d\n", rc);
        return rc;
    }
    fprintf(stderr, "Made extractor for %s\n", fname);

    GeneralWriter* gw = new GeneralWriter(1); // stdout
    // GeneralWriter* gw = new GeneralWriter("out_path");
    char bname[PATH_MAX];
    snprintf(bname, sizeof bname, "%s", fname);
    char* base = basename(bname);
    char dbname[PATH_MAX];
    snprintf(dbname, sizeof dbname, "%s.db", base);
    gw->setRemotePath(dbname);
    gw->useSchema("bamdb.schema", "NCBI:align:db:BAM_DB #1.0.0");
    char buffer[512];
    // ver_t version=KAppVersion();
    // snprintf(buffer,sizeof buffer, "%x", version);
    snprintf(buffer, sizeof buffer, "0.1");
    // SraReleaeVersionPrint(
    gw->setSoftwareName(tool_name, buffer);

    int tbl_id = gw->addTable("ir");
    int read_name_id = gw->addColumn(tbl_id, "READ_NAME", 8);
    int read_id = gw->addColumn(tbl_id, "READ", 8);
    int sam_record_flags_id
        = gw->addIntegerColumn(tbl_id, "sam_record_flags", 32);
    int quality_id = gw->addColumn(tbl_id, "QUALITY", 8);
    int cigar_id = gw->addColumn(tbl_id, "CIGAR", 8);
    int projected_len_id = gw->addIntegerColumn(tbl_id, "PROJECTED_LEN", 64);
    int pcr_dup_id = gw->addColumn(tbl_id, "PCR_DUPLICATE", 8);
    int bad_id = gw->addColumn(tbl_id, "BAD", 8);

    int tv_id = gw->addTable("hdrs");
    int group_id = gw->addColumn(tv_id, "GROUP", 64);
    int hdr_id = gw->addColumn(tv_id, "HDR", 8);
    int tag_id = gw->addColumn(tv_id, "TAG", 8);
    int val_id = gw->addColumn(tv_id, "VALUE", 8);

    int meta_id = gw->addTable("meta");
    int tool_id = gw->addColumn(meta_id, "TOOL", 8);
    int cmdline_id = gw->addColumn(meta_id, "CMDLINE", 8);
    int input_file_id = gw->addColumn(meta_id, "INPUT_FILE", 8);
    int num_headers_id = gw->addColumn(meta_id, "NUM_HEADERS", 64);
    int num_alignments_id = gw->addColumn(meta_id, "NUM_ALIGNMENTS", 64);

    gw->open();

    Vector headers;
    rc = SAMExtractorGetHeaders(extractor, &headers);
    if (rc) return rc;
    fprintf(stderr, "Got %d headers\n", VectorLength(&headers));

    uint64_t num_headers = VectorLength(&headers);

    for (uint64_t i = 0; i != num_headers; ++i) {
        Header* hdr = (Header*)VectorGet(&headers, i);
        Vector* tvs = &hdr->tagvalues;
        for (uint64_t j = 0; j != VectorLength(tvs); ++j) {
            TagValue* tv = (TagValue*)VectorGet(tvs, j);
            gw->write(group_id, 64, &i, 1);
            gw->write(hdr_id, 8, hdr->headercode, strlen(hdr->headercode));
            gw->write(tag_id, 8, tv->tag, strlen(tv->tag));
            gw->write(val_id, 8, tv->value, strlen(tv->value));
            gw->nextRow(tv_id);
        }
        // Do stuff with headers
    }
    SAMExtractorInvalidateHeaders(extractor);

    fprintf(stderr, "Getting Alignments\n");
    uint64_t total = 0;
    uint32_t vlen;
    time_t tprev, tcur;
    do {
        Vector alignments;
        rc = SAMExtractorGetAlignments(extractor, &alignments);
        if (rc) {
            fprintf(stderr, "GetAligned returned rc\n");
            return rc;
        }
        vlen = VectorLength(&alignments);
        total += vlen;
        tprev = time(NULL);
        if (tprev != tcur) {
            tcur = tprev;
            fprintf(stderr, "Loaded %lu alignments\n", total);
        }
        for (uint32_t i = 0; i != vlen; ++i) {
            Alignment* align = (Alignment*)VectorGet(&alignments, i);
            //            fprintf(stderr, "\tAlignment%2d: %s\n", i,
            //            align->read);
            gw->write(read_id, 8, align->read, strlen(align->read));
            gw->write(read_name_id, 8, align->qname, strlen(align->qname));
            uint32_t flags = align->flags;
            gw->write(sam_record_flags_id, 32, &flags, 1);
            gw->write(quality_id, 8, align->qual, strlen(align->qual));
            int64_t tlen = align->tlen;
            gw->write(projected_len_id, 64, &tlen, 1);
            gw->write(cigar_id, 8, align->cigar, strlen(align->cigar));
            uint8_t pcr_dup = 0;
            if (align->flags & 0x400) pcr_dup = 1;
            gw->write(pcr_dup_id, 8, &pcr_dup, 1);
            uint8_t bad = 0;
            if (align->flags & 0x200) bad = 1;
            gw->write(bad_id, 8, &bad, 1);

            gw->nextRow(tbl_id);
        }
        SAMExtractorInvalidateAlignments(extractor);
    } while (vlen);

    gw->write(tool_id, 8, tool_name, strlen(tool_name));
    gw->write(cmdline_id, 8, fname, strlen(fname));
    gw->write(input_file_id, 8, fname, strlen(fname));
    gw->write(num_headers_id, 64, &num_headers, 1);
    gw->write(num_alignments_id, 64, &total, 1);
    gw->nextRow(meta_id);

    gw->endStream();
    delete gw;

    rc = SAMExtractorRelease(extractor);
    if (rc) {
        fprintf(stderr, "ExtractorRelease returned rc %d\n", rc);
        return rc;
    }

    fprintf(stderr, "Done with file, %lu alignments\n", total);
    clock_gettime(CLOCK_REALTIME, &etime);
    u64 nanos = etime.tv_sec - stime.tv_sec;
    nanos *= 1000000000;
    nanos += (etime.tv_nsec - stime.tv_nsec);
    fprintf(stderr, "Parse time %lu ms\n", nanos / 1000000l);

    KFileRelease(infile);
    infile = NULL;
    return 0;
}

} // namespace ncbi

extern "C" {
ver_t CC KAppVersion(void) { return 0x1000000; }

rc_t CC UsageSummary(char const* name)
{
    fprintf(stderr, "Usage: %s file.{sb}am [file...]\n", name);
    return 0;
}

rc_t CC Usage(Args const* args) { return 0; }

rc_t CC KMain(int argc, char* argv[])
{
    rc_t rc;
    if (argc == 1) {
        UsageSummary(argv[0]);
        return 0;
    }
    while (--argc) {
        const char* fname = *(++argv);
        rc = ncbi::process(fname);
    }
    return rc;
}
}
