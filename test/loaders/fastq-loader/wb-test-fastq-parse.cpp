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
#include <kfs/directory.h>
#include <kfs/file.h>

#include "../../tools/loaders/fastq-loader/fastq-parse.h"
#include "../../tools/loaders/fastq-loader/fastq-tokens.h"

#include <sysalloc.h>
#include <stdlib.h>
#include <cstring>
#include <stdexcept>
#include <list>

using namespace std;

TEST_SUITE(FastqParseTestSuite);

//////////////////////////////////////////// tests for flex-generated scanner


// test fixture for scanner tests
class FastqScanFixture
{
public:
    FastqScanFixture()
    {
        pb.self = this;
        pb.input = Input;
        consumed = 0;
    }
    ~FastqScanFixture()
    {
        FASTQScan_yylex_destroy(&pb);
        if (pb.record != 0)
        {
            KDataBufferWhack( & pb.record->source );
            free(pb.record);
        }
    }
    void InitScan(const char* p_input, bool trace=false)
    {
        input = p_input;
        FASTQScan_yylex_init(&pb, trace);
        pb.record = (FastqRecord*)calloc(sizeof(FastqRecord), 1);
        KDataBufferMakeBytes ( & pb.record->source, 0 );
        FASTQ_ParseBlockInit ( &pb );
    }
    int Scan()
    {
        int tokenId = FASTQ_lex ( & sym, & pb );
        if (tokenId != 0)
        {
            tokenText=string(TokenTextPtr(&pb, &sym), sym.tokenLength);
        }
        else
        {
            tokenText.clear();
        }

        return tokenId;
    }
    static size_t CC Input(FASTQParseBlock* sb, char* buf, size_t max_size)
    {
        FastqScanFixture* self = (FastqScanFixture*)sb->self;
        if (self->input.size() < self->consumed)
            return 0;

        size_t to_copy = min(self->input.size() - self->consumed, max_size);
        if (to_copy == 0)
            return 0;

        memmove(buf, self->input.c_str(), to_copy);
        if (to_copy < max_size && buf[to_copy-1] != '\n')
        {
            buf[to_copy] = '\n';
            ++to_copy;
        }
        self->consumed += to_copy;
        return to_copy;
    }

    string input;
    size_t consumed;
    FASTQParseBlock pb;
    FASTQToken sym;
    string tokenText;
};

FIXTURE_TEST_CASE(EmptyInput, FastqScanFixture)
{
    InitScan("");
    REQUIRE_EQUAL(Scan(), 0);
}
#define REQUIRE_TOKEN(tok)              REQUIRE_EQUAL((int)tok, Scan());
#define REQUIRE_TOKEN_TEXT(tok, text)   REQUIRE_TOKEN(tok); REQUIRE_EQ(tokenText, string(text));

#define REQUIRE_TOKEN_COORD(tok, text, line, col)  \
    REQUIRE_TOKEN_TEXT(tok, text); \
    REQUIRE_EQ(pb.lastToken->line_no, (size_t)line); \
    REQUIRE_EQ(pb.lastToken->column_no, (size_t)col);

FIXTURE_TEST_CASE(TagLine1, FastqScanFixture)
{
    InitScan("@HWUSI-EAS499_1:1:3:9:1822\n");
    REQUIRE_TOKEN('@');
    REQUIRE_TOKEN_COORD(fqALPHANUM, "HWUSI-EAS499", 1, 2);
    REQUIRE_TOKEN('_');
    REQUIRE_TOKEN_TEXT(fqNUMBER, "1");
    REQUIRE_TOKEN_TEXT(fqCOORDS, ":1:3:9:1822");
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN(0);
}

FIXTURE_TEST_CASE(SequenceQuality, FastqScanFixture)
{
    InitScan("@8\n" "GATC\n" "+8:1:46:673\n" "!**'\n");
    REQUIRE_TOKEN('@');
    REQUIRE_TOKEN_TEXT(fqNUMBER, "8");
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN_COORD(fqBASESEQ, "GATC", 2, 1);
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN('+');
    REQUIRE_TOKEN_TEXT(fqTOKEN,  "8:1:46:673");
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN_TEXT(fqASCQUAL, "!**'");
    REQUIRE_TOKEN(fqENDLINE);
}

FIXTURE_TEST_CASE(QualityOnly, FastqScanFixture)
{
    InitScan(">8\n" "\x7F!**'\n");
    REQUIRE_TOKEN('>');
    REQUIRE_TOKEN_TEXT(fqNUMBER, "8");
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN_TEXT(fqASCQUAL, "\x7F!**'");
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN(0);
}

FIXTURE_TEST_CASE(NoEOL_InQuality, FastqScanFixture)
{
    InitScan("@8\n" "GATC\n" "+\n" "!**'");
    REQUIRE_TOKEN('@');
    REQUIRE_TOKEN_TEXT(fqNUMBER, "8");
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN_TEXT(fqBASESEQ, "GATC");
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN('+');
    REQUIRE_TOKEN(fqENDLINE);

    REQUIRE_TOKEN_TEXT(fqASCQUAL, "!**'");
    REQUIRE_TOKEN(fqENDLINE); /* this is auto-inserted by FastqScanFixture::Input() */
    REQUIRE_TOKEN(0);
}

FIXTURE_TEST_CASE(CRnoLF, FastqScanFixture)
{
    InitScan("@8\r", 0);
    REQUIRE_TOKEN('@');
    REQUIRE_TOKEN_TEXT(fqNUMBER, "8");
    REQUIRE_TOKEN(fqENDLINE);
}

FIXTURE_TEST_CASE(CommaSeparatedQuality3, FastqScanFixture)
{
    InitScan("@8\n" "GATC\n" "+\n" "0047044004,046,,4000,04444000,--,6-\n");
    REQUIRE_TOKEN('@');
    REQUIRE_TOKEN_TEXT(fqNUMBER, "8");
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN_TEXT(fqBASESEQ, "GATC");
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN('+');
    REQUIRE_TOKEN(fqENDLINE);

    REQUIRE_TOKEN_TEXT(fqASCQUAL, "0047044004,046,,4000,04444000,--,6-");
    REQUIRE_EQUAL(Scan(), (int)fqENDLINE);
    REQUIRE_EQUAL(Scan(), 0);
}

FIXTURE_TEST_CASE(WsBeforeEol, FastqScanFixture)
{
    InitScan("@ \n");
    REQUIRE_TOKEN('@');
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_EQUAL(Scan(), 0);
}

FIXTURE_TEST_CASE(SkipToEol, FastqScanFixture)
{
    InitScan("@ kjalkjaldkj \nGATC\n");
    REQUIRE_TOKEN('@');
    FASTQScan_skip_to_eol(&pb);
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_TOKEN_TEXT(fqBASESEQ, "GATC"); // back to normal
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_EQUAL(Scan(), 0);
}

FIXTURE_TEST_CASE(InlineBaseSequence, FastqScanFixture)
{
    InitScan("@:GATC.NNNN\n");
    REQUIRE_TOKEN('@');
    REQUIRE_TOKEN(':');
    FASTQScan_inline_sequence(&pb);
    REQUIRE_TOKEN_TEXT(fqBASESEQ, "GATC.NNNN");
    REQUIRE_TOKEN(fqENDLINE);
    REQUIRE_EQUAL(Scan(), 0);
}

///////////////////////////////////////////////// Fastq Parser test fixture
class ParserFixture
{
public:
    ParserFixture()
    {
        pb.record = 0;
    }
    ~ParserFixture()
    {
        FASTQScan_yylex_destroy(&pb);
        if (pb.record != 0)
        {
            KDataBufferWhack( & pb.record->source );
            free(pb.record);
        }
    }

    bool Parse( bool traceLex = false, bool traceBison = false )
    {
        pb.self = this;
        pb.input = Input;
        pb.qualityFormat = FASTQphred33;
        pb.defaultReadNumber = 9;
        pb.secondaryReadNumber = 0;
        pb.ignoreSpotGroups = false;

        if (FASTQScan_yylex_init(& pb, traceLex) != 0)
            FAIL("ParserFixture::ParserFixture: FASTQScan_yylex_init failed");

        pb.record = (FastqRecord*)calloc(1, sizeof(FastqRecord));
        if (pb.record == 0)
            FAIL("ParserFixture::ParserFixture: malloc failed");
        KDataBufferMakeBytes ( & pb.record->source, 0 );

        FASTQ_debug = traceBison ? 1 : 0;
        FASTQ_ParseBlockInit ( &pb );
        return FASTQ_parse( &pb ) == 1 && pb.record->rej == 0;
    }

    void AddBuffer(const string& p_text)
    {
        buffers.push_back(p_text);
    }

    static size_t CC Input(struct FASTQParseBlock* sb, char* buf, size_t max_size)
    {
        ParserFixture* self = (ParserFixture*)sb->self;
        if (self->buffers.empty())
            return 0;

        string s = self->buffers.front();
        self->buffers.pop_front();
        memmove(buf, s.c_str(), s.size()); // ignore max_size for our short test lines
        return s.size();
    }

    list<string> buffers;
    FASTQParseBlock pb;
};

FIXTURE_TEST_CASE(BufferBreakInTag, ParserFixture)
{
    AddBuffer("@HWI-ST226:170:AB075UABXX:3:1101:10089:7031 ");
    AddBuffer("1:N:0:GCCAAT\n"
              "TACA\n"
              "+\n"
              "GEGE\n");
    REQUIRE(Parse());
    REQUIRE_EQ(1, (int)pb.record->seq.readnumber);
}

FIXTURE_TEST_CASE(BufferBreakInMultilineRead, ParserFixture)
{
    AddBuffer ( "@HWI-ST226\n" );
    AddBuffer ( "AAAA\nCC" );
    AddBuffer ( "TTT\n" );

    REQUIRE ( Parse() );

    REQUIRE_EQ ( string ( "AAAACCTTT" ),
                 string ( ( const char *) ( pb . record -> source . base ) + pb . readOffset, pb . readLength ) );
}

FIXTURE_TEST_CASE(BufferBreakInMultilineQuality, ParserFixture)
{
    AddBuffer ( "@HWI-ST226\n" );
    AddBuffer ( "AAAA\nCC" );
    AddBuffer ( "TTT\n" );
    AddBuffer ( "+\nabcd\nef" );
    AddBuffer ( "ggg\n" );

    REQUIRE ( Parse() );

    REQUIRE_EQ ( string ( "abcdefggg" ),
                 string ( ( const char *) ( pb . record -> source . base ) + pb . qualityOffset, pb . qualityLength ) );
}

FIXTURE_TEST_CASE(ReadNumberNotSeparated, ParserFixture)
{   // VDB-4531
    char buf[] = "@V/2\nC\n+\nF\nA\n";
    AddBuffer ( buf );
    REQUIRE(Parse());
    REQUIRE_EQ(2, (int)pb.record->seq.readnumber);
    REQUIRE_EQ(string( "V" ) , string( buf + pb.spotNameOffset, pb.spotNameLength ) );
}

FIXTURE_TEST_CASE(ReadNumberNotSeparated_Variation, ParserFixture)
{   // VDB-4531; extra whitespace after the read #
    char buf[] = "@V/2 \nC\n+\nF\nA\n";
    AddBuffer ( buf );
    REQUIRE(Parse());
    REQUIRE_EQ(2, (int)pb.record->seq.readnumber);
    REQUIRE_EQ(string( "V" ) , string( buf + pb.spotNameOffset, pb.spotNameLength ) );
}

FIXTURE_TEST_CASE(BarcodesReadNumbersJunkOhMy, ParserFixture)
{   // VDB-4532
    char buf[] = "@SNPSTER4_246_30GCDAAXX_PE:1:1:3:896/1 run=090102_SNPSTER4_0246_30GCDAAXX_PE\nC\n+\nF\n";
    AddBuffer ( buf );
    REQUIRE(Parse());
}

FIXTURE_TEST_CASE(Aviti, ParserFixture)
{   // VDB-5143
    char buf[] = "@PLT-04:KOL-0149:2140948423:1:10102:0003:0040 2:N:0:GCATGTCACG+TTTAGACCAT\nC\n+\nF\n";
    AddBuffer ( buf );
    REQUIRE( Parse() );
    REQUIRE_EQ( 2, (int)pb.record->seq.readnumber );
    REQUIRE_EQ( string( "PLT-04:KOL-0149:2140948423:1:10102:0003:0040 2:N:0" ) , string( buf + pb.spotNameOffset, pb.spotNameLength ) );
    REQUIRE_EQ( string("GCATGTCACG+TTTAGACCAT"), string( buf + pb.spotGroupOffset, pb.spotGroupLength ) );
}



//////////////////////////////////////////// Main
extern "C"
{

#include <kfg/config.h>
int main ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    return FastqParseTestSuite(argc, argv);
}

}
