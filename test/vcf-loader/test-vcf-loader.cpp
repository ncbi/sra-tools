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
* tests for VCF reader
*/

#include <ktst/unit_test.hpp>

#include <klib/out.h>
#include <klib/namelist.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <align/writer-reference.h>

#include <sysalloc.h>
#include <stdexcept>
#include <sstream>
#include <memory.h>

extern "C" {
#include "../../tools/vcf-loader/vcf-grammar.h"
#include "../../tools/vcf-loader/vcf-parse.h"
#include "../../tools/vcf-loader/vcf-reader.h"
#include "../../tools/vcf-loader/vcf-database.h"
}

using namespace std;
using namespace ncbi::NK;

TEST_SUITE(VcfLoaderTestSuite);

// test fixture for scanner tests
class VcfScanFixture
{
public:
    VcfScanFixture() 
    : consumed(0)
    {
        pb.self = this;
        pb.input = Input;
    }
    ~VcfScanFixture() 
    {
        VCFScan_yylex_destroy(&pb);
    }
    void InitScan(const char* p_input, bool trace=false)
    {
        input = p_input;
        VCFScan_yylex_init(&pb, trace);
    }
    int Scan()
    {
        int tokenId=VCF_lex(&sym, pb.scanner);
        
        if (tokenId != 0)
            tokenText=string(input.c_str() + sym.tokenStart, sym.tokenLength);
            
        return tokenId;
    }
    static size_t CC Input(VCFParseBlock* sb, char* buf, size_t max_size)
    {
        VcfScanFixture* self = (VcfScanFixture*)sb->self;
        if (self->input.size() < self->consumed)
            return 0;

        size_t to_copy = min(self->input.size() - self->consumed, max_size);
        if (to_copy == 0)
            return 0;

        memcpy(buf, self->input.c_str(), to_copy);
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
    VCFParseBlock pb;
    VCFToken sym;
    string tokenText;
};

#define REQUIRE_TOKEN(tok)              REQUIRE_EQUAL(Scan(), (int)tok);
#define REQUIRE_TOKEN_TEXT(tok, text)   REQUIRE_TOKEN(tok); REQUIRE_EQ(tokenText, string(text));

#define REQUIRE_TOKEN_COORD(tok, text, line, col)  \
    REQUIRE_TOKEN_TEXT(tok, text); \
    REQUIRE_EQ(pb.lastToken->line_no, (size_t)line); \
    REQUIRE_EQ(pb.lastToken->column_no, (size_t)col);

FIXTURE_TEST_CASE(EmptyInput, VcfScanFixture)
{   
    InitScan("");
    REQUIRE_TOKEN(0); 
}
FIXTURE_TEST_CASE(MetaLineSimple, VcfScanFixture)
{   
    InitScan("##fileformat=VCFv4.1\n");
    REQUIRE_TOKEN_TEXT(vcfMETAKEY_FORMAT, "fileformat"); 
    REQUIRE_TOKEN_TEXT('=', "="); 
    REQUIRE_TOKEN_TEXT(vcfMETAVALUE, "VCFv4.1"); 
    REQUIRE_TOKEN(vcfENDLINE);
    REQUIRE_TOKEN(0); 
}
FIXTURE_TEST_CASE(MetaLineComposite, VcfScanFixture)
{   
    InitScan("##fileformat=<key1=val1,key2=val2>\n");
    REQUIRE_TOKEN_TEXT(vcfMETAKEY, "fileformat"); 
    REQUIRE_TOKEN_TEXT('=', "="); 
    REQUIRE_TOKEN_TEXT('<', "<"); 
    REQUIRE_TOKEN_TEXT(vcfMETAKEY, "key1"); 
    REQUIRE_TOKEN_TEXT('=', "="); 
    REQUIRE_TOKEN_TEXT(vcfMETAVALUE, "val1"); 
    REQUIRE_TOKEN_TEXT(',', ","); 
    REQUIRE_TOKEN_TEXT(vcfMETAKEY, "key2"); 
    REQUIRE_TOKEN_TEXT('=', "="); 
    REQUIRE_TOKEN_TEXT(vcfMETAVALUE, "val2"); 
    REQUIRE_TOKEN_TEXT('>', ">"); 
    REQUIRE_TOKEN(vcfENDLINE);
    REQUIRE_TOKEN(0); 
}
FIXTURE_TEST_CASE(HeaderLine, VcfScanFixture)
{   
    InitScan("#CHROM\tPOS\n");
    REQUIRE_TOKEN_TEXT(vcfHEADERITEM, "CHROM");
    REQUIRE_TOKEN_TEXT(vcfHEADERITEM, "POS");
    REQUIRE_TOKEN(vcfENDLINE);
    REQUIRE_TOKEN(0); 
}
FIXTURE_TEST_CASE(DataItems, VcfScanFixture)
{   
    InitScan("20\tNS=3;DP=14;AF=0.5;DB;H2\n");
    REQUIRE_TOKEN_TEXT(vcfDATAITEM, "20"); 
    REQUIRE_TOKEN_TEXT(vcfDATAITEM, "NS=3;DP=14;AF=0.5;DB;H2"); 
    REQUIRE_TOKEN(vcfENDLINE);
    REQUIRE_TOKEN(0); 
}
FIXTURE_TEST_CASE(DataLine_TrailingTab, VcfScanFixture)
{   
    InitScan("20\tNS=3;DP=14;AF=0.5;DB;H2\t\n");
    REQUIRE_TOKEN_TEXT(vcfDATAITEM, "20"); 
    REQUIRE_TOKEN_TEXT(vcfDATAITEM, "NS=3;DP=14;AF=0.5;DB;H2"); 
    REQUIRE_TOKEN(vcfENDLINE);
    REQUIRE_TOKEN(0); 
}
FIXTURE_TEST_CASE(DataLine_EOF, VcfScanFixture)
{   
    InitScan("20\tNS=3;DP=14;AF=0.5;DB;H2");
    REQUIRE_TOKEN_TEXT(vcfDATAITEM, "20"); 
    REQUIRE_TOKEN_TEXT(vcfDATAITEM, "NS=3;DP=14;AF=0.5;DB;H2"); 
    REQUIRE_TOKEN(vcfENDLINE);
    REQUIRE_TOKEN(0); 
}

// parser tests

class VcfParseFixture 
{
public:
    VcfParseFixture()
    : expectError(false)
    {
        pb.self = this;
        pb.input = Input;
        pb.error = Error;
        pb.metaLine = AddMetaLine;
        pb.openMetaLine = OpenMetaLine;
        pb.closeMetaLine = CloseMetaLine;
        pb.keyValue = KeyValue;
        pb.headerItem = HeaderItem;
        pb.openDataLine = OpenDataLine;
        pb.dataItem = DataItem;
        pb.closeDataLine = CloseDataLine;
    }
    ~VcfParseFixture()
    {
        VCFScan_yylex_destroy(&pb);
    }
    
protected:
    static size_t Input(VCFParseBlock* pb, char* buf, size_t max_size)
    {
        VcfParseFixture& self = *reinterpret_cast<VcfParseFixture*>(pb->self);
        self.input.read(buf, max_size);
        return self.input.gcount();
    }
    static void Error(VCFParseBlock* pb, const char* message)
    {
        VcfParseFixture& self = *reinterpret_cast<VcfParseFixture*>(pb->self);
        if (!self.expectError)
            throw logic_error(string("VcfParseFixture::Error:") + message);
    }
    
    string TokenToString(const VCFToken& t)
    {
        return string(input.str().c_str() + t.tokenStart, t.tokenLength);
    }
    
    static void AddMetaLine(VCFParseBlock* pb, VCFToken* key, VCFToken* value)
    {
        VcfParseFixture& self = *reinterpret_cast<VcfParseFixture*>(pb->self);
        MetaLine::value_type p;
        p.first = self.TokenToString(*key);
        if (value)
            p.second = self.TokenToString(*value);
        MetaLine ml;
        ml.push_back(p);
        self.meta.push_back(ml);
    }
    static void OpenMetaLine(VCFParseBlock* pb, VCFToken* key)
    {
        AddMetaLine(pb, key, NULL);
    }
    static void CloseMetaLine(VCFParseBlock* pb)
    {
    }
    static void KeyValue(VCFParseBlock* pb, VCFToken* key, VCFToken* value)
    {
        VcfParseFixture& self = *reinterpret_cast<VcfParseFixture*>(pb->self);
        MetaLine::value_type p;
        p.first  = self.TokenToString(*key);
        p.second = self.TokenToString(*value);

        self.meta.back().push_back(p);
    }
    static void HeaderItem(VCFParseBlock* pb, VCFToken* value)
    {
        VcfParseFixture& self = *reinterpret_cast<VcfParseFixture*>(pb->self);
        self.header.push_back(self.TokenToString(*value));
    }
    static void OpenDataLine(VCFParseBlock* pb)
    {
        VcfParseFixture& self = *reinterpret_cast<VcfParseFixture*>(pb->self);
        self.data.push_back(DataLine());
    }
    static void DataItem(VCFParseBlock* pb, VCFToken* value)
    {
        VcfParseFixture& self = *reinterpret_cast<VcfParseFixture*>(pb->self);
        self.data.back().push_back(self.TokenToString(*value));
    }
    static void CloseDataLine(VCFParseBlock* pb)
    {
    }
    
    void InitScan(const char* p_input, bool trace=false)
    {
        input.str(p_input);
        VCFScan_yylex_init(&pb, trace);
    }
    
public:
    VCFParseBlock pb;
    
    istringstream input;
    
    typedef vector<pair<string, string> > MetaLine;
    vector<MetaLine> meta;    
    
    vector<string> header;  // all tokens from the header line
    
    typedef vector<string> DataLine;
    vector<DataLine> data; 
    
    bool expectError;
};

// parser tests
FIXTURE_TEST_CASE(EmptyFile, VcfParseFixture)
{
    InitScan("");
    expectError = true;
    REQUIRE_EQ( VCF_parse(&pb), 0 ); // EOF
}

// meta lines 
FIXTURE_TEST_CASE(FormatLineOnly, VcfParseFixture)
{
    InitScan("##fileformat=VCFv4.0\n"
             "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n"
             "1\t2827693\t.\tCCGTC\tC\t.\tPASS\tSVTYPE=DEL\n");
    REQUIRE_EQ( VCF_parse(&pb), 1 );
    REQUIRE_EQ( meta.size(), (size_t)1 ); 
    REQUIRE_EQ( meta[0].size(), (size_t)1 ); 
    REQUIRE_EQ( meta[0][0].first,   string("fileformat") ); 
    REQUIRE_EQ( meta[0][0].second,  string("VCFv4.0") ); 
}

FIXTURE_TEST_CASE(MetaLines, VcfParseFixture)
{
    InitScan("##fileformat=VCFv4.0\n"
             "##fileDate=20090805\n"
             "##source=myImputationProgramV3.1\n"
             "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n"
             "1\t2827693\t.\tCCGTC\tC\t.\tPASS\tSVTYPE=DEL\n");
    REQUIRE_EQ( VCF_parse(&pb), 1 ); // EOF
    REQUIRE_EQ( meta.size(), (size_t)3 ); 

    REQUIRE_EQ( meta[0].size(), (size_t)1 ); 
    REQUIRE_EQ( meta[0][0].first,   string("fileformat") ); 
    REQUIRE_EQ( meta[0][0].second,  string("VCFv4.0") ); 
    
    REQUIRE_EQ( meta[1].size(), (size_t)1 ); 
    REQUIRE_EQ( meta[1][0].first,   string("fileDate") ); 
    REQUIRE_EQ( meta[1][0].second,  string("20090805") ); 

    REQUIRE_EQ( meta[2].size(), (size_t)1 ); 
    REQUIRE_EQ( meta[2][0].first,   string("source") ); 
    REQUIRE_EQ( meta[2][0].second,  string("myImputationProgramV3.1") ); 
}

FIXTURE_TEST_CASE(Header, VcfParseFixture)
{
    InitScan("##fileformat=VCFv4.0\n"
             "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n"
             "1\t2827693\t.\tCCGTC\tC\t.\tPASS\tSVTYPE=DEL\n");
    REQUIRE_EQ( VCF_parse(&pb), 1 ); 
    REQUIRE_EQ( header.size(), (size_t)8 ); 
    REQUIRE_EQ( header[0], string("CHROM") );
    REQUIRE_EQ( header[1], string("POS") );
    REQUIRE_EQ( header[2], string("ID") );
    REQUIRE_EQ( header[3], string("REF") );
    REQUIRE_EQ( header[4], string("ALT") );
    REQUIRE_EQ( header[5], string("QUAL") );
    REQUIRE_EQ( header[6], string("FILTER") );
    REQUIRE_EQ( header[7], string("INFO") );
}

FIXTURE_TEST_CASE(MetaHeaderData, VcfParseFixture)
{
    InitScan(
        "##fileformat=VCFv4.0\n"
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT\tNA00001\n"
        "1\t2827693\t.\tCCGTC\tC\t.\tPASS\tSVTYPE=DEL;END=2827680;BKPTID=Pindel_LCS_D1099159;HOMLEN=1;HOMSEQ=C;SVLEN=-66\tGT:GQ\t1/1:13.9\n");
    REQUIRE_EQ( VCF_parse(&pb), 1 ); 
    REQUIRE_EQ( data.size(), (size_t)1 ); 
    
    REQUIRE_EQ( data[0].size(), (size_t)10 ); 
    REQUIRE_EQ( data[0][0], string("1") ); 
    REQUIRE_EQ( data[0][1], string("2827693") ); 
    REQUIRE_EQ( data[0][2], string(".") ); 
    REQUIRE_EQ( data[0][3], string("CCGTC") ); 
}

FIXTURE_TEST_CASE(InfoLine, VcfParseFixture)
{
    InitScan("##fileformat=VCFv4.0\n"
             "##INFO=<ID=DP,Number=1,Type=Integer,Description=\"Total Depth\">\n"
             "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT\tNA00001\n"
             "1\t2827693\t.\tCCGTC\tC\t.\tPASS\tSVTYPE=DEL\n");
        
    REQUIRE_EQ( VCF_parse(&pb), 1 ); 
    REQUIRE_EQ( meta.size(), (size_t)2 ); 
    REQUIRE_EQ( meta[1].size(), (size_t)5 ); 
    MetaLine& ml = meta[1];
    REQUIRE_EQ( ml[0].first,   string("INFO") ); 
    REQUIRE_EQ( ml[0].second,  string() ); 
    REQUIRE_EQ( ml[1].first,   string("ID") ); 
    REQUIRE_EQ( ml[1].second,  string("DP") ); 
    REQUIRE_EQ( ml[2].first,   string("Number") ); 
    REQUIRE_EQ( ml[2].second,  string("1") ); 
    REQUIRE_EQ( ml[3].first,   string("Type") ); 
    REQUIRE_EQ( ml[3].second,  string("Integer") ); 
    REQUIRE_EQ( ml[4].first,   string("Description") ); 
    REQUIRE_EQ( ml[4].second,  string("\"Total Depth\"") ); 
}


// data lines 
FIXTURE_TEST_CASE(MultiipleDataLines, VcfParseFixture)
{
    InitScan(
        "##fileformat=VCFv4.0\n"
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT\tNA00001\n"
        "1\t2827693\n"
        "2\t3\n");
    REQUIRE_EQ( VCF_parse(&pb), 1 ); 
    REQUIRE_EQ( data.size(), (size_t)2 ); 
    
    REQUIRE_EQ( data[0].size(), (size_t)2 ); 
    REQUIRE_EQ( data[0][0], string("1") ); 
    REQUIRE_EQ( data[0][1], string("2827693") ); 

    REQUIRE_EQ( data[1].size(), (size_t)2 ); 
    REQUIRE_EQ( data[1][0], string("2") ); 
    REQUIRE_EQ( data[1][1], string("3") ); 
}

// VcfReader

class VcfReaderFixture
{
public:
    VcfReaderFixture()
    : reader(0), messages(0), messageCount(0)
    {
        if (KDirectoryNativeDir(&wd) != 0)
            throw logic_error(string("VcfReaderFixture: KDirectoryNativeDir failed"));
            
        if (VcfReaderMake((const VcfReader**)&reader) != 0)
            throw logic_error(string("VcfReaderFixture: VcfReaderMake failed"));
    }
    ~VcfReaderFixture()
    {
        KDirectoryRemove(wd, true, filename.c_str());
            
        if (reader != 0 && VcfReaderWhack(reader) != 0)
            throw logic_error(string("~VcfReaderFixture: VcfReaderWhack failed"));
            
        if (KDirectoryRelease(wd) != 0)
            throw logic_error(string("~VcfReaderFixture: KDirectoryRelease failed"));
    }
    
    rc_t CreateFile(const char* p_filename, const char* contents)
    {   
        KDirectoryRemove(wd, true, p_filename);
        
        filename = p_filename;
        
        KFile* file;
        rc_t rc = KDirectoryCreateFile(wd, &file, true, 0664, kcmInit, p_filename);
        if (rc == 0)
        {
            size_t num_writ=0;
            rc = KFileWrite(file, 0, contents, strlen(contents), &num_writ);
            if (rc == 0)
                rc = KFileRelease(file);
            else
                KFileRelease(file);
        }
        return rc;
    }
    
    rc_t ParseFile(const char* p_filename)
    {
        messages = 0;
        messageCount = 0;
        
        KFile* file;
        rc_t rc = KDirectoryOpenFileRead(wd, (const KFile**)&file, p_filename);
        if (rc == 0)
        {   
            rc = VcfReaderParse(reader, file, &messages);
            if (messages != NULL)
            {
                rc_t rc2 = VNameListCount(messages, &messageCount);
                if (rc == 0)
                    rc = rc2;
            }
            
            if (rc == 0)
                rc = KFileRelease(file);
            else
                KFileRelease(file);
        }
        return rc;
    }
    
    KDirectory* wd;
    VcfReader *reader;
    string filename;
    const struct VNamelist* messages;
    uint32_t messageCount;
};

FIXTURE_TEST_CASE(VcfReader_EmptyFile, VcfReaderFixture)
{
    REQUIRE_RC(CreateFile(GetName(), "")); 
    REQUIRE_RC_FAIL(ParseFile(GetName())); 
    REQUIRE_NOT_NULL(messages);
    REQUIRE_EQ(1u, messageCount);
    const char* msg;
    REQUIRE_RC(VNameListGet ( messages, 0, &msg ));
    REQUIRE_EQ(string("Empty file"), string(msg)); 
}

static string StringToSTL(const String& str)
{
    return string(str.addr, str.len);
}

FIXTURE_TEST_CASE(VcfReader_Parse, VcfReaderFixture)
{   
    REQUIRE_RC(CreateFile(GetName(), 
        // this is taken from the spec:
        //  http://www.1000genomes.org/wiki/analysis/variant-call-format/vcf-variant-call-format-version-42 
        "##fileformat=VCFv4.2\n"
        "##fileDate=20090805\n"
        "##source=myImputationProgramV3.1\n"
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n"
        "20\t14370\trs6054257\tG\tA\t29\tPASS\tNS=3;DP=14;AF=0.5;DB;H2\n"
        "20\t17330\t.\tT\tA\t3\tq10\tNS=3;DP=11;AF=0.017\n"
        ));
    REQUIRE_RC(ParseFile(GetName())); 
        
    uint32_t count;
    REQUIRE_RC(VcfReaderGetDataLineCount(reader, &count));
    REQUIRE_EQ(2u, count);
    
    const VcfDataLine* line;
    REQUIRE_RC(VcfReaderGetDataLine(reader, 0, &line));
    REQUIRE_NOT_NULL(line);
    REQUIRE_EQ(string("20"),        StringToSTL(line->chromosome));
    REQUIRE_EQ(14370u,              line->position);   
    REQUIRE_EQ(string("rs6054257"), StringToSTL(line->id));
    REQUIRE_EQ(string("G"),         StringToSTL(line->refBases));
    REQUIRE_EQ(string("A"),         StringToSTL(line->altBases));
    REQUIRE_EQ(29u,                 (unsigned int)line->quality);   
    REQUIRE_EQ(string("PASS"),      StringToSTL(line->filter));
    REQUIRE_EQ(string("NS=3;DP=14;AF=0.5;DB;H2"), StringToSTL(line->info));
    
    REQUIRE_RC(VcfReaderGetDataLine(reader, 1, &line));
    REQUIRE_NOT_NULL(line);
    REQUIRE_EQ(string("20"),    StringToSTL(line->chromosome));
    REQUIRE_EQ(17330u,          line->position);   
    REQUIRE_EQ(string("."),     StringToSTL(line->id));
    REQUIRE_EQ(string("T"),     StringToSTL(line->refBases));
    REQUIRE_EQ(string("A"),     StringToSTL(line->altBases));
    REQUIRE_EQ(3u,              (unsigned int)line->quality);   
    REQUIRE_EQ(string("q10"),   StringToSTL(line->filter));
    REQUIRE_EQ(string("NS=3;DP=11;AF=0.017"), StringToSTL(line->info));

    REQUIRE_RC(VcfReaderGetDataLine(reader, 2, &line));
    REQUIRE_NULL(line);
}

FIXTURE_TEST_CASE(VcfReader_Parse_GenotypeFields, VcfReaderFixture)
{   
    REQUIRE_RC(CreateFile(GetName(), 
        "##fileformat=VCFv4.2\n"
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n"
        "20\t14370\trs6054257\tG\tA\t29\tPASS\tNS=3;DP=14;AF=0.5;DB;H2\tblah1\t2blah\tetc...\n"
        ));
    REQUIRE_RC(ParseFile(GetName())); 
        
    const VcfDataLine* line;
    REQUIRE_RC(VcfReaderGetDataLine(reader, 0, &line));
    REQUIRE_NOT_NULL(line);
    
    uint32_t fieldCount;
    REQUIRE_RC( VNameListCount(line->genotypeFields, &fieldCount) );    
    REQUIRE_EQ(3u, fieldCount);
    
    const char* name;
    
    REQUIRE_RC(VNameListGet ( line->genotypeFields, 0, &name ));
    REQUIRE_NOT_NULL(name);
    REQUIRE_EQ(string("blah1"), string(name));
    
    REQUIRE_RC(VNameListGet ( line->genotypeFields, 1, &name ));
    REQUIRE_NOT_NULL(name);
    REQUIRE_EQ(string("2blah"), string(name));

    REQUIRE_RC(VNameListGet ( line->genotypeFields, 2, &name ));
    REQUIRE_NOT_NULL(name);
    REQUIRE_EQ(string("etc..."), string(name));
}

FIXTURE_TEST_CASE(VcfReader_Parse_Errors, VcfReaderFixture)
{   
    REQUIRE_RC(CreateFile(GetName(), 
        "##fileformat=VCFv4.2\n"
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n"
        "20\t14370\trs6054257\tG\tA\t1000\tPASS\tNS=3;DP=14;AF=0.5;DB;H2\tblah1\t2blah\tetc...\n"
        "20\t14370\trs6054257\tG\tA\t10\tPASS\n"
        )); // 1. 1000 is too much for a quality
            // 2. not all mandatory fields present
    REQUIRE_RC_FAIL(ParseFile(GetName())); 
    REQUIRE_EQ(2u, messageCount);
    const char* msg;
    REQUIRE_RC(VNameListGet ( messages, 0, &msg ));
    REQUIRE_EQ(string("line 3 column 24: invalid numeric value for 'quality'"), string(msg));
    REQUIRE_RC(VNameListGet ( messages, 1, &msg ));
    REQUIRE_EQ(string("line 4 column 31: one or more of the 8 mandatory columns are missing"), string(msg));
}

// VcfDatabase
class VcfDatabaseFixture : public VcfReaderFixture
{
public:
    VcfDatabaseFixture()
    {
        if (VDBManagerMakeUpdate(&m_vdbMgr, wd) != 0)
            throw logic_error(string("VcfDatabaseFixture(): VDBManagerMakeUpdate failed"));
    }
    ~VcfDatabaseFixture()
    {
        if (VDBManagerRelease(m_vdbMgr) != 0)
            throw logic_error(string("~VcfDatabaseFixture: VDBManagerRelease failed"));
    }
    
    void Setup(const string& p_caseName)
    {
        m_dbName = p_caseName+".db";
        m_cfgName = p_caseName+".cfg";
        
        KDirectoryRemove(wd, true, m_dbName.c_str());
        
        if (VDBManagerMakeSchema(m_vdbMgr, &m_schema) != 0)
            throw logic_error(string("VcfDatabaseFixture::Setup: VDBManagerMakeSchema failed"));
        
        if (VSchemaParseText ( m_schema, "vcf_schema", schema_text, string_measure(schema_text, NULL) ) != 0)
            throw logic_error(string("VcfDatabaseFixture::Setup: VSchemaParseText failed"));

        if (VDBManagerCreateDB(m_vdbMgr, &m_db, m_schema, "vcf", kcmCreate | kcmMD5, m_dbName.c_str()) != 0)
            throw logic_error(string("VcfDatabaseFixture::Setup: VDBManagerCreateDB failed"));
        
        if (CreateFile(m_cfgName.c_str(), "20	CM000682.1"))
            throw logic_error(string("VcfDatabaseFixture::Setup: CreateFile failed"));
    }
    void Teardown()
    {
        if (VDatabaseRelease(m_db) != 0)
            throw logic_error(string("VcfDatabaseFixture::Teardown: VDatabaseRelease failed"));
        if (VSchemaRelease(m_schema) != 0)
            throw logic_error(string("VcfDatabaseFixture::Teardown: VSchemaRelease failed"));
        // this remove fails on Windows for unexplained reasons ("access denied" to <dbname>/tbl/REFERENCE/col), so ignore the result:
        KDirectoryRemove(wd, true, m_dbName.c_str());
        if (KDirectoryRemove(wd, true, m_cfgName.c_str()) != 0)
            throw logic_error(string("VcfDatabaseFixture::Teardown: KDirectoryRemove(m_cfgName) failed"));
    }
    
protected:
    static const char schema_text[];
    static const uint32_t basesPerRow = 5000;
    
    VDBManager* m_vdbMgr;
    VSchema* m_schema;
    VDatabase* m_db;
    string m_cfgName;
    string m_dbName;
};

const char VcfDatabaseFixture::schema_text[] =
"version 1; "

"include 'align/align.vschema';"

" table variant #1 { "
"     extern    column U32 ref_id = .ref_id;"
"     physical  column U32 .ref_id = ref_id;"

"     extern    column U32 position = .position;"
"     physical  column U32 .position = position;"

"     extern    column U32 length = .length;"
"     physical  column U32 .length = length;"

"     extern    column ascii sequence = .sequence;"
"     physical  column ascii .sequence = sequence;"

" };"

" table variant_phase #1 { "
"     extern    column U32 variant_id = .variant_id;"
"     physical  column U32 .variant_id = variant_id;"

"     extern    column U32 phase = .phase;"
"     physical  column U32 .phase = phase;"
" };"

" table alignment #1 { "
"     extern    column U32 align_id = .align_id;"
"     physical  column U32 .align_id = align_id;"

"     extern    column U32 ref_id = .ref_id;"
"     physical  column U32 .ref_id = ref_id;"

"     extern    column U32 position = .position;"
"     physical  column U32 .position = position;"

"     extern    column U32 phase = .phase;"
"     physical  column U32 .phase = phase;"

"     extern    column ascii cigar = .cigar;"
"     physical  column ascii .cigar = cigar;"

"     extern    column ascii mismatch = .mismatch;"
"     physical  column ascii .mismatch = mismatch;"

" };"

"database vcf #1 { "
"    table NCBI:align:tbl:reference #2  REFERENCE;"
"    table variant #1                   VARIANT;"
"    table variant_phase #1             VARIANT_PHASE;"
"    table alignment #1                 ALIGNMENT;"
"};";

FIXTURE_TEST_CASE(VcfDatabase_GetReference, VcfDatabaseFixture)
{
    // this would be better done as a unit test on the ReferenceSeq interface
    // (there is no appropriate suite for that at this point)
    Setup(GetName());
    const ReferenceMgr* refMgr;
    REQUIRE_RC(ReferenceMgr_Make(&refMgr, m_db, m_vdbMgr, 0, m_cfgName.c_str(), NULL, 0, 0, 0));
    const ReferenceSeq* seq = NULL;
#ifdef VDB_1415
    bool dummy = false;
    REQUIRE_RC(ReferenceMgr_GetSeq(refMgr, &seq, "20", &dummy));

    INSDC_coord_zero coord = 14370;
    int64_t ref_id;
    INSDC_coord_zero ref_start;
    uint64_t global_ref_start;
    REQUIRE_RC(ReferenceSeq_TranslateOffset_int(seq, coord, &ref_id, &ref_start, &global_ref_start));
    REQUIRE_EQ(ref_id, (int64_t)(1 + coord / basesPerRow));
    REQUIRE_EQ(ref_start, (INSDC_coord_zero)(coord % basesPerRow));
    REQUIRE_EQ(global_ref_start, (uint64_t)coord);
#endif

    REQUIRE_RC(ReferenceSeq_Release(seq));
    REQUIRE_RC(ReferenceMgr_Release(refMgr, true, NULL, false, NULL));
    Teardown();
}

FIXTURE_TEST_CASE(VcfDatabaseBasic, VcfDatabaseFixture)
{
    Setup(GetName());

    REQUIRE_RC(CreateFile(GetName(), 
        // this is taken from the spec:
        //  http://www.1000genomes.org/wiki/analysis/variant-call-format/vcf-variant-call-format-version-42 
        "##fileformat=VCFv4.2\n"
        "##fileDate=20090805\n"
        "##source=myImputationProgramV3.1\n"
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n"
        "20\t14370\trs6054257\tG\tCCCC\t29\tPASS\tNS=3;DP=14;AF=0.5;DB;H2\n"
        "20\t17330\t.\tT\tA\t3\tq10\tNS=3;DP=11;AF=0.017\n"
        ));
    REQUIRE_RC(ParseFile(GetName())); 

    REQUIRE_RC(VcfDatabaseSave(reader, m_cfgName.c_str(), m_db));

    // verify
    const VTable *tbl;
    REQUIRE_RC(VDBManagerOpenTableRead(m_vdbMgr, &tbl, m_schema, (m_dbName+"/tbl/VARIANT").c_str()));
    VCursor *cur = NULL;
    REQUIRE_RC(VTableCreateCursorRead( tbl, (const VCursor**)&cur ));
    
    uint32_t ref_id_idx, position_idx, length_idx, sequence_idx;
    REQUIRE_RC(VCursorAddColumn( cur, &ref_id_idx, "ref_id" ));
    REQUIRE_RC(VCursorAddColumn( cur, &position_idx, "position" ));
    REQUIRE_RC(VCursorAddColumn( cur, &length_idx, "length" ));
    REQUIRE_RC(VCursorAddColumn( cur, &sequence_idx, "sequence" ));    
    
#ifdef VDB_1415
    REQUIRE_RC(VCursorOpen( cur ));

    char buf[256];
    uint32_t row_len;
    const uint32_t elemBits = 8;

    int64_t rowId = 1;
    REQUIRE_RC(VCursorReadDirect(cur, rowId, 1, elemBits, buf, sizeof(buf), &row_len ));    
    REQUIRE_EQ(8u, row_len);
    REQUIRE_EQ(3u, *(uint32_t*)buf);
    
    REQUIRE_RC(VCursorReadDirect(cur, rowId, 2, elemBits, buf, sizeof(buf), &row_len ));    
    REQUIRE_EQ(sizeof(INSDC_coord_zero), (size_t)row_len);
    REQUIRE_EQ((INSDC_coord_zero)(14370u % basesPerRow), *(INSDC_coord_zero*)buf);
    
    REQUIRE_RC(VCursorReadDirect(cur, rowId, 3, elemBits, buf, sizeof(buf), &row_len ));    
    REQUIRE_EQ(sizeof(uint32_t), (size_t)row_len);
    REQUIRE_EQ(4u, *(uint32_t*)buf);

    REQUIRE_RC(VCursorReadDirect(cur, rowId, 4, elemBits, buf, sizeof(buf), &row_len ));    
    REQUIRE_EQ(4u, row_len);
    REQUIRE_EQ(string(buf, 4), string("CCCC"));

    rowId = 2;
    REQUIRE_RC(VCursorReadDirect(cur, rowId, 1, elemBits, buf, sizeof(buf), &row_len ));    
    REQUIRE_EQ(8u, row_len);
    REQUIRE_EQ(4u, *(uint32_t*)buf);
    
    REQUIRE_RC(VCursorReadDirect(cur, rowId, 2, elemBits, buf, sizeof(buf), &row_len ));    
    REQUIRE_EQ(sizeof(INSDC_coord_zero), (size_t)row_len);
    REQUIRE_EQ((INSDC_coord_zero)(17330u % basesPerRow), *(INSDC_coord_zero*)buf);
    
    REQUIRE_RC(VCursorReadDirect(cur, rowId, 3, elemBits, buf, sizeof(buf), &row_len ));    
    REQUIRE_EQ(sizeof(uint32_t), (size_t)row_len);
    REQUIRE_EQ(1u, *(uint32_t*)buf);

    REQUIRE_RC(VCursorReadDirect(cur, rowId, 4, elemBits, buf, sizeof(buf), &row_len ));    
    REQUIRE_EQ(1u, row_len);
    REQUIRE_EQ(string(buf, 1), string("A"));

    rowId = 3;
    REQUIRE_RC_FAIL(VCursorReadDirect(cur, rowId, 1, elemBits, buf, sizeof(buf), &row_len ));    
#endif
    
    REQUIRE_RC(VCursorRelease(cur));
    REQUIRE_RC(VTableRelease(tbl));
    Teardown();
}

//////////////////////////////////////////// Main
#include <kapp/args.h>
#include <kfg/config.h>

extern "C"
{

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}

const char UsageDefaultName[] = "vcf-loader-test";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ( "Usage:\n" "\t%s [options] -o path\n\n", progname );
}

rc_t CC Usage( const Args* args )
{
    return 0;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    // need user settings to pick up the correct schema include directory,
    // so do not disable them
    //KConfigDisableUserSettings();
    
    rc_t rc = VcfLoaderTestSuite(argc, argv);
    return rc;
}

}

