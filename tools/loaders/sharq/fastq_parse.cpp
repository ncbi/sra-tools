/**
 * @file fastq_parse.cpp
 * @brief SharQ application
 *
 */

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
* Author:  Andrei Shkeda
* File Description: SharQ application
*
* ===========================================================================
*/
// command line
#include "CLI11.hpp"
// logging
#include <spdlog/fmt/fmt.h>

#include "version.h"
#include "fastq_utils.hpp"
#include "fastq_error.hpp"
#include "fastq_parser.hpp"
#include "fastq_writer.hpp"

#include <json.hpp>
#include <algorithm>

#if __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::experimental::filesystem;
#endif

using json = nlohmann::json;

#define LOCALDEBUG

/**
 * @brief SharQ application class
 *
 * After processing the command line arguments and input files,
 * the application builds a digest (json data structure) for the first 500K spots.
 *
 * Digest captures the payload properties as well as identifies quality score encoding.
 *
 * Parser and writer are set up using the digest data
 *
 * Witer's output (stdout) is expected to be piped in general_loader application.
 *
 * --debug parameter can be used to send the output to stdout
 */
class CFastqParseApp
{
public:
    int AppMain(int argc, const char* argv[]);
private:
    int Run();
    int xRun();
    int xRunDigest();
    int xRunSpotAssembly();

    template <typename ScoreValidator, typename parser_t>
    void xParseWithAssembly(json& group, parser_t& parser);


    void xSetupInput();
    void xSetupOutput();

    // check consistency of digest data
    // verify the same platform, the same platform
    void xProcessDigest(json& data);

    void xBreakdownOutput();
    void xCheckInputFiles(vector<string>& files);

    void xReportTelemetry();
    void xCheckErrorLimits(fastq_error& e);

    void xCreateWriterFromDigest(json& data);
    bool xIsSingleFileInput() const;

    string mDestination;                ///< path to sra archive
    bool mDebug{false};                 ///< Debug mode
    bool mNoTimeStamp{false};           ///< No time stamp in debug mode
    vector<char> mReadTypes;            ///< ReadType parameter value
    using TInputFiles = vector<string>;
    vector<TInputFiles> mInputBatches;  ///< List of input batches
    bool mDiscardNames{false};          ///< If set spot names are not written in the db, the same effect as mNameColumn = 'NONE'
    bool mAllowEarlyFileEnd{false};     ///< Flag to continue if one of the streams ends
    bool mSpotAssembly{false};          ///< spot assembly mode 
    int mQuality{-1};                   ///< quality score interpretation (0, 33, 64)
    int mDigest{0};                     ///< Number of digest lines to produce
    bool mHasReadPairs{false};          ///< Flag to indicate that read pairs are defined in the command line 
    unsigned int mThreads{24};          ///< Number of threads to use
    string mTelemetryFile;              ///< Telemetry report file name
    string mSpotFile;                   ///< Spot_name file, optional request to serialize  all spot names
    string mNameColumn;                 ///< NAME column's name, ('NONE', 'NAME', 'RAW_NAME')
    string mOutputFile;                 ///< Outut file name - not currently used
    json mExperimentSpecs;              ///< Json from Experiment file
    ostream* mpOutStr{nullptr};         ///< Output stream pointer  = not currently used
    shared_ptr<fastq_writer> m_writer;  ///< FASTQ writer
    json  mReport;                      ///< Telemetry report
    uint32_t mMaxErrCount{100};         ///< Maximum numbers of errors allowed when parsing reads
    atomic<uint32_t> mErrorCount{0};            ///< Global error counter
    size_t mHotReadsThreshold{10000000};      ///< Threshold for hot reads
    set<int> mErrorSet = { 100, 110, 111, 120, 130, 140, 160, 190}; ///< Error codes that will be allowed up to mMaxErrCount
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

void s_print_deflines(std::ostream& os)
{
        os << fmt::format("{:-^80}", " SharQ defline patterns ") << "\n";
        os << fmt::format("{:<30}", "Name");
        os << fmt::format("{:<128}", "RegEx");
        os << "\n";
        CDefLineParser defline_parser;
        //os << fmt::format("{:-^80}", "") << "\n";
        for (const auto& it : defline_parser.GetDeflineMatchers()) {
            if (it->Defline() == "NoMatch")
                continue;
            os << fmt::format("{:<30}", it->Defline());
            os << it->GetPattern();
            os << "\n";
        }
}

//  ----------------------------------------------------------------------------
int CFastqParseApp::AppMain(int argc, const char* argv[])
{
    int ret_code = 0;
    spdlog::stopwatch stop_watch;

    try {
        CLI::App app{"SharQ"};

        mOutputFile.clear();
        mDestination = "sra.out";

        app.set_version_flag("--version,-V", SHARQ_VERSION);
        bool print_deflines = false;
        app.add_flag("--print-deflines", print_deflines);


        app.add_option("--output", mDestination, "Output archive path");

        string platform;
        app.add_option("--platform", platform, "Optional platform");

        string read_types;
        app.add_option("--readTypes", read_types, "file read types <B|T(s)>");

        app.add_flag("--useAndDiscardNames", mDiscardNames, "Discard file names");

        app.add_flag("--allowEarlyFileEnd", mAllowEarlyFileEnd, "Complete load at early end of one of the files");
        app.add_flag("--sa, --spot-assembly", mSpotAssembly);
        

        bool print_errors = false;
        app.add_flag("--help_errors,--help-errors", print_errors, "Print error codes and descriptions");

        app.add_option("--name-column", mNameColumn, "Database name for NAME column")
            ->default_str("NAME")
            ->default_val("NAME")
            ->excludes("--useAndDiscardNames")
            ->check(CLI::IsMember({"NONE", "NAME", "RAW_NAME"}));

        mQuality = -1;
        app.add_option("--quality,-q", mQuality, "Interpretation of ascii quality")
            ->check(CLI::IsMember({0, 33, 64}));

        mDigest = 0;
        app.add_flag("--digest{500000}", mDigest, "Report summary of input data (set optional value to indicate the number of spots to analyze)");
        mThreads = 24;
        app.add_option("--threads", mThreads, "Max number of threads to use (set optional value to indicate the number of threads to use")
            //->check(CLI::Range(1, std::thread::hardware_concurrency()));
            ->default_val(24)
            ->check(CLI::Range(1, 256));

        mTelemetryFile.clear();
        app.add_option("--telemetry,-t", mTelemetryFile, "Telemetry report file");

        vector<string> read_pairs(4);
        app.add_option("--read1PairFiles", read_pairs[0], "Read 1 files");
        app.add_option("--read2PairFiles", read_pairs[1], "Read 2 files");
        app.add_option("--read3PairFiles", read_pairs[2], "Read 3 files");
        app.add_option("--read4PairFiles", read_pairs[3], "Read 4 files");
        app.add_option("--max-err-count", mMaxErrCount, "Maximum number of errors allowed")
            ->default_val(100)
            ->check(CLI::Range(uint32_t(0), numeric_limits<uint32_t>::max()));

        vector<string> input_files;
        app.add_option("files", input_files, "FastQ files to parse");

        auto opt = app.add_option_group("Debugging options");
        mNoTimeStamp = false;
        opt->add_flag("--no-timestamp", mNoTimeStamp, "No time stamp in debug mode");

        string log_level = "info";
        opt->add_option("--log-level", log_level, "Log level")
            ->default_val("info")
            ->check(CLI::IsMember({"trace", "debug", "info", "warning", "error"}));

        app.add_option("--hot-reads-threshold", mHotReadsThreshold, "Hot reads threshold");

        string experiment_file;
        app.add_option("--experiment", experiment_file, "Read structure description");

        string hash_file;
        opt->add_option("--hash", hash_file, "Check hash file");
        opt->add_option("--spot_file", mSpotFile, "Save spot names");
        opt->add_flag("--debug", mDebug, "Debug mode");

        CLI11_PARSE(app, argc, argv);
        if (print_errors) {
            fastq_error::print_error_codes(cout);
            return 0;
        }
        if (print_deflines) {
            s_print_deflines(cout);
            return 0;
        }
        if (mDigest < 0)
            mDigest = -1;
        if (mDigest != 0)
            spdlog::set_level(spdlog::level::from_str("error"));
        else
            spdlog::set_level(spdlog::level::from_str(log_level));

        if (!hash_file.empty()) {
            check_hash_file(hash_file);
            return 0;
        }
        vector<string> options2log = {"--platform", "--readTypes", "--useAndDiscardNames", "--allowEarlyFileEnd", "--name-column", "--quality"};
        for (const auto& opt_name : options2log) {
            auto opt = app.get_option(opt_name);
            if (opt && *opt)
                mReport[opt_name] = opt->as<string>();
        }

        if (mDiscardNames)
            mNameColumn = "NONE";

        if (mNoTimeStamp)
            spdlog::set_pattern("[%l] %v");
        else        
            mReport["version"] = SHARQ_VERSION;


        xSetupOutput();

        if (!experiment_file.empty()) try {
            std::ifstream f(experiment_file);
            mExperimentSpecs = json::parse(f);      
        } catch (exception& e) {
            throw fastq_error(220, "Invalid experiment file {}: {}", experiment_file, e.what());
        }

        if (read_types.find_first_not_of("TBA") != string::npos)
            throw fastq_error(150, "Invalid --readTypes values '{}'", read_types);

        copy(read_types.begin(), read_types.end(), back_inserter(mReadTypes));

        if (!read_pairs[0].empty()) {
            mHasReadPairs = true;
            if (mDigest == 0 && mReadTypes.empty())
                throw fastq_error(20, "No readTypes provided");
            for (const auto& p : read_pairs) {
                vector<string> b;
                sharq::split(p, b);
                xCheckInputFiles(b);
                s_AddReadPairBatch(b, mInputBatches);
            }
        } else {
            if (input_files.empty()) {
                mInputBatches.push_back({"-"});
            } else {
                for (auto& s : input_files) 
                   s.erase(remove(s.begin(), s.end(), '\''), s.end());
                stable_sort(input_files.begin(), input_files.end());
                xCheckInputFiles(input_files);
                if (mSpotAssembly) {
                    mAllowEarlyFileEnd = true;                        
                    mInputBatches.push_back(input_files);
                } else {
                    fastq_reader::cluster_files(input_files, mInputBatches);
                }
            }
        }
        ret_code = Run();
    } catch (fastq_error& e) {
        spdlog::error(e.Message());
        mReport["error"] = e.Message();
        ret_code = 1;
    } catch(std::exception const& e) {
        string error = fmt::format("[code:0] Runtime error: {}", e.what());
        spdlog::error(error);
        mReport["error"] = error;
        ret_code = 1;
    }

    if (mNoTimeStamp == false)
        mReport["timing"]["exec"] =  ceil(stop_watch.elapsed().count() * 100.0) / 100.0;

    xReportTelemetry();
    return ret_code;
}

// max memory usage in bytes
#ifdef __linux__

static 
unsigned long getPeakRSS() {
    unsigned long rss = 0;
    std::ifstream in("/proc/self/status");
    if(in.is_open()) {
        std::string line;
        while(std::getline(in, line)) {
            if(line.substr(0, 6) == "VmHWM:") {
                std::istringstream iss(line.substr(6));
                iss >> rss;
                break; // No need to read further
            }
        }
    }
    return rss; // Size is in kB
}

#endif

void CFastqParseApp::xReportTelemetry()
{
    if (mTelemetryFile.empty())
        return;
#ifdef __linux__
    if (mNoTimeStamp == false)
        mReport["max_memory_kb"] = getPeakRSS();
#endif
    try {
        ofstream f(mTelemetryFile.c_str(), ios::out);
        f << mReport.dump(4, ' ', true) << endl;
    } catch(std::exception const& e) {
        spdlog::error("[code:0] Runtime error: {}", e.what());
    }
}


void CFastqParseApp::xCheckInputFiles(vector<string>& files)
{
    for (auto& f : files) {
        if (f == "-" || fs::exists(f)) continue;
        bool not_found = true;
        auto ext = fs::path(f).extension();
        if (ext != ".gz" && ext != ".bz2") {
            if (fs::exists(f + ".gz")) {
                spdlog::debug("File '{}': .gz extension added", f);
                f += ".gz";
                not_found = false;
            } else if (fs::exists(f + ".bz2")) {
                spdlog::debug("File '{}': .bz2 extension added", f);
                f += ".bz2";
                not_found = false;
            }
        } else if (ext == ".gz" || ext == ".bz2") {
            auto new_fn = f.substr(0, f.size() - ext.string().size());
            if (fs::exists(new_fn)) {
                spdlog::debug("File '{}': {} extension ignored", f, ext.string());
                f = new_fn;
                not_found = false;
            }

        }
        if (not_found)
            throw fastq_error(40, "File '{}' does not exist", f);
    }
}

//  ----------------------------------------------------------------------------
int CFastqParseApp::Run()
{
    int retStatus = 0;
    if (mDigest != 0)
        retStatus = xRunDigest();
    else if (mSpotAssembly)
        retStatus = xRunSpotAssembly();
    else
        retStatus = xRun();
    xBreakdownOutput();
    return retStatus;
}


void CFastqParseApp::xProcessDigest(json& data)
{
    assert(data.contains("groups"));
    assert(data["groups"].front().contains("files"));
    const auto& first = data["groups"].front()["files"].front();
    if (first["platform_code"].size() > 1)
        throw fastq_error(70, "Input file has data from multiple platforms ({} != {})", int(first["platform_code"][0]), int(first["platform_code"][1]));
    bool is10x = data["groups"].front()["is_10x"];
    int platform = first["platform_code"].front();
    int total_reads = 0;
    for (auto& gr : data["groups"]) {
        int max_reads = 0;
        int group_reads = 0;
        if (gr["is_10x"] != is10x)
            throw fastq_error(80);// "Inconsistent submission: 10x submissions are mixed with different types.");

        auto& files = gr["files"];
        for (auto& f : files) {
            if (f["defline_type"].empty() || (f["defline_type"].size() == 1 && f["defline_type"].front() == "undefined")) {
                string fname = fs::path(f["file_path"]).filename();
                throw fastq_error(100, "Defline not recognized [{}:1]", fname);
            }
            if (mQuality != -1)
                f["quality_encoding"] = mQuality; // Override quality
            if (f["platform_code"].size() > 1)
                throw fastq_error(70, "Input file has data from multiple platforms ({} != {})", int(f["platform_code"][0]), int(f["platform_code"][1]));
           if (platform != f["platform_code"].front())
                throw fastq_error(70, "Input files have deflines from different platforms ({} != {})", platform, int(f["platform_code"].front()));
            max_reads = max<int>(max_reads, f["max_reads"]);
            group_reads += (int)f["max_reads"];

            // if read types are specified (mix of B and T)
            // and the reads in an interleaved file don't have read numbers
            // and orphans are present
            // then sharq should fail.
            if (mSpotAssembly == false && !mReadTypes.empty() && max_reads > 1 && f["has_orphans"] && f["readNums"].empty())
                throw fastq_error(190); // "Unsupported interleaved file with orphans"
        }

        if (mHasReadPairs || mSpotAssembly == false ) {
            if (!mReadTypes.empty()) {
                if ((int)mReadTypes.size() != group_reads)
                    throw fastq_error(30, "readTypes number should match the number of reads {} != {}", mReadTypes.size(), group_reads);
            }
        }
        total_reads = max<int>(group_reads, total_reads);
        gr["total_reads"] = total_reads;
    }

    if (mSpotAssembly == false) {
        if (mReadTypes.empty()) {
            //auto num_files = data["groups"].front().size();
            if (is10x)
                mReadTypes.resize(total_reads, 'A');
            else if (total_reads < 3) {
                mReadTypes.resize(total_reads, 'B');
            } else {
                throw fastq_error(20, "The input data have spots with {} reads. Read types must be provided via parameter.", total_reads); // "No readTypes provided");
            }
        }
        for (auto& gr : data["groups"]) {
            auto& files = gr["files"];
            int total_reads = gr["total_reads"];
            if ((int)mReadTypes.size() < total_reads)
                throw fastq_error(30, "readTypes number should match the number of reads");
            int j = 0;
            // assign read types for each file
            for (auto& f : files) {
                int num_reads = f["max_reads"];
                while (num_reads) {
                    f["readType"].push_back(char(mReadTypes[j]));
                    --num_reads;
                    ++j;
                }
            }
        }
    } 
/*
    else {
        for (auto& gr : data["groups"]) {
            for (auto& f : gr["files"]) {
                f["readType"] = mReadTypes.empty() ? json::array({}) : json::array({mReadTypes.begin(), mReadTypes.end()});
            }
        }
    }
*/
}

//  -----------------------------------------------------------------------------
int CFastqParseApp::xRunDigest()
{
    if (mInputBatches.empty())
        return 1;
    spdlog::set_level(spdlog::level::from_str("off"));
    json json_data;
    string error;
    try {
        get_digest(json_data, mInputBatches, [this](fastq_error& e) { CFastqParseApp::xCheckErrorLimits(e);}, mDigest);
        set<string> deflines;
        for (const auto& gr : json_data["groups"]) {
            for (const auto& f : gr["files"]) {
                for (const auto& d : f["defline_type"])
                    deflines.emplace(d);
            }
        }
        json_data["defline"] = deflines.empty() ? "unknown" : sharq::join(deflines.begin(), deflines.end(), ",");
    } catch (fastq_error& e) {
        error = e.Message();
    } catch(std::exception const& e) {
        error = fmt::format("[code:0] Runtime error: {}", e.what());
    }
    if (!error.empty()) {
        // remove special character if any
        error.erase(remove_if(error.begin(), error.end(),[](char c) { return !isprint(c); }), error.end());
        json_data["error"] = error;
    }

    cout << json_data.dump(4, ' ', true) << endl;
    return 0;
}

/**
 * @brief Check if the load specification describes multiple reads with defined base coords
 * 
 * @return true 
 * @return false 
 */
static 
bool s_has_split_read_spec(const json& ExperimentSpecs)
{
    if (ExperimentSpecs.is_null()) 
        return false;
    auto read_specs = ExperimentSpecs["EXPERIMENT"]["DESIGN"]["SPOT_DESCRIPTOR"]["SPOT_DECODE_SPEC"]["READ_SPEC"];
    if (read_specs.is_null())
        throw fastq_error("Experiment does not contains READ_SPEC");
    return (read_specs.is_array() && read_specs.size() > 1 && read_specs.front().contains("BASE_COORD"));
    /*
    if (j.is_array()) {
        for (auto it = j.begin();it != j.end(); ++it) {     
            if (it->contains("BASE_COORD"))                      
                return true;
        }
    } 
    return false;
    */
}

bool CFastqParseApp::xIsSingleFileInput() const
{
    return all_of(mInputBatches.begin(), mInputBatches.end(), [](const auto& it) { return it.size() == 1; });
}   

void CFastqParseApp::xCreateWriterFromDigest(json& data)
{
    assert(data.contains("groups"));
    assert(data["groups"].front().contains("files"));
    const auto& first = data["groups"].front()["files"].front();
    int platform_code = first["platform_code"].front();

    if (mDebug) {
        m_writer = make_shared<fastq_writer_debug>();
    } else {
        if (platform_code == SRA_PLATFORM_454 && s_has_split_read_spec(mExperimentSpecs) && xIsSingleFileInput()) {
            m_writer = make_shared<fastq_writer_exp>(mExperimentSpecs, *mpOutStr);
        } else {
            m_writer = make_shared<fastq_writer_vdb>(*mpOutStr);
        }
    }

    m_writer->set_attr("name_column", mNameColumn);
    m_writer->set_attr("destination", mDestination);
    m_writer->set_attr("version", SHARQ_VERSION);
    m_writer->set_attr("readTypes", string(mReadTypes.begin(), mReadTypes.end()));

    switch ((int)first["quality_encoding"]) {
    case 0:
        m_writer->set_attr("quality_expression", "(INSDC:quality:phred)QUALITY");
        break;
    case 33:
        m_writer->set_attr("quality_expression", "(INSDC:quality:text:phred_33)QUALITY");
        break;
    case 64:
        m_writer->set_attr("quality_expression", "(INSDC:quality:text:phred_64)QUALITY");
        break;
    default:
        throw fastq_error(200); // "Invalid quality encoding"
    }
    m_writer->set_attr("platform", to_string(platform_code));
}

int CFastqParseApp::xRun()
{
    if (mInputBatches.empty())
        return 1;
    json data;
    get_digest(data, mInputBatches, [this](fastq_error& e) { CFastqParseApp::xCheckErrorLimits(e);});
    xProcessDigest(data);
    mErrorCount = 0; //Reset error counts after initial digest
    xCreateWriterFromDigest(data);
    size_t total_spots = 0;
    for (auto& group : data["groups"]) 
        total_spots += group["estimated_spots"].get<size_t>();;

    spot_name_check name_checker(total_spots);

    fastq_parser<fastq_writer> parser(m_writer);
    try {
        if (!mDebug)
            parser.set_spot_file(mSpotFile);
        parser.set_allow_early_end(mAllowEarlyFileEnd);
        m_writer->open();
        auto err_checker = [this](fastq_error& e) -> void { CFastqParseApp::xCheckErrorLimits(e);};
        for (auto& group : data["groups"]) {
            parser.set_readers(group);
            if (!group["files"].empty()) {
                switch ((int)group["files"].front()["quality_encoding"]) {
                    case 0:
                        parser.parse<validator_options<eNumeric, -5, 40>>(name_checker, err_checker);
                        break;
                    case 33:
                        parser.parse<validator_options<ePhred, 33, 126>>(name_checker, err_checker);
                        break;
                    case 64:
                        parser.parse<validator_options<ePhred, 64, 126>>(name_checker, err_checker);
                        break;
                    default:
                        throw runtime_error("Invalid quality encoding");
                }
            }
        }
        spdlog::stopwatch sw;
        //parser.check_duplicates(err_checker);
        //if (mNoTimeStamp == false)
        //    mReport["timing"]["collation_check"] =  ceil(sw.elapsed().count() * 100.0) / 100.0;
        spdlog::info("Parsing complete");
        m_writer->close();
    } catch (exception& e) {
        if (!mTelemetryFile.empty()) 
            parser.report_telemetry(mReport);
        throw;
    }
    if (!mTelemetryFile.empty()) 
        parser.report_telemetry(mReport);

    return 0;
}

template <typename ScoreValidator, typename parser_t>
void CFastqParseApp::xParseWithAssembly(json& group, parser_t& parser)
{
    assert(group["files"].empty() == false);
    if (group["files"].empty())
        return;
    mReport["is_spot_assembly"] = 1;

    auto err_checker = [this](fastq_error& e) { CFastqParseApp::xCheckErrorLimits(e);};
    int platform = group["files"].front()["platform_code"].front();
    bool is_nanopore = platform == SRA_PLATFORM_OXFORD_NANOPORE;
    mErrorCount = 0;
    parser.set_readers(group, false);
    spdlog::stopwatch sw;
    parser.template first_pass<ScoreValidator>(err_checker);
    if (mNoTimeStamp == false)
        mReport["timing"]["first_pass"] =  ceil(sw.elapsed().count() * 100.0) / 100.0;
    sw.reset();        

    //Reset readers
    mErrorCount = 0;
    parser.set_readers(group);
    if (is_nanopore)
        parser.template second_pass<ScoreValidator, true>(err_checker);
    else
        parser.template second_pass<ScoreValidator, false>(err_checker);

    if (mNoTimeStamp == false)
        mReport["timing"]["second_pass"] =  ceil(sw.elapsed().count() * 100.0) / 100.0;

    parser.template update_readers_telemetry<ScoreValidator>();

}


int CFastqParseApp::xRunSpotAssembly()
{
    if (mInputBatches.empty())
        return 1;
    json data;
    get_digest(data, mInputBatches, [this](fastq_error& e) { CFastqParseApp::xCheckErrorLimits(e); });
    xProcessDigest(data);
    mErrorCount = 0; //Reset error counts after initial digest
    xCreateWriterFromDigest(data);
    
    m_writer->open();
    fastq_parser<fastq_writer> parser(m_writer, mReadTypes);
    try {
        if (!mDebug)
            parser.set_spot_file(mSpotFile);
        parser.set_allow_early_end(mAllowEarlyFileEnd);
        parser.set_hot_reads_threshold(mHotReadsThreshold);

        //auto err_checker = [this](fastq_error& e) { CFastqParseApp::xCheckErrorLimits(e);};
        for (auto& group : data["groups"]) {
            if (!group["files"].empty()) {
                switch ((int)group["files"].front()["quality_encoding"]) {
                    case 0:
                        xParseWithAssembly<validator_options<eNumeric, -5, 40>>(group, parser);
                        break;
                    case 33:
                        xParseWithAssembly<validator_options<ePhred, 33, 126>>(group, parser);
                        break;
                    case 64:
                        xParseWithAssembly<validator_options<ePhred, 64, 126>>(group, parser);
                        break;
                    default:
                        throw runtime_error("Invalid quality encoding");
                }
            }
        }
        spdlog::stopwatch sw;
        spdlog::info("Parsing complete");
        m_writer->close();
    } catch (exception& e) {
        if (!mTelemetryFile.empty()) 
            parser.report_telemetry(mReport);
        throw;        
    }
    if (!mTelemetryFile.empty()) 
        parser.report_telemetry(mReport);

    return 0;
}

//  -----------------------------------------------------------------------------
void CFastqParseApp::xSetupOutput()
{
    mpOutStr = &cout;
    if (mOutputFile.empty())
        return;
    mpOutStr = dynamic_cast<ostream*>(new ofstream(mOutputFile, ios::binary));
    mpOutStr->exceptions(std::ofstream::badbit);
}



//  ----------------------------------------------------------------------------
void CFastqParseApp::xBreakdownOutput()
{
    if (mpOutStr != &cout) {
        delete mpOutStr;
    }
}

void CFastqParseApp::xCheckErrorLimits(fastq_error& e )
{
    if (mMaxErrCount == 0)
        throw e;
    auto it = mErrorSet.find(e.error_code());
    if (it == mErrorSet.end())
        throw e;
    spdlog::warn(e.Message());
    if (++mErrorCount >= mMaxErrCount)
        throw fastq_error(e.error_code(), "Exceeded maximum number of errors {}", mMaxErrCount);
}

//  ----------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
    ios_base::sync_with_stdio(false);   // turn off synchronization with standard C streams
    std::locale::global(std::locale("en_US.UTF-8")); // enable comma as thousand separator
    auto stderr_logger = spdlog::stderr_logger_mt("stderr"); // send log to stderr
    spdlog::set_default_logger(stderr_logger);

    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v"); // default logging pattern (datetime, error level, error text)
    return CFastqParseApp().AppMain(argc, argv);
}
