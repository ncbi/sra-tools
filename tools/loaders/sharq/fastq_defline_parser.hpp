#ifndef __CFASTQDEFLINEPARSER_HPP__
#define __CFASTQDEFLINEPARSER_HPP__

/**
 * @file fastq_defline_parser.hpp
 * @brief Defline Parser
 *
 */

#include <string_view>
#include <vector>
#include "fastq_read.hpp"
#include "fastq_defline_matcher.hpp"
#include "fastq_error.hpp"
#include <spdlog/spdlog.h>
#include <set>

using namespace std;
class CDefLineParser
/// Parse defline into CFastQRead using list of registered defline matcher
{
public:
    /**
     * @brief Construct a new CDefLineParser object
     *
     * Registers a list of supported matchers
     */
    CDefLineParser();

    /**
     * @brief Destroy the CDefLineParser object
     *
     */
    ~CDefLineParser();

    /**
     * @brief Reset internal state
     *
     */
    void Reset();

    /**
     * @brief Parse defline into CFastqRead object
     *
     * @param[in] defline defline string_view to parse
     * @param[in, out] read CFastqRead with populated defline related field
     */
    void Parse(const string_view& defline, CFastqRead& read);

    /**
     * @brief Check if parser can match a given defline
     *
     * @param[in] defline defline string_view to parse
     * @param[in] strict if true Matchall pattern cannot be used, otherwise it will match any defline
     * @return true if defline can be parsed
     * @return false if defline cannot be parsed
     */
    bool Match(const string_view& defline, bool strict = false);

    /**
     * @brief Enable MatchAll pattern
     *
     */
    void SetMatchAll();

    /**
     * @brief Get last matched defline type
     *
     * @return const string& defline type
     */
    const string& GetDeflineType() const;

    /**
     * @brief Get last matched platform code
     *
     * @return uint8_t platform code
     */
    uint8_t GetPlatform() const;

    /**
     * @brief Get all defline type there were matched
     *
     * @return const set<string>&
     */
    const set<string>& AllDeflineTypes() const { return mDeflineTypes;}
private:
    std::vector<std::shared_ptr<CDefLineMatcher>> mDefLineMatchers; ///< Vector of all registered Defline matchers
    size_t mIndexLastSuccessfulMatch = 0; ///< Index of the last sucessfull matcher
    size_t mAllMatchIndex = -1;           ///< Index of Match everything matcher
    std::set<string> mDeflineTypes;       ///< Set of deflines types processed by this reader
};


CDefLineParser::CDefLineParser()
{
    Reset();
    mDefLineMatchers.emplace_back(new CDefLineMatcher_NoMatch);
    mDefLineMatchers.emplace_back(new CDefLineMatcherBgiNew);
    mDefLineMatchers.emplace_back(new CDefLineMatcherBgiOld);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNew);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNewNoPrefix);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNewWithSuffix);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNewWithPeriods);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNewWithUnderscores);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaOldWithSuffix);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaOldColon);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaOldUnderscore);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaOldWithSuffix2);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaOldNoPrefix);
    mDefLineMatchers.emplace_back(new CDefLineMatcherLS454);    
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNewDataGroup);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIonTorrent2);
    mDefLineMatchers.emplace_back(new CDefLineMatcherIonTorrent);
    mDefLineMatchers.emplace_back(new CDefLineMatcherNanopore1);
    mDefLineMatchers.emplace_back(new CDefLineMatcherNanopore2);
    mDefLineMatchers.emplace_back(new CDefLineMatcherNanopore3);
    mDefLineMatchers.emplace_back(new CDefLineMatcherNanopore3_1);
    mDefLineMatchers.emplace_back(new CDefLineMatcherNanopore5); // before Nanopore4, to match fastq-load.py
    mDefLineMatchers.emplace_back(new CDefLineMatcherNanopore4);
}

CDefLineParser::~CDefLineParser()
{

}

void CDefLineParser::SetMatchAll()
{
    mDefLineMatchers.emplace_back(new CDefLineMatcher_AllMatch);
    mAllMatchIndex = mDefLineMatchers.size() - 1;
}


void CDefLineParser::Reset()
{
    mIndexLastSuccessfulMatch = 0;
}

bool CDefLineParser::Match(const string_view& defline, bool strict)
{
    if (mDefLineMatchers[mIndexLastSuccessfulMatch]->Matches(defline)) {
        return true;
    }
    for (size_t i = 0; i < mDefLineMatchers.size(); ++i) {
        if (i == mIndexLastSuccessfulMatch) {
            continue;
        }
        if (!mDefLineMatchers[i]->Matches(defline)) {
            continue;
        }
        if (strict && i == mAllMatchIndex)
            return false;
        mIndexLastSuccessfulMatch = i;
        mDeflineTypes.insert(mDefLineMatchers[mIndexLastSuccessfulMatch]->Defline());
        //spdlog::info("Current pattern: {}", mDefLineMatchers[mIndexLastSuccessfulMatch]->Defline());
        return true;
    }
    return false;
}


void CDefLineParser::Parse(const string_view& defline, CFastqRead& read)
{
    if (Match(defline)) {
        mDefLineMatchers[mIndexLastSuccessfulMatch]->GetMatch(read);
        return;
    }
    // VDB-4970: do not include the bad defline's text since it may contain XML-breaking characters
    throw fastq_error(100, "Defline not recognized");
}

inline
uint8_t CDefLineParser::GetPlatform() const
{
    return mDefLineMatchers[mIndexLastSuccessfulMatch]->GetPlatform();
}

inline
const string& CDefLineParser::GetDeflineType() const
{
    return mDefLineMatchers[mIndexLastSuccessfulMatch]->Defline();
}



#endif
