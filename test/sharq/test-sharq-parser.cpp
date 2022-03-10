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
* Unit tests for SHARQ loader
*/
#include <ktst/unit_test.hpp>
#include <klib/rc.h>
#include <loader/common-writer.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <sstream>

#include "../../tools/sharq/fastq_parser.hpp"
#include "../../tools/sharq/fastq_read.hpp"
#include "../../tools/sharq/fastq_error.hpp"

#include <sysalloc.h>
#include <stdlib.h>
#include <cstring>
#include <stdexcept>
#include <list>

using namespace std;

TEST_SUITE(SharQParserTestSuite);

class LoaderFixture
{
public:
    LoaderFixture()
    {
    }
    ~LoaderFixture()
    {
    }

    shared_ptr<istream> create_stream(const string& str = "") {
        shared_ptr<stringstream> ss(new stringstream);
        *ss << str;
        return ss;
    }
    string filename;
};

///////////////////////////////////////////////// SHARQ test cases
FIXTURE_TEST_CASE(EmptyFile, LoaderFixture)
{
    fastq_reader reader("test", create_stream());
    CFastqRead read;
    REQUIRE(read.Spot().empty());
    REQUIRE(read.LineNumber() == 0);
    REQUIRE(reader.parse_read<>(read) == false);
    REQUIRE(reader.eof());
}


FIXTURE_TEST_CASE(EndLines, LoaderFixture)
{
    //shared_ptr<stringstream> ss(new stringstream);
    //*ss << "\n\n";
    fastq_reader reader("test", create_stream("\n\n"));
    CFastqRead read;
    REQUIRE(reader.parse_read<>(read) == false);
    REQUIRE(reader.eof());
}


FIXTURE_TEST_CASE(InvalidDefline, LoaderFixture)
{
    fastq_reader reader("test", create_stream("qqq abcd"));
    CFastqRead read;
    REQUIRE_THROW(reader.parse_read<>(read));
}


FIXTURE_TEST_CASE(InvalidDeflineError, LoaderFixture)
{
    fastq_reader reader("test", create_stream("qqq abcd"));
    CFastqRead read;
    try {
        reader.parse_read<>(read);
        FAIL("Should not reach this point");
    } catch (fastq_error& err) {
        CHECK_EQ(err.Message(), string("[code:100] Defline 'qqq abcd' not recognized"));
    }
}

const string cSPOT1 = "NB501550:336:H75GGAFXY:2:11101:10137:1038";
const string cSPOT_GROUP = "CTAGGTGA";
const string cDEFLINE1 = cSPOT1 + " 1:N:0:" + cSPOT_GROUP;
const string cDEFLINE2 = cSPOT1 + " 2:N:0:" + cSPOT_GROUP;

const string cSPOT2 = "NB501550:336:H75GGAFXY:2:11101:10138:1038";
const string cDEFLINE2_1 = cSPOT2 + " 1:N:0:" + cSPOT_GROUP;
const string cDEFLINE2_2 = cSPOT2 + " 2:N:0:" + cSPOT_GROUP;


const string cSEQ = "GATT";
const string cSEQ_MULTILINE = "GA\nTT";

const string cQUAL = "!''*";
const string cQUAL_MULTILINE = "!'\n'*";

#define _READ(defline, sequence, quality)\
    string("@" + string(defline)  + "\n" + sequence  + "\n+\n" + quality + "\n")\

FIXTURE_TEST_CASE(GoodRead, LoaderFixture)
{   // a good record
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, cSEQ, cQUAL)));
    REQUIRE(reader.parse_read<>(read));
    REQUIRE(reader.platform()== 2);
    REQUIRE_EQ(read.Spot(), cSPOT1);
    REQUIRE_EQ(read.ReadNum(), string("1"));
    REQUIRE_EQ(read.SpotGroup(), cSPOT_GROUP);
    REQUIRE_EQ(read.Sequence(), cSEQ);
    REQUIRE_EQ(read.Quality(), cQUAL);
    REQUIRE(read.Suffix().empty());
}

FIXTURE_TEST_CASE(GoodReadFollowedByJunk, LoaderFixture)
{   // good record followed by junk
    //string cREAD = CREATE_READ(cDEFLINE1, cSEQ, cQUAL);
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, cSEQ, cQUAL) + "qqq abcd"));
    REQUIRE(reader.parse_read<>(read));
    REQUIRE_THROW(reader.parse_read<>(read));
}

FIXTURE_TEST_CASE(MultiLineRead, LoaderFixture)
{   // multiline record
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, cSEQ_MULTILINE, cQUAL_MULTILINE)));
    REQUIRE(reader.parse_read<>(read));
    CHECK_EQ(read.Sequence(), cSEQ);
    CHECK_EQ(read.Quality(), cQUAL);
}

FIXTURE_TEST_CASE(NoLastLF, LoaderFixture)
{
    fastq_reader reader("test", create_stream(
        _READ(cDEFLINE1, "GATT", cQUAL) + "@" + cDEFLINE2  + "\nACGTACGT\n+\n!''*!''*"));
    CFastqRead read;
    REQUIRE(reader.parse_read<>(read));
    REQUIRE(reader.parse_read<>(read));
    REQUIRE_EQ(read.LineNumber(), 5lu);
    REQUIRE_EQ(read.Quality().size(), 8lu);
    REQUIRE(reader.eof());
}


FIXTURE_TEST_CASE(InvalidSequence, LoaderFixture)
{   // invalid sequence character
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, "GA^TT", cQUAL)));
    REQUIRE(reader.parse_read<>(read));
    REQUIRE_THROW(reader.validate_read<>(read));
}

FIXTURE_TEST_CASE(EmptyQuality, LoaderFixture)
{   // invalid quality scores
    CFastqRead read;
    fastq_reader reader("test", create_stream(string("@" + cDEFLINE1  + "\nGATT\n+")));
    REQUIRE_THROW(reader.get_read(read));
}
FIXTURE_TEST_CASE(EmptyQuality2, LoaderFixture)
{   // invalid quality scores
    CFastqRead read;
    fastq_reader reader("test", create_stream(string("\
@NB501550:336:H75GGAFXY:2:11101:10137:1038 1:N:0:CTAGGTGA\n\
NCTATCTAGAATTCCCTACTACTCCC\n\
@NB501550:336:H75GGAFXY:2:11101:9721:1038 1:N:0:CTAGGTGA\n\
NAGCCGCGTAAGGGAATTAGGCAGCA")));
    REQUIRE_THROW(reader.get_read(read));
}

FIXTURE_TEST_CASE(TestSequence, LoaderFixture)
{
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, cSEQ, cQUAL)));
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(read.Sequence(), cSEQ);
    REQUIRE_EQ(read.Sequence().size(), 4lu);
}

FIXTURE_TEST_CASE(TestSequence_Multi, LoaderFixture)
{
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, "GATT\nTAGGA", cQUAL)));
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(read.Sequence(), string("GATTTAGGA"));
    REQUIRE_EQ(read.Sequence().size(), 9lu);
}

FIXTURE_TEST_CASE(UridineTranslation, LoaderFixture)
{
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, "U", cQUAL)));
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(read.Sequence(), string("T"));
}

//////////////////// tag line parsing

#define TEST_TAGLINE(line, _defline_type)\
    string cREAD = string(line) + "\n" +cSEQ + "\n+\n" + cQUAL + "\n"; \
    CFastqRead read; \
    fastq_reader reader("test", create_stream(cREAD)); \
    REQUIRE(reader.get_read(read)); \
    REQUIRE_EQ(reader.defline_type(), string(_defline_type)); \
\

FIXTURE_TEST_CASE(IlluminaNew1, LoaderFixture)  { TEST_TAGLINE("@M00730:68:000000000-A2307:1:1101:14701:1383 1:N:0:1", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew2, LoaderFixture)  { TEST_TAGLINE("@HWI-962:74:C0K69ACXX:8:2104:14888:94110 2:N:0:CCGATAT", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew3, LoaderFixture)  { TEST_TAGLINE("@HWI-ST808:130:H0B8YADXX:1:1101:1914:2223 1:N:0:NNNNNN-GGTCCA-AAAA", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew4, LoaderFixture)  { TEST_TAGLINE("@HWI-M01380:63:000000000-A8KG4:1:1101:17932:1459 1:N:0:Alpha29 CTAGTACG|0|GTAAGGAG|0", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew5, LoaderFixture)  { TEST_TAGLINE("@HWI-ST959:56:D0AW4ACXX:8:1101:1233:2026 2:N:0:", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew6, LoaderFixture)  { TEST_TAGLINE("@DJB77P1:546:H8V5MADXX:2:1101:11528:3334 1:N:0:_I_GACGAC", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew7, LoaderFixture)  { TEST_TAGLINE("@HET-141-007:154:C391TACXX:6:1216:12924:76893 1:N:0", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew8, LoaderFixture)  { TEST_TAGLINE("@DG7PMJN1:293:D12THACXX:2:1101:1161:1968_1:N:0:GATCAG", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew9, LoaderFixture)  { TEST_TAGLINE("@M01321:49:000000000-A6HWP:1:1101:17736:2216_1:N:0:1/M01321:49:000000000-A6HWP:1:1101:17736:2216_2:N:0:1", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew10, LoaderFixture)  { TEST_TAGLINE("@MISEQ:36:000000000-A5BCL:1:1101:24982:8584;smpl=12;brcd=ACTTTCCCTCGA 1:N:0:ACTTTCCCTCGA", "illuminaNewWithSuffix"); }
FIXTURE_TEST_CASE(IlluminaNew11, LoaderFixture)  { TEST_TAGLINE("@HWI-ST1234:33:D1019ACXX:2:1101:1415:2223/1 1:N:0:ATCACG", "illuminaNewWithSuffix"); }
FIXTURE_TEST_CASE(IlluminaNew12, LoaderFixture)  { TEST_TAGLINE("@aa,HWI-7001455:146:H97PVADXX:2:1101:1498:2093 1:Y:0:ACAAACGGAGTTCCGA", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew13, LoaderFixture)  { TEST_TAGLINE("@NS500234:97:HC75GBGXX:1:11101:6479:1067 1:N:0:ATTCAG+NTTCGC", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew14, LoaderFixture)  { TEST_TAGLINE("@M01388:38:000000000-A49F2:1:1101:14022:1748 1:N:0:0", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew15, LoaderFixture)  { TEST_TAGLINE("@M00388:100:000000000-A98FW:1:1101:17578:2134 1:N:0:1|isu|119|c95|303", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew16, LoaderFixture)  { TEST_TAGLINE("@HISEQ:191:H9BYTADXX:1:1101:1215:1719 1:N:0:TTAGGC##NGTCCG", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew17, LoaderFixture)  { TEST_TAGLINE("@HWI:1:X:1:1101:1298:2061 1:N:0: AGCGATAG (barcode is discarded)", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew18, LoaderFixture)  { TEST_TAGLINE("@8:1101:1486:2141 1:N:0:/1", "illuminaNewNoPrefix"); }
FIXTURE_TEST_CASE(IlluminaNew19, LoaderFixture)  { TEST_TAGLINE("@HS2000-1017_69:7:2203:18414:13643|2:N:O:GATCAG", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew20, LoaderFixture)  { TEST_TAGLINE("@HISEQ:258:C6E8AANXX:6:1101:1823:1979:CGAGCACA:1:N:0:CGAGCACA:NG:GT", "illuminaNewWithSuffix"); }
FIXTURE_TEST_CASE(IlluminaNew21, LoaderFixture)  { TEST_TAGLINE("@HWI-ST226:170:AB075UABXX:3:1101:1436:2127 1:N:0:GCCAAT", "illuminaNew"); }
FIXTURE_TEST_CASE(IlluminaNew22, LoaderFixture)  { TEST_TAGLINE("@HISEQ06:187:C0WKBACXX:6:1101:1198:2254 1:N:0:", "illuminaNew"); }

FIXTURE_TEST_CASE(IlluminaNewDataGroup1, LoaderFixture)  { TEST_TAGLINE("@SEDCJ:00674:05781 1:N:0:AAAAA", "illuminaNewDataGroup"); }

FIXTURE_TEST_CASE(BGI1, LoaderFixture)  { TEST_TAGLINE("@V300019058_8BL1C001R0010000000 1:N:0:ATGGTAGG", "BgiNew"); }
FIXTURE_TEST_CASE(BGI2, LoaderFixture)  { TEST_TAGLINE("@V300103666L2C001R0010000000:0:0:0:0 1:N:0:ATAGTCTC", "BgiNew"); }
FIXTURE_TEST_CASE(BGI3, LoaderFixture)  { TEST_TAGLINE("@CL100159005L1C001R001_2 2:N:0:0", "BgiNew"); }

FIXTURE_TEST_CASE(BGIOld1, LoaderFixture)  { TEST_TAGLINE("@CL100050407L1C001R001_1#224_1078_917/1 1       1", "BgiOld"); }

FIXTURE_TEST_CASE(BgiOld8, LoaderFixture)  { TEST_TAGLINE("@V350012516L1C001R00100001492/1", "BgiOld"); }
FIXTURE_TEST_CASE(BgiNewd8, LoaderFixture)  { TEST_TAGLINE("@V300019058_8BL1C001R00112345678 1:N:0:ATGGTAG", "BgiNew") }


FIXTURE_TEST_CASE(SequenceGetSpotGroupBarcode, LoaderFixture)
{
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ("HWI-ST1234:33:D1019ACXX:2:1101:1415:2223/1 1:N:0:ATCACG", cSEQ, cQUAL)));
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(string("ATCACG"), read.SpotGroup());
}


FIXTURE_TEST_CASE(BarCode_Plus, LoaderFixture)
{
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ("M05096:131:000000000-C9DF2:1:1101:16623:1990 2:N:0:TAAGGCGA+CCTGCATA", cSEQ, cQUAL)));
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(string("TAAGGCGA+CCTGCATA"), read.SpotGroup());
}


FIXTURE_TEST_CASE(BarCode_Underscore, LoaderFixture)
{
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ("M05096:131:000000000-C9DF2:1:1101:16623:1990 2:N:0:TAAGGCGA_CCTGCATA", cSEQ, cQUAL)));
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(string("TAAGGCGA_CCTGCATA"), read.SpotGroup());
}


FIXTURE_TEST_CASE(NoColonAtTheEnd_Error, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("HET-141-007:154:C391TACXX:6:2316:3220:70828 1:N:0", cSEQ, cQUAL)));
    CFastqRead read;
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(string("1"), read.ReadNum());
    REQUIRE(read.SpotGroup().empty());
    REQUIRE_EQ(string("HET-141-007:154:C391TACXX:6:2316:3220:70828"), read.Spot());
}


FIXTURE_TEST_CASE(SequenceGetSpotGroupUnderscore, LoaderFixture)
{
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ("HWI-ST1234:33:D1019ACXX:2:1101:1415:2223/1 1:N:0:ATCACG_AGCTCGGT", cSEQ, cQUAL)));
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(string("ATCACG_AGCTCGGT"), read.SpotGroup());
}

FIXTURE_TEST_CASE(RetainIlluminaSuffix, LoaderFixture)
{
    // VDB-4614
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ("MISEQ:36:000000000-A5BCL:1:1101:13252:2382;smpl=12;brcd=ACTTTCCCTCGA 1:N:0:ACTTTCCCTCGA", cSEQ, cQUAL)));
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(string("MISEQ:36:000000000-A5BCL:1:1101:13252:2382"), read.Spot());
    REQUIRE_EQ(string(";smpl=12;brcd=ACTTTCCCTCGA"), read.Suffix());
    REQUIRE_EQ(string("ACTTTCCCTCGA"), read.SpotGroup());
}

FIXTURE_TEST_CASE(RetainBgiSuffix, LoaderFixture)
{
    // VDB-4614
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ("V300103666L2C001R0010000000:0:0:0:0 1:N:0:ATAGTCTC", cSEQ, cQUAL)));
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(string("V300103666L2C001R0010000000"), read.Spot());
    REQUIRE_EQ(string(":0:0:0:0"), read.Suffix());
    REQUIRE_EQ(string("ATAGTCTC"), read.SpotGroup());
}


FIXTURE_TEST_CASE(Test_get_next_spot_empty, LoaderFixture)
{
    vector<CFastqRead> reads;
    fastq_reader reader("test", create_stream(""));
    string spot;
    REQUIRE(reader.get_next_spot(spot, reads) == false);
    REQUIRE(reads.empty());
}

FIXTURE_TEST_CASE(Test_get_next_spot, LoaderFixture)
{
    vector<CFastqRead> reads;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, "GATT\nTAGGA", cQUAL)));
    string spot;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE_EQ(spot, cSPOT1);
    REQUIRE(reads.size() == 1);
}

FIXTURE_TEST_CASE(Test_get_next_spot_multi_2, LoaderFixture)
{
    vector<CFastqRead> reads;
    auto ss = create_stream(_READ(cDEFLINE1, "GATT\nTAGGA", cQUAL) + _READ(cDEFLINE2, "GATT\nTAGGA", cQUAL));
    fastq_reader reader("test", ss, {'B', 'B'});
    string spot;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE_EQ(spot, cSPOT1);
    REQUIRE(reads.size() == 2);
}


FIXTURE_TEST_CASE(Test_get_next_spot_multi_1, LoaderFixture)
{
    vector<CFastqRead> reads;
    auto ss = create_stream(_READ(cDEFLINE1, "GATT\nTAGGA", cQUAL) + _READ(cDEFLINE2_1, "GATT\nTAGGA", cQUAL));
    fastq_reader reader("test", ss);
    string spot;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE_EQ(spot, cSPOT1);
    REQUIRE(reads.size() == 1);
}


FIXTURE_TEST_CASE(Test_get_spot_empty, LoaderFixture)
{
    vector<CFastqRead> reads;
    fastq_reader reader("test", create_stream(""));
    REQUIRE(reader.get_spot(cSPOT1, reads) == false);
    REQUIRE(reads.size() == 0);
}


FIXTURE_TEST_CASE(Test_get_spot_no_match, LoaderFixture)
{
    vector<CFastqRead> reads;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE2_1, "GATT\nTAGGA", cQUAL)));
    REQUIRE(reader.get_spot(cSPOT1, reads) == false);
    REQUIRE(reads.size() == 0);
}

FIXTURE_TEST_CASE(Test_get_spot_no_match2, LoaderFixture)
{
    vector<CFastqRead> reads;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE2_1, "GATT\nTAGGA", cQUAL) + _READ(cDEFLINE1, "GATT\nTAGGA", cQUAL)));
    REQUIRE(reader.get_spot("SOME_SPOT", reads) == false);
    REQUIRE(reads.size() == 0);
}


FIXTURE_TEST_CASE(Test_get_spot_match, LoaderFixture)
{
    vector<CFastqRead> reads;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, "GATT\nTAGGA", cQUAL)));
    REQUIRE(reader.get_spot(cSPOT1, reads));
    REQUIRE(reads.size() == 1);
    REQUIRE_EQ(reads.front().Spot(), cSPOT1);
}


FIXTURE_TEST_CASE(Test_get_spot_match_multi, LoaderFixture)
{
    vector<CFastqRead> reads;
    auto ss = create_stream(_READ(cDEFLINE1, "GATT\nTAGGA", cQUAL) + _READ(cDEFLINE2, "GATT\nTAGGA", cQUAL));
    fastq_reader reader("test", ss, {'B','B'});
    REQUIRE(reader.get_spot(cSPOT1, reads));
    REQUIRE(reads.size() == 2);
    REQUIRE_EQ(reads[0].Spot(), cSPOT1);
    REQUIRE_EQ(reads[1].Spot(), cSPOT1);
}


FIXTURE_TEST_CASE(Test_get_spot_match_multi_skip, LoaderFixture)
{
    vector<CFastqRead> reads;
    auto ss = create_stream(_READ(cDEFLINE2_1, "GATT\nTAGGA", cQUAL) + _READ(cDEFLINE1, "GATT\nTAGGA", cQUAL) + _READ(cDEFLINE2, "GATT\nTAGGA", cQUAL));
    fastq_reader reader("test", ss, {'B', 'B'});
    REQUIRE(reader.get_spot(cSPOT1, reads));
    REQUIRE(reads.size() == 2);
    REQUIRE_EQ(reads[0].Spot(), cSPOT1);
    REQUIRE_EQ(reads[1].Spot(), cSPOT1);
    string spot;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE(reads.size() == 1);
    REQUIRE_EQ(spot, cSPOT2);
    REQUIRE_EQ(reads[0].Spot(), cSPOT2);
}


FIXTURE_TEST_CASE(Test_get_spot_match_multi_skip_2, LoaderFixture)
{
    vector<CFastqRead> reads;
    auto ss = create_stream(
        _READ(cDEFLINE2_1, "GATTTAGGA", cQUAL)
        + _READ(cDEFLINE2_2, "GATTTAGGA", cQUAL)
        + _READ(cDEFLINE1, "GATTTAGGA", cQUAL)
        + _READ(cDEFLINE2, "GATTTAGGA", cQUAL));
    fastq_reader reader("test", ss, {'B', 'B'});
    REQUIRE(reader.get_spot(cSPOT1, reads));
    REQUIRE(reads.size() == 2);
    REQUIRE_EQ(reads[0].Spot(), cSPOT1);
    REQUIRE_EQ(reads[1].Spot(), cSPOT1);
    string spot;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE(reads.size() == 2);
    REQUIRE_EQ(spot, cSPOT2);
    REQUIRE_EQ(reads[0].Spot(), cSPOT2);
    REQUIRE_EQ(reads[1].Spot(), cSPOT2);
}


// Illumina spot names
FIXTURE_TEST_CASE(IlluminaCasava_1_8, LoaderFixture)
{
    // source: SAMN01860354.fastq
    fastq_reader reader("test", create_stream(_READ("HWI-ST273:315:C0LKAACXX:7:1101:1487:2221 2:Y:0:GGCTAC", "AACA", "$.%0")));
    string spot;
    vector<CFastqRead> reads;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE(reads.empty() == false);
    REQUIRE_EQ(string("HWI-ST273:315:C0LKAACXX:7:1101:1487:2221"), spot);
    REQUIRE_EQ(string("GGCTAC"), reads.front().SpotGroup());
    REQUIRE_EQ(string("2"), reads.front().ReadNum());
}


FIXTURE_TEST_CASE(IlluminaCasava_1_8_SpotGroupNumber, LoaderFixture)
{ // source: SAMN01860354.fastq
    fastq_reader reader("test", create_stream(_READ("HWI-ST273:315:C0LKAACXX:7:1101:1487:2221 2:Y:0:1", "AACA", "$.%0")));
    string spot;
    vector<CFastqRead> reads;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE(reads.empty() == false);
    REQUIRE_EQ(string("2"), reads.front().ReadNum());
    REQUIRE_EQ(string("1"), reads.front().SpotGroup());
}


FIXTURE_TEST_CASE(IlluminaCasava_1_8_SpotGroup_MoreMadness, LoaderFixture)
{ // source: SRR1106612

    fastq_reader reader("test", create_stream(_READ("HWI-ST808:130:H0B8YADXX:1:1101:1914:2223 1:N:0:NNNNNN.GGTCCA.AAAA", "AACA", "$.%0")));
    string spot;
    vector<CFastqRead> reads;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE(reads.empty() == false);
    REQUIRE_EQ(string("NNNNNN.GGTCCA.AAAA"), reads.front().SpotGroup());
}

FIXTURE_TEST_CASE(IlluminaCasava_1_8_EmptyTag, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("HWI-ST959:56:D0AW4ACXX:8:1101:1233:2026 2:N:0:", "AACA", "$.%0")));
    string spot;
    vector<CFastqRead> reads;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE(reads.empty() == false);
    REQUIRE_EQ(string("HWI-ST959:56:D0AW4ACXX:8:1101:1233:2026"), spot);
    REQUIRE(reads.front().SpotGroup().empty());
}


FIXTURE_TEST_CASE(Illumina_Underscore, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG", "AACA", "$.%0")));
    string spot;
    vector<CFastqRead> reads;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE(reads.empty() == false);
    REQUIRE_EQ(string("DG7PMJN1:293:D12THACXX:2:1101:1161:1968"), spot);
    REQUIRE_EQ(string("2"), reads.front().ReadNum());
    REQUIRE_EQ(string("GATCAG"), reads.front().SpotGroup());
}


FIXTURE_TEST_CASE(SRA192487, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("HWI-ST1234:33:D1019ACXX:2:1101:1415:2223/1 1:N:0:ATCACG", cSEQ, cQUAL)));
    CFastqRead read;
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(string("HWI-ST1234:33:D1019ACXX:2:1101:1415:2223"), read.Spot());
}

//VDB-4693
FIXTURE_TEST_CASE(BgiNew8Digit, LoaderFixture)
{
  fastq_reader reader("test", create_stream(_READ("V300019058_8BL1C001R00100000012 3:N:0:CGCCGTTT", cSEQ, cQUAL)));
  CFastqRead read;
  REQUIRE(reader.get_read(read));
  REQUIRE_EQ(string("V300019058_8BL1C001R00100000012"), read.Spot());
}

FIXTURE_TEST_CASE(Quality33TooLow, LoaderFixture)
{   // negative qualities are not allowed for Phred33
    // source: SRR016872
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG",
        "GAAACCCCCTATTAGANNNNCNNNNCNATCATGTCA", "II IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII")), {}, 2);
    //reader.set_validator(false, 33, 74);
    CFastqRead read;
    try {
        reader.get_read<validator_options<ePhred, 33, 74>>(read);
        FAIL("Should not reach this point");
    } catch (fastq_error& err) {
        REQUIRE(err.Message().find("unexpected quality score value") != string::npos);
    }
}


FIXTURE_TEST_CASE(Quality33Adjusteed, LoaderFixture)
{   // negative qualities are not allowed for Phred33
    // source: SRR016872
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG",
        "GAAA", "II")), {}, 2);
    //reader.set_validator(false, 33, 74);
    CFastqRead read;
    REQUIRE((reader.get_read<validator_options<ePhred, 33, 74>>(read)));
    REQUIRE_EQ(read.Quality(), string("II??"));
}


FIXTURE_TEST_CASE(Quality64TooLow, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG",
        "GGGGTTAGTGGCAGGGGGGGGGTCTCGGGGGGGGGG", "IIIIIIIIIIIIIIIIII;IIIIIIIIIIIIIIIII")), {}, 2);
    //reader.set_validator(false, 64, 105);
    CFastqRead read;
    try {
        reader.get_read<validator_options<ePhred, 64, 105>>(read);
        FAIL("Should not reach this point");
    } catch (fastq_error& err) {
        REQUIRE(err.Message().find("unexpected quality score value '59'") != string::npos);
    }
}

FIXTURE_TEST_CASE(Quality64Adjusted, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG",
        "GGGG", "II")), {}, 2);
    //reader.set_validator(false, 64, 105);
    CFastqRead read;
    REQUIRE((reader.get_read<validator_options<ePhred, 64, 105>>(read)));
    REQUIRE_EQ(read.Quality(), string("II^^"));
}



FIXTURE_TEST_CASE(TextQualityRejected, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG",
        "GTCGCTTCTCGGAAGNGTGAAAGACAANAATNTTNN",
        "40 3 1 22 17 18 34 8 13 21 3 7 5 0 0 5 1 0 7 3 2 3 3 3 1 1 4 5 5 2 2 5 0 1 5 5")), {}, 2);
    //reader.set_validator(false, -5, 40);
    CFastqRead read;
    REQUIRE_THROW((reader.get_read<validator_options<ePhred, -5, 40>>(read)));
}


FIXTURE_TEST_CASE(TextQualityAccepted, LoaderFixture)
{
    string cNUM_QUAL = "40 3 1 22 17 18 34 8 13 21 3 7 5 0 0 5 1 0 7 3 2 3 3 3 1 1 4 5 5 2 2 5 0 1 5 5";
    vector<uint8_t> cNUM_QUAL_VEC{40,3,1,22,17,18,34,8,13,21,3,7,5,0,0,5,1,0,7,3,2,3,3,3,1,1,4,5,5,2,2,5,0,1,5,5};

    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG",
        "GTCGCTTCTCGGAAGNGTGAAAGACAANAATNTTNN", cNUM_QUAL)), {}, 2);
    //reader.set_validator(true, -5, 40);
    CFastqRead read;
    REQUIRE((reader.get_read<validator_options<eNumeric, -5, 40>>(read)));
    REQUIRE_EQ(read.Quality(), cNUM_QUAL);
    vector<uint8_t> qual_scores_out;
    read.GetQualScores(qual_scores_out);
    REQUIRE(cNUM_QUAL_VEC == qual_scores_out);

}

FIXTURE_TEST_CASE(TextQualityAdjusted, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG",
        "GTCG", "40 3")), {}, 2);
    //reader.set_validator(true, -5, 40);
    CFastqRead read;
    REQUIRE((reader.get_read<validator_options<eNumeric, -5, 40>>(read)));
    REQUIRE_EQ(read.Quality(), string("40 3 25 25"));
    vector<uint8_t> qual_scores_in{40, 3, 25 , 25};
    vector<uint8_t> qual_scores_out;
    read.GetQualScores(qual_scores_out);
    REQUIRE(qual_scores_in == qual_scores_out);

}

// ############################################################
// # Nanopore/MinION fastq
// #
// # @77_2_1650_1_ch100_file0_strand_twodirections (XXX2761339)
// # @77_2_1650_1_ch100_file16_strand_twodirections:pass\77_2_1650_1_ch100_file16_strand.fast5 (SRR2761339)
// # @channel_108_read_11_twodirections:flowcell_17/LomanLabz_PC_E.coli_MG1655_ONI_3058_1_ch108_file21_strand.fast5 (XXX637417 or YYY637417)
// # @channel_108_read_11_complement:flowcell_17/LomanLabz_PC_E.coli_MG1655_ONI_3058_1_ch108_file21_strand.fast5 (XXX637417 or YYY637417)
// # @channel_108_read_11_template:flowcell_17/LomanLabz_PC_E.coli_MG1655_ONI_3058_1_ch108_file21_strand.fast5 (XXX637417 or YYY637417)
// # @channel_346_read_183-1D (SRR1747417)
// # @channel_346_read_183-complement (SRR1747417)
// # @channel_346_read_183-2D (SRR1747417)
// # @ch120_file13-1D (SRR1980822)
// # @ch120_file13-2D (SRR1980822)
// # @channel_108_read_8:LomanLabz_PC_E.coli_MG1655_ONI_3058_1_ch108_file18_strand.fast5 (R-based poRe fastq requires filename, too) (WWW000025)
// # @1dc51069-f61f-45db-b624-56c857c4e2a8_Basecall_2D_000_2d oxford_PC_HG02.3attempt_0633_1_ch96_file81_strand_twodirections:CORNELL_Oxford_Nanopore/oxford_PC_HG02.3attempt_0633_1_ch96_file81_strand.fast5 (SRR2848544 - nanopore3)
// # @ae74c4fb-2c1d-4176-9584-3dfcc6dce41e_Basecall_2D_2d UT317077_20160808_FNFAD22478_MN19846_sequencing_run_FHV_Barcoded_TakeII_88358_ch93_read2620_strand NB06\UT317077_20160808_FNFAD22478_MN19846_sequencing_run_FHV_Barcoded_TakeII_88358_ch93_read2620_strand.fast5 (SRR5085901 - nanopore3)
// # @ddb7d987-73c0-4d9a-8ac0-ac0dbc462ab5_Basecall_2D_2d UT317077_20160808_FNFAD22478_MN19846_sequencing_run_FHV_Barcoded_TakeII_88358_ch100_read4767_strand1 NB06\UT317077_20160808_FNFAD22478_MN19846_sequencing_run_FHV_Barcoded_TakeII_88358_ch100_read4767_strand1.fast5 (SRR5085901 - nanopore3)
// # @f286a4e1-fb27-4ee7-adb8-60c863e55dbb_Basecall_Alignment_template MINICOL235_20170120_FN__MN16250_sequencing_throughput_ONLL3135_25304_ch143_read16010_strand (nanopore5)
// # @channel_101_read_1.1C|1T|2D (ERR1121618 bam converted to fastq)
// # @channel_100_read_20_twodirections:/Users/blbrown/Documents/DATA/Biology Stuff/New Building/Oxford Nanopore/Pan-MAP/VCUEGLequalFAA23773/reads/downloads/pass/Bonnie_PC_VCUEGLequalFAA23773_2353_1_ch100_file20_strand.fast5 (SRR3473970)
// # @5f8415e3-46ae-48fc-9092-a291b8b6a9b9 run_id=47b8d024d71eef532d676f4aa32d8867a259fc1b read=279 mux=3 ch=87 start_time=2017-01-20T16:26:27Z (SRR5621803 - nanopore4)
// # >85d5c1f1-fbc7-4dbf-bf17-215992eb7a08_Basecall_Barcoding_2d MinION2_MinION2_PoreCamp_FAA81710_GrpB2_1140_1_ch103_file54_strand /mnt/data/bioinformatics/Projects/MINion/Aureus_2998-174/Data/minion_run2(demultiplexed)/pass//MinION2_MinION2_PoreCamp_FAA81710_GrpB2_1140_1_ch103_file54_strand.fast5 (ERR1424936 - nanopore3)
// # @2ba6978b-759c-47a5-a3b9-77f03ca3ba66_Basecall_2D_template UNC_MJ0127RV_20160907_FN028_MN17984_sequencing_run_hansen_pool_32604_ch468_read5754_strand (SRR5817721 - nanopore3)
// # >9f328492-d6be-43b1-b2c9-3bf524508841_Basecall_Alignment_template minion_20160730_FNFAD16610_MN17211_sequencing_run_rhodotorula_36861_ch234_read302508_strand pass/minion_20160730_FNFAD16610_MN17211_sequencing_run_rhodotorula_36861_ch234_read302508_strand.fast5 (SRR5821557 - nanopore3)
// # >9f328492-d6be-43b1-b2c9-3bf524508841_Basecall_2D_2d minion_20160730_FNFAD16610_MN17211_sequencing_run_rhodotorula_36861_ch234_read302508_strand pass/minion_20160730_FNFAD16610_MN17211_sequencing_run_rhodotorula_36861_ch234_read302508_strand.fast5 (SRR5821557 - nanopore3)
// # @dd6189de-1023-4092-b1e9-76b291c07723_Basecall_1D_template Kelvin_20170412_FNFAF12973_MN19810_sequencing_run_170412_fungus_scedo_29052_ch239_read26_strand (SRR5812844 - nanopore3)
// # @channel_181_b44bbc58-3753-46d6-882c-0021c0697b55_template pass/6/Athena_20170324_FNFAF13858_MN19255_sequencing_run_fc2_real1_0_53723_ch181_read15475_strand.fast5 (SRR6329415 - nanopore3)
// # @channel_95_2663511f-6459-4e8b-8201-2360d199b9d8_template (SRR6329415 - nanopore)
// # @08441923-cb1a-490c-89b4-d209e234eb30_Basecall_1D_template GPBE6_F39_20170913_FAH26527_MN19835_sequencing_run_R265_r1_FAH26527_96320_read_2345_ch_440_strand fast5/GPBE6_F39_20170913_FAH26527_MN19835_sequencing_run_R265_r1_FAH26527_96320_read_2345_ch_440_strand.fast5 (SRR6377102 - nanopore3_1)
// # @d1aa0fa2-0e49-4030-b9c3-2785d2efd8ed_Basecall_1D_template ifik_cm401_20170420_FNFAE22716_MN16142_sequencing_run_CFsamplesB2bis_15133_ch74_read12210_strand
// # @6bbe187c-50a2-457e-997e-6be564f5980a_Basecall_2D 9B93VG2_20170410_FNFAF20574_MN19395_sequencing_run_AML_001_run2_82880_ch166_read9174_strand (WWW000050 - nanopore3 with no poreRead specified)
// # @aba5dfd4-af02-46d1-9bce-3b62557aa8c1 runid=91c917caaf7b201766339e506ba26eddaf8c06d9 read=29 ch=350 start_time=2018-03-02T16:12:39Z barcode=barcode01(SRR8695851 - nanopore4)
// # @72ad9b11-af72-4a2f-b943-650c9d88962f protocol_group_id=NASA_WCR_PCR_BC_083019 ch=503 barcode=BC08 read=17599 start_time=2019-08-30T21:26:18Z flow_cell_id=FAK67070 runid=4976f978bd6496df7a0ff30873137235afba9834 sample_id=NASA_WCR_083019 (SRR10303645 - nanopore4)
// ############################################################
//
// NANOPORE regular expressions from fastq-load.py
//
// nanopore="[@>+]+?(channel_)(\d+)(_read_)?(\d+)?([!-~]*?)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D)?(:[!-~ ]+?_ch\d+_file\d+_strand.fast5)?(\s+|$)"
// nanopore2="[@>+]([!-~]*?ch)(\d+)(_file)(\d+)([!-~]*?)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D)(:[!-~ ]+?_ch\d+_file\d+_strand.fast5)?(\s+|$)"
// nanopore3="[@>+]([!-~]*?)[: ]?([!-~]+?Basecall)(_[12]D[_0]*?|_Alignment[_0]*?|_Barcoding[_0]*?|)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D|)[: ]([!-~]*?)[: ]?([!-~ ]+?_ch)_?(\d+)(_read|_file)_?(\d+)(_strand\d*.fast5|_strand\d*.*|)(\s+|$)"
// nanopore3_1="[@>+]([!-~]+?)[: ]?([!-~]+?Basecall)(_[12]D[_0]*?|_Alignment[_0]*?|_Barcoding[_0]*?|)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D|)[: ]([!-~]*?)[: ]?([!-~ ]+?_read_)(\d+)(_ch_)(\d+)(_strand\d*.fast5|_strand\d*.*)(\s+|$)"
// nanopore4="[@>+]([!-~]*?\S{8}-\S{4}-\S{4}-\S{4}-\S{12}\S*[_]?\d?)[\s+[!-~ ]*?|]$"
// nanopore5="[@>+]([!-~]*?[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}_Basecall)(_[12]D[_0]*?|_Alignment[_0]*?|_Barcoding[_0]*?)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D)\S*?($)"

class NanoporeFixture : public LoaderFixture
{
public:
    void Nanopore( const string & defline )
    {
        fastq_reader reader("test", create_stream(_READ(defline,"AAGT", "IIII")), {}, 2);
        THROW_ON_FALSE( reader.get_read(m_read) );
        THROW_ON_FALSE( reader.platform() == SRA_PLATFORM_OXFORD_NANOPORE );
        m_type = reader.defline_type();
    }

    string m_type;
    CFastqRead m_read;
};

FIXTURE_TEST_CASE(Nanopore1_readno_early, NanoporeFixture)
{
    Nanopore("channel_108_read_11_twodirections:flowcell_17/LomanLabz_PC_E.coli_MG1655_ONI_3058_1_ch108_file21_strand.fast5");
    REQUIRE_EQ( string("Nanopore1"), m_type );

    REQUIRE_EQ( m_read.Spot(), string( "channel_108_read_11" ) );
    REQUIRE_EQ( m_read.Channel(), string( "108" ) );
    REQUIRE_EQ( m_read.NanoporeReadNo(), string( "11" ) );
}

FIXTURE_TEST_CASE(Nanopore1_readno_late, NanoporeFixture)
{
    Nanopore("channel_181_b44bbc58-3753-46d6-882c-0021c0697b55_template pass/6/Athena_20170324_FNFAF13858_MN19255_sequencing_run_fc2_real1_0_53723_ch181_read15475_strand.fast5");
    REQUIRE_EQ( string("Nanopore1"), m_type );

    REQUIRE_EQ( m_read.Spot(), string( "channel_181_b44bbc58-3753-46d6-882c-0021c0697b55" ) );
    REQUIRE_EQ( m_read.Channel(), string( "181" ) );
    REQUIRE_EQ( m_read.NanoporeReadNo(), string( "15475" ) );
}

FIXTURE_TEST_CASE(Nanopore2, NanoporeFixture)
{
    Nanopore("ch120_file13-1D");
    REQUIRE_EQ( string("Nanopore2"), m_type );

    REQUIRE_EQ( m_read.Spot(), string( "ch120_file13" ) );
    REQUIRE_EQ( m_read.Channel(), string( "120" ) );
    REQUIRE_EQ( m_read.NanoporeReadNo(), string( "0" ) );
}

FIXTURE_TEST_CASE(Nanopore3, NanoporeFixture)
{
    Nanopore("f286a4e1-fb27-4ee7-adb8-60c863e55dbb_Basecall_Alignment_template MINICOL235_20170120_FN__MN16250_sequencing_throughput_ONLL3135_25304_ch143_read16010_strand");
    REQUIRE_EQ( string("Nanopore3"), m_type );

    REQUIRE_EQ( m_read.Spot(), string( "f286a4e1-fb27-4ee7-adb8-60c863e55dbb_Basecall" ) );
    REQUIRE_EQ( m_read.Suffix(), string( "_Alignment" ) );
    REQUIRE_EQ( m_read.Channel(), string( "143" ) );
    REQUIRE_EQ( m_read.NanoporeReadNo(), string( "16010" ) );
}

FIXTURE_TEST_CASE(Nanopore3_1, NanoporeFixture)
{
    Nanopore("08441923-cb1a-490c-89b4-d209e234eb30_Basecall_1D_template GPBE6_F39_20170913_FAH26527_MN19835_sequencing_run_R265_r1_FAH26527_96320_read_2345_ch_440_strand fast5/GPBE6_F39_20170913_FAH26527_MN19835_sequencing_run_R265_r1_FAH26527_96320_read_2345_ch_440_strand.fast5");
    REQUIRE_EQ( string("Nanopore3_1"), m_type );

    //NB: the leading 0 is matched against the prefix "([!-~]+?)" - ask Bob
    REQUIRE_EQ( m_read.Spot(), string( "8441923-cb1a-490c-89b4-d209e234eb30_Basecall" ) );
    REQUIRE_EQ( m_read.Suffix(), string( "_1D" ) );
    REQUIRE_EQ( m_read.Channel(), string( "440" ) );
    REQUIRE_EQ( m_read.NanoporeReadNo(), string( "2345" ) );
}

FIXTURE_TEST_CASE(Nanopore4, NanoporeFixture)
{
    Nanopore("5f8415e3-46ae-48fc-9092-a291b8b6a9b9 run_id=47b8d024d71eef532d676f4aa32d8867a259fc1b m_read=279 mux=3 ch=87 start_time=2017-01-20T16:26:27Z");
    REQUIRE_EQ( string("Nanopore4"), m_type );

    REQUIRE_EQ( m_read.Spot(), string( "5f8415e3-46ae-48fc-9092-a291b8b6a9b9" ) );
    REQUIRE_EQ( m_read.NanoporeReadNo(), string( "279" ) );
    REQUIRE_EQ( m_read.Channel(), string( "87" ) );
}

FIXTURE_TEST_CASE(Nanopore4_WithBarcode, NanoporeFixture)
{
    Nanopore("aba5dfd4-af02-46d1-9bce-3b62557aa8c1 runid=91c917caaf7b201766339e506ba26eddaf8c06d9 read=29 ch=350 start_time=2018-03-02T16:12:39Z barcode=barcode01");
    REQUIRE_EQ( string("Nanopore4"), m_type );

    REQUIRE_EQ( m_read.Spot(), string( "aba5dfd4-af02-46d1-9bce-3b62557aa8c1" ) );
    REQUIRE_EQ( m_read.NanoporeReadNo(), string( "29" ) );
    REQUIRE_EQ( m_read.Channel(), string( "350" ) );
    REQUIRE_EQ( m_read.SpotGroup(), string( "barcode01" ) );
}

FIXTURE_TEST_CASE(Nanopore4_WithBarcodeUnclassified, NanoporeFixture)
{
    Nanopore("aba5dfd4-af02-46d1-9bce-3b62557aa8c1 runid=91c917caaf7b201766339e506ba26eddaf8c06d9 read=29 ch=350 start_time=2018-03-02T16:12:39Z barcode=unclassified");
    REQUIRE_EQ( string("Nanopore4"), m_type );

    REQUIRE_EQ( m_read.SpotGroup(), string() );
}

FIXTURE_TEST_CASE(Nanopore4_EmptyChannel, NanoporeFixture)
{
    Nanopore("aba5dfd4-af02-46d1-9bce-3b62557aa8c1 runid=91c917caaf7b201766339e506ba26eddaf8c06d9 start_time=2018-03-02T16:12:39Z barcode=unclassified");
    REQUIRE_EQ( string("Nanopore4"), m_type );

    REQUIRE_EQ( m_read.Channel(), string("0") );
    REQUIRE_EQ( m_read.NanoporeReadNo(), string("0") );
    REQUIRE( m_read.SpotGroup().empty() );
}

FIXTURE_TEST_CASE(Nanopore5, NanoporeFixture)
{
    Nanopore("a69dd3c2-c98f-4f17-9da5-fe64f97494f6_Basecall_1D_template:1D_000:template");
    REQUIRE_EQ( string("Nanopore5"), m_type );

    REQUIRE_EQ( string( "a69dd3c2-c98f-4f17-9da5-fe64f97494f6_Basecall" ), m_read.Spot() );
    REQUIRE_EQ( string("_1D"), m_read.Suffix());
    REQUIRE_EQ( string("_template"), m_read.NanoporeReadNo() );
}

FIXTURE_TEST_CASE(Nanopore_DefaultReadno, NanoporeFixture)
{
    Nanopore("f286a4e1-fb27-4ee7-adb8-60c863e55dbb_Basecall_Alignment_template MINICOL235_20170120_FN__MN16250_sequencing_throughput_ONLL3135_25304_strand");
    REQUIRE_EQ( m_read.NanoporeReadNo(), string( "0" ) );
}
FIXTURE_TEST_CASE(Nanopore_ReadNoIsAFile, NanoporeFixture)
{
    Nanopore("1dc51069-f61f-45db-b624-56c857c4e2a8_Basecall_2D_000_2d oxford_PC_HG02.3attempt_0633_1_ch96_file81_strand_twodirections:CORNELL_Oxford_Nanopore/oxford_PC_HG02.3attempt_0633_1_ch96_file81_strand.fast5");

    REQUIRE_EQ( m_read.NanoporeReadNo(), string( "0" ) );
}

FIXTURE_TEST_CASE(Nanopore_ReadFilter_Default, NanoporeFixture)
{
    Nanopore("77_2_1650_1_ch100_file16_strand_twodirections:2_1650_1_ch100_file16_strand.fast5");
    REQUIRE_EQ( (int)m_read.ReadFilter(), 0 );
}
FIXTURE_TEST_CASE(Nanopore_ReadFilter_Pass, NanoporeFixture)
{
    Nanopore("77_2_1650_1_ch100_file16_strand_twodirections:pass\\77_2_1650_1_ch100_file16_strand.fast5");
    REQUIRE_EQ( (int)m_read.ReadFilter(), 0 );
}
FIXTURE_TEST_CASE(Nanopore_ReadFilter_Fail, NanoporeFixture)
{
    Nanopore("77_2_1650_1_ch100_file16_strand_twodirections:fail\\77_2_1650_1_ch100_file16_strand.fast5");
    REQUIRE_EQ( (int)m_read.ReadFilter(), 1 );
}

FIXTURE_TEST_CASE(Nanopore_Barcode_BC, NanoporeFixture)
{
    Nanopore("77_2_1650_1_ch100_file16_strand_twodirections:BC01\\77_2_1650_1_ch100_file16_strand.fast5");
    REQUIRE_EQ( m_read.SpotGroup(), string( "BC01") );
}
FIXTURE_TEST_CASE(Nanopore_Barcode_Edit, NanoporeFixture)
{
    Nanopore("77_2_1650_1_ch100_file16_strand_twodirections:barcode01\\77_2_1650_1_ch100_file16_strand.fast5");
    REQUIRE_EQ( m_read.SpotGroup(), string( "BC01") );
}

////////////////////////////////////////////

int main (int argc, char *argv [])
{
    return SharQParserTestSuite(argc, argv);
}
