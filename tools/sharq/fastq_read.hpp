#ifndef __CFASTQREAD_HPP__
#define __CFASTQREAD_HPP__
/**
 * @file fastq_read.hpp
 * @brief FASTQ read
 * 
 */

#include "fastq_error.hpp"
#include <re2/re2.h>

using namespace std;

class CFastqRead
/// FASTQ read
{
public:
    /// ReadTypes 
    enum {
        SRA_READ_TYPE_TECHNICAL  = 0,
        SRA_READ_TYPE_BIOLOGICAL = 1
    };

    CFastqRead();
    ~CFastqRead() = default;

    void Reset();

    void AddSequenceLine(const string_view& sequence);
    void AddQualityLine(const string_view& quality);

    bool Empty() const { return mSpot.empty(); }

    bool IsSameSpot(const CFastqRead& read) const { return read.Spot() == mSpot; }

    size_t LineNumber() const { return mLineNumber;}
    const string& Spot() const { return mSpot; }
    const string& Suffix() const { return mSuffix; }
    const string& ReadNum() const { return mReadNum; }
    const string& SpotGroup() const { return mSpotGroup; }
    const uint8_t ReadFilter() const { return mReadFilter; }
    const string& Sequence() const { return mSequence; } ///< Sequence string
    const string& Quality() const { return mQuality; } ///< Quality string

    /**
     * Get quality scores as vector of uint8
     * 
     * @param[in,out] qual_score vector of scores 
     * 
    */
    void GetQualScores(vector<uint8_t>& qual_score) const; ///< Get quality scores as vector of uint8

    void SetType(char readType);

    uint8_t Type() const { return mReadType;} ///< return ReadType 

    void SetLineNumber(size_t line_number) { mLineNumber = line_number;}
    void SetSpot(const string& spot) { mSpot = spot; }
    void SetSpot(const re2::StringPiece& spot) {  spot.CopyToString(&mSpot); }

    void SetReadNum(const string& readNum) { mReadNum = readNum; }
    void SetReadNum(const re2::StringPiece& readNum) { readNum.CopyToString(&mReadNum); }

    void SetSuffix(const string& suffix) { mSuffix = suffix; }
    void SetSuffix(const re2::StringPiece& suffix) { suffix.CopyToString(&mSuffix); }

    void SetSpotGroup(const string& spotGroup);
    void SetSpotGroup(const re2::StringPiece& spotGroup);

    void SetReadFilter(uint8_t readFilter) { mReadFilter = readFilter; }

private:
    friend class fastq_reader;
    size_t mLineNumber{0};        ///< Line number the read starts with
    string mSpot;                 ///< Spot name
    string mReadNum;              ///< optional read number
    string mSpotGroup;            ///< spot group (barcode) 
    uint8_t mReadFilter{0};       ///< read filter 0, 1
    uint8_t mReadType{0};         ///< read type - SRA_READ_TYPE_TECHNICAL|SRA_READ_TYPE_BIOLOGICAL
    string mSuffix;               ///< Illumina suffix
    string mSequence;             ///< Sequence string
    string mQuality;              ///< Quality string as it comes from file adjusted to seq length
    vector<uint8_t> mQualScores;  ///< Numeric quality scores
};


CFastqRead::CFastqRead() 
{ 
    Reset(); 
};

void CFastqRead::Reset() 
{
    mSpot.clear();
    mSuffix.clear();
    mReadNum.clear();
    mSpotGroup.clear();
    mReadFilter = 0;
    mSequence.clear();
    mQuality.clear();
    mQualScores.clear();
    mLineNumber = 0;
}

void CFastqRead::AddSequenceLine(const string_view& sequence) 
{
    mSequence.append(sequence.begin(), sequence.end());
}


/*
void CFastqRead::AddSequenceLine(const string& sequence) 
{
    // check isalpha
    if (std::any_of(sequence.begin(), sequence.begin(), [](const char c) { return !isalpha(c);})) {
        throw CFastqError("Invalid sequence");
    }

    mSequence += sequence;
}
*/
void CFastqRead::AddQualityLine(const string_view& quality) 
{
    mQuality.append(quality.begin(), quality.end());
}

void CFastqRead::GetQualScores(vector<uint8_t>& qual_score) const
{
    if (mQualScores.empty()) {
        copy(mQuality.begin(), mQuality.end(), back_inserter(qual_score));
    } else {
        copy(mQualScores.begin(), mQualScores.end(), back_inserter(qual_score));
    }
}

void CFastqRead::SetType(char readType) 
{
    switch (readType) {
        case 'T':
        mReadType = SRA_READ_TYPE_TECHNICAL;
        break;
    case 'B':
        mReadType = SRA_READ_TYPE_BIOLOGICAL;
        break;
    case 'A':
            mReadType = Sequence().size() < 40 ? SRA_READ_TYPE_TECHNICAL: SRA_READ_TYPE_BIOLOGICAL;   
        break;
    default:
        throw fastq_error(150, "Read {}: invalid readtType '{}'", mSpot, readType);
    }
}


void CFastqRead::SetSpotGroup(const string& spotGroup) 
{ 
    if (spotGroup == "0")
        mSpotGroup.clear();
    else 
        mSpotGroup = spotGroup; 
}

void CFastqRead::SetSpotGroup(const re2::StringPiece& spotGroup) 
{ 
    if (spotGroup == "0")
        mSpotGroup.clear();
    else 
        spotGroup.CopyToString(&mSpotGroup); 
}


#endif

