#ifndef __CFASTQDEFLINEPARSER_HPP__
#define __CFASTQDEFLINEPARSER_HPP__

#include <string_view>
#include <vector>
#include "fastq_read.hpp"
#include "fastq_defline_matcher.hpp"
#include "fastq_error.hpp"

using namespace std;

//  ============================================================================
class CDefLineParser
//  ============================================================================
{
public:
    CDefLineParser();
    ~CDefLineParser();
    void Reset();
    void Parse(const string_view& defline, CFastqRead& read);
    bool Match(const string_view& defline);

    uint8_t GetPlatform() const;
private:
    size_t mIndexLastSuccessfulMatch = 0;
    std::vector<std::shared_ptr<CDefLineMatcher>> mDefLineMatchers;
};


CDefLineParser::CDefLineParser() 
{ 
    Reset(); 
    mDefLineMatchers.emplace_back(new CDefLineMatcherBgiOld());
    mDefLineMatchers.emplace_back(new CDefLineMatcherBgiNew());
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNew());
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNewNoPrefix());
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNewWithSuffix());
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNewWithPeriods());
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNewWithUnderscores());
    mDefLineMatchers.emplace_back(new CDefLineMatcherIlluminaNewDataGroup);
}

CDefLineParser::~CDefLineParser() 
{

}

void CDefLineParser::Reset() 
{
    mIndexLastSuccessfulMatch = 0;
}

bool CDefLineParser::Match(const string_view& defline) 
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
        mIndexLastSuccessfulMatch = i;
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
    throw fastq_error(100, "[line:{}] Defline '{}' not recognized", read.LineNumber(), defline);
}

inline
uint8_t CDefLineParser::GetPlatform() const 
{
    return mDefLineMatchers[mIndexLastSuccessfulMatch]->GetPlatform();
}




#endif

