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


/**
 * @brief 
 * 
 */
 
static std::map<TSharqErrorCode, std::tuple<TSharqErrorMsg, TSharqErrorDescription>> 
SHARQ_ERR_CODES = {
    {0   ,{ "Runtime error.", "Runtime error."}},
    {10  ,{ "Invalid command line parameters, inconsistent number of read pairs", "Number of comma-separated files in all readNPairFiles parameters is expected to be the same."}},
    {11  ,{ "Inconsistent file sets: first group ({}), second group ({})", "Input files are clustered into groups. Number of files in each groups is expected to be the same."}},
    {20  ,{ "No readTypes provided", "'--readTypes' parameter is expected if readNPairFiles parameters are present."}},
    {30  ,{ "readTypes number should match the number of reads", "'--readTypes' number should match the number the number of reads."}},
    {40  ,{ "File '{}' does not exists", "Failure to find input file passed in the parameters."}},
    {50  ,{ "File '{}' has no reads", "No reads found in the file."}},
    //{60  ,{ "Platform detected from defline '{}' does not match paltform passed as parameter '{}'", "Platform detected from defline does not match paltform passed as parameter."}},
    {70  ,{ "Input files have deflines from different platforms", "Input files have deflines from different platforms."}},
    {80  ,{ "10x input files are mixed with different types.", "10x input files are mixed with different types (check file names)."}},
    {100 ,{ "Defline '{}' not recognized", "SharQ failed to parse defline."}},
    {101 ,{ "Illumina defline '{}' is not recognized", "SharQ failed to parse defline."}},
    {110 ,{ "Read {}: no sequence data", "FastQ read has no sequence data."}},
    {111 ,{ "Read {}: no quality scores", "FastQ read has no quality scores."}},
    {120 ,{ "Read {}: unexpected quality score value '{}'", "Quality score is out of expected range."}},
    {130 ,{ "Read {}: quality score length exceeds sequence length", "Quality score length exceeds sequence length."}},
    {140 ,{ "Read {}: quality score contains unexpected character '{}'", "Quality score contains unexpected characters."}},
    {150 ,{ "Read {}: invalid readtType '{}'", "Unexpected '--readTypes' parameter values."}},
    {160 ,{ "Read {}: invalid sequence characters", "Sequence contains non-alphabetical character."}},
    {170 ,{ "Collation check. Duplicate spot '{}'", "Collation check found duplicated spot name."}},
    {180 ,{ "{} ended early at line {}. Use '--allowEarlyFileEnd' to allow load to finish.", "One of the files is shorter than the other. Use '--allowEarlyFileEnd' to allow load to finish."}},
    {190 ,{ "Unsupported interleaved file with orphans", "Unsupported interleaved file with orphans."}},
    {200 ,{ "Invalid quality encoding", "Failure to calculate quality score encoding."}},
    {210 ,{ "Spot {} has more than 4 reads", "Assembled spot has more than 4 reads."}},
    {220 ,{ "Invalid experiment file", "Invalid experiment file."}},
    {230 ,{ "Internal QC failure", "Internal QC failure."}},
    {240 ,{ "Invalid platfrom code", "Invalid platfrom code."}},
    {250 ,{ "SRAE-70: Estimated number of spots excceds the limit for this mode. Re-run with --spot-assembly parameter", "SRAE-70: Estimated number of spots excceds the limit for this mode. Re-run with --spot-assembly parameter."}},

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
        static const std::string err_code_prefix = "[code:{}] ";
        std::string str = err_code_prefix;
        str += message;
        mMessage = fmt::format(str, error_code, args...);
    }

    fastq_error(int error_code) 
        : m_error_code(error_code)
    {
        static const std::string err_code_prefix = "[code:{}] ";
        std::string str = err_code_prefix;
        str += std::get<0>(SHARQ_ERR_CODES.at(error_code));
        mMessage = fmt::format(str, error_code);
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
        os << fmt::format("{:-^80}", " SharQ error codes ") << "\n";
        os << fmt::format("{:<10}", "Code");
        os << fmt::format("{:<70}", "Description");
        os << "\n";
        os << fmt::format("{:-^80}", "") << "\n";
        for (const auto& it : SHARQ_ERR_CODES) {
            os << fmt::format("{:<10}", it.first);
            os << fmt::format("{:<70}", std::get<1>(it.second));
            os << "\n";
        }
    }


private:
    int m_error_code{0};
    std::string mMessage;
};

#endif

