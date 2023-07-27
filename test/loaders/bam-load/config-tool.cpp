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
* Unit tests for Reference config
*/

#include <klib/rc.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <align/writer-reference.h>

#include <ktst/unit_test.hpp>
#include <ktst/unit_test_suite.hpp>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

struct CommandLine {
    static void usage(char const *argv0) {
        for (auto cp = argv0; *cp; ++cp) {
            if (cp[0] == '/' && cp[1] != '\0')
                argv0 = cp + 1;
        }
        std::cerr << "usage: " << argv0 << " [--skip-verify] [--only-verify] <config file> [-|<reference file>] [<fasta file>]" << std::endl;
    }
    CommandLine(int argc, char *argv[])
    : argument(argv)
    , count(argc)
    , skip_verify_index(0)
    , only_verify_index(0)
    , config_file_index(0)
    , fasta_file_index(0)
    , references_file_index(0)
    , references(nullptr)
    {
        if (count <= 0)
            throw std::invalid_argument("argument count");
            
        for (int i = 1; i < count; ++i) {
            auto const &arg = std::string_view{ argv[i] };
            
            if (arg == "--")
                break;
            if (arg == "--skip-verify") {
                skip_verify_index = i;
                continue;
            }
            if (arg == "--only-verify") {
                only_verify_index = i;
                continue;
            }
            if (arg == "-") {
                references_file_index = -1;
                continue;
            }
            if (config_file_index == 0) {
                config_file_index = i;
                continue;
            }
            if (references_file_index == 0) {
                references_file_index = i;
                continue;
            }
            if (fasta_file_index == 0) {
                fasta_file_index = i;
                continue;
            }
            usage(argv[0]);
            throw std::invalid_argument(argv[i]);
        }
    }
    ~CommandLine() {
        delete references;
    }
    
    /// argv[0]
    char const *progname() const { return argument[0]; }

    char const *config_file() const {
        if (config_file_index > 0)
            return argument[config_file_index];
        return nullptr;
    }

    char const *fasta_file() const {
        if (fasta_file_index > 0)
            return argument[fasta_file_index];
        return nullptr;
    }

    std::istream &references_file(std::ios_base::openmode mode = std::ios::in) const {
        if (references)
            return *references;

        if (references_file_index <= 0)
            return std::cin;
            
        references = new std::ifstream();

        auto const save = references->exceptions();
        references->exceptions(std::ios::failbit);
        references->open(argument[references_file_index], mode & ~std::ios::out);
        references->exceptions(save);
        
        return *references;
    }
    
    /// is `--skip-verify` on   
    bool skip_verify() const { return skip_verify_index != 0; }
    
    /// is `--only-verify` on   
    bool only_verify() const { return only_verify_index != 0; }

private:
    char **argument;
    int count;

    int skip_verify_index = 0;
    int only_verify_index = 0;
    int config_file_index = 0;
    int fasta_file_index = 0;
    int references_file_index = 0;

    mutable std::ifstream *references = nullptr;
};
static CommandLine const *arguments;

struct ConfigFile {
    struct Entry {
    	std::string name;
        std::string seqId;
        std::string extra;
        
        Entry() = default;
        Entry(std::string const &line)
        {
            std::istringstream strm(line);
            
            strm >> name >> seqId;
            if (strm) {
                strm >> std::ws;
                std::getline(strm, extra);
            }
            else
                throw std::ios_base::failure("unparsable");
        }
    };
    std::vector<Entry> entries;
    
    ConfigFile(char const *filePath) {
        auto file = std::ifstream{ filePath };
        
        file >> std::ws;
        for (std::string line; std::getline(file, line); file >> std::ws) {
#if ALLOW_COMMENT_LINES
            if (line.substr(0, 1) == "#")
                continue;
#endif
            try {
                Entry e(line);
                entries.emplace_back(e);
            }
            catch (std::ios_base::failure const &e) {
                throw std::ios_base::failure("unparsable config file");
            }
        }
    }
};

static std::vector<std::string> referenceList(CommandLine const &cmdline)
{
    auto result = std::vector<std::string>{};
    auto &strm = cmdline.references_file();

    while (strm) {
        auto word = std::string{};
        if ((strm >> word))
            result.emplace_back(word);
    }
    return result;
}

using namespace std;
using ncbi::NK::test_skipped;

struct ErrorCode {
    rc_t rc;
};

struct Fixture {
    Fixture()
    : mgr(referenceManager("empty.config"))
    {}
    
    Fixture(CommandLine const &cmdline)
    : mgr(referenceManager(cmdline.config_file()))
    {
        auto const fastaFile = cmdline.fasta_file();
        if (fastaFile) {
            auto const rc = ReferenceMgr_FastaPath(mgr, fastaFile);
            if (rc)
                throw ErrorCode{ rc };
        }
    }
    
    ~Fixture() {
        ReferenceMgr_Release(mgr, false, nullptr, false, nullptr);
    }
    ReferenceSeq const *getSeq(char const *key, bool *shouldUnmap = nullptr, bool* wasRenamed = nullptr) const
    {
        bool dummy1{false}, dummy2{false};
        ReferenceSeq const *seq = nullptr;
        rc_t const rc = ReferenceMgr_GetSeq(mgr, &seq, key, shouldUnmap ? shouldUnmap : &dummy1, false, wasRenamed ? wasRenamed : &dummy2);
        return rc == 0 ? seq : nullptr;
    }
    ReferenceSeq const *getSeq(std::string const &key, bool *shouldUnmap = nullptr, bool* wasRenamed = nullptr) const
    {
        return getSeq(key.c_str(), shouldUnmap, wasRenamed);
    }
    bool verifySeq(char const *key, bool allowMultiMapping = true, bool* wasRenamed = nullptr, unsigned length = 0, uint8_t const *md5 = nullptr) const
    {
        bool dummy{ false };
        auto const rc = ReferenceMgr_Verify(mgr, key, length, md5, allowMultiMapping, wasRenamed ? wasRenamed : &dummy);
        return rc == 0 || ((int)GetRCObject(rc) == rcId && (int)GetRCState(rc) == rcUndefined);
    }
    bool verifySeq(std::string const &key, bool allowMultiMapping = true, bool* wasRenamed = nullptr, unsigned length = 0, uint8_t const *md5 = nullptr) const
    {
        return verifySeq(key.c_str(), allowMultiMapping, wasRenamed, length, md5);
    }
private:
    ReferenceMgr const *mgr;

    static VDBManager *updateManager()
    {
        VDBManager *vmgr = nullptr;
        rc_t const rc = VDBManagerMakeUpdate(&vmgr, nullptr);
        if (rc)
            throw ErrorCode{ rc };
        return vmgr;
    }

    static ReferenceMgr const *referenceManager(char const *conf
                                                , char const *path = "."
                                                , VDBManager *vmgr_p = nullptr
                                                )
    {
        auto const vmgr = vmgr_p ? vmgr_p : updateManager();
        ReferenceMgr const *rmgr = nullptr;
        auto const rc = ReferenceMgr_Make(&rmgr, nullptr, vmgr, 0, conf, path, 5000, 1024, 4);
        if (vmgr_p == nullptr)
            VDBManagerRelease(vmgr);
        if (rc)
            throw ErrorCode{ rc };
        return rmgr;
    }
};

TEST_SUITE(LoaderTestSuite);

TEST_CASE ( LoadNoConfig )
{
    auto const h = Fixture{ };
    
    REQUIRE_NOT_NULL(h.getSeq("NC_000001.11"));
    REQUIRE_NULL(h.getSeq("NC_000001"));
}

TEST_CASE ( LoadConfig )
{
    auto const unmapped = std::string("*UNMAPPED");
    
    if (arguments == nullptr)
        throw std::logic_error("no command line arguments!?");

    auto const configFile = arguments->config_file();
    if (configFile == nullptr)
        throw test_skipped{"no config file"};
    
    auto const h = Fixture{ *arguments };
    auto const config = ConfigFile{ configFile };
    
    for (auto & entry : config.entries) {
        bool shouldUnmap = false;
        auto const seq = h.getSeq(entry.name, &shouldUnmap);

        if (shouldUnmap) {
            REQUIRE_EQ(entry.seqId, unmapped);
        }
        else {
            // must be able to find every name that was in the config file
            REQUIRE_NOT_NULL(seq);

            // should be able to find the same entry by seqId
            REQUIRE_EQ(seq, h.getSeq(entry.seqId));
        }
    }
}

TEST_CASE ( VerifyConfig )
{
    if (arguments == nullptr)
        throw std::logic_error("no command line arguments!?");
    
    auto const configFile = arguments->config_file();
    if (configFile == nullptr)
        throw test_skipped{ "no config file" };
    
    auto const references = referenceList(*arguments);
    if (references.empty())
        throw test_skipped{ "no reference list" };

    auto const config = ConfigFile{ configFile };
    auto const h = Fixture{ *arguments };

    if (!arguments->skip_verify()) {
        for (auto & ref : references) {
            REQUIRE(h.verifySeq(ref));
        }
    }
    if (!arguments->only_verify()) {
        for (auto & ref : references) {
            bool shouldUnmap = false;
            auto const seq = h.getSeq(ref, &shouldUnmap);
            
            if (shouldUnmap)
                ;
            else
                REQUIRE_NOT_NULL(seq);
        }
    }
}


//////////////////////////////////////////// Main
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/out.h>
#include <kfg/config.h>

extern "C"
{

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}

const char UsageDefaultName[] = "test-loader";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ( "Usage:\n" "\t%s [options]\n\n", progname );
}

rc_t CC Usage( const Args* args )
{
    return 0;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    try {
        auto const args = CommandLine(argc, argv);
        arguments = &args;
        return LoaderTestSuite(argc, argv);
    }
    catch (std::invalid_argument const &e) {
        std::cerr << "invalid argument: " << e.what() << std::endl;
    }
    return -1;
}

}

