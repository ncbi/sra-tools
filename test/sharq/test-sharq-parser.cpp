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
    REQUIRE(reader.parse_read(read) == false);
    REQUIRE(reader.eof());
}


FIXTURE_TEST_CASE(EndLines, LoaderFixture)
{
    //shared_ptr<stringstream> ss(new stringstream);
    //*ss << "\n\n";
    fastq_reader reader("test", create_stream("\n\n"));
    CFastqRead read;
    REQUIRE(reader.parse_read(read) == false);
    REQUIRE(reader.eof());
}


FIXTURE_TEST_CASE(InvalidDefline, LoaderFixture)
{
    fastq_reader reader("test", create_stream("qqq abcd"));
    CFastqRead read;
    REQUIRE_THROW(reader.parse_read(read));
}


FIXTURE_TEST_CASE(InvalidDeflineError, LoaderFixture)
{
    fastq_reader reader("test", create_stream("qqq abcd"));
    CFastqRead read;
    try {
        reader.parse_read(read);
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
    REQUIRE(reader.parse_read(read));
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
    REQUIRE(reader.parse_read(read));
    REQUIRE_THROW(reader.parse_read(read));
}

FIXTURE_TEST_CASE(MultiLineRead, LoaderFixture)
{   // multiline record 
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, cSEQ_MULTILINE, cQUAL_MULTILINE)));
    REQUIRE(reader.parse_read(read));
    CHECK_EQ(read.Sequence(), cSEQ);
    CHECK_EQ(read.Quality(), cQUAL);
}

FIXTURE_TEST_CASE(NoLastLF, LoaderFixture)
{
    fastq_reader reader("test", create_stream(
        _READ(cDEFLINE1, "GATT", cQUAL) + "@" + cDEFLINE2  + "\nACGTACGT\n+\n!''*!''*"));
    CFastqRead read;
    REQUIRE(reader.parse_read(read));
    REQUIRE(reader.parse_read(read));
    REQUIRE_EQ(read.LineNumber(), 5lu);
    REQUIRE_EQ(read.Quality().size(), 8lu);
    REQUIRE(reader.eof());
}


FIXTURE_TEST_CASE(InvalidSequence, LoaderFixture)
{   // invalid sequence character 
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, "GA^TT", cQUAL)));
    REQUIRE_THROW(reader.parse_read(read));
}

FIXTURE_TEST_CASE(EmptyQuality, LoaderFixture)
{   // invalid quality scores 
    CFastqRead read;
    fastq_reader reader("test", create_stream(_READ(cDEFLINE1, "GA^TT", "")));
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


//////////////////// tag line parsing
#define TEST_TAGLINE(line)\
    string cREAD = string(line) + "\n" +cSEQ + "\n+\n" + cQUAL + "\n"; \
    CFastqRead read; \
    fastq_reader reader("test", create_stream(cREAD)); \
    REQUIRE(reader.get_read(read)); \
\

FIXTURE_TEST_CASE(IlluminaNew1, LoaderFixture)  { TEST_TAGLINE("@M00730:68:000000000-A2307:1:1101:14701:1383 1:N:0:1"); }
FIXTURE_TEST_CASE(IlluminaNew2, LoaderFixture)  { TEST_TAGLINE("@HWI-962:74:C0K69ACXX:8:2104:14888:94110 2:N:0:CCGATAT"); }
FIXTURE_TEST_CASE(IlluminaNew3, LoaderFixture)  { TEST_TAGLINE("@HWI-ST808:130:H0B8YADXX:1:1101:1914:2223 1:N:0:NNNNNN-GGTCCA-AAAA"); }
FIXTURE_TEST_CASE(IlluminaNew4, LoaderFixture)  { TEST_TAGLINE("@HWI-M01380:63:000000000-A8KG4:1:1101:17932:1459 1:N:0:Alpha29 CTAGTACG|0|GTAAGGAG|0"); }
FIXTURE_TEST_CASE(IlluminaNew5, LoaderFixture)  { TEST_TAGLINE("@HWI-ST959:56:D0AW4ACXX:8:1101:1233:2026 2:N:0:"); }
FIXTURE_TEST_CASE(IlluminaNew6, LoaderFixture)  { TEST_TAGLINE("@DJB77P1:546:H8V5MADXX:2:1101:11528:3334 1:N:0:_I_GACGAC"); }
FIXTURE_TEST_CASE(IlluminaNew7, LoaderFixture)  { TEST_TAGLINE("@HET-141-007:154:C391TACXX:6:1216:12924:76893 1:N:0"); }
FIXTURE_TEST_CASE(IlluminaNew8, LoaderFixture)  { TEST_TAGLINE("@DG7PMJN1:293:D12THACXX:2:1101:1161:1968_1:N:0:GATCAG"); }
FIXTURE_TEST_CASE(IlluminaNew9, LoaderFixture)  { TEST_TAGLINE("@M01321:49:000000000-A6HWP:1:1101:17736:2216_1:N:0:1/M01321:49:000000000-A6HWP:1:1101:17736:2216_2:N:0:1"); }
FIXTURE_TEST_CASE(IlluminaNew10, LoaderFixture)  { TEST_TAGLINE("@MISEQ:36:000000000-A5BCL:1:1101:24982:8584;smpl=12;brcd=ACTTTCCCTCGA 1:N:0:ACTTTCCCTCGA"); }
FIXTURE_TEST_CASE(IlluminaNew11, LoaderFixture)  { TEST_TAGLINE("@HWI-ST1234:33:D1019ACXX:2:1101:1415:2223/1 1:N:0:ATCACG"); }
FIXTURE_TEST_CASE(IlluminaNew12, LoaderFixture)  { TEST_TAGLINE("@aa,HWI-7001455:146:H97PVADXX:2:1101:1498:2093 1:Y:0:ACAAACGGAGTTCCGA"); }
FIXTURE_TEST_CASE(IlluminaNew13, LoaderFixture)  { TEST_TAGLINE("@NS500234:97:HC75GBGXX:1:11101:6479:1067 1:N:0:ATTCAG+NTTCGC"); }
FIXTURE_TEST_CASE(IlluminaNew14, LoaderFixture)  { TEST_TAGLINE("@M01388:38:000000000-A49F2:1:1101:14022:1748 1:N:0:0"); }
FIXTURE_TEST_CASE(IlluminaNew15, LoaderFixture)  { TEST_TAGLINE("@M00388:100:000000000-A98FW:1:1101:17578:2134 1:N:0:1|isu|119|c95|303"); }
FIXTURE_TEST_CASE(IlluminaNew16, LoaderFixture)  { TEST_TAGLINE("@HISEQ:191:H9BYTADXX:1:1101:1215:1719 1:N:0:TTAGGC##NGTCCG"); }
FIXTURE_TEST_CASE(IlluminaNew17, LoaderFixture)  { TEST_TAGLINE("@HWI:1:X:1:1101:1298:2061 1:N:0: AGCGATAG (barcode is discarded)"); }
FIXTURE_TEST_CASE(IlluminaNew18, LoaderFixture)  { TEST_TAGLINE("@8:1101:1486:2141 1:N:0:/1"); }
FIXTURE_TEST_CASE(IlluminaNew19, LoaderFixture)  { TEST_TAGLINE("@HS2000-1017_69:7:2203:18414:13643|2:N:O:GATCAG"); }
FIXTURE_TEST_CASE(IlluminaNew20, LoaderFixture)  { TEST_TAGLINE("@HISEQ:258:C6E8AANXX:6:1101:1823:1979:CGAGCACA:1:N:0:CGAGCACA:NG:GT"); }
FIXTURE_TEST_CASE(IlluminaNew21, LoaderFixture)  { TEST_TAGLINE("@HWI-ST226:170:AB075UABXX:3:1101:1436:2127 1:N:0:GCCAAT"); }
FIXTURE_TEST_CASE(IlluminaNew22, LoaderFixture)  { TEST_TAGLINE("@HISEQ06:187:C0WKBACXX:6:1101:1198:2254 1:N:0:"); }

FIXTURE_TEST_CASE(IlluminaNewDataGroup1, LoaderFixture)  { TEST_TAGLINE("@SEDCJ:00674:05781 1:N:0:AAAAA"); }

FIXTURE_TEST_CASE(BGI1, LoaderFixture)  { TEST_TAGLINE("@V300019058_8BL1C001R0010000000 1:N:0:ATGGTAGG"); }
FIXTURE_TEST_CASE(BGI2, LoaderFixture)  { TEST_TAGLINE("@V300103666L2C001R0010000000:0:0:0:0 1:N:0:ATAGTCTC"); }
FIXTURE_TEST_CASE(BGI3, LoaderFixture)  { TEST_TAGLINE("@CL100159005L1C001R001_2 2:N:0:0"); }

FIXTURE_TEST_CASE(BGIOld1, LoaderFixture)  { TEST_TAGLINE("@CL100050407L1C001R001_1#224_1078_917/1 1       1"); }


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
    fastq_reader reader("test", ss);
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
    fastq_reader reader("test", ss);
    REQUIRE(reader.get_spot(cSPOT1, reads));
    REQUIRE(reads.size() == 2);
    REQUIRE_EQ(reads[0].Spot(), cSPOT1);
    REQUIRE_EQ(reads[1].Spot(), cSPOT1);
}


FIXTURE_TEST_CASE(Test_get_spot_match_multi_skip, LoaderFixture)
{
    vector<CFastqRead> reads;
    auto ss = create_stream(_READ(cDEFLINE2_1, "GATT\nTAGGA", cQUAL) + _READ(cDEFLINE1, "GATT\nTAGGA", cQUAL) + _READ(cDEFLINE2, "GATT\nTAGGA", cQUAL));
    fastq_reader reader("test", ss);
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
    fastq_reader reader("test", ss);
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

/*
FIXTURE_TEST_CASE(Illumina_SpaceAndIdentifierAtFront, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ(" QSEQ161 EAS139:136:FC706VJ:2:2104:15343:197393 1:Y:18:ATCACG", "AACA", "$.%0")));
    string spot;
    vector<CFastqRead> reads;
    REQUIRE(reader.get_next_spot(spot, reads));
    REQUIRE(reads.empty() == false);
    REQUIRE_EQ(string("EAS139:136:FC706VJ:2:2104:15343:197393"), spot);
    REQUIRE_EQ(string("1"), reads.front().ReadNum());
    REQUIRE_EQ(string("ATCACG"), reads.front().SpotGroup());
}

FIXTURE_TEST_CASE(SRR1778155 , LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("2-796964       M01929:5:000000000-A46YE:1:1108:16489:18207 1:N:0:2     orig_bc=TATCGGGA        new_bc=TATCGGGA   bc_diffs=0", cSEQ, cQUAL)));
    CFastqRead read;
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(string("M01929:5:000000000-A46YE:1:1108:16489:18207"), read.Spot());

}
*/

FIXTURE_TEST_CASE(SRA192487, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("HWI-ST1234:33:D1019ACXX:2:1101:1415:2223/1 1:N:0:ATCACG", cSEQ, cQUAL)));
    CFastqRead read;
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(string("HWI-ST1234:33:D1019ACXX:2:1101:1415:2223"), read.Spot());
}


FIXTURE_TEST_CASE(Quality33TooLow, LoaderFixture)
{   // negative qualities are not allowed for Phred33
    // source: SRR016872
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG", 
        "GAAACCCCCTATTAGANNNNCNNNNCNATCATGTCA", "II IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII")));
    reader.set_qual_validator(true, 33, 74);
    CFastqRead read;
    try {
        reader.get_read(read);
        FAIL("Should not reach this point");
    } catch (fastq_error& err) {
        REQUIRE(err.Message().find("unexpected quality score value") != string::npos);
    }
}

FIXTURE_TEST_CASE(Quality33Adjusteed, LoaderFixture)
{   // negative qualities are not allowed for Phred33
    // source: SRR016872
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG", 
        "GAAA", "II")));
    reader.set_qual_validator(true, 33, 74);
    CFastqRead read;
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(read.Quality(), string("II??"));
}


FIXTURE_TEST_CASE(Quality64TooLow, LoaderFixture)
{   
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG", 
        "GGGGTTAGTGGCAGGGGGGGGGTCTCGGGGGGGGGG", "IIIIIIIIIIIIIIIIII;IIIIIIIIIIIIIIIII")));
    reader.set_qual_validator(true, 64, 105);
    CFastqRead read;
    try {
        reader.get_read(read);
        FAIL("Should not reach this point");
    } catch (fastq_error& err) {
        REQUIRE(err.Message().find("unexpected quality score value '59'") != string::npos);
    }
}

FIXTURE_TEST_CASE(Quality64Adjusted, LoaderFixture)
{   
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG", 
        "GGGG", "II")));
    reader.set_qual_validator(true, 64, 105);
    CFastqRead read;
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(read.Quality(), string("II^^"));
}



FIXTURE_TEST_CASE(TextQualityRejected, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG", 
        "GTCGCTTCTCGGAAGNGTGAAAGACAANAATNTTNN", 
        "40 3 1 22 17 18 34 8 13 21 3 7 5 0 0 5 1 0 7 3 2 3 3 3 1 1 4 5 5 2 2 5 0 1 5 5")));
    reader.set_qual_validator(true, -5, 40);
    CFastqRead read;
    REQUIRE_THROW(reader.get_read(read));
}


FIXTURE_TEST_CASE(TextQualityAccepted, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG", 
        "GTCGCTTCTCGGAAGNGTGAAAGACAANAATNTTNN", 
        "40 3 1 22 17 18 34 8 13 21 3 7 5 0 0 5 1 0 7 3 2 3 3 3 1 1 4 5 5 2 2 5 0 1 5 5")));
    reader.set_qual_validator(false, -5, 40);
    CFastqRead read;
    REQUIRE(reader.get_read(read));
}

FIXTURE_TEST_CASE(TextQualityAdjusted, LoaderFixture)
{
    fastq_reader reader("test", create_stream(_READ("DG7PMJN1:293:D12THACXX:2:1101:1161:1968_2:N:0:GATCAG", 
        "GTCG", "40 3")));
    reader.set_qual_validator(false, -5, 40);
    CFastqRead read;
    REQUIRE(reader.get_read(read));
    REQUIRE_EQ(read.Quality(), string("40 3 25 25"));
}


int main (int argc, char *argv []) 
{
    return SharQParserTestSuite(argc, argv);
}

