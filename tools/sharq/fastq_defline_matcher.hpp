#ifndef __CFASTQ_DEFLINE_MATCHER_HPP__
#define __CFASTQ_DEFLINE_MATCHER_HPP__

/**
 * @file fastq_defline_matcher.hpp
 * @brief Defline matcher classes
 *
 */

#include "fastq_read.hpp"
#include <re2/re2.h>
#include <insdc/sra.h>

using namespace std;

/* platform id
#define sra_platform_id_t "INSDC:SRA:platform_id"
typedef uint8_t INSDC_SRA_platform_id;
enum
{
    SRA_PLATFORM_UNDEFINED         = 0,
    SRA_PLATFORM_454               = 1,
    SRA_PLATFORM_ILLUMINA          = 2,
    SRA_PLATFORM_ABSOLID           = 3,
    SRA_PLATFORM_COMPLETE_GENOMICS = 4,
    SRA_PLATFORM_HELICOS           = 5,
    SRA_PLATFORM_PACBIO_SMRT       = 6,
    SRA_PLATFORM_ION_TORRENT       = 7,
    SRA_PLATFORM_CAPILLARY         = 8,
    SRA_PLATFORM_OXFORD_NANOPORE   = 9
};
*/
class CDefLineMatcher
/// Base class for all defline matchers
{
public:
    /**
     * @brief Construct a new CDefLineMatcher object
     *
     * @param defLineName defline description
     * @param pattern defline regex pattern
     *
     * @throws runtime error on invalid pattern
     */
    CDefLineMatcher(
        const string& defLineName,
        const string& pattern)
    {
        mDefLineName = defLineName;
        re.reset(new re2::RE2(pattern));
        if (!re->ok())
            throw runtime_error("Invalid regex '" + pattern + "'");
        int groupSize = re->NumberOfCapturingGroups();
        argv.resize(groupSize);
        args.resize(groupSize);
        match.resize(groupSize);
        for (int i = 0; i < groupSize; ++i) {
            args[i] = &argv[i];
            argv[i] = &match[i];
        }

    }

    virtual ~CDefLineMatcher() {}

    /**
     * @brief Check if matcher recrogizes defline
     *
     * @param[in] defline string_view de fline to check
     * @return true if defline matches
     * @return false if defline does not match
     */
    virtual bool Matches(const string_view& defline)
    {
        return re2::RE2::PartialMatchN(defline, *re, args.empty() ? nullptr : &args[0], (int)args.size());
    }

    /**
     * @brief retrun Defline description
     *
     * @return const string&
     */
    const string& Defline() const { return mDefLineName;}

    /**
     * @brief Fill CFastqRead with the data from matched defline
     *
     * @param read
     */
    virtual void GetMatch(CFastqRead& read) = 0;

    /**
     * @brief Return matcher's platform code
     *
     * @return uint8_t
     */
    virtual uint8_t GetPlatform() const = 0;

protected:
    string mDefLineName;             ///< Defline description
    unique_ptr<re2::RE2> re;         ///< RE2 object
    vector<re2::RE2::Arg> argv;      ///< internal structure to facilitate capture of matched groups
    vector<re2::RE2::Arg*> args;     ///< internal structure to facilitate capture of matched groups
    vector<re2::StringPiece> match;  ///< captured macthed groups
};

class CDefLineMatcher_NoMatch : public CDefLineMatcher
/// Matcher that matches nothing
{
public:
    CDefLineMatcher_NoMatch():
        CDefLineMatcher("NoMatch", "a^")
    {
    }

    bool Matches(const string_view& defline) override {return false;}
    virtual void GetMatch(CFastqRead& read) override  {}
    virtual uint8_t GetPlatform() const override { return 0;}

protected:
};

class CDefLineMatcher_AllMatch : public CDefLineMatcher
/// Matcher that matches everything similar to defline
{
public:
    CDefLineMatcher_AllMatch():
        CDefLineMatcher("undefined", R"([@>+]([!-~]+)(\s+|$))")
    {
    }

    //bool Matches(const string_view& defline) override { return true;}
    virtual void GetMatch(CFastqRead& read) override  {
        read.SetSpot(match[0]);
    }
    virtual uint8_t GetPlatform() const override { return 0;}
protected:
};

class CDefLineMatcherIlluminaNewDataGroup : public CDefLineMatcher
/// illuminaNewDataGroup matcher
{
public:
    CDefLineMatcherIlluminaNewDataGroup() :
        CDefLineMatcher(
            "illuminaNewDataGroup",
            "^[@>+]([!-~]+?)(\\s+|[_|])([12345]|):([NY]):(\\d+|O):?([!-~]*?)(\\s+|$)")
    {
    }

    uint8_t GetPlatform() const override {
        return SRA_PLATFORM_ILLUMINA;
    };

    virtual void GetMatch(CFastqRead& read) override
    {
        read.SetSpot(match[0]);
        read.SetReadNum(match[2]);
        read.SetReadFilter(match[3] == "Y" ? 1 : 0);
        read.SetSpotGroup(match[5]);
    }

private:
};

class CDefLineMatcherIlluminaNewBase : public CDefLineMatcher
/// Base class for IlluminaNew matchers
{
public:
    CDefLineMatcherIlluminaNewBase(
        const string& displayName,
        const string& pattern): CDefLineMatcher(displayName, pattern)
    {
    }

    uint8_t GetPlatform() const override {
        return SRA_PLATFORM_ILLUMINA;
    };

    virtual void GetMatch(CFastqRead& read) override
    {
        string spot;
        if (!match[0].empty()) {
            match[0].CopyToString(&spot);
            spot.append(1, (match[1][0] == '_') ? ':' : match[1][0]);
        }
        match[2].AppendToString(&spot); //lane
        spot.append(1, (match[3][0] == '_') ? ':' : match[3][0]);
        match[4].AppendToString(&spot); //tile
        spot.append(1, (match[5][0] == '_') ? ':' : match[5][0]);
        match[6].AppendToString(&spot); //x
        spot.append(1, (match[7][0] == '_') ? ':' : match[7][0]);
        match[8].AppendToString(&spot); //y
        read.SetSpot(spot);

        read.SetReadNum(match[10]);

        read.SetReadFilter(match[11] == "Y" ? 1 : 0);

        read.SetSpotGroup(match[13]);
    }
private:

};


class CDefLineMatcherIlluminaNew : public CDefLineMatcherIlluminaNewBase
/// illuminaNew matcher
{
public:
    CDefLineMatcherIlluminaNew() :
        CDefLineMatcherIlluminaNewBase(
            "illuminaNew",
            "^[@>+]([!-~]+?)([:_])(\\d+)([:_])(\\d+)([:_])(-?\\d+\\.?\\d*)([:_])(-?\\d+\\.\\d+|\\d+)(\\s+|[_|-])([12345]|):([NY]):(\\d+|O):?([!-~]*?)(\\s+|$)")
    {}
};


class CDefLineMatcherIlluminaNewNoPrefix : public CDefLineMatcherIlluminaNewBase
/// illuminaNewNoPrefix matcher
{
public:
    CDefLineMatcherIlluminaNewNoPrefix() :
        CDefLineMatcherIlluminaNewBase(
            "illuminaNewNoPrefix",
            "^[@>+]([!-~]*?)(:?)(\\d+)([:_])(\\d+)([:_])(\\d+)([:_])(\\d+)(\\s+|_)([12345]|):([NY]):(\\d+|O):?([!-~]*?)(\\s+|$)")
    {}

};


class CDefLineMatcherIlluminaNewWithSuffix : public CDefLineMatcherIlluminaNewBase
/// IlluminaNewWithSuffix (aka IlluminaNewWithJunk in fastq-load.py) matcher
{
public:
    CDefLineMatcherIlluminaNewWithSuffix() :
        CDefLineMatcherIlluminaNewBase(
            "illuminaNewWithSuffix",
            "^[@>+]([!-~]+)([:_])(\\d+)([:_])(\\d+)([:_])(-?\\d+\\.?\\d*)([:_])(-?\\d+\\.\\d+|\\d+)([!-~]+?\\s+|[!-~]+?[:_|-])([12345]|):([NY]):(\\d+|O):?([!-~]*?)(\\s+|$)")
    {
        assert(mSuffixPattern.ok());
        static const int IlluminaSuffixGroups = 6;
        mSuffixArgv.resize(IlluminaSuffixGroups);
        mSuffixArgs.resize(IlluminaSuffixGroups);
        mSuffixMatch.resize(IlluminaSuffixGroups);
        for (int i = 0; i < IlluminaSuffixGroups; ++i) {
            mSuffixArgs[i] = &mSuffixArgv[i];
            mSuffixArgv[i] = &mSuffixMatch[i];
        }
    }
    virtual void GetMatch(CFastqRead& read) override
    {
        CDefLineMatcherIlluminaNewBase::GetMatch(read);
        if (match[9].size() > 2) {
            if (re2::RE2::PartialMatchN(match[9], mSuffixPattern, &mSuffixArgs[0], (int)mSuffixArgs.size())) {
                read.SetSuffix(mSuffixMatch[2]);
            }
        }
    }

private:
    re2::RE2 mSuffixPattern{"(#[!-~]*?|)(/[12345]|\\[12345])?([!-~]*?)(#[!-~]*?|)(/[12345]|\\[12345])?([:_|]?)(\\s+|$)"};
    vector<re2::RE2::Arg> mSuffixArgv;
    vector<re2::RE2::Arg*> mSuffixArgs;
    vector<re2::StringPiece> mSuffixMatch;
};


class CDefLineMatcherIlluminaNewWithPeriods : public CDefLineMatcherIlluminaNewBase
/// illuminaNewWithPeriods matcher
{
public:
    CDefLineMatcherIlluminaNewWithPeriods() :
        CDefLineMatcherIlluminaNewBase(
            "illuminaNewWithPeriods",
            "^[@>+]([!-~]+?)(\\.)(\\d+)(\\.)(\\d+)(\\.)(\\d+)(\\.)(\\d+)(\\s+|_)([12345]|)\\.([NY])\\.(\\d+|O)\\.?([!-~]*?)(\\s+|$)")
    {}
};


class CDefLineMatcherIlluminaNewWithUnderscores : public CDefLineMatcherIlluminaNewBase
/// illuminaNewWithUnderscores matcher
{
public:
    CDefLineMatcherIlluminaNewWithUnderscores() :
        CDefLineMatcherIlluminaNewBase(
            "illuminaNewWithUnderscores",
            "^[@>+]([!-~]+?)(_)(\\d+)(_)(\\d+)(_)(\\d+)(_)(\\d+)(\\s+|_)([12345]|)_([NY])_(\\d+|O)_?([!-~]*?)(\\s+|$)")
    {}
};


class CDefLineMatcherBgiOld : public CDefLineMatcher
/// BgiOld matcher
{
public:
    CDefLineMatcherBgiOld() :
        CDefLineMatcher(
            "BgiOld",
            R"(^[@>+](\S{1,3}\d{9}\S{0,3})(L\d{1})(C\d{3})(R\d{3})([_]?\d{1,8})(#[!-~]*?|)(/[1234]\S*|)(\s+|$))")
    {
    }
    uint8_t GetPlatform() const override {
        return 0;//SRA_PLATFORM_UNDEFINED
    };

    virtual void GetMatch(CFastqRead& read) override
    {

        // flowcell, lane, column, row, readNo, spotGroup, readNum, endSep
        string spot;
        match[0].AppendToString(&spot); //flowcell
        match[1].AppendToString(&spot); //lane
        match[2].AppendToString(&spot); //column
        match[3].AppendToString(&spot); //row
        match[4].AppendToString(&spot); // readNo
        read.SetSpot(spot);

        if (!match[6].empty() && match[6].starts_with("/"))
            match[6].remove_prefix(1);
        read.SetReadNum(match[6]);

        if (!match[5].empty())
            match[5].remove_prefix(1); // spotGroup
        read.SetSpotGroup(match[5]);
    }

};

class CDefLineMatcherBgiNew : public CDefLineMatcher
/// BgiNew matcher
{
public:
    CDefLineMatcherBgiNew() :
        CDefLineMatcher(
            "BgiNew",
            R"(^[@>+](\S{1,3}\d{9}\S{0,3})(L\d{1})(C\d{3})(R\d{3})([_]?\d{1,8})(\S*)(\s+|[_|-])([12345]|):([NY]):(\d+|O):?([!-~]*?)(\s+|$))")
    {}

    uint8_t GetPlatform() const override {
        return 0;//SRA_PLATFORM_UNDEFINED
    };

    virtual void GetMatch(CFastqRead& read) override
    {
        //  0         1     2       3    4       5       6     7        8           9         10         11
        //  flowcell, lane, column, row, readNo, suffix, sep1, readNum, filterRead, reserved, spotGroup, endSep
        string spot;
        match[0].AppendToString(&spot); //flowcell
        match[1].AppendToString(&spot); //lane
        match[2].AppendToString(&spot); //column
        match[3].AppendToString(&spot); //row
        match[4].AppendToString(&spot); // readNo
        read.SetSpot(spot);

        read.SetSuffix(match[5]);

        read.SetReadNum(match[7]);

        read.SetSpotGroup(match[10]);

        read.SetReadFilter(match[8] == "Y" ? 1 : 0);
    }
};

class CDefLineMatcherNanopore : public CDefLineMatcher
{
public:
    CDefLineMatcherNanopore() :
        CDefLineMatcher(
            "Nanopore3",
            R"([@>+]([!-~]*?)[: ]?([!-~]+?Basecall)(_[12]D[_0]*?|_Alignment[_0]*?|_Barcoding[_0]*?|)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D|)[: ]([!-~]*?)[: ]?([!-~ ]+?_ch)_?(\d+)(_read|_file)_?(\d+)(_strand\d*.fast5|_strand\d*.*|)(\s+|$))"
        )
    {}

    uint8_t GetPlatform() const override {
        return SRA_PLATFORM_OXFORD_NANOPORE;
    };

    virtual void GetMatch(CFastqRead& read) override
    {
//for( auto m : match ) cout << m << endl;
        //  0    1    2      3    4       5       6     7        8           9         10         11
        //       name suffix poreRead
        read.SetSpot( match[1] );
        read.SetSuffix( match[2] );
        // For now, poreRead is expected to be "_template", other variants will be passed to the regular fastq-load.py
        //TODO: ch match[6]
        //TODO: read match[8]
    }
};

#endif

