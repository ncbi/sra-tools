#ifndef __CFASTQERROR_HPP__
#define __CFASTQERROR_HPP__

/**
 * @file fastq_error.hpp
 * @brief Error handling
 * 
 */

#include <string>
#include <map>
#include <spdlog/fmt/fmt.h>

#if __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif



using TSharqErrorCode = int;
using TSharqErrorMsg = std::string;
using TSharqErrorDescription = std::string;  ///< s
using TSRAECode = std::string;


/**
 * @brief 
 * 
 */
 
static std::map<TSharqErrorCode, std::tuple<TSharqErrorMsg, TSharqErrorDescription, TSRAECode>> 
SHARQ_ERR_CODES = {
    {0   ,{ "Runtime error.", "Runtime error.", "SRAE-190"}},
    {10  ,{ "Invalid command line parameters, inconsistent number of read pairs", 
        "Number of comma-separated files in all readNPairFiles parameters is expected to be the same.", "SRAE-191"}},
    {11  ,{ "Inconsistent file sets: first group ({}), second group ({})", 
        "Input files are clustered into groups. Number of files in each groups is expected to be the same.", "SRAE-192"}},
    {20  ,{ "No readTypes provided", "'--readTypes' parameter is expected if readNPairFiles parameters are present.", "SRAE-193"}},
    {30  ,{ "readTypes number should match the number of reads", "'--readTypes' number should match the number the number of reads.", "SRAE-194"}},
    {40  ,{ "File '{}' does not exists", "Failure to find input file passed in the parameters.", "SRAE-195"}},
    {50  ,{ "File '{}' has no reads", "No reads found in the file.", "SRAE-196"}},
    //{60  ,{ "Platform detected from defline '{}' does not match paltform passed as parameter '{}'", "Platform detected from defline does not match paltform passed as parameter."}},
    {70  ,{ "Input files have deflines from different platforms", "Input files have deflines from different platforms.", "SRAE-197"}},
    {80  ,{ "10x input files are mixed with different types.", "10x input files are mixed with different types (check file names).", "SRAE-198"}},
    {100 ,{ "Defline '{}' not recognized", "SharQ failed to parse defline.", "SRAE-199"}},
    {101 ,{ "Illumina defline '{}' is not recognized", "SharQ failed to parse defline.", "SRAE-200"}},
    {110 ,{ "Read {}: no sequence data", "FastQ read has no sequence data.", "SRAE-201"}},
    {111 ,{ "Read {}: no quality scores", "FastQ read has no quality scores.", "SRAE-202"}},
    {120 ,{ "Read {}: unexpected quality score value '{}'", "Quality score is out of expected range.", "SRAE-203"}},
    {130 ,{ "Read {}: quality score length exceeds sequence length", "Quality score length exceeds sequence length.", "SRAE-204"}},
    {140 ,{ "Read {}: quality score contains unexpected character '{}'", "Quality score contains unexpected characters.", "SRAE-205"}},
    {150 ,{ "Read {}: invalid readType '{}'", "Unexpected '--readTypes' parameter values.", "SRAE-206"}},
    {160 ,{ "Read {}: invalid sequence characters", "Sequence contains non-alphabetical character.", "SRAE-207"}},
    {170 ,{ "Collation check. Duplicate spot '{}'", "Collation check found duplicated spot name.", "SRAE-75"}},
    {180 ,{ "{} ended early at line {}. Use '--allowEarlyFileEnd' to allow load to finish.", 
            "One of the files is shorter than the other. Use '--allowEarlyFileEnd' to allow load to finish.", "SRAE-208"}},
    {190 ,{ "Unsupported interleaved file with orphans", "Unsupported interleaved file with orphans.", "SRAE-209"}},
    {200 ,{ "Invalid quality encoding", "Failure to calculate quality score encoding.", "SRAE-210"}},
    {210 ,{ "Spot {} has more than 4 reads", "Assembled spot has more than 4 reads.", "SRAE-211"}},
    {220 ,{ "Invalid experiment file", "Invalid experiment file.", "SRAE-212"}},
    {230 ,{ "Internal QC failure", "Internal QC failure.", "SRAE-213"}},
    {240 ,{ "Invalid platform code", "Invalid platform code.", "SRAE-214"}},
    {250 ,{ "Estimated number of spots exceeds the limit for this mode. Re-run with --spot-assembly parameter", 
            "Estimated number of spots exceeds the limit for this mode. Re-run with --spot-assembly parameter.", "SRAE-70"}},
    {260 ,{ "Spots with more than {} reads are not supported.", "Spots with more than {} reads are not supported.", "SRAE-215"}}
};

class fastq_error: public std::exception
/// Error class used throughout application 
{
public:
    
    template<typename... Args> 
    fastq_error(const std::string& message, Args... args) {
        mMessage = fmt::format(message, args...);
    }

    template<typename... Args> 
    fastq_error(int error_code, const std::string& message, Args... args) 
        : m_error_code(error_code)
    {
        const auto& srae_code = std::get<2>(SHARQ_ERR_CODES.at(error_code));
        std::string str = fmt::format("{}: {} [code:{}]", srae_code, message, error_code);
        mMessage = fmt::format(str, args...);
    }

    fastq_error(int error_code) 
        : m_error_code(error_code)
    {
        const auto& srae_code = std::get<2>(SHARQ_ERR_CODES.at(error_code));
        const auto& message = std::get<0>(SHARQ_ERR_CODES.at(error_code));
        mMessage = fmt::format("{}: {} [code:{}]", srae_code, message, error_code);
    }

    const char* what() const noexcept
    {
        return mMessage.c_str();
    }

    int error_code() const 
    {
        return m_error_code;
    } 

    const std::string& Message() const
    {
        return mMessage;
    }

    void set_file(const std::string& file, int line_number) 
    {
        std::string fname = fs::path(file).filename();
        if (line_number > 0)
            mMessage += fmt::format(" [{}:{}]", fname, line_number);
        else 
            mMessage += fmt::format(" [{}]", fname);
    }    



    static void print_error_codes(std::ostream& os) 
    {
        os << fmt::format("{:-^90}", " SharQ error codes ") << "\n";
        os << fmt::format("{:<6}", "Code");
        os << fmt::format("{:<10}", "SRAE");
        os << fmt::format("{:<70}", "Description");
        os << "\n";
        os << fmt::format("{:-^90}", "") << "\n";
        for (const auto& it : SHARQ_ERR_CODES) {
            os << fmt::format("{:<6}", it.first);
            os << fmt::format("{:<10}", std::get<2>(it.second));
            os << fmt::format("{:<70}", std::get<1>(it.second));
            os << "\n";
        }
    }


private:
    int m_error_code{0};
    std::string mMessage;
};

#endif

