// ===========================================================================
//
//                            PUBLIC DOMAIN NOTICE
//               National Center for Biotechnology Information
//
//  This software/database is a "United States Government Work" under the
//  terms of the United States Copyright Act.  It was written as part of
//  the author's official duties as a United States Government employee and
//  thus cannot be copyrighted.  This software/database is freely available
//  to the public for use. The National Library of Medicine and the U.S.
//  Government have not placed any restriction on its use or reproduction.
//
//  Although all reasonable efforts have been taken to ensure the accuracy
//  and reliability of the software and data, the NLM and the U.S.
//  Government do not and cannot warrant the performance or results that
//  may be obtained by using this software or data. The NLM and the U.S.
//  Government disclaim all warranties, express or implied, including
//  warranties of performance, merchantability or fitness for any particular
//  purpose.
//
//  Please cite the author in any work or product based on this material.
//
// ===========================================================================

#include "samextract-pool.h"
#include "samextract.h"
#include <align/samextract-lib.h>
#include <ctype.h>
#include <glob.h>
#include <kapp/args.h>
#include <kapp/main.h>
#include <kfg/config.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <klib/defs.h>
#include <klib/out.h>
#include <klib/rc.h>
#include <klib/text.h>
#include <klib/vector.h>
#include <ktst/unit_test.hpp>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strtol.h>
#include <time.h>
#include <unistd.h>

#include <cstdlib>
#include <stdexcept>
#include <sysalloc.h>

using namespace std;

static const size_t NUM_RAND = 100000;
//#define TEST_ALL_THE_INTEGERS // Takes about 2.5 hours

static bool tst_fast_u32toa(u32 val)
{
    char slow[100];
    char fast[100];
    sprintf(slow, "%u", val);
    size_t len = fast_u32toa(fast, val);
    if (strcmp(fast, slow)) {
        fprintf(stderr, "mismatch %u '%s' '%s'\n", val, slow, fast);
        return false;
    }
    if (len != strlen(slow)) {
        fprintf(stderr, "length mismatch %u '%s=%zu' '%s=%zu'", val, slow,
                strlen(slow), fast, len);
        return false;
    }
    return true;
}

static bool tst_fast_i32toa(u32 val)
{
    char slow[100];
    char fast[100];
    sprintf(slow, "%d", val);
    size_t len = fast_i32toa(fast, val);
    if (strcmp(fast, slow)) {
        fprintf(stderr, "mismatch %d '%s' '%s'\n", val, slow, fast);
        return false;
    }
    if (len != strlen(slow)) {
        fprintf(stderr, "length mismatch %d '%s=%zu' '%s=%zu'", val, slow,
                strlen(slow), fast, len);
        return false;
    }
    return true;
}

static rc_t extract_file(const char* fname, SAMExtractor** extractor)
{
    static struct KDirectory* srcdir = NULL;
    static const struct KFile* infile = NULL;
    rc_t rc;
    rc = KDirectoryNativeDir(&srcdir);
    if (rc) return rc;

    rc = KDirectoryOpenFileRead(srcdir, &infile, fname);
    if (rc) return rc;
    KDirectoryRelease(srcdir);

    String sfname;
    StringInitCString(&sfname, fname);

    rc = SAMExtractorMake(extractor, infile, &sfname, -1);

    return rc;
}
static rc_t extract_buf(const char* buf, const size_t sz,
                        SAMExtractor** extractor)
{
    rc_t rc;
    char* tmpfname = tempnam(NULL, "tst");
    FILE* fout = fopen(tmpfname, "wbx");
    fwrite(buf, sz, 1, fout);
    fclose(fout);
    rc = extract_file(tmpfname, extractor);
    unlink(tmpfname);
    free(tmpfname);
    return rc;
}

TEST_SUITE(SAMExtractTestSuite)

TEST_CASE(Test_Qual_Decode)
{
    unsigned char tst1[] = {0, 1, 2, 3};
    unsigned char tst2[] = {0xff, 0};
    unsigned char tst3[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    unsigned char tst4[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    unsigned char tst5[]
        = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    unsigned char tst6[]
        = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    unsigned char tst7[]
        = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
    char qual[30];

    decode_qual(tst1, sizeof(tst1), qual);
    REQUIRE_EQUAL(strcmp(qual, "!\"#$"), 0);
    decode_qual(tst2, sizeof(tst2), qual);
    REQUIRE_EQUAL(strcmp(qual, "*"), 0);
    decode_qual(tst3, sizeof(tst3), qual);
    REQUIRE_EQUAL(strcmp(qual, "!\"#$%&'()*+"), 0);
    decode_qual(tst4, sizeof(tst4), qual);
    REQUIRE_EQUAL(strcmp(qual, "!\"#$%&'()"), 0);
    decode_qual(tst5, sizeof(tst5), qual);
    REQUIRE_EQUAL(strcmp(qual, "!\"#$%&'()*+,-./0"), 0);
    decode_qual(tst6, sizeof(tst6), qual);
    REQUIRE_EQUAL(strcmp(qual, "!\"#$%&'()*+,-./01"), 0);
    decode_qual(tst7, sizeof(tst7), qual);
    REQUIRE_EQUAL(strcmp(qual, "!\"#$%&'()*+,-./012"), 0);
}

TEST_CASE(Fast_Sequence)
{
    unsigned char test[]
        = {0x00, 0xFF, 0x01, 0x10, 0x0F, 0xF0, 0x00, 0x01, 0x01, 0x01};
    char dest[30];

    decode_seq(test, 0, dest);
    REQUIRE_EQUAL(strcmp(dest, ""), 0);
    decode_seq(test, 1, dest);
    REQUIRE_EQUAL(strcmp(dest, "="), 0);
    decode_seq(test, 2, dest);
    REQUIRE_EQUAL(strcmp(dest, "=="), 0);
    decode_seq(test, 3, dest);
    REQUIRE_EQUAL(strcmp(dest, "==N"), 0);
    decode_seq(test, 4, dest);
    REQUIRE_EQUAL(strcmp(dest, "==NN"), 0);
    decode_seq(test, 5, dest);
    REQUIRE_EQUAL(strcmp(dest, "==NN="), 0);
    decode_seq(test, 20, dest);
    REQUIRE_EQUAL(strcmp(dest, "==NN=AA==NN====A=A=A"), 0);
}

TEST_CASE(Fast_u32toa)
{
    static const u32 tsts[]
        = {0,          1,          2,           9,           10,
           11,         99,         100,         101,         110,
           199,        200,        201,         999,         1000,
           1001,       1010,       1019,        1900,        1988,
           1990,       1992,       2003,        2005,        9999,
           10000,      10001,      10099,       10002,       10009,
           10010,      100110,     10100,       10101,       10999,
           101010,     99999,      20030613,    20050722,    100000,
           1000000,    10000000,   10000000,    100000000,   1000000000,
           2000000000, 2147483647, 2147483648l, 4294967294l, 4294967295l};
    for (int i = 0; i != sizeof(tsts) / sizeof(tsts[0]); ++i)
        REQUIRE_EQUAL(tst_fast_u32toa(tsts[i]), true);

    for (int i = 0; i != NUM_RAND; ++i)
        REQUIRE_EQUAL(tst_fast_u32toa(random() + random()), true);

#ifdef TEST_ALL_THE_INTEGERS
    fprintf(stderr, "all u32toa\n");
    for (u32 u = 0; u != UINT32_MAX; ++u)
        REQUIRE_EQUAL(tst_fast_u32toa(u), true);
#endif
}

TEST_CASE(Fast_i32toa)
{
    static const i32 tsts[]
        = {0,         1,          2,          9,          10,        11,
           99,        199,        200,        999,        1000,      9999,
           10000,     99999,      100000,     1000000,    10000000,  10000000,
           100000000, 1000000000, 2000000000, 2147483646, 2147483647};

    for (int i = 0; i != sizeof(tsts) / sizeof(tsts[0]); ++i) {
        REQUIRE_EQUAL(tst_fast_i32toa(tsts[i]), true);
        REQUIRE_EQUAL(tst_fast_i32toa(-tsts[i]), true);
    }

    for (int i = 0; i != NUM_RAND; ++i) {
        REQUIRE_EQUAL(tst_fast_i32toa(random()), true);
        REQUIRE_EQUAL(tst_fast_i32toa(-random()), true);
    }

#ifdef TEST_ALL_THE_INTEGERS
    fprintf(stderr, "all i32toa\n");
    for (int i = INT32_MIN; i != INT32_MAX; ++i)
        REQUIRE_EQUAL(tst_fast_i32toa(i), true);
#endif
}

static bool tst_strtoi64(const char* str)
{
    i64 slow = strtoi64(str, NULL, 10);
    i64 fast = fast_strtoi64(str);
    if (fast != slow) {
        fprintf(stderr, "mismatch '%s' %ld %ld\n", str, slow, fast);
        return false;
    }
    return true;
}

TEST_CASE(Fast_strtoi64)
{
    static const char* tsts[]
        = {"0",          "1",          "9",          "10",
           "99",         "100",        "101",        "199",
           "999",        "999999",     "1000000",    "100000000",
           "2147483645", "2147483646", "8589934592", "9223372036854775807"};
    char str[32];

    for (int i = 0; i != sizeof(tsts) / sizeof(tsts[0]); ++i) {
        REQUIRE_EQUAL(tst_strtoi64(tsts[i]), true);
        sprintf(str, "-%s", tsts[i]);
        REQUIRE_EQUAL(tst_strtoi64(str), true);
    }

    for (int i = 0; i != NUM_RAND; ++i) {
        sprintf(str, "%ld", (random() << 33) + (random() << 16) + random());
        REQUIRE_EQUAL(tst_strtoi64(str), true);
    }

#ifdef TEST_ALL_THE_INTEGERS
    fprintf(stderr, "all strtoi32\n");
    for (long long i = 2l * INT32_MIN; i != 2l * INT32_MAX; ++i) {
        sprintf(str, "%lld", i);
        REQUIRE_EQUAL(tst_strtoi64(str), true);
    }
#endif
}

TEST_CASE(Decode_Cigar)
{
    pool_init();

    u32 incigar1[] = {0x10, 0x21, 0x38};
    char* outcigar; // pool allocated
    outcigar = decode_cigar(incigar1, sizeof(incigar1) / sizeof(incigar1[0]));
    REQUIRE_EQUAL(strcmp("1M2I3X", outcigar), 0);

    u32 incigar2[] = {0xfffffff0};
    outcigar = decode_cigar(incigar2, sizeof(incigar2) / sizeof(incigar2[0]));
    REQUIRE_EQUAL(strcmp("268435455M", outcigar), 0);

    pool_destroy();
}

TEST_CASE(Check_Cigar)
{
    pool_init();
    REQUIRE_EQUAL(check_cigar("1M", "AA"), false);
    REQUIRE_EQUAL(check_cigar("1M", "A"), true);
    REQUIRE_EQUAL(check_cigar("2M", "AA"), true);
    REQUIRE_EQUAL(check_cigar("*", "AAAAA"), true);
    REQUIRE_EQUAL(check_cigar("1H2M", "AA"), true);
    REQUIRE_EQUAL(check_cigar("2M1H", "AA"), true);
    REQUIRE_EQUAL(check_cigar("1H2M1H", "AA"), true);
    REQUIRE_EQUAL(check_cigar("2M1H1M", "AA"), false);
    REQUIRE_EQUAL(check_cigar("1H3M1H", "AAA"), true);
    REQUIRE_EQUAL(check_cigar("2M1S1H", "AAA"), true);
    REQUIRE_EQUAL(check_cigar("1M1S1H1M", "AA"), false);
    REQUIRE_EQUAL(check_cigar("1M1S1M", "AA"), false);
    REQUIRE_EQUAL(check_cigar("3S9H", "AAA"), false);
    REQUIRE_EQUAL(check_cigar("3S3M9H", "AAAAAA"), true);
    REQUIRE_EQUAL(check_cigar("3S9D", "AAA"), true);
    REQUIRE_EQUAL(check_cigar("1H1S1M1S1H", "AAA"), true);
    REQUIRE_EQUAL(check_cigar("1H1S1M1H", "AA"), true);
    REQUIRE_EQUAL(check_cigar("1H1M1S1H", "AA"), true);
    REQUIRE_EQUAL(check_cigar("1H1M1H", "A"), true);
    REQUIRE_EQUAL(check_cigar("1S1M1H", "AA"), true);
    REQUIRE_EQUAL(check_cigar("1H1M1S", "AA"), true);
    REQUIRE_EQUAL(check_cigar("1H1M1H1S", "AA"), false);
    REQUIRE_EQUAL(check_cigar("1H1S1M1S", "AAA"), true);
    REQUIRE_EQUAL(check_cigar("1M1H", "A"), true);
    REQUIRE_EQUAL(check_cigar("1M1S", "AA"), true);
    REQUIRE_EQUAL(check_cigar("1H1M", "A"), true);
    REQUIRE_EQUAL(check_cigar("1S1M", "AA"), true);
    REQUIRE_EQUAL(check_cigar("1S1H1M", "AA"), false);
    REQUIRE_EQUAL(check_cigar("1M1H1S", "AA"), false);
    REQUIRE_EQUAL(check_cigar("1S1M1S", "AAA"), true);
    REQUIRE_EQUAL(check_cigar("1H1M1S", "AA"), true);
    REQUIRE_EQUAL(check_cigar("1S1M1H", "AA"), true);
    REQUIRE_EQUAL(check_cigar("1S1S1M1H", "AAA"), true);
    REQUIRE_EQUAL(check_cigar("1H1H1M1H", "A"), true);
    REQUIRE_EQUAL(check_cigar("1M1H1H", "A"), true);
    REQUIRE_EQUAL(check_cigar("1H1H", "A"), false);
    REQUIRE_EQUAL(check_cigar("1S1H", "AA"), false);
    REQUIRE_EQUAL(check_cigar("1H1S", "AA"), false);
    REQUIRE_EQUAL(check_cigar("1H", "A"), false);
    pool_destroy();
}

TEST_CASE(In_Range)
{
    REQUIRE_EQUAL(inrange("0", 0, 0), true);
    REQUIRE_EQUAL(inrange("0", 1, 0), false);
    REQUIRE_EQUAL(inrange("0", 1, 2), false);
    REQUIRE_EQUAL(inrange("0", -1, -1), false);
}

TEST_CASE(Is_MD5)
{
    REQUIRE_EQUAL(ismd5(""), false);
    REQUIRE_EQUAL(ismd5("A"), false);
    REQUIRE_EQUAL(ismd5("-"), false);
    REQUIRE_EQUAL(ismd5("7-ce4e3aa8b07d1f448ace526a3977fd"), false);
    REQUIRE_EQUAL(ismd5("7fce4e3aa8b07d1f448ace526a3977fd"), true);
    REQUIRE_EQUAL(ismd5("7fce4e3aa8b07d1f448ace526a3977f*"), true);
}

TEST_CASE(Is_floworder)
{
    REQUIRE_EQUAL(isfloworder("*"), true);
    REQUIRE_EQUAL(isfloworder("ACMGRSVTWYHKDBN"), true);
    REQUIRE_EQUAL(isfloworder("0"), false);
    REQUIRE_EQUAL(isfloworder("a"), false);
}

TEST_CASE(header1)
{
    SAMExtractor* extractor = NULL;
    const char* header_text
        = "@HD\tVN:1.5\tSO:coordinate\n"
          "@SQ\tSN:test\tLN:45\n"
          "r001\t99\ttest\t1\t30\t16M\t=\t37\t39\tTAGATAAAGGATACTG\t*"
          "\n"
          "r002\t99\ttest\t20\t30\t2M3M3M\t=\t50\t20\tTAGCATAT\t*\n"
          "r003\t99\ttest\t30\t30\t2M3I3M\t=\t70\t20\tAGATTACA\t*\n";
    REQUIRE_RC(extract_buf(header_text, strlen(header_text), &extractor));
    Vector headers;
    REQUIRE_RC(SAMExtractorGetHeaders(extractor, &headers));
    u32 numheaders = VectorLength(&headers);
    REQUIRE_EQUAL(numheaders, (uint32_t)2);

    Header* hdr = (Header*)VectorGet(&headers, 0);
    REQUIRE_EQUAL(strcmp(hdr->headercode, "HD"), 0);
    Vector* tvs = &hdr->tagvalues;
    REQUIRE_EQUAL(VectorLength(tvs), (uint32_t)2);
    TagValue* tv = (TagValue*)VectorGet(tvs, 0);
    REQUIRE_EQUAL(strcmp(tv->tag, "VN"), 0);
    REQUIRE_EQUAL(strcmp(tv->value, "1.5"), 0);
    tv = (TagValue*)VectorGet(tvs, 1);
    REQUIRE_EQUAL(strcmp(tv->tag, "SO"), 0);
    REQUIRE_EQUAL(strcmp(tv->value, "coordinate"), 0);

    hdr = (Header*)VectorGet(&headers, 1);
    REQUIRE_EQUAL(strcmp(hdr->headercode, "SQ"), 0);
    tvs = &hdr->tagvalues;
    REQUIRE_EQUAL(VectorLength(tvs), (uint32_t)2);
    tv = (TagValue*)VectorGet(tvs, 0);
    REQUIRE_EQUAL(strcmp(tv->tag, "SN"), 0);
    REQUIRE_EQUAL(strcmp(tv->value, "test"), 0);
    tv = (TagValue*)VectorGet(tvs, 1);
    REQUIRE_EQUAL(strcmp(tv->tag, "LN"), 0);
    REQUIRE_EQUAL(strcmp(tv->value, "45"), 0);

    Vector alignments;
    REQUIRE_RC(SAMExtractorGetAlignments(extractor, &alignments));
    u32 numalignments = VectorLength(&alignments);
    REQUIRE_EQUAL(numalignments, (uint32_t)3);

    Alignment* align;
    align = (Alignment*)VectorGet(&alignments, 0);
    REQUIRE_EQUAL(strcmp(align->qname, "r001"), 0);
    REQUIRE_EQUAL(align->flags, (uint16_t)99);
    REQUIRE_EQUAL(strcmp(align->rname, "test"), 0);
    REQUIRE_EQUAL(align->pos, (int32_t)1);
    REQUIRE_EQUAL(align->mapq, (uint8_t)30);
    REQUIRE_EQUAL(strcmp(align->cigar, "16M"), 0);
    REQUIRE_EQUAL(strcmp(align->rnext, "test"), 0);
    REQUIRE_EQUAL(align->pnext, (int32_t)37);
    REQUIRE_EQUAL(align->tlen, (int32_t)39);
    REQUIRE_EQUAL(strcmp(align->read, "TAGATAAAGGATACTG"), 0);
    REQUIRE_EQUAL(strcmp(align->qual, "*"), 0);

    align = (Alignment*)VectorGet(&alignments, 1);
    REQUIRE_EQUAL(strcmp(align->qname, "r002"), 0);
    REQUIRE_EQUAL(align->flags, (uint16_t)99);
    REQUIRE_EQUAL(strcmp(align->rname, "test"), 0);
    REQUIRE_EQUAL(align->pos, (int32_t)20);
    REQUIRE_EQUAL(align->mapq, (uint8_t)30);
    REQUIRE_EQUAL(strcmp(align->cigar, "2M3M3M"), 0);
    REQUIRE_EQUAL(strcmp(align->rnext, "test"), 0);
    REQUIRE_EQUAL(align->pnext, (int32_t)50);
    REQUIRE_EQUAL(align->tlen, (int32_t)20);
    REQUIRE_EQUAL(strcmp(align->read, "TAGCATAT"), 0);
    REQUIRE_EQUAL(strcmp(align->qual, "*"), 0);

    REQUIRE_RC(SAMExtractorRelease(extractor));
}

TEST_CASE(BAMfile)
{
    //$ xxd -i test.bam
    static unsigned const char test_bam[]
        = {0x1f, 0x8b, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x06,
           0x00, 0x42, 0x43, 0x02, 0x00, 0x59, 0x00, 0x73, 0x72, 0xf4, 0x65,
           0xd4, 0x66, 0x60, 0x60, 0x70, 0xf0, 0x70, 0xe1, 0x0c, 0xf3, 0xb3,
           0x32, 0xd4, 0x33, 0xe5, 0x0c, 0xf6, 0xb7, 0x4a, 0xce, 0xcf, 0x2f,
           0x4a, 0xc9, 0xcc, 0x4b, 0x2c, 0x49, 0xe5, 0x72, 0x08, 0x0e, 0xe4,
           0x0c, 0xf6, 0xb3, 0x2a, 0x49, 0x2d, 0x2e, 0xe1, 0xf4, 0xf1, 0xb3,
           0x32, 0x31, 0xe5, 0x62, 0x04, 0x2a, 0x67, 0x05, 0x62, 0x90, 0x10,
           0x83, 0x2e, 0x90, 0x01, 0x00, 0xb4, 0xdf, 0xf9, 0x29, 0x44, 0x00,
           0x00, 0x00, 0x1f, 0x8b, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
           0xff, 0x06, 0x00, 0x42, 0x43, 0x02, 0x00, 0x74, 0x00, 0x73, 0x64,
           0x40, 0x00, 0x56, 0x39, 0x4f, 0x21, 0x46, 0x86, 0x64, 0x06, 0x01,
           0x28, 0x5f, 0x05, 0x88, 0xd5, 0x81, 0xb8, 0xc8, 0xc0, 0xc0, 0x90,
           0x81, 0x81, 0x91, 0x81, 0xa1, 0xd1, 0xb1, 0x51, 0xd0, 0x45, 0x42,
           0xa8, 0xe5, 0x3f, 0x1a, 0xb0, 0x85, 0x6a, 0x10, 0x86, 0x1a, 0xc2,
           0x0c, 0x34, 0x84, 0x03, 0x2a, 0x06, 0xd4, 0xc9, 0x20, 0x02, 0x31,
           0xc4, 0x88, 0x41, 0x01, 0xc8, 0x30, 0x80, 0xe2, 0x46, 0x27, 0x09,
           0x09, 0x74, 0x03, 0x64, 0xb1, 0x18, 0xe0, 0x8a, 0x30, 0xc0, 0x18,
           0x6c, 0x80, 0x21, 0xd4, 0x00, 0x11, 0x89, 0x46, 0x45, 0x98, 0x01,
           0x00, 0x36, 0x21, 0x0c, 0x91, 0xc7, 0x00, 0x00, 0x00, 0x1f, 0x8b,
           0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x06, 0x00, 0x42,
           0x43, 0x02, 0x00, 0x1b, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x00};
    unsigned int test_bam_len = 235;

    SAMExtractor* extractor = NULL;

    REQUIRE_RC(extract_buf((const char*)test_bam, test_bam_len, &extractor));
    Vector headers;
    REQUIRE_RC(SAMExtractorGetHeaders(extractor, &headers));
    u32 numheaders = VectorLength(&headers);
    REQUIRE_EQUAL(numheaders, (uint32_t)2);

    Header* hdr = (Header*)VectorGet(&headers, 0);
    REQUIRE_EQUAL(strcmp(hdr->headercode, "HD"), 0);
    Vector* tvs = &hdr->tagvalues;
    REQUIRE_EQUAL(VectorLength(tvs), (uint32_t)2);
    TagValue* tv = (TagValue*)VectorGet(tvs, 0);
    REQUIRE_EQUAL(strcmp(tv->tag, "VN"), 0);
    REQUIRE_EQUAL(strcmp(tv->value, "1.5"), 0);
    tv = (TagValue*)VectorGet(tvs, 1);
    REQUIRE_EQUAL(strcmp(tv->tag, "SO"), 0);
    REQUIRE_EQUAL(strcmp(tv->value, "coordinate"), 0);

    hdr = (Header*)VectorGet(&headers, 1);
    REQUIRE_EQUAL(strcmp(hdr->headercode, "SQ"), 0);
    tvs = &hdr->tagvalues;
    REQUIRE_EQUAL(VectorLength(tvs), (uint32_t)2);
    tv = (TagValue*)VectorGet(tvs, 0);
    REQUIRE_EQUAL(strcmp(tv->tag, "SN"), 0);
    REQUIRE_EQUAL(strcmp(tv->value, "test"), 0);
    tv = (TagValue*)VectorGet(tvs, 1);
    REQUIRE_EQUAL(strcmp(tv->tag, "LN"), 0);
    REQUIRE_EQUAL(strcmp(tv->value, "45"), 0);

    Vector alignments;
    REQUIRE_RC(SAMExtractorGetAlignments(extractor, &alignments));
    u32 numalignments = VectorLength(&alignments);
    REQUIRE_EQUAL(numalignments, (uint32_t)3);

    Alignment* align;
    align = (Alignment*)VectorGet(&alignments, 0);
    REQUIRE_EQUAL(strcmp(align->qname, "r001"), 0);
    REQUIRE_EQUAL(align->flags, (uint16_t)99);
    REQUIRE_EQUAL(strcmp(align->rname, "test"), 0);
    REQUIRE_EQUAL(align->pos, (int32_t)1);
    REQUIRE_EQUAL(align->mapq, (uint8_t)30);
    REQUIRE_EQUAL(strcmp(align->cigar, "16M"), 0);
    REQUIRE_EQUAL(strcmp(align->rnext, "test"), 0);
    REQUIRE_EQUAL(align->pnext, (int32_t)37);
    REQUIRE_EQUAL(align->tlen, (int32_t)39);
    REQUIRE_EQUAL(strcmp(align->read, "TAGATAAAGGATACTG"), 0);
    REQUIRE_EQUAL(strcmp(align->qual, "*"), 0);

    align = (Alignment*)VectorGet(&alignments, 1);
    REQUIRE_EQUAL(strcmp(align->qname, "r002"), 0);
    REQUIRE_EQUAL(align->flags, (uint16_t)99);
    REQUIRE_EQUAL(strcmp(align->rname, "test"), 0);
    REQUIRE_EQUAL(align->pos, (int32_t)20);
    REQUIRE_EQUAL(align->mapq, (uint8_t)30);
    REQUIRE_EQUAL(strcmp(align->cigar, "2M3M3M"), 0);
    REQUIRE_EQUAL(strcmp(align->rnext, "test"), 0);
    REQUIRE_EQUAL(align->pnext, (int32_t)50);
    REQUIRE_EQUAL(align->tlen, (int32_t)20);
    REQUIRE_EQUAL(strcmp(align->read, "TAGCATAT"), 0);
    REQUIRE_EQUAL(strcmp(align->qual, "*"), 0);

    REQUIRE_RC(SAMExtractorRelease(extractor));
}

#if 0
TEST_CASE(SAMfile)
{
    const char* fname = "small.sam";

    struct KDirectory* srcdir = NULL;
    const struct KFile* infile = NULL;
    REQUIRE_RC(KDirectoryNativeDir(&srcdir));

    REQUIRE_RC(KDirectoryOpenFileRead(srcdir, &infile, fname));
    KDirectoryRelease(srcdir);

    String sfname;
    StringInitCString(&sfname, fname);

    SAMExtractor* extractor = NULL;
    REQUIRE_RC(SAMExtractorMake(&extractor, infile, &sfname, -1));

    Vector headers;
    REQUIRE_RC(SAMExtractorGetHeaders(extractor, &headers));
    SAMExtractorInvalidateHeaders(extractor);

    int total = 0;
    uint32_t vlen;
    do {
        Vector alignments;
        REQUIRE_RC(SAMExtractorGetAlignments(extractor, &alignments));
        vlen = VectorLength(&alignments);
        total += vlen;
        SAMExtractorInvalidateAlignments(extractor);
    } while (vlen);
    REQUIRE_EQUAL(total, 9956);

    REQUIRE_RC(SAMExtractorRelease(extractor));

    KFileRelease(infile);
}
#endif

TEST_CASE(Filter)
{
    String filter_rname;
    String other;
    StringInitCString(&filter_rname, "r001");
    StringInitCString(&other, "r002");
    SAMExtractor extractor;
    extractor.filter_rname = &filter_rname;
    extractor.filter_pos = -1;
    extractor.filter_length = 10;

    REQUIRE_EQUAL(filter(&extractor, &filter_rname, 0), true);
    REQUIRE_EQUAL(filter(&extractor, &other, -1), false);
    REQUIRE_EQUAL(filter(&extractor, &other, 0), false);
    extractor.filter_pos = 5;
    REQUIRE_EQUAL(filter(&extractor, &filter_rname, 4), true);
    REQUIRE_EQUAL(filter(&extractor, &filter_rname, 20), true);
}

TEST_CASE(Fuzz_Hangs)
{
    glob_t globbuf;

    if (glob("fuzz/*", GLOB_ERR, NULL, &globbuf)) {
        fprintf(stderr, "Couldn't glob fuzz/*");
    }
    REQUIRE_EQUAL((int)globbuf.gl_pathc, 66);
    KOutMsg("None of below files should hang:\n");
    for (size_t i = 0; i != globbuf.gl_pathc; ++i) {
        KOutMsg("\t%s\n", globbuf.gl_pathv[i]);
        SAMExtractor extractor;
        SAMExtractor* e = &extractor;
        rc_t rc = extract_file(globbuf.gl_pathv[i], &e);
        REQUIRE_RC(rc);
        pool_destroy();
    }
    KOutMsg("\n");

    globfree(&globbuf);
}

// TODO: mempool, how to test an allocator?
// TODO: negative tests, syntax errors

extern "C" {
ver_t CC KAppVersion(void) { return 0x1000000; }
rc_t CC UsageSummary(const char* progname) { return 0; }

rc_t CC Usage(const Args* args) { return 0; }

const char UsageDefaultName[] = "test-samextract";

rc_t CC KMain(int argc, char* argv[])
{
    srandom(time(NULL));
    rc_t rc = SAMExtractTestSuite(argc, argv);
    return rc;
}
}
