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

/**
* Unit tests for FASTQ loader
*/
#include <ktst/unit_test.hpp>
#include <klib/rc.h>
#include <loader/common-writer.h>
#include <kfs/directory.h>
#include <kfs/file.h>

#include "../../tools/loaders/fastq-loader/fastq-parse.h"
#include "../../tools/loaders/fastq-loader/fastq-tokens.h"
#include "../../tools/loaders/fastq-loader/fastq-reader.h"

#include <sysalloc.h>
#include <stdlib.h>
#include <cstring>
#include <stdexcept>
#include <list>

using namespace std;

TEST_SUITE(FastqLoaderWbTestSuite);

class LoaderFixture
{
public:
    LoaderFixture()
    :   wd(0), rf(0),
        record(0), seq(0), reject(0),
        read(0), readLength(0),
        name(0), length(0),
        errorText(0), errorLine(0), column(0),
        quality(0), qualityAsciiOffset(0), qualityType(-1),
        qualityFormat(FASTQphred33), defaultReadNumber(0),
        ignoreSpotGroups(false),
        flexDebug(false)
    {
        if ( KDirectoryNativeDir ( & wd ) != 0 )
            FAIL("KDirectoryNativeDir failed");
    }
    ~LoaderFixture()
    {
        delete [] read;

        if (seq != 0 && SequenceRelease(seq) != 0)
            FAIL("SequenceRelease failed");

        if (record != 0 && RecordRelease(record) != 0)
            FAIL("RecordRelease failed");

        if (reject != 0 && RejectedRelease(reject) != 0)
            FAIL("RejectedRelease failed");

        if ( rf != 0 && ReaderFileRelease( rf ) != 0)
            FAIL("ReaderFileRelease failed");

        if ( !filename.empty() && KDirectoryRemove(wd, true, filename.c_str()) != 0)
            FAIL("KDirectoryRemove failed");

        if ( KDirectoryRelease ( wd ) != 0 )
            FAIL("KDirectoryRelease failed");

        FASTQ_debug = 0;
    }
    rc_t CreateFile(const char* p_filename, const char* contents)
    {   // create and open for read
        KFile* file;
        filename=p_filename;
        rc_t rc=KDirectoryCreateFile(wd, &file, true, 0664, kcmInit, p_filename);
        if (rc == 0)
        {
            size_t num_writ=0;
            rc=KFileWrite(file, 0, contents, strlen(contents), &num_writ);
            if (rc == 0)
            {
                rc=KFileRelease(file);
            }
            else
            {
                KFileRelease(file);
            }
            file=0;
        }
        return FastqReaderFileMake(&rf, wd, p_filename, qualityFormat, defaultReadNumber, ignoreSpotGroups, flexDebug);
    }
    void CreateFileGetRecord(const char* fileName, const char* contents)
    {
        if (record != 0 && RecordRelease(record) != 0)
            throw logic_error("CreateFileGetRecord: RecordRelease failed");

        if (CreateFile(fileName, contents) != 0)
            throw logic_error("CreateFileGetRecord: CreateFile failed");

        if (ReaderFileGetRecord(rf, &record) != 0 || record == 0)
            throw logic_error("CreateFileGetRecord: ReaderFileGetRecord failed");
    }
    bool GetRecord()
    {
        if (rf == 0)
            return false;

        if (record != 0 && RecordRelease(record) != 0)
            return false;
        if (reject != 0 && RejectedRelease(reject) != 0)
            return false;
        reject = 0;
        if (seq != 0 && SequenceRelease(seq) != 0)
            return false;
        seq = 0;
        record = 0;

        return ReaderFileGetRecord(rf, &record) == 0;
    }

    bool GetRejected()
    {
        if (reject != 0 && RejectedRelease(reject) != 0)
            throw logic_error("GetRejected: RejectedRelease failed");
        if (record == 0)
            throw logic_error("GetRejected: record == 0");
        if (RecordGetRejected(record, &reject) != 0)
            throw logic_error("GetRejected: RecordGetRejected failed");
        if (reject == 0)
            return false;
        if (RejectedGetError(reject, &errorText, &errorLine, &column, &fatal) != 0)
            throw logic_error("IsFatal: RejectedGetError failed");
        return true;
    }

    bool CreateFileGetSequence(const char* name, const char* contents)
    {
        if (CreateFile(name, contents) != 0)
            throw logic_error("CreateFileGetSequence: CreateFile failed");

        if (!GetRecord() || record == 0)
            throw logic_error("CreateFileGetSequence: GetRecord failed");

        if (GetRejected())
            throw logic_error(string("CreateFileGetSequence: record rejected, ")+string(errorText));

        if (RecordGetSequence(record, &seq) != 0)
            throw logic_error("CreateFileGetSequence: RecordGetSequence failed:");

        return seq != 0;
    }

    bool MakeReadBuffer()
    {
        if (SequenceGetReadLength(seq, &readLength) != 0 || readLength == 0)
            return false;

        delete [] read;
        read = new char[readLength];
        return true;
    }

    void BisonDebugOn()
    {
        FASTQ_debug = 1;
    }

    void FlexDebugOn()
    {   // call before CreateFile()
        flexDebug = true;
    }

    KDirectory* wd;
    string filename;
    const ReaderFile* rf;
    const Record* record;
    const Sequence* seq;
    const Rejected* reject;

    char* read;
    uint32_t readLength;

    const char* name;
    size_t length;

    const char* errorText;
    uint64_t errorLine;
    uint64_t column;
    bool fatal;
    const void* errorData;

    const int8_t* quality;
    uint8_t qualityAsciiOffset;
    int qualityType;

    enum FASTQQualityFormat qualityFormat;
    int8_t defaultReadNumber;
    bool ignoreSpotGroups;

    bool flexDebug;
};

///////////////////////////////////////////////// FASTQ test cases
FIXTURE_TEST_CASE(EmptyFile, LoaderFixture)
{
    REQUIRE_RC(CreateFile(GetName(), ""));
    REQUIRE_EQ( string(ReaderFileGetPathname(rf)), string(GetName()) );
    REQUIRE(GetRecord());
    REQUIRE_NULL(record);
}

FIXTURE_TEST_CASE(EndLines, LoaderFixture)
{
    REQUIRE_RC(CreateFile(GetName(), "\n\n"));
    REQUIRE_EQ( string(ReaderFileGetPathname(rf)), string(GetName()) );
    REQUIRE(GetRecord());
    REQUIRE_NULL(record);
}

//////////////////// syntax errors and recovery
const string SyntaxError("syntax error");

FIXTURE_TEST_CASE(SyntaxError1, LoaderFixture)
{
    string input="qqq abcd";

    CreateFileGetRecord(GetName(), input.c_str());

    REQUIRE(GetRejected());
    REQUIRE(!fatal);
    REQUIRE_NOT_NULL(errorText);
    REQUIRE_EQ(SyntaxError, string (errorText).substr(0, SyntaxError.size()));
    REQUIRE_EQ(errorLine, (uint64_t)1);
    REQUIRE_EQ(column, (uint64_t)4);

    const void* data;
    REQUIRE_RC(RejectedGetData(reject, &data, &length));
    REQUIRE_NOT_NULL(data);
    REQUIRE_EQ(input, string((const char*)data, length));
}

FIXTURE_TEST_CASE(SyntaxError2, LoaderFixture)
{   // a good record followed by junk
    #define input "qqq abcd"

    CreateFileGetRecord(GetName(), "@SEQ_ID1\n" "GATT\n" "+\n" "!''*\n" input );
    REQUIRE(GetRecord()); // skip the well formed record

    REQUIRE(GetRejected());
    REQUIRE(!fatal);
    REQUIRE_NOT_NULL(errorText);
    REQUIRE_EQ(SyntaxError, string (errorText).substr(0, SyntaxError.size()));
    REQUIRE_EQ(errorLine, (uint64_t)5);
    REQUIRE_EQ(column, (uint64_t)4);

    const void* data;
    REQUIRE_RC(RejectedGetData(reject, &data, &length));
    REQUIRE_EQ(string(input), string((const char*)data, length));


    #undef input
}

FIXTURE_TEST_CASE(RecoveryFromErrorAtTopLevel, LoaderFixture)
{
    CreateFileGetRecord(GetName(), "qqq abcd\n" "@SEQ_ID1\n" "GATT\n" "+\n" "!''*\n");
    REQUIRE(GetRejected());

    REQUIRE(GetRecord());
    REQUIRE_NOT_NULL(record);

    REQUIRE_RC(RecordGetSequence(record, &seq));
    REQUIRE_NOT_NULL(seq);

    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("SEQ_ID1"), string(name, length));
}

FIXTURE_TEST_CASE(RecoveryFromErrorInHeader, LoaderFixture)
{
    CreateFileGetRecord(GetName(), "@SEQ_ID1^\n" "GATT\n" "+\n" "!''*\n" "@SEQ_ID2\n" "GATT\n" "+\n" "!''*\n");
    REQUIRE(GetRejected());

    REQUIRE(GetRecord());
    REQUIRE_NOT_NULL(record);

    REQUIRE_RC(RecordGetSequence(record, &seq));
    REQUIRE_NOT_NULL(seq);

    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("SEQ_ID2"), string(name, length));
}

FIXTURE_TEST_CASE(RecoveryFromErrorInRead, LoaderFixture)
{
    CreateFileGetRecord(GetName(), "@SEQ_ID1\n" "G^ATT\n" "+\n" "!''*\n" "@SEQ_ID2\n" "GATT\n" "+\n" "!''*\n");
    REQUIRE(GetRejected());

    REQUIRE(GetRecord());
    REQUIRE_NOT_NULL(record);

    REQUIRE_RC(RecordGetSequence(record, &seq));
    REQUIRE_NOT_NULL(seq);

    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("SEQ_ID2"), string(name, length));
}

FIXTURE_TEST_CASE(RecoveryFromErrorInQuality, LoaderFixture)
{
    CreateFileGetRecord(GetName(), "@SEQ_ID1\n" "GATT\n" "+\n" "\n" "@SEQ_ID2\n" "GATT\n" "+\n" "!''*\n");
    REQUIRE(GetRejected());

    REQUIRE(GetRecord());
    REQUIRE_NOT_NULL(record);

    REQUIRE_RC(RecordGetSequence(record, &seq));
    REQUIRE_NOT_NULL(seq);

    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("SEQ_ID2"), string(name, length));
}

FIXTURE_TEST_CASE(ErrorLineNumber, LoaderFixture)
{
    CreateFileGetRecord(GetName(),
        "@HWI-ST959:56:D0AW4ACXX:8:2108:6958:112042 1:Y:0#\n"
        "ATT\n");
    REQUIRE(GetRejected());
    REQUIRE_EQ(SyntaxError, string (errorText).substr(0, SyntaxError.size()));
    REQUIRE_EQ(errorLine, (uint64_t)1);
    REQUIRE_EQ(column, (uint64_t)49);
}

//////////////////// tag line parsing
#define TEST_TAGLINE(line)\
    CreateFileGetRecord(GetName(), line "\n" "GATT\n" "+\n" "!''*\n");\
    REQUIRE(! GetRejected()); /* no error */ \
    REQUIRE(GetRecord()); /* parsing done */ \
    REQUIRE_NULL(record); /* input consumed */

FIXTURE_TEST_CASE(Tag1,   LoaderFixture)  { TEST_TAGLINE("@HWUSI-EAS499:1:3:9:1822"); }
FIXTURE_TEST_CASE(Tag2_1, LoaderFixture)  { TEST_TAGLINE("@HWUSI-EAS499:1:3:9:1822:.2"); }
FIXTURE_TEST_CASE(Tag2_2, LoaderFixture)  { TEST_TAGLINE("@HWUSI-EAS499:1:3:9:1822:1.2"); }
FIXTURE_TEST_CASE(Tag3,   LoaderFixture)  { TEST_TAGLINE("@BILLIEHOLIDAY_1_FC20F3DAAXX:8:2:342:540"); }
FIXTURE_TEST_CASE(Tag4,   LoaderFixture)  { TEST_TAGLINE("@BILLIEHOLIDAY_1_FC200TYAAXX_3_1_751_675"); }
FIXTURE_TEST_CASE(Tag5,   LoaderFixture)  { TEST_TAGLINE("@HWUSI-EAS499:1:3:9:1822#0/1"); }
FIXTURE_TEST_CASE(Tag6,   LoaderFixture)  { TEST_TAGLINE("@1:3:9:1822:33.44"); }
FIXTURE_TEST_CASE(Tag7,   LoaderFixture)  { TEST_TAGLINE("@1:3:9:1822#1/2"); }
FIXTURE_TEST_CASE(Tag8,   LoaderFixture)  { TEST_TAGLINE("@HWUSI-EAS499:1:3:9:1822#CAT/1"); }
FIXTURE_TEST_CASE(Tag9,   LoaderFixture)  { TEST_TAGLINE("@ERBRDQF01EGP9U"); }
FIXTURE_TEST_CASE(Tag10,  LoaderFixture)  { TEST_TAGLINE("@ID57_120908_30E4FAAXX:3:1:1772:953.1"); }
FIXTURE_TEST_CASE(Tag11,  LoaderFixture)  { TEST_TAGLINE(">ID57_120908_30E4FAAXX:3:1:1772:953"); }
FIXTURE_TEST_CASE(Tag12,  LoaderFixture)  { TEST_TAGLINE("@741:6:1:1204:10747"); }
FIXTURE_TEST_CASE(Tag13,  LoaderFixture)  { TEST_TAGLINE("@741:6:1:1204:10747/1"); }
FIXTURE_TEST_CASE(Tag14,  LoaderFixture)  { TEST_TAGLINE("@SNPSTER4_246_30GCDAAXX_PE:1:1:3:896/1 run=090102_SNPSTER4_0246_30GCDAAXX_PE"); }
FIXTURE_TEST_CASE(Tag15,  LoaderFixture)  { TEST_TAGLINE("@G15-D_3_1_903_603_0.81"); }

//////////////////// building Sequence objects
FIXTURE_TEST_CASE(NotRejected, LoaderFixture)
{
    CreateFileGetRecord(GetName(), "@HWUSI-EAS499:1:3:9:1822:1.2\n" "GATT\n" "+\n" "!''*\n");
    reject = (const Rejected*)1;
    REQUIRE_RC(RecordGetRejected(record, &reject));
    REQUIRE_NULL(reject);
}

FIXTURE_TEST_CASE(SequenceNoAlignment, LoaderFixture)
{
    CreateFileGetRecord(GetName(), "@HWUSI-EAS499:1:3:9:1822:1.2\n" "GATT\n" "+\n" "!''*\n");

    const Alignment* align;
    REQUIRE_RC(RecordGetAlignment(record, &align));
    REQUIRE_NULL(align);

    REQUIRE_RC(RecordGetSequence(record, &seq));
    REQUIRE_NOT_NULL(seq);
}

FIXTURE_TEST_CASE(TestSequenceGetReadLength, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "GATT\n" "+\n" "!''*\n" ));
    REQUIRE_RC(SequenceGetReadLength(seq, &readLength));
    REQUIRE_EQ(readLength, 4u);
}

FIXTURE_TEST_CASE(TestSequenceGetReadLength_MultiLine, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "GATT\nTAGGA\n" "+\n" "!''*\n" ));
    REQUIRE_RC(SequenceGetReadLength(seq, &readLength));
    REQUIRE_EQ(readLength, 9u);
}

FIXTURE_TEST_CASE(TestSequenceGetRead, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "GATT\n" "+\n" "!''*\n" ));
    REQUIRE(MakeReadBuffer());
    REQUIRE_RC(SequenceGetRead(seq, read));
    REQUIRE_EQ(string(read, readLength), string("GATT"));
}

FIXTURE_TEST_CASE(TestSequenceGetRead_MultiLine, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "AAAA\n" "GG\n" "+\n" "!''*\n" ));
    REQUIRE(MakeReadBuffer());
    REQUIRE_RC(SequenceGetRead(seq, read));
    REQUIRE_EQ(string(read, readLength), string("AAAAGG"));
}

FIXTURE_TEST_CASE(TestSequenceGetRead2, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "GATT\n" "+\n" "!''*\n" ));
    REQUIRE(MakeReadBuffer());

    REQUIRE_RC(SequenceGetRead2(seq, read, 0, 2));
    REQUIRE_EQ(string(read, 2), string("GA"));
}
FIXTURE_TEST_CASE(TestSequenceGetRead2_StopOutOfRange, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "GATT\n" "+\n" "!''*\n" ));
    REQUIRE(MakeReadBuffer());

    REQUIRE_RC_FAIL(SequenceGetRead2(seq, read, 2, 6));
}

FIXTURE_TEST_CASE(TestSequenceGetRead2_StartOutOfRange, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "GATT\n" "+\n" "!''*\n" ));
    REQUIRE(MakeReadBuffer());

    REQUIRE_RC_FAIL(SequenceGetRead2(seq, read, 20, 1));
}

FIXTURE_TEST_CASE(TestSequenceGetRead2_MultiLine_InsideFirstLine, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "ACG\nTGCA\n" "+\n" "!''*\n" ));
    REQUIRE(MakeReadBuffer());

    REQUIRE_RC(SequenceGetRead2(seq, read, 0, 2));
    REQUIRE_EQ(string(read, 2), string("AC"));
}
FIXTURE_TEST_CASE(TestSequenceGetRead2_MultiLine_InsideSecondLine, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "ACG\nTGCA\n" "+\n" "!''*\n" ));
    REQUIRE(MakeReadBuffer());

    REQUIRE_RC(SequenceGetRead2(seq, read, 4, 6));
    REQUIRE_EQ(string(read, 2), string("GC"));
}
FIXTURE_TEST_CASE(TestSequenceGetRead2_MultiLine_AcrossEOL, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "ACG\nTGCA\n" "+\n" "!''*\n" ));
    REQUIRE(MakeReadBuffer());

    REQUIRE_RC(SequenceGetRead2(seq, read, 2, 6));
    REQUIRE_EQ(string(read, 4), string("GTGC"));
}
FIXTURE_TEST_CASE(TestSequenceGetRead2_MultiLine_StopOutOfRange, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "ACG\nTGCA\n" "+\n" "!''*\n" ));
    REQUIRE(MakeReadBuffer());

    REQUIRE_RC_FAIL(SequenceGetRead2(seq, read, 2, 66));
}
FIXTURE_TEST_CASE(TestSequenceGetRead2_MultiLine_StartOutOfRange, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "ACG\nTGCA\n" "+\n" "!''*\n" ));
    REQUIRE(MakeReadBuffer());

    REQUIRE_RC_FAIL(SequenceGetRead2(seq, read, 20, 1));
}

FIXTURE_TEST_CASE(SequenceGetQuality33, LoaderFixture)
{
    qualityFormat = FASTQphred33;

    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "GATT\n" "+\n" "!''*\n" ));

    REQUIRE_RC(SequenceGetQuality(seq, &quality, &qualityAsciiOffset, &qualityType));
    REQUIRE_NOT_NULL(quality);
    uint32_t l;
    REQUIRE_RC(SequenceGetReadLength(seq, &l));
    REQUIRE_EQ(qualityAsciiOffset, (uint8_t)33);
    REQUIRE_EQ(qualityType, (int)QT_Phred);
    REQUIRE_EQ(quality[0],  (int8_t)'!');
}

FIXTURE_TEST_CASE(SequenceGetQuality64, LoaderFixture)
{
    qualityFormat = FASTQphred64;

    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "GATT\n" "+\n" "BBCC\n" ));

    REQUIRE_RC(SequenceGetQuality(seq, &quality, &qualityAsciiOffset, &qualityType));
    REQUIRE_NOT_NULL(quality);
    uint32_t l;
    REQUIRE_RC(SequenceGetReadLength(seq, &l));
    REQUIRE_EQ(qualityAsciiOffset, (uint8_t)64);
    REQUIRE_EQ(qualityType, (int)QT_Phred);
    REQUIRE_EQ(quality[0],  (int8_t)'B');
}

FIXTURE_TEST_CASE(SequenceGetQualityLogOdds, LoaderFixture)
{
    qualityFormat = FASTQlogodds;

    REQUIRE(CreateFileGetSequence(GetName(), "@SEQ_ID1\n" "GATT\n" "+\n" ";>@H\n" ));

    REQUIRE_RC(SequenceGetQuality(seq, &quality, &qualityAsciiOffset, &qualityType));
    REQUIRE_NOT_NULL(quality);
    uint32_t l;
    REQUIRE_RC(SequenceGetReadLength(seq, &l));
    REQUIRE_EQ(qualityAsciiOffset, (uint8_t)64);
    REQUIRE_EQ(qualityType, (int)QT_LogOdds);
    REQUIRE_EQ(quality[0],  (int8_t)';');
}

FIXTURE_TEST_CASE(SequenceBaseSpace, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@q\n" "GATC\n" "+\n" "!''*\n" ));
    REQUIRE(!SequenceIsColorSpace(seq));
    char k;
    REQUIRE_RC(SequenceGetCSKey(seq, &k)); // RC is 0 but k is undefined
}

FIXTURE_TEST_CASE(SequenceColorSpace, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@q\n" "G123\n" "+\n" "!''*\n" ));
    REQUIRE(SequenceIsColorSpace(seq));

    char k;
    REQUIRE_RC(SequenceGetCSKey(seq, &k));
    REQUIRE_EQ(k, 'G');

    uint32_t l;
    REQUIRE_RC(SequenceGetReadLength(seq, &l));
    REQUIRE_EQ(l, (uint32_t)0);
    REQUIRE_RC(SequenceGetCSReadLength(seq, &l));
    REQUIRE_EQ(l, (uint32_t)3);

    delete [] read;
    read = new char[l];
    REQUIRE_RC(SequenceGetCSRead(seq, read));
    REQUIRE_EQ(string(read, l), string("123"));

    REQUIRE_RC(SequenceGetCSQuality(seq, &quality, &qualityAsciiOffset, &qualityType));
    REQUIRE_NOT_NULL(quality);
    REQUIRE_EQ(qualityType, (int)QT_Phred);
    REQUIRE_EQ(qualityAsciiOffset,   (uint8_t)33);
    REQUIRE_EQ(quality[0],  (int8_t)'\'');
    REQUIRE_EQ(quality[1],  (int8_t)'\'');
    REQUIRE_EQ(quality[2],  (int8_t)'*' );
}

FIXTURE_TEST_CASE(SequenceGetOrientationIsReverse, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@q\n" "G123\n" "+\n" "!''*\n" ));
    REQUIRE_EQ(SequenceGetOrientationSelf(seq), (int)ReadOrientationUnknown);
    REQUIRE_EQ(SequenceGetOrientationMate(seq), (int)ReadOrientationUnknown);
}

//  Read Number
FIXTURE_TEST_CASE(ReadNumber, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@HWUSI-EAS499:1:3:9:1822#0/1\n" "GATT\n" "+\n" "!''*\n"));
    REQUIRE(SequenceIsFirst(seq));
    REQUIRE(!SequenceIsSecond(seq));
}
FIXTURE_TEST_CASE(ReadNumberMissing, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@HWUSI-EAS499:1:3:9:1822\n" "GATT\n" "+\n" "!''*\n"));
    REQUIRE(!SequenceIsFirst(seq));
    REQUIRE(!SequenceIsSecond(seq));
}
FIXTURE_TEST_CASE(ReadNumberDefault, LoaderFixture)
{
    defaultReadNumber = 2;
    REQUIRE(CreateFileGetSequence(GetName(), "@HWUSI-EAS499:1:3:9:1822\n" "GATT\n" "+\n" "!''*\n"));
    REQUIRE(!SequenceIsFirst(seq));
    REQUIRE(SequenceIsSecond(seq));
}
FIXTURE_TEST_CASE(ReadNumberOverride, LoaderFixture)
{
    defaultReadNumber = 1;
    REQUIRE(CreateFileGetSequence(GetName(), "@HWUSI-EAS499:1:3:9:1822/2\n" "GATT\n" "+\n" "!''*\n"));
    REQUIRE(!SequenceIsFirst(seq));
    REQUIRE(SequenceIsSecond(seq));
}

FIXTURE_TEST_CASE(RunSpotRead, LoaderFixture)
{
    defaultReadNumber = 1;
    REQUIRE(CreateFileGetSequence(GetName(), "@SRR390728.1.2\n" "GATT\n" "+\n" "!''*\n"));
    REQUIRE(SequenceIsSecond(seq));
}

FIXTURE_TEST_CASE(RunSpotRead_withTail, LoaderFixture)
{
    defaultReadNumber = 1;
    REQUIRE(CreateFileGetSequence(GetName(), "@SRR390728.1.2 123\n" "GATT\n" "+\n" "!''*\n"));
    REQUIRE(SequenceIsSecond(seq));
}

FIXTURE_TEST_CASE(RunSpotNothing, LoaderFixture)
{
    defaultReadNumber = 1;
    REQUIRE(CreateFileGetSequence(GetName(), "@SRR390728.1\n" "GATT\n" "+\n" "!''*\n"));
}

// Components of Illumina tag lines:
// @HWUSI-EAS499:1:3:9:1822#0/1"
// spot-name HWUSI-EAS499:1:3:9:1822    tag line up to and including coordinates
// spot-group "0"                       token following '#'
// read-number "1"                      1 or 2 following '/'

// not implemented for now:
// run-group HWUSI-EAS499               tag line up to and excluding coordinates
// coords [ 1 3 9 182 ]
// fmt-name HWUSI-EAS499:1:3:$X:$Y

FIXTURE_TEST_CASE(SequenceGetSpotNameIllumina, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@HWUSI-EAS499:1:3:9:1822#0/1\n" "GATT\n" "+\n" "!''*\n"));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("HWUSI-EAS499:1:3:9:1822"), string(name, length));
    REQUIRE(!SequenceIsSecond(seq));
    REQUIRE(SequenceIsFirst(seq));
}

FIXTURE_TEST_CASE(SequenceGetSpotGroupIllumina, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@1:3:9:1822#CAT/1\n" "GATT\n" "+\n" "!''*\n"));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string("CAT"), string(name, length));
    REQUIRE(!SequenceIsSecond(seq));
    REQUIRE(SequenceIsFirst(seq));
}

FIXTURE_TEST_CASE(SequenceGetSpotGroupIllumina_Ignored, LoaderFixture)
{
    ignoreSpotGroups = true;
    REQUIRE(CreateFileGetSequence(GetName(), "@1:3:9:1822#CAT/1\n" "GATT\n" "+\n" "!''*\n"));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string(""), string(name, length));
    REQUIRE(!SequenceIsSecond(seq));
    REQUIRE(SequenceIsFirst(seq));
}

FIXTURE_TEST_CASE(SequenceGetSpotGroup_Empty, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        ">HWI-EAS6_4_FC2010T:1:1:80:366\n"
        "GATT\n"
        "+\n"
        "!''*\n"
    ));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string(""), string(name, length));
    REQUIRE(!SequenceIsSecond(seq));
    REQUIRE(!SequenceIsFirst(seq));
}

FIXTURE_TEST_CASE(SequenceGetSpotGroup_Zero, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        ">HWI-EAS6_4_FC2010T:1:1:80:366#0\n"
        "GATT\n"
        "+\n"
        "!''*\n"
    ));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string(""), string(name, length));
    REQUIRE(!SequenceIsSecond(seq));
    REQUIRE(!SequenceIsFirst(seq));
}

FIXTURE_TEST_CASE(SequenceGetSpotGroupBarcode, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@HWI-ST1234:33:D1019ACXX:2:1101:1415:2223/1 1:N:0:ATCACG\nATCG\n"));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string("ATCACG"), string(name, length));
    REQUIRE(!SequenceIsSecond(seq));
    REQUIRE(SequenceIsFirst(seq));
}

FIXTURE_TEST_CASE(SequenceGetSpotGroupBarcode_WithPluses, LoaderFixture)
{   // VDB-4533
    // BisonDebugOn();
    // FlexDebugOn();
    REQUIRE(CreateFileGetSequence(GetName(), "@A00197:49:HCYYGDMXX:1:1101:10004:10175 1:N:0:ATTACTCG+AGGCGAAGCGCTCATT+TAATCTTA\nATCG\n"));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string("ATTACTCG+AGGCGAAGCGCTCATT+TAATCTTA"), string(name, length));
    REQUIRE(!SequenceIsSecond(seq));
    REQUIRE(SequenceIsFirst(seq));
}

FIXTURE_TEST_CASE(SequenceGetSpotGroupUnderscore, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(), "@FCA5PJ4:1:1101:14707:1407#GTAGTCGC_AGCTCGGT/1\nATCG\n"));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string("GTAGTCGC_AGCTCGGT"), string(name, length));
    REQUIRE(!SequenceIsSecond(seq));
    REQUIRE(SequenceIsFirst(seq));
}

FIXTURE_TEST_CASE(HashStyleBarcodeAndReadNumbersAndJunk, LoaderFixture)
{   // VDB-4532
    //BisonDebugOn();
    REQUIRE(CreateFileGetSequence(GetName(), "@CL100050407L1C001R001_1#224_1078_917/1 1       1/2\nATCG\n"));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string("224_1078_917"), string(name, length));
    REQUIRE(!SequenceIsSecond(seq));
    REQUIRE(SequenceIsFirst(seq));
}


#define TEST_PAIRED(line, paired)\
    REQUIRE(CreateFileGetSequence(GetName(), line "\n" "GATT\n" "+\n" "!''*\n"));\
    if (paired)\
        REQUIRE(SequenceWasPaired(seq));\
    else\
        REQUIRE(!SequenceWasPaired(seq));

FIXTURE_TEST_CASE(SequenceWasPaired1, LoaderFixture) { TEST_PAIRED("@HWUSI-EAS499:1:3:9:1822",         false); }
FIXTURE_TEST_CASE(SequenceWasPaired2, LoaderFixture) { TEST_PAIRED("@HWUSI-EAS499:1:3:9:1822#0/0",     false); }
FIXTURE_TEST_CASE(SequenceWasPaired3, LoaderFixture) { TEST_PAIRED("@HWUSI-EAS499:1:3:9:1822#0/1",     true); }
FIXTURE_TEST_CASE(SequenceWasPaired4, LoaderFixture) { TEST_PAIRED("@HWUSI-EAS499:1:3:9:1822#CAT/1",   true); }
FIXTURE_TEST_CASE(SequenceWasPaired5, LoaderFixture) { TEST_PAIRED("@HWUSI-EAS499:1:3:9:1822/2",       true); }

FIXTURE_TEST_CASE(SequenceGetSpotNameOneLine, LoaderFixture)
{   // source: SRR014283
    REQUIRE(CreateFileGetSequence(GetName(), "USI-EAS50_1:6:1:392:881:GCT:!!!"));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("USI-EAS50_1:6:1:392:881"), string(name, length));
}

FIXTURE_TEST_CASE(SequenceGetNameNumeric, LoaderFixture)
{   // source: SRR094419
    qualityFormat = FASTQphred64;
    REQUIRE(CreateFileGetSequence(GetName(),
        "@741:6:1:1204:10747/1\n"
        "GTCGTTGTCCCGCTCCTCATATTCNNNNNNNNNNNN\n"
        "+\n"
        "bbbbbbbbbbbbbbbbbbbb````BBBBBBBBBBBB\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("741:6:1:1204:10747"), string(name, length));
}

//////////////////// parsing Quality lines

FIXTURE_TEST_CASE(GtStartsQualityOnly, LoaderFixture)
{   // source: XXX001656
    CreateFileGetRecord(GetName(),
        ">HWI-EAS6_4_FC2010T:1:1:80:366\n"
        "!!\n"
        ">HWI-EAS6_4_FC2010T:1:1:80:366\n"
        "!!\n"
        );
    REQUIRE(GetRejected());
    REQUIRE(!fatal);
}

FIXTURE_TEST_CASE(OneLineRead, LoaderFixture)
{
// source: SRR016872
    CreateFileGetRecord(GetName(), "USI-EAS50_1:6:1:392:881:GCTC:!!!!\n");
    REQUIRE(!GetRejected());
}

FIXTURE_TEST_CASE(ForcePhredOffset, LoaderFixture)
{   // quality line looks like it may be Phred64, but we know we are dealing with Phred33
// source: SRR014126
    qualityFormat = FASTQphred33;
    CreateFileGetSequence(GetName(),
        "@R16:8:1:19:1012#0/2\n"
        "TTAAATGACTCTTTAAAAAACACAACATACATTGATATATTTATTCCTAGATATTTGCTTATAAGACTCTAATCA\n"
        "+\n"
        "BCCBBBACBBCCCCBCCCCCCCCCCBCBCCCABBBBBBCCBBCBBCCBBCBCCCABBCAAABC@CCCAB@CBACC\n"
    );
    REQUIRE_RC(SequenceGetQuality(seq, &quality, &qualityAsciiOffset, &qualityType));
    REQUIRE_NOT_NULL(quality);
    REQUIRE_EQ(qualityType, (int)QT_Phred);
    REQUIRE_EQ(qualityAsciiOffset,  (uint8_t)33);
    REQUIRE_EQ(quality[0],    (int8_t)'B');
}

// Illumina spot names
FIXTURE_TEST_CASE(IlluminaCasava_1_8, LoaderFixture)
{ // source: SAMN01860354.fastq
    REQUIRE(CreateFileGetSequence(GetName(),
                "@HWI-ST273:315:C0LKAACXX:7:1101:1487:2221 2:Y:0:GGCTAC\n"
                "AACA\n+\n$.%0\n"
    ));

    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("HWI-ST273:315:C0LKAACXX:7:1101:1487:2221"), string(name, length));
    REQUIRE(SequenceIsSecond(seq));
    REQUIRE(SequenceIsLowQuality(seq));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string("GGCTAC"), string(name, length));
}

FIXTURE_TEST_CASE(IlluminaCasava_1_8_SpotGroupNumber, LoaderFixture)
{ // source: SAMN01860354.fastq
    REQUIRE(CreateFileGetSequence(GetName(),
                "@HWI-ST273:315:C0LKAACXX:7:1101:1487:2221 2:Y:0:1\n"
                "AACA\n+\n$.%0\n"
    ));

    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("HWI-ST273:315:C0LKAACXX:7:1101:1487:2221"), string(name, length));
    REQUIRE(SequenceIsSecond(seq));
    REQUIRE(SequenceIsLowQuality(seq));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string("1"), string(name, length));
}

FIXTURE_TEST_CASE(IlluminaCasava_1_8_SpotGroup_MoreMadness, LoaderFixture)
{ // source: SRR1106612
    REQUIRE(CreateFileGetSequence(GetName(),
                "@HWI-ST808:130:H0B8YADXX:1:1101:1914:2223 1:N:0:NNNNNN.GGTCCA.AAAA\n"
                "AACA\n+\n$.%0\n"
    ));

    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string("NNNNNN.GGTCCA.AAAA"), string(name, length));
}

FIXTURE_TEST_CASE(IlluminaCasava_1_8_EmptyTag, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
                "@HWI-ST959:56:D0AW4ACXX:8:1101:1233:2026 2:N:0:\n"
                "AACA\n+\n$.%0\n"

    ));

    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("HWI-ST959:56:D0AW4ACXX:8:1101:1233:2026"), string(name, length));
    REQUIRE(SequenceIsSecond(seq));
    REQUIRE(! SequenceIsLowQuality(seq));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string(), string(name, length));
}

FIXTURE_TEST_CASE(Illumina_NegativeCoords, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@HWUSI-EAS1679-0005:4:113:4454:-51#0\n"
        "AACA\n+\n$.%0\n"
    ));

    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("HWUSI-EAS1679-0005:4:113:4454:-51"), string(name, length));
    REQUIRE_RC(SequenceGetSpotGroup(seq, &name, &length));
    REQUIRE_EQ(string(""), string(name, length));
}

FIXTURE_TEST_CASE(Illumina_Underscore, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG\n"
        "AGAGTTTGAT\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("DG7PMJN1:293:D12THACXX:2:1101:1161:1968"), string(name, length));
    REQUIRE(SequenceIsSecond(seq));
}

FIXTURE_TEST_CASE(Illumina_IdentifierAtFront, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@QSEQ161.65 DBV2SVN1:1:1101:1474:2213#0/1\n"
        "AGAGTTTGAT\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("DBV2SVN1:1:1101:1474:2213"), string(name, length));
}

FIXTURE_TEST_CASE(Illumina_SpaceAndIdentifierAtFront, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@ QSEQ161 EAS139:136:FC706VJ:2:2104:15343:197393 1:Y:18:ATCACG\n"
        "AGAGTTTGAT\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("EAS139:136:FC706VJ:2:2104:15343:197393"), string(name, length));
}

FIXTURE_TEST_CASE(SRR1778155 , LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@2-796964       M01929:5:000000000-A46YE:1:1108:16489:18207 1:N:0:2     orig_bc=TATCGGGA        new_bc=TATCGGGA   bc_diffs=0\n"
        "AACA\n+\n$.%0\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("M01929:5:000000000-A46YE:1:1108:16489:18207"), string(name, length));
}

FIXTURE_TEST_CASE(SRA192487, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@HWI-ST1234:33:D1019ACXX:2:1101:1415:2223/1 1:N:0:ATCACG\n"
        "AACA\n+\n$.%0\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("HWI-ST1234:33:D1019ACXX:2:1101:1415:2223"), string(name, length));
}

//////////////////// odd syntax cases
FIXTURE_TEST_CASE(NoEolAtEof, LoaderFixture)
{
    CreateFileGetRecord(GetName(),
        "@SEQ_ID1\n" "GATT\n" "+\n" "!''*\n"
        "@SEQ_ID2\n" "GATT\n" "+\n" "!''*"
    );
    REQUIRE_NOT_NULL(record);
    REQUIRE(! GetRejected());

    REQUIRE(GetRecord());
    REQUIRE(! GetRejected());
    REQUIRE_NOT_NULL(record);
}

FIXTURE_TEST_CASE(GtStartsReadOnly, LoaderFixture)
{   // source: XXX001656
    CreateFileGetRecord(GetName(),
        ">q\n"
        "AAC\n"
        ">q\n"
        "ACA\n"
    );
    REQUIRE(!GetRejected());
}

//////////////////// detecting older formats

FIXTURE_TEST_CASE(Quality33TooLow, LoaderFixture)
{   // negative qualities are not allowed for Phred33
// source: SRR016872
    qualityFormat = FASTQphred33;
    CreateFileGetRecord(GetName(),
            "@HWI-EAS102_1_30LWPAAXX:5:1:1792:566\n"
            "GAAACCCCCTATTAGANNNNCNNNNCNATCATGTCA\n"
            "+HWI-EAS102_1_30LWPAAXX:5:1:1792:566\n"
            "II IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII\n"
    );
    REQUIRE(GetRejected());
    REQUIRE(fatal);
}

FIXTURE_TEST_CASE(Quality64TooLow, LoaderFixture)
{   // negative qualities are not allowed for Phred64
// source: SRR016872
    qualityFormat = FASTQphred64;
    CreateFileGetRecord(GetName(),
            "@HWI-EAS102_1_30LWPAAXX:5:1:1511:102\n"
            "GGGGTTAGTGGCAGGGGGGGGGTCTCGGGGGGGGGG\n"
            "+HWI-EAS102_1_30LWPAAXX:5:1:1511:102\n"
            "IIIIIIIIIIIIIIIIII;IIIIIIIIIIIIIIIII\n"
        );
    REQUIRE(GetRejected());
    REQUIRE(fatal);
}

FIXTURE_TEST_CASE(QualityLogoddsTooLow, LoaderFixture)
{   // qualities under 59 are not allowed for LogOdds
    qualityFormat = FASTQlogodds;
    CreateFileGetRecord(GetName(),
            "@HWI-EAS102_1_30LWPAAXX:5:1:1511:102\n"
            "GGGGTTAGTGGCAGGGGGGGGGTCTCGGGGGGGGGG\n"
            "+HWI-EAS102_1_30LWPAAXX:5:1:1511:102\n"
            "I:IIIIIIIIIIIIIIII;IIIIIIIIIIIIIIIII\n" // ':' = 58
        );
    REQUIRE(GetRejected());
    REQUIRE(fatal);
}

FIXTURE_TEST_CASE(DecimalQualityRejected, LoaderFixture)
{
    CreateFileGetRecord(GetName(),
        "@BILLIEHOLIDAY_1_FC20F3DAAXX:8:2:342:540\n"
        "GTCGCTTCTCGGAAGNGTGAAAGACAANAATNTTNN\n"
        "+BILLIEHOLIDAY_1_FC20F3DAAXX:8:2:342:540\n"
        "40 3 1 22 17 18 34 8 13 21 3 7 5 0 0 5 1 0 7 3 2 3 3 3 1 1 4 5 5 2 2 5 0 1 5 5\n"
    );
    REQUIRE(GetRejected());
    REQUIRE(fatal);
}

////////////////// detecting alternative formats
FIXTURE_TEST_CASE(PacbioRaw, LoaderFixture)
{
    defaultReadNumber = -1;
    REQUIRE(CreateFileGetSequence(GetName(),
        "@m121205_055009_42163_c100416332550000001523041801151327_s1_p0/19\n"
        "AGAGTTTGAT\n"
        "+m121205_055009_42163_c100416332550000001523041801151327_s1_p0/19\n"
        "LLhf>>>>[[\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("m121205_055009_42163_c100416332550000001523041801151327_s1_p0/19"), string(name, length));
}
FIXTURE_TEST_CASE(PacbioCcs, LoaderFixture)
{
    defaultReadNumber = -1;
    REQUIRE(CreateFileGetSequence(GetName(),
        "@m121205_055009_42163_c100416332550000001523041801151327_s1_p0/19/ccs\n"
        "AGAGTTTGAT\n"
        "+m121205_055009_42163_c100416332550000001523041801151327_s1_p0/19/ccs\n"
        "LLhf>>>>[[\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("m121205_055009_42163_c100416332550000001523041801151327_s1_p0/19/ccs"), string(name, length));
}

FIXTURE_TEST_CASE(PacbioNoReadNumbers, LoaderFixture)
{
    defaultReadNumber = -1;
    REQUIRE(CreateFileGetSequence(GetName(),
        "@m121205_055009_42163_c100416332550000001523041801151327_s1_p0/1\n"
        "AGAGTTTGAT\n"
        "+m121205_055009_42163_c100416332550000001523041801151327_s1_p0/1\n"
        "LLhf>>>>[[\n"
    ));
    REQUIRE(!SequenceIsFirst(seq));
    REQUIRE(!SequenceIsSecond(seq));
}

FIXTURE_TEST_CASE(PacbioWsCcs, LoaderFixture)
{
    defaultReadNumber = -1;
    REQUIRE(CreateFileGetSequence(GetName(),
        "@m101210_094054_00126_c000028442550000000115022402181134_s1_p0/2 ccs\n"
        "AGAGTTTGAT\n"
        "+m101210_094054_00126_c000028442550000000115022402181134_s1_p0/2 ccs\n"
        "LLhf>>>>[[\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("m101210_094054_00126_c000028442550000000115022402181134_s1_p0/2"), string(name, length));
}

FIXTURE_TEST_CASE(PacbioError, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@m130727_021351_42150_c100538232550000001823086511101336_s1_p0/53/0_106\n"
        "TTTTTCCAAAAAAGGAGACGTAAACATTTCTTAACTTGCCAGCACTCTAATTCCAAAATCAAGTCGCATTTCTGACATTGCGGTAAGATTGTGCAATATCATATCT\n"
        "+\n"
        ")*'&*'+*(*+#-,-/'+-)+,'-./+*+.()*()*,$#,)'+**%+'*/,+(/,,*,'/&,+--%.-.*),,+.,,.'./%-/,/(.%.,*(.+/-/(../.&'#\n"
    ));
}

FIXTURE_TEST_CASE(NotPairedRead_Error, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@HWI-ST226:170:AB075UABXX:3:1101:10089:7031 1:N:0:GCCAAT\n"
        "TACA\n"
    ));
    REQUIRE(SequenceIsFirst(seq));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("HWI-ST226:170:AB075UABXX:3:1101:10089:7031"), string(name, length));
}

FIXTURE_TEST_CASE(NoFragmentInfo_Error, LoaderFixture)
{
    defaultReadNumber = -1;
    REQUIRE(CreateFileGetSequence(GetName(),
        "@m130727_021351_42150_c100538232550000001823086511101336_s1_p0/283/0_9315\n"
        "AACA\n+\n$.%0\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("m130727_021351_42150_c100538232550000001823086511101336_s1_p0/283/0_9315"), string(name, length));
}

FIXTURE_TEST_CASE(NoColonAtTheEnd_Error, LoaderFixture)
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@HET-141-007:154:C391TACXX:6:2316:3220:70828 1:N:0\n"
        "AACA\n+\n$.%0\n"
    ));
    REQUIRE(SequenceIsFirst(seq));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("HET-141-007:154:C391TACXX:6:2316:3220:70828"), string(name, length));
}

FIXTURE_TEST_CASE ( MissingRead, LoaderFixture )
{ // source: SRR529889
    REQUIRE(CreateFileGetSequence(GetName(),
        "@GG3IVWD03HIDOA length=3 xy=2962_2600 region=3 run=R_2010_05_11_11_15_22_\n"
        "AACA\n+\n$.%0\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("GG3IVWD03HIDOA"), string(name, length));
}

FIXTURE_TEST_CASE ( UnexpectedEOLreported, LoaderFixture )
{ // source: SRR1915965
    REQUIRE(CreateFileGetSequence(GetName(),
        "@HWI-ST1106:381:D1CDRACXX:8:1101:10000:110594\t2\n"
        "AACA\n+\n$.%0\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("HWI-ST1106:381:D1CDRACXX:8:1101:10000:110594"), string(name, length));
    REQUIRE(SequenceIsSecond(seq));
}

FIXTURE_TEST_CASE ( AnotherUnexpectedEOLreported, LoaderFixture )
{ // source: SRR1686805
    REQUIRE(CreateFileGetSequence(GetName(),
        "@HWI-ST225:626:C2Y82ACXX:3:1304:7988:75799_2\n"
        "AACA\n+\n$.%0\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("HWI-ST225:626:C2Y82ACXX:3:1304:7988:75799"), string(name, length));
    REQUIRE(SequenceIsSecond(seq));
}

FIXTURE_TEST_CASE ( FastqDumpOutput, LoaderFixture )
{ // VDB-2835 source: fastq-dump SRR000123
    REQUIRE(CreateFileGetSequence(GetName(),
        "@SRR000123.1 EXRHO8E16JTGUV length=157\n"
        "TCAGTTTCGGGAACTATGTAAACAATACGCTAAGCGTGTTCTATGGTTTTACAATGTTCAGGATGGAAACATTGTAGGAGATAACAACCGAGAAGTATCGCTGCCTGTCGGCTGAGACACGCAACAGGGGATAGGCAAGGCACTACAGGGGATAGGN\n"
        "+SRR000123.1 EXRHO8E16JTGUV length=157\n"
        "=<:7F?)9F?)>3<=<===F?*<D:<<<=<:F>=<==:D;==;=D;LE8,49E<==E==;E===E<F@+==E<==<D=<8<=C;;A9@7;<9>6<<===<=<<A:8<:;C<<=<;<<<9<:=B;88GC6(6<<C==C=C=<=<&<9=GC7):<<B:!\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("SRR000123.1"), string(name, length));
}

FIXTURE_TEST_CASE ( Barcodes_Plus, LoaderFixture )
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@M05096:131:000000000-C9DF2:1:1101:16623:1990 2:N:0:TAAGGCGA+CCTGCATA\n"
        "TCAG\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("M05096:131:000000000-C9DF2:1:1101:16623:1990"), string(name, length));
}

FIXTURE_TEST_CASE ( Barcodes_Underscore, LoaderFixture )
{
    REQUIRE(CreateFileGetSequence(GetName(),
        "@M05096:131:000000000-C9DF2:1:1101:16623:1990 2:N:0:TAAGGCGA_CCTGCATA\n"
        "TCAG\n"
    ));
    REQUIRE_RC(SequenceGetSpotName(seq, &name, &length));
    REQUIRE_EQ(string("M05096:131:000000000-C9DF2:1:1101:16623:1990"), string(name, length));
}

FIXTURE_TEST_CASE ( FastqMultiLineSequenceFasta, LoaderFixture )
{ // VDB-3413
#define LINE1 "AAAGAGAGAGGATGCTTGCAAAGGCTAGCATCGGTCTACTTTCAAACTTCTCAAGAATGTCCTTGAGATA"
#define LINE2 "TTGGCTTTGGGACAAGAAATCCTTCTTGAAATTGTTGAATTTGAATCGAGAAAGACTTCAGTCGGCATTG"
#define LINE3 "AGTGCATATCAAAGGTCTTGGTCATGGAGGCT"

    REQUIRE(CreateFileGetSequence(GetName(),
        ">m151125_043638_42219_c100898392550000001823200804231640_s1_p0/24/0_34066 RQ=0.887\n" LINE1 "\n" LINE2 "\n" LINE3 "\n"
    ));
    REQUIRE(MakeReadBuffer());
    REQUIRE_RC(SequenceGetRead(seq, read));
    REQUIRE_EQ ( string ( read, readLength ), string ( LINE1 LINE2 LINE3 ) );
#undef LINE1
#undef LINE2
#undef LINE3
}

FIXTURE_TEST_CASE ( FastqMultiLineSequenceFastq, LoaderFixture )
{ // VDB-3413
#define LINE1 "GGATGGTCCGAGCCGCAAGCGCACCGAACTGCTGCGCGAGCTGCGGATCA"
#define LINE2 "AAAGATGACGATCAACGTCGCGCGGCCCGGTGACATGAGATTACCGGCGG"
#define LINE3 "CGGAGCAACCAGCAACTGAATCGAGA"
#define QUAL1 "##%$--,&//#.'$&'!!$)*)('./%.'$)$&..-.//-//..///+/*"
#define QUAL2 "(#'*%.,)+-,%''---)'--./*+&&--*)-+*&)'%.,-.-%'##((*"
#define QUAL3 "(+-!$+'#$---$%(#%-$%/-.&('"

    REQUIRE(CreateFileGetSequence(GetName(),
        ">m151125_043638_42219_c100898392550000001823200804231640_s1_p0/24/0_34066 RQ=0.887\n"
        LINE1 "\n" LINE2 "\n" LINE3 "\n"
        "+\n"
        QUAL1 "\n" QUAL2 "\n" QUAL3 "\n"
    ));
    REQUIRE(MakeReadBuffer());
    REQUIRE_RC(SequenceGetRead(seq, read));
    REQUIRE_EQ ( string ( read, readLength ), string ( LINE1 LINE2 LINE3 ) );

    REQUIRE_RC(SequenceGetQuality(seq, &quality, &qualityAsciiOffset, &qualityType));
    REQUIRE_RC(SequenceGetRead(seq, read));
    REQUIRE_EQ ( string ( (const char*)quality, readLength ), string ( QUAL1 QUAL2 QUAL3 ) );
#undef LINE1
#undef LINE2
#undef LINE3
#undef QUAL1
#undef QUAL2
#undef QUAL3
}

// FIXTURE_TEST_CASE(Pacbio, LoaderFixture)
// {
    // REQUIRE(CreateFileGetSequence(GetName(),
                // "@m120419_100821_42161_c100329130310000001523018509161273_s1_p0/11/2511_3149\n"
                // "CTGCTTCTCCTGCTCTTCCTACTGTCCTCTCCCTGCTGTCGCTTCGCCCC\n"
                // "TCGGTGGAGGCCGCGTTTGAGCGGCCGGTGTCCGCTGC\n"
                // "+\n"
                // "+*.,+*(,$+-.)**('#%*',*.*&,.(,$',,,($.,)#.-.-%'*#%\n"
                // ",../*+*!.%!!-!/-&(.+.!.'!!//,/)!//!*-'\n"
    // ));

    // REQUIRE_RC(SequenceGetReadLength(seq, &readLength));
    // REQUIRE_EQ(readLength, 88u);
    // REQUIRE(MakeReadBuffer());
    // REQUIRE_RC(SequenceGetRead(seq, read));
    // REQUIRE_EQ(string(read, readLength), string("CTGCTTCTCCTGCTCTTCCTACTGTCCTCTCCCTGCTGTCGCTTCGCCCCTCGGTGGAGGCCGCGTTTGAGCGGCCGGTGTCCGCTGC"));

    // REQUIRE_RC(SequenceGetQuality(seq, &quality, &qualityAsciiOffset, &qualityType));
    // REQUIRE_NOT_NULL(quality);
    // REQUIRE_EQ(qualityType, (int)QT_Phred);
    // REQUIRE_EQ((unsigned int)qualityAsciiOffset,  (unsigned int)phredOffset);
    // REQUIRE_EQ(quality[0],  (int8_t)'+');
    // REQUIRE_EQ(quality[87], (int8_t)'\'');
// }

// @ERBRDQF01EGP9U
// spot-name
// spot-group
// read-number
// run-group
// coords
// fmt-name

//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>

int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return FastqLoaderWbTestSuite(argc, argv);
}

}
