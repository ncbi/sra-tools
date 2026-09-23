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
 * Purpose:
 *  Generate a test file using error-report-json.hpp
 *  Unit tests for error-report-json.hpp
 */

#include <sstream>
#include <JSON_ostream.hpp>
#include <loader/error-report-json.hpp>

#include <iostream>
#include <fstream>
using namespace std::string_literals;

static void report_file_1(ErrorReport &report, int const file)
{
    auto const recordCount = 10000000ull;
    auto baseCount = 1500000000ull;
    auto const avg_bases = baseCount / recordCount;
    static ErrorReport::File::ReadError const errors[] = {
        // The first 10 appear in the report, the rest are to meet the expected issue counts.
        {
            48273,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1048 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1048 1:N:0:ATCGTG\nACGTGCTAGCTAGCTAGCTAGCTAGCTAGCTA\n+\nFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48274,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1049 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1049 1:N:0:ATCGTG\nTTGCGATCGATCGATCGATCGATC\n+\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48275,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1050 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1050 1:N:0:ATCGTG\nACGTAGCTA5CTAGCTAGCTAGCTAGCTAGCT\n+\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_160 }
        },
        {
            48276,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1051 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1051 1:N:0:ATCGTG\nGCTAGCTAGCTAGCTA\n+\nFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48277,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1052 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1052 1:N:0:ATCGTG\nCCTGATGCTAGCTAGNCTAGCTAGCTAGCTA\n+\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_160 }
        },
        {
            48278,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1053 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1053 1:N:0:ATCGTG\nTGCATGCATGCATGCA\n+\nFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48279,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1054 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1054 1:N:0:ATCGTG\nAAGCTTAGCTAGCTAGCTAG\n+\nFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48280,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1055 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1055 1:N:0:ATCGTG\nGGCTTAGCTAGCTXAGCTAGCTAGCTAGCTA\n+\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_160 }
        },
        {
            48281,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1056 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1056 1:N:0:ATCGTG\nATCGATCGATCGATCGATCGATCG\n+\nFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48282,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1057 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1057 1:N:0:ATCGTG\nCCGTAACCGGTTAA\n+\nFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        // The rest of these are duplicates to meet the expected issue counts.
        {
            48283,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1055 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1055 1:N:0:ATCGTG\nGGCTTAGCTAGCTXAGCTAGCTAGCTAGCTA\n+\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_160, ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48285,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1057 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1057 1:N:0:ATCGTG\nCCGTAACCGGTTAA\n+\nFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48288,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1056 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1056 1:N:0:ATCGTG\nATCGATCGATCGATCGATCGATCG\n+\nFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48289,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1057 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1057 1:N:0:ATCGTG\nCCGTAACCGGTTAA\n+\nFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48290,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1056 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1056 1:N:0:ATCGTG\nATCGATCGATCGATCGATCGATCG\n+\nFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48291,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1057 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1057 1:N:0:ATCGTG\nCCGTAACCGGTTAA\n+\nFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48292,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1056 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1056 1:N:0:ATCGTG\nATCGATCGATCGATCGATCGATCG\n+\nFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            48293,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1057 1:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1101:18234:1057 1:N:0:ATCGTG\nCCGTAACCGGTTAA\n+\nFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
    };
    auto constexpr n_errors = sizeof(errors) / sizeof(errors[0]);
    int nextError = 0;
    
    report.files[file].errorReadSamples.sampleLimit = 10;

    for (unsigned long long i = 0; i < recordCount; ++i) {
        auto const bases = baseCount < avg_bases ? baseCount : avg_bases;
        auto const recNo = i + 1;
        
        if (nextError < n_errors && recNo == errors[nextError].recordNumber)
            report.addIssue(file, bases, errors[nextError++]);
        else
            report.addRecord(file, bases);
        
        baseCount -= bases;
    }
    assert(baseCount == 0);

    report.finish(file, 8740000000ull, 100.0, true);
}

static void report_file_2(ErrorReport &report, int const file)
{
    auto const recordCount = 8000000ull;
    auto baseCount = 1200000000ull;
    auto const avg_bases = baseCount / recordCount;
    static ErrorReport::File::ReadError const errors[] = {
        {
            9185,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1045 2:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1045 2:N:0:ATCGTG\nTTGCGATCGATCGATCGATCGATC\n+\nFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            9186,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1046 2:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1046 2:N:0:ATCGTG\nGATCGATCGATCGATC\n+\nFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            9187,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1047 2:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1047 2:N:0:ATCGTG\nACGTTGCAACGTTGCA\n+\nFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            9188,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1048 2:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1048 2:N:0:ATCGTG\nTTCGATCGATCGATCGATCGATCG\n+\nFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            9189,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1049 2:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1049 2:N:0:ATCGTG\nCGATCGATCGATCGATCG\n+\nFFFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            9190,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1050 2:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1050 2:N:0:ATCGTG\nATCGATCGATCGATCGATCG\n+\nFFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            9191,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1051 2:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1051 2:N:0:ATCGTG\nGGCATGGCATGGCATG\n+\nFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            9192,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1052 2:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1052 2:N:0:ATCGTG\nTATATATATATATATA\n+\nFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        },
        {
            9193,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1053 2:N:0:ATCGTG"s,
            "@A00456:27:H2W7JDSX3:1:1102:14567:1053 2:N:0:ATCGTG\nCCGGAATTCCGGAATTCC\n+\nFFFFFFFFFFFFFFFFFFFFFFFFFFF"s,
            { ErrorReport::SRAE_Codes::Value::srae_210 }
        }
    };
    auto constexpr n_errors = sizeof(errors) / sizeof(errors[0]);
    int nextError = 0;
    
    report.files[file].errorReadSamples.sampleLimit = 10;

    for (unsigned long long i = 0; i < recordCount; ++i) {
        auto const bases = baseCount < avg_bases ? baseCount : avg_bases;
        auto const recNo = i + 1;
        
        if (nextError < n_errors && recNo == errors[nextError].recordNumber)
            report.addIssue(file, bases, errors[nextError++]);
        else
            report.addRecord(file, bases);
        
        baseCount -= bases;
    }
    assert(baseCount == 0);

    report.finish(file, 6120000000ull, 100.0, true);
}

static int generate_error_report_json(std::ostream &strm)
{
    ErrorReport report;
    
    report.reportId = "VR-20260629-000002"s;
    report.generatedAt = "2026-06-29T14:35:12Z"s;
    
    report.validator.name = "FASTQ Validator"s;
    report.validator.version = "2.3.1"s;
    
    report.submission.submissionId = "SUB123456"s;
    
    report_file_1(report, report.addFile("sample_R1.fastq.gz", "FASTQ"));
    report_file_2(report, report.addFile("sample_R2.fastq.gz"s, "FASTQ"));

    report.printJSON(strm);
    
    return 0;
}

int main (int argc, char *argv [])
{
    if (argc == 2) {
        std::ofstream ofs{argv[1]};
        if (ofs)
            return generate_error_report_json(ofs);
        std::cerr << "could not open '" << argv[1] << "' for output!" << std::endl;
    }
    else if (argc == 1)
        return generate_error_report_json(std::cout);
    else
        std::cerr << "usage:\n\t" << argv[0] << "[<path>]" << std::endl;
    return 1;
}


