/*  $Id: fastq_parse.cpp 637208 2021-09-08 21:30:39Z shkeda $
* ===========================================================================
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
* Author:  Many, by the time it's done.
*
* File Description:
*
* ===========================================================================
*/
// comand line 
#include "CLI11.hpp"
//loging 
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

#include "fastq_error.hpp"
#include "fastq_parser.hpp"
#include "fastq_writer.hpp"
#include <experimental/filesystem>
// for brevity
namespace fs = std::experimental::filesystem;

#define LOCALDEBUG

//  ============================================================================
class CFastqParseApp
//  ============================================================================
{
public:    
    int AppMain(int argc, const char* argv[]);
private:
    int Run();
    int xRun();

    void xSetupInput();
    void xSetupOutput();

    void xBreakdownOutput();
    void xCheckInputFiles();

    string mOutputFile;
    bool mDebug{false};
    vector<char> mReadTypes;
    using TInptuFiles = vector<string>;
    vector<TInptuFiles> mInputBatches;
    bool mDiscardNames{false};
    bool mAllowEarlyFileEnd{false}; ///< Flag to continue if one of the streams ends
    string mSpotFile;
    ostream* mpOutStr{nullptr};
    shared_ptr<fastq_writer> m_writer;
};


void s_AddReadPairBatch(vector<string>& batch, vector<vector<string>>& out)
{
    if (batch.empty())
        return;
    if (out.empty()) {
        out.resize(batch.size());
    } else if (batch.size() != out.size()) {
        throw fastq_error(10, "Invalid command line parameters, inconsistent number of read pairs");
    }
    for (size_t i = 0; i < batch.size(); ++i) {
        out[i].push_back(move(batch[i]));
    }
}

static 
void s_split(const string& str, vector<string>& out, char c = ',')
{
    if (str.empty())
        return;
    auto begin = str.begin();
    auto end  = str.end();
    while (begin != end) {
        auto it = begin;
        while (it != end && *it != c) ++it;
        out.emplace_back(begin, it);
        if (it == end)
            break;
        begin = ++it;
    } 
}
//  ----------------------------------------------------------------------------
int CFastqParseApp::AppMain(int argc, const char* argv[])
//  ----------------------------------------------------------------------------
{
    try {
        CLI::App app{"SharQ"};
        // Add new options/flags here
        app.add_option("--o", mOutputFile, "Optional output file");        

        string platform;
        app.add_option("--platform", platform, "Optional platform");

        string read_types;
        app.add_option("--readTypes", read_types, "file read types <B|T(s)>");

        app.add_option("--useAndDiscardNames", mDiscardNames, "Discard file names (Boolean)");

        app.add_flag("--allowEarlyFileEnd", mAllowEarlyFileEnd, "Complete load at early end of one of the files");
        app.add_flag("--debug", mDebug, "Debug mode");
        bool print_errors;
        app.add_flag("--print_errors", print_errors, "Print error codes");
        string hash_file;
        app.add_option("--hash", hash_file, "Check hash file");
        app.add_option("--spot_file", mSpotFile, "Save spot names");

        vector<string> read_pairs(4);
        app.add_option("--read1PairFiles", read_pairs[0], "Read 1 files");
        app.add_option("--read2PairFiles", read_pairs[1], "Read 2 files");
        app.add_option("--read3PairFiles", read_pairs[2], "Read 3 files");
        app.add_option("--read4PairFiles", read_pairs[3], "Read 4 files");

        vector<string> input_files;
        app.add_option("files", input_files, "FastQ files to parse");
        CLI11_PARSE(app, argc, argv);

        if (print_errors) {
            fastq_error::print_error_codes(cout);
            return 0;
        }

        if (!hash_file.empty()) {
            check_hash_file(hash_file);
            return 0;
        }
        if (mDebug) {
            m_writer = make_shared<fastq_writer>();
        } else {
            spdlog::set_pattern("%v");
            m_writer = make_shared<fastq_writer_vdb>();
        }
        
        copy(read_types.begin(), read_types.end(), back_inserter(mReadTypes));

        if (!read_pairs[0].empty()) {
            if (mReadTypes.empty())
                throw fastq_error(20, "No readTypes provided");
            for (auto p : read_pairs) {
                vector<string> b;
                s_split(p, b);
                s_AddReadPairBatch(b, mInputBatches);
            }
        } else {
            if (input_files.empty()) {
                mInputBatches.push_back({"-"});
            } else {
                stable_sort(input_files.begin(), input_files.end());
                mInputBatches.push_back(move(input_files));
            }
        }
        if (!mReadTypes.empty() && !mInputBatches.empty() && mReadTypes.size() != mInputBatches.front().size())
            throw fastq_error(30, "readTypes number should match number of input files");

        xCheckInputFiles();

        return Run();
    } catch(std::exception const& e) {
        spdlog::error(e.what());
    }
    return 1;
}


void CFastqParseApp::xCheckInputFiles()
{
    for (auto& files : mInputBatches) {
        for (auto& f : files) {
            if (f == "-") continue;
            if(!fs::exists(f)) {
                bool not_found = true;
                auto ext = fs::path(f).extension();
                if (ext != ".gz" && ext != ".bz2") {
                    if (fs::exists(f + ".gz")) {
                        spdlog::info("File '{}': .gz extension added", f);
                        f += ".gz";
                        not_found = false;
                    } else if (fs::exists(f + ".bz2")) {
                        spdlog::info("File '{}': .bz2 extension added", f);
                        f += ".bz2";
                        not_found = false;
                    } 
                } else if (ext == ".gz" || ext == ".bz2") {
                    auto new_fn = f.substr(0, f.size() - ext.string().size());
                    if (fs::exists(new_fn)) {
                        spdlog::info("File '{}': {} extension ignored", f, ext.string());
                        f = new_fn;
                        not_found = false;
                    }

                }
                if (not_found) {
                    throw fastq_error(40, "File '{}' does not exist", f);
                }
            }
        }
    }

}


//  ----------------------------------------------------------------------------
int CFastqParseApp::Run()
//  ----------------------------------------------------------------------------
{
    xSetupOutput();
    int retStatus = xRun();
    xBreakdownOutput();
    return retStatus;
}


//  -----------------------------------------------------------------------------
int CFastqParseApp::xRun()
//  -----------------------------------------------------------------------------
{
    m_writer->open("sra.out");
    fastq_parser<fastq_writer> parser(m_writer);
    if (!mDebug)
        parser.set_spot_file(mSpotFile);
    parser.set_allow_early_end(mAllowEarlyFileEnd);
    for (const auto& batch : mInputBatches) {
        parser.setup_readers(batch, mReadTypes);
        parser.parse();
    }
    parser.check_duplicates();
    m_writer->close();
    return 0;
}

//  -----------------------------------------------------------------------------
void CFastqParseApp::xSetupOutput()
//  -----------------------------------------------------------------------------
{
    mpOutStr = &cout;
    if (mOutputFile.empty()) 
        return;
    mpOutStr = dynamic_cast<ostream*>(new ofstream(mOutputFile, ios::binary));
}



//  ----------------------------------------------------------------------------
void CFastqParseApp::xBreakdownOutput()
    //  ----------------------------------------------------------------------------
{
    if (mpOutStr != &cout) {
        delete mpOutStr;
    }
}


//  ----------------------------------------------------------------------------
int main(int argc, const char* argv[])
//  ----------------------------------------------------------------------------
{
    std::locale::global(std::locale("en_US.UTF-8")); // enable comma as thousand separator
    auto stderr_logger = spdlog::stderr_logger_mt("stderr");
    spdlog::set_default_logger(stderr_logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    return CFastqParseApp().AppMain(argc, argv);
}

