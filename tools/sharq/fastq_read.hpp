#ifndef __CFASTQREAD_HPP__
#define __CFASTQREAD_HPP__

#include "fastq_error.hpp"
#include <re2/re2.h>

using namespace std;

//  ============================================================================
class CFastqRead
//  ============================================================================
{
public:
    enum {
        SRA_READ_TYPE_TECHNICAL  = 0,
        SRA_READ_TYPE_BIOLOGICAL = 1
    };

    CFastqRead();
    ~CFastqRead() = default;

    void Reset();


#if (__cplusplus >= 201703L) 
    void AddSequenceLine(const string_view& sequence);
    void AddQualityLine(const string_view& quality);
#else
    void AddSequenceLine(const string& sequence);
    void AddQualityLine(const string& quality);
#endif

    //void Validate();

    bool Empty() const { return mSpot.empty(); }

    bool IsSameSpot(const CFastqRead& read) const { return read.Spot() == mSpot; }

    size_t LineNumber() const { return mLineNumber;}
    const string& Spot() const { return mSpot; }
    const string& ReadNum() const { return mReadNum; }
    const string& SpotGroup() const { return mSpotGroup; }
    const uint8_t ReadFilter() const { return mReadFilter; }
    const string& Sequence() const { return mSequence; }
    const string& Quality() const { return mQuality; }

    void SetType(char readType) {
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
                throw fastq_error(150, "[line:{}] Read {}: invalid readtType '{}'", mLineNumber, mSpot, readType);
         }
    }

    uint8_t Type() const { return mReadType;}

    void SetLineNumber(size_t line_number) { mLineNumber = line_number;}
    void SetSpot(const string& spot) { mSpot = spot; }
    void SetSpot(const re2::StringPiece& spot) {  spot.CopyToString(&mSpot); }

    void SetReadNum(const string& readNum) { mReadNum = readNum; }
    void SetReadNum(const re2::StringPiece& readNum) { readNum.CopyToString(&mReadNum); }

    void SetSpotGroup(const string& spotGroup) { 
        if (spotGroup == "0")
            mSpotGroup.clear();
        else 
            mSpotGroup = spotGroup; 
    }
    void SetSpotGroup(const re2::StringPiece& spotGroup) { 
        if (spotGroup == "0")
            mSpotGroup.clear();
        else 
            spotGroup.CopyToString(&mSpotGroup); 
    }

    void SetReadFilter(uint8_t readFilter) { mReadFilter = readFilter; }

    void FormatData(ostream& os) const;

private:
    friend class fastq_reader;
    size_t mLineNumber{0};
    string mSpot;
    string mReadNum;
    string mSpotGroup;
    uint8_t mReadFilter{0};
    uint8_t mReadType{0};
    //string mSuffix;
    string mSequence;
    string mQuality;
};


CFastqRead::CFastqRead() 
{ 
    Reset(); 
};

void CFastqRead::Reset() 
{
    mSpot.clear();
    mReadNum.clear();
    mSpotGroup.clear();
    mReadFilter = 0;
    mSequence.clear();
    mQuality.clear();
    mLineNumber = 0;
    //mSequenceSize = mQualitySize = 0;
}

void CFastqRead::FormatData(ostream& os) const 
{
    os << "Spot    : " << Spot() << '\n';
    os << "Read    : " << ReadNum() << '\n';
    os << "Sequence: " << Sequence() << '\n';
    os << "Quality : " << Quality() << '\n';
    os << '\n';

}

void CFastqRead::AddSequenceLine(const string_view& sequence) {
    // check isalpha
    if (std::any_of(sequence.begin(), sequence.begin(), [](const char c) { return !isalpha(c);})) {
        throw fastq_error(160, "[line:{}] Read {}: invalid sequence characters", mLineNumber, mSpot);
    }
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

#endif

