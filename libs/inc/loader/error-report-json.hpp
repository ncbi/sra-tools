/* ===========================================================================
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
 * Project:
 *  Loader Error Reporting
 *
 * Purpose:
 *  Record errors and format generate JSON
 */

#pragma once

#ifndef error_report_json_hpp
#define error_report_json_hpp

#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <stdexcept>
#include <cassert>
#include <cstdio>

class ErrorReport {
    std::map<std::string, int> fileIndex;
public:
    class SRAE_Code {
    public:
        std::string
            code,
            name,
            documentationUrl;
    };
    
    class SRAE_Codes {
    public:
        enum class Value {
            srae_160 = 160,
            srae_202 = 202,
            srae_210 = 210
        };
        static SRAE_Code const &getCodeFor(Value which) {
            using namespace std::string_literals;
            static SRAE_Code const code[] = {
                { "N/A"s, "N/A"s, "N/A"s },
                { "SRAE-160"s, "Sequence contains non-alphabetical character"s, "https://trace.ncbi.nlm.nih.gov/sra/docs/errors/SRAE-160"s },
                { "SRAE-202"s, "Read has no quality scores"s, "https://trace.ncbi.nlm.nih.gov/sra/docs/errors/SRAE-202"s },
                { "SRAE-210"s, "Quality score length does not match sequence length"s, "https://trace.ncbi.nlm.nih.gov/sra/docs/errors/SRAE-210"s },
            };
            switch (which) {
            case Value::srae_160:
                return code[1];
            case Value::srae_202:
                return code[2];
            case Value::srae_210:
                return code[3];
            default:
                return code[0];
            }
        }
        static SRAE_Code const &getCodeFor(int which) {
            return getCodeFor((Value)which);
        }
    };

    class Status {
        enum Value {
            pending,
            processing,
            success,
            failure
            ///< TODO: what are the values here?
        };
        Value value;
        Status(Value value_) : value(value_) {}
    public:
        Status() : value{pending} {};
        Status(Status const &) = default;
        
        static Status Pending() { return Status{pending}; }
        static Status Processing() { return Status{processing}; }
        static Status Success() { return Status{success}; }
        static Status Failure() { return Status{failure}; }
        
        bool isPending() const { return value == pending; }
        bool isProcessing() const { return value == processing; }
        bool isSuccess() const { return value == success; }
        bool isFailure() const { return value == failure; }

        std::string const &get() const {
            using namespace std::string_literals;
            static std::string const values[] = {
                "Pending"s,
                "Processing"s,
                "SUCCESS"s,
                "FAILED"s
            };
            switch (value) {
            case pending:
                return values[0];
            case processing:
                return values[1];
            case success:
                return values[2];
            case failure:
                return values[3];
            default:
                throw std::logic_error("Invalid Status value");
            }
        }
#ifdef JSON_ostream_hpp
        friend JSON_ostream &operator <<(JSON_ostream &strm, Status const &self) {
            return strm << self.get();
        }
#endif
    };

    class Validator {
    public:
        std::string name,           ///< loader name, e.g. "bam-load".
                    version;        ///< loader version, e.g. "3.4.1".
#ifdef JSON_ostream_hpp
        friend JSON_ostream &operator <<(JSON_ostream &strm, Validator const &self) {
            return strm
                << JSON_Member{"name"} << self.name
                << JSON_Member{"version"} << self.version
            ;
        }
#endif
#if TOOLKIT_VERS
        static std::string currentVersion() {
            auto const rev = TOOLKIT_VERS & 0xFFFF;
            auto const mnr = (TOOLKIT_VERS >> 16) & 0xFF;
            auto const mjr = (TOOLKIT_VERS >> 24) & 0xFF;
            return std::to_string(mjr) + '.' + std::to_string(mnr) + '.' + std::to_string(rev);
        }
#endif
    };

    class Submission {
    public:
        std::string submissionId;   ///< note, loaders do not know this value, leave empty.
        Status status;
        Submission()
        : submissionId({})
        , status(Status::Processing())
        {}
        Submission(Submission const &) = default;
        Submission(Submission &&) = default;
#ifdef JSON_ostream_hpp
        friend JSON_ostream &operator <<(JSON_ostream &strm, Submission const &self) {
            return strm
                << JSON_Member{"submissionId"} << self.submissionId
                << JSON_Member{"status"} << self.status
            ;
        }
#endif
    };

    class SubmissionSummary {
    public:
        unsigned
            filesPending = 0,
            filesProcessing = 0,
            filesPassed = 0,
            filesFailed = 0;
        unsigned filesProcessed() const {
            return filesPassed + filesFailed;
        }
        
        /// For FASTQ, these counts arein `bam-load`, these are counts of records, not reads
        unsigned long long
            readsPassingValidation = 0,
            readsFailingValidation = 0;
        unsigned long long totalReadsExamined() const {
            return readsPassingValidation + readsFailingValidation;
        }
        double readSuccessRatePercent() const {
            return totalReadsExamined() > 0 ? (100.0 * (double)readsPassingValidation / (double)totalReadsExamined()) : 0.0;
        }

        unsigned long long
            passingBasePairs = 0,
            failingBasePairs = 0;
        unsigned long long totalBasePairsExamined() const {
            return passingBasePairs + failingBasePairs;
        }
        double basePairSuccessRatePercent() const {
            return totalBasePairsExamined() > 0 ? (100.0 * (double)passingBasePairs / (double)totalBasePairsExamined()) : 0.0;
        }
        unsigned totalValidationErrors = 0;
        unsigned uniqueErrorTypes = 0;
        
        Status status() const {
            if (filesFailed > 0)
                return Status::Failure();
            if (filesPassed > 0)
                return Status::Success();
            if (filesProcessing > 0)
                return Status::Processing();
            return Status::Pending();
        }
#ifdef JSON_ostream_hpp
        friend JSON_ostream &operator <<(JSON_ostream &strm, SubmissionSummary const &self) {
            return strm
                << JSON_Member{"filesProcessed"} << self.filesProcessed()
                << JSON_Member{"filesPassed"} << self.filesPassed
                << JSON_Member{"filesFailed"} << self.filesFailed
                << JSON_Member{"totalReadsExamined"} << self.totalReadsExamined()
                << JSON_Member{"readsPassingValidation"} << self.readsPassingValidation
                << JSON_Member{"readsFailingValidation"} << self.readsFailingValidation
                << JSON_Member{"readSuccessRatePercent"} << self.readSuccessRatePercent()
                << JSON_Member{"totalBasePairsExamined"} << self.totalBasePairsExamined()
                << JSON_Member{"passingBasePairs"} << self.passingBasePairs
                << JSON_Member{"failingBasePairs"} << self.failingBasePairs
                << JSON_Member{"basePairSuccessRatePercent"} << self.basePairSuccessRatePercent()
                << JSON_Member{"totalValidationErrors"} << self.totalValidationErrors
                << JSON_Member{"uniqueErrorTypes"} << self.uniqueErrorTypes
            ;
        }
#endif
    };
    
    class SubmissionQualityMetrics : public SubmissionSummary {
    public:
        class ErrorDistributionEntry {
        public:
            SRAE_Codes::Value code;
            unsigned occurences; ///< this error occured this many times during the load process
            std::vector<std::string> filesAffected;
#ifdef JSON_ostream_hpp
            friend JSON_ostream &operator <<(JSON_ostream &strm, ErrorDistributionEntry const &self) {
                auto const &code = SRAE_Codes::getCodeFor(self.code);
                strm
                    << JSON_Member{"code"} << code.code
                    << JSON_Member{"name"} << code.name
                    << JSON_Member{"occurrences"} << self.occurences
                    << JSON_Member{"filesAffected"} << '[';
                for (auto const &file : self.filesAffected) {
                    strm << file << ',';
                }
                return strm << ']';
            }
#endif
        };
        using ErrorDistribution = std::vector<ErrorDistributionEntry>;
        
        ErrorDistribution errorDistribution;
#ifdef JSON_ostream_hpp
        friend JSON_ostream &operator <<(JSON_ostream &strm, SubmissionQualityMetrics const &self) {
            strm << static_cast<SubmissionSummary const &>(self)
                << JSON_Member{"errorDistribution"} << '[';
            for (auto const & e : self.errorDistribution) {
                strm << '{' << e << '}';
            }
            return strm << ']';
        }
#endif
    };
    
    class File {
    public:
        class ReadError {
        public:
            unsigned long long recordNumber;
            std::string readHeader; ///< defline for FASTQ, QNAME for SAM/BAM.
            std::string rawRecord; ///< Whole record for FASTQ; SAM record SAM/BAM.
            std::vector<SRAE_Codes::Value> detectedErrors;
#ifdef JSON_ostream_hpp
            friend JSON_ostream &operator <<(JSON_ostream &strm, ReadError const &self) {
                strm
                    << JSON_Member{"recordNumber"} << self.recordNumber
                    << JSON_Member{"readHeader"} << self.readHeader
                    << JSON_Member{"detectedErrors"} << '[';
                for (auto err : self.detectedErrors) {
                    strm << SRAE_Codes::getCodeFor(err).code;
                }
                strm << ']' // detectedErrors
                    << JSON_Member{"rawRecord"} << self.rawRecord;
                return strm;
            }
#endif
        };

        class ErrorReadSamples {
        public:
            using Reads = std::vector<ReadError>;
            
            unsigned sampleLimit = 1;
            Reads reads;
            
            unsigned sampledReadCount() const { return (unsigned)reads.size(); };
            void add(ReadError const &error) {
                if (sampledReadCount() < sampleLimit)
                    reads.push_back(error);
            }
#ifdef JSON_ostream_hpp
            friend JSON_ostream &operator <<(JSON_ostream &strm, ErrorReadSamples const &self) {
                strm
                    << JSON_Member{"sampleLimit"} << self.sampleLimit
                    << JSON_Member{"sampleStrategy"} << "first_reads_with_detected_errors"
                    << JSON_Member{"sampledReadCount"} << self.sampledReadCount()
                    << JSON_Member{"reads"} << '[';
                for (auto & read : self.reads) {
                    strm << '{'
                        << JSON_Member{"readIndex"} << (&read - &self.reads[0]) + 1
                        << read
                        << '}';
                }
                strm << ']';
                return strm;
            }
#endif
        };

        std::string filename;
        std::string fileFormat; ///< e.g. "FASTQ"
        Status status;
        ErrorReadSamples errorReadSamples;
        
        unsigned long long fileSizeBytes = 0;
        double validationCoveragePercent = 0.0;
        unsigned long long
            readsPassingValidation = 0,
            readsFailingValidation = 0,
            passingBasePairs = 0,
            failingBasePairs = 0;
        std::map<SRAE_Codes::Value, unsigned> issueCount;
        bool processingCompleted = false;
        int issueBase = 0;

        unsigned long long totalReadsExamined() const {
            return readsPassingValidation + readsFailingValidation;
        }
        double readSuccessRatePercent() const {
            return totalReadsExamined() > 0 ? 100.0 * (double)readsPassingValidation / (double)totalReadsExamined() : 0.0;
        }
        unsigned long long totalBasePairsExamined() const {
            return passingBasePairs + failingBasePairs;
        }
        double basePairSuccessRatePercent() const {
            return totalBasePairsExamined() > 0 ? 100.0 * (double)passingBasePairs / (double)totalBasePairsExamined() : 0.0;
        }
        unsigned totalValidationErrors() const {
            unsigned result = 0;
            for (auto &[code, count] : issueCount) {
                result += count;
            }
            return result;
        }
        void finish(unsigned long long fileSizeBytes, double validationCoveragePercent, bool completed) {
            this->fileSizeBytes = fileSizeBytes;
            this->validationCoveragePercent = validationCoveragePercent;
            processingCompleted = completed;
            if (totalReadsExamined() > 0 && !status.isFailure())
                status = Status::Success();
        }
        void addRecord(unsigned readLength) {
            readsPassingValidation += 1;
            passingBasePairs += readLength;
            if (!status.isFailure())
                status = Status::Processing();
        }
        bool addIssue(unsigned readLength, ReadError const &error) {
            bool is_new = false;
            
            readsFailingValidation += 1;
            failingBasePairs += readLength;
            
            assert(error.detectedErrors.size() >= 1);

            for (auto & err : error.detectedErrors) {
                if ((int)err == 0) continue;
                auto [iter, inserted] = issueCount.emplace(err, 1);
                if (inserted)
                    is_new = true;
                else
                    iter->second += 1;
            }
            errorReadSamples.add(error);
            if (!status.isFailure())
                status = Status::Failure();
            
            return is_new;
        }
#ifdef JSON_ostream_hpp
        JSON_ostream &reportSummary(JSON_ostream &strm) const {
            return strm << '{'
                << JSON_Member{"filename"} << filename
                << JSON_Member{"fileFormat"} << fileFormat
                << JSON_Member{"status"} << status
                << JSON_Member{"errorCount"} << issueCount.size()
                << JSON_Member{"warningCount"} << 0
                << JSON_Member{"failedReads"} << readsFailingValidation
                << JSON_Member{"totalReadsExamined"} << readsFailingValidation + readsPassingValidation
            << '}';
        }
        friend JSON_ostream &operator <<(JSON_ostream &strm, File const &self) {
            strm
                << JSON_Member{"filename"} << self.filename
                << JSON_Member{"fileFormat"} << self.fileFormat
                << JSON_Member{"status"} << self.status
                << JSON_Member{"qualityMetrics"} << '{'
                    << JSON_Member{"fileSizeBytes"} << self.fileSizeBytes
                    << JSON_Member{"validationCoveragePercent"} << self.validationCoveragePercent
                    << JSON_Member{"processingCompleted"} << self.processingCompleted
                    << JSON_Member{"totalReadsExamined"} << self.readsPassingValidation + self.readsFailingValidation
                    << JSON_Member{"readsPassingValidation"} << self.readsPassingValidation
                    << JSON_Member{"readsFailingValidation"} << self.readsFailingValidation
                    << JSON_Member{"readSuccessRatePercent"} << self.readSuccessRatePercent()
                    << JSON_Member{"totalBasePairsExamined"} << self.totalBasePairsExamined()
                    << JSON_Member{"passingBasePairs"} << self.passingBasePairs
                    << JSON_Member{"failingBasePairs"} << self.failingBasePairs
                    << JSON_Member{"basePairSuccessRatePercent"} << self.basePairSuccessRatePercent()
                    << JSON_Member{"totalValidationErrors"} << self.totalValidationErrors()
                    << JSON_Member{"uniqueErrorTypes"} << self.issueCount.size()
                    << JSON_Member{"errorDistribution"} << '[';
                    for (auto &[code, count] : self.issueCount) {
                        auto const &c = SRAE_Codes::getCodeFor(code);
                        strm << '{'
                            << JSON_Member{"code"} << c.code
                            << JSON_Member{"name"} << c.name
                            << JSON_Member{"occurrences"} << count
                        << '}';
                    }
            strm << ']'; // errorDistribution
            strm << '}'; // qualityMetrics
            strm << JSON_Member{"issues"} << '[';
                int i = 0;
                for (auto &[code, count] : self.issueCount) {
                    char issueId[32];
                    auto const &c = SRAE_Codes::getCodeFor(code);
                    auto n = std::snprintf(issueId, sizeof(issueId), "ERR-%04u", ++i + self.issueBase);
                    assert(n < sizeof(issueId));
                    strm << '{'
                        << JSON_Member{"issueId"} << issueId
                        << JSON_Member{"severity"} << "ERROR"
                        << JSON_Member{"code"} << c.code
                        << JSON_Member{"name"} << c.name
                        << JSON_Member{"occurrences"} << count
                        << JSON_Member{"documentationUrl"} << c.documentationUrl
                    << '}';
                }
            strm << ']'; // issues
            strm << JSON_Member{"errorReadSamples"} << '{' << self.errorReadSamples << '}';
            return strm;
        }
#endif
    };
    std::string reportId;
    std::string generatedAt; ///< ISO 8601 timestamp; e.g. "2026-06-29T14:35:12Z"
    Validator validator;
    Submission submission;
    std::vector<File> files;

    SubmissionQualityMetrics submissionQualityMetrics() const {
        SubmissionQualityMetrics result{};
        std::map<SRAE_Codes::Value, unsigned> issueCount;
        
        for (auto & file : files) {
            if (file.status.isSuccess()) {
                result.filesPassed += 1;
            }
            else if (file.status.isFailure()) {
                result.filesFailed += 1;
            }
            else if (file.status.isProcessing()) {
                result.filesProcessing += 1;
            }
            else {
                result.filesPending += 1;
            }
            result.readsPassingValidation += file.readsPassingValidation;
            result.readsFailingValidation +=  file.readsFailingValidation;
            result.passingBasePairs += file.passingBasePairs;
            result.failingBasePairs += file.failingBasePairs;
            for (auto &[code, count] : file.issueCount) {
                issueCount[code] += count;
            }
        }
        result.uniqueErrorTypes = issueCount.size();
        result.errorDistribution.reserve(issueCount.size());
        for (auto &[code, count] : issueCount) {
            SubmissionQualityMetrics::ErrorDistributionEntry entry{code, count};

            result.totalValidationErrors += count;
            for (auto & file : files) {
                for (auto &[fcode, count] : file.issueCount) {
                    if (code == fcode)
                        entry.filesAffected.push_back(file.filename);
                }
            }
            result.errorDistribution.emplace_back(entry);
        }
        return result;
    }

    /// Add a new file to the report.
    /// - Parameter filename: the unique name of the file.
    /// - Parameter format: the format of the file, e.g. FASTQ.
    /// - returns -1 if the filename exists, else the index of the new file.
    int addFile(std::string_view filename, std::string_view format) {
        for (auto const &entry : files) {
            if (entry.filename == filename)
                return -1;
        }
        int result = files.size();
        files.push_back(File{std::string{filename}, std::string{format}});
        return result;
    }
    
    /// Report an issue.
    /// - Parameters:
    ///   - file: the file number returned from `addFile`.
    ///   - readLength: the length of the sequence.
    ///   - error: the full description of the issue.
    bool addIssue(int file, unsigned readLength, File::ReadError const &error) {
        assert(0 <= file && file < files.size());
        if (file < 0 || file > files.size())
            throw std::out_of_range{"invalid file number"};
        return files[file].addIssue(readLength, error);
    }
    
    /// Report a normal record (i.e. a record with no issues).
    /// - Parameters:
    ///   - file: the file number returned from `addFile`.
    ///   - readLength: the length of the sequence.
    void addRecord(int file, unsigned readLength) {
        assert(0 <= file && file < files.size());
        if (file < 0 || file > files.size())
            throw std::out_of_range{"invalid file number"};
        files[file].addRecord(readLength);
    }
    
    /// Set final state of the file.
    /// - Parameters:
    ///   - file: the file number returned from `addFile`.
    ///   - fileSizeBytes: the file size in bytes.
    ///   - validationCoveragePercent: percentage of the file that was processed/checked.
    ///   - completed: is processing complete?
    void finish(int file, unsigned long long fileSizeBytes, double validationCoveragePercent, bool completed) {
        assert(0 <= file && file < files.size());
        if (file < 0 || file > files.size())
            throw std::out_of_range{"invalid file number"};
        files[file].finish(fileSizeBytes, validationCoveragePercent, completed);
    }
#ifdef JSON_ostream_hpp
    friend JSON_ostream &operator <<(JSON_ostream &strm, ErrorReport const &self) {
        auto const &metrics{self.submissionQualityMetrics()};
        auto submission{self.submission};
        submission.status = metrics.status();
        auto files = self.files;
        
        {
            int issues = 0;
            for (auto &f : files) {
                f.issueBase = issues;
                issues += f.issueCount.size();
            }
        }

        strm
            << JSON_Member{"schemaVersion"} << "1.1"
            << JSON_Member{"reportType"} << "SRA Validation Report"
            << JSON_Member{"reportId"} << self.reportId
            << JSON_Member{"generatedAt"} << self.generatedAt
            << JSON_Member{"validator"} << '{' << self.validator << '}'
            << JSON_Member{"submission"} << '{' << submission << '}'
            << JSON_Member{"submissionSummary"} << '{' << static_cast<SubmissionSummary const &>(metrics) << '}'
            << JSON_Member{"submissionQualityMetrics"} << '{' << metrics << '}'
            << JSON_Member{"fileSummary"} << '[';
        for (auto &f : files) {
            f.reportSummary(strm);
        }
        strm
            << ']'  // fileSummary
            << JSON_Member{"files"} << '[';
                for (auto &f : files) {
                    strm << '{' << f << '}';
                }
        strm
            << ']'; // files
        return strm;
    }
    std::ostream &printJSON(std::ostream &strm) const {
        {
            JSON_ostream json{strm};
            json << '{' << *this << '}';
        }
        return strm << std::endl;
    }
#endif
#ifdef _h_klib_time_
    static std::string currentTimestamp() {
        char buffer[32];
        return std::string{buffer, KTimeIso8601(KTimeStamp(), buffer, sizeof(buffer))};
    }
#endif
};

#endif
