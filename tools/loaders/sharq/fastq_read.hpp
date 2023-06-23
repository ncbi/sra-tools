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

/// FASTQ read
class CFastqRead
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
    const string& Channel() const { return mChannel; } ///< Nanopore channel
    const string& NanoporeReadNo() const { return mNanoporeReadNo; } ///< Nanopore read number

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
    void SetSpot(string spot) { mSpot = move(spot); }
    void SetSpot(const re2::StringPiece& spot) {  spot.CopyToString(&mSpot); }

    void MoveSpot(string&& spot) { mSpot = move(spot); }

    void SetReadNum(string readNum) { mReadNum = move(readNum); }
    void SetReadNum(const re2::StringPiece& readNum) { readNum.CopyToString(&mReadNum); }

    void SetSuffix(string suffix) { mSuffix = move(suffix); }
    void SetSuffix(const re2::StringPiece& suffix) { suffix.CopyToString(&mSuffix); }

    void SetSpotGroup(string spotGroup);
    void SetSpotGroup(const re2::StringPiece& spotGroup);

    void SetReadFilter(uint8_t readFilter) { mReadFilter = readFilter; }

    void SetChannel(const re2::StringPiece& channel) { channel.CopyToString(&mChannel); }
    void SetNanoporeReadNo(const re2::StringPiece& readNo) { readNo.CopyToString(&mNanoporeReadNo); }

    void SetSequence(string sequence) { mSequence = move(sequence); }
    void SetQualScores(vector<uint8_t> qual_scores) { mQualScores = move(qual_scores); }

    size_t m_SpotId = 0;     ///< Assigned spot_id  
    uint8_t m_ReaderIdx = 0; /// Reader's index
    size_t mLineNumber{0};        ///< Line number the read starts with
    uint8_t mReadType{0};         ///< read type - SRA_READ_TYPE_TECHNICAL|SRA_READ_TYPE_BIOLOGICAL
private:
    friend class fastq_reader;
    string mSpot;                 ///< Spot name
    string mReadNum;              ///< optional read number
    string mSpotGroup;            ///< spot group (barcode)
    uint8_t mReadFilter{0};       ///< read filter 0, 1
    string mSuffix;               ///< Illumina suffix
    string mSequence;             ///< Sequence string
    string mQuality;              ///< Quality string as it comes from file adjusted to seq length
    string mChannel;              ///< Nanopore channel
    string mNanoporeReadNo;       ///< Nanopore read number; not to be confused with mReadNum
    vector<uint8_t> mQualScores;  ///< Numeric quality scores
};

typedef CFastqRead fastq_read;

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
    mChannel.clear();
    mNanoporeReadNo.clear();
    mLineNumber = 0;
    m_SpotId = 0;
    m_ReaderIdx = 0;
}

static
char
translate( char ch )
{   // '-uUX?' -> 'NtTNN'
    switch( ch )
    {
        case 'u':
        case 'U':
            return 'T';
        case '-':
        case 'X':
        case '?':
        case '.':
            return 'N';
        default:
            return toupper(ch);
    }
}

void CFastqRead::AddSequenceLine(const string_view& sequence)
{   // self.transDashUridineX = str.maketrans('-uUX?', 'NtTNN')
    std::transform(sequence.begin(), sequence.end(), std::back_inserter(mSequence), translate);
    //mSequence.append(sequence.begin(), sequence.end());
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
        throw fastq_error(150, "Read {}: invalid readType '{}'", mSpot, readType);
    }
}


void CFastqRead::SetSpotGroup(string spotGroup)
{
    if (spotGroup == "0")
        mSpotGroup.clear();
    else
        mSpotGroup = move(spotGroup);
}

void CFastqRead::SetSpotGroup(const re2::StringPiece& spotGroup)
{
    if (spotGroup == "0")
        mSpotGroup.clear();
    else
        spotGroup.CopyToString(&mSpotGroup);
}


#endif
