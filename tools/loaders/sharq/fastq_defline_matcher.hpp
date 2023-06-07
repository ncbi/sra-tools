#ifndef __CFASTQ_DEFLINE_MATCHER_HPP__
#define __CFASTQ_DEFLINE_MATCHER_HPP__

/**
 * @file fastq_defline_matcher.hpp
 * @brief Defline matcher classes
 *
 */

#include "fastq_read.hpp"
#include "regexpr.hpp"
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
    :   mDefLineName( defLineName ),
        re( pattern )
    {
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
        return re.Matches(defline);
    }

    /**
     * @brief retrun Defline description
     *
     * @return const string&
     */
    const string& Defline() const { return mDefLineName;}

    /**
     * @brief Return matcher's pattern
    */
    const string& GetPattern() const { return re.GetPattern();}

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
    CRegExprMatcher re;              ///< regexpr matcher
    string m_tmp_spot;               ///< variable for spot name assembly 

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
        read.SetSpot(re.GetMatch()[0]);
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
        read.SetSpot(re.GetMatch()[0]);
        read.SetReadNum(re.GetMatch()[2]);
        read.SetReadFilter(re.GetMatch()[3] == "Y" ? 1 : 0);
        read.SetSpotGroup(re.GetMatch()[5]);
    }

private:
};

static inline
void s_add_sep(string& s, re2::StringPiece& sep)
{
    if (!sep.empty())
        s.append(1, (sep[0] == '-') ? ':' : sep[0]);
}

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
        m_tmp_spot.clear();
        if (!re.GetMatch()[0].empty()) {
            re.GetMatch()[0].CopyToString(&m_tmp_spot);
            s_add_sep(m_tmp_spot, re.GetMatch()[1]);
        }
        re.GetMatch()[2].AppendToString(&m_tmp_spot); //lane
        s_add_sep(m_tmp_spot, re.GetMatch()[3]);
        re.GetMatch()[4].AppendToString(&m_tmp_spot); //tile
        s_add_sep(m_tmp_spot, re.GetMatch()[5]);
        re.GetMatch()[6].AppendToString(&m_tmp_spot); //x
        s_add_sep(m_tmp_spot, re.GetMatch()[7]);
        re.GetMatch()[8].AppendToString(&m_tmp_spot); //y
        read.MoveSpot(move(m_tmp_spot));

        read.SetReadNum(re.GetMatch()[10]);

        read.SetReadFilter(re.GetMatch()[11] == "Y" ? 1 : 0);

        read.SetSpotGroup(re.GetMatch()[13]);
    }
private:

};


static 
bool s_is_number(const string_view& s)
{
    return !s.empty() && find_if(s.begin(), s.end(), [](unsigned char c) { return !isdigit(c); }) == s.end();
}


class CDefLineMatcherIlluminaOldBase : public CDefLineMatcher
/// Base class for IlluminaNew matchers
{
    CRegExprMatcher sub_re1;              ///< additional regex to support numDiscards matching 
    CRegExprMatcher sub_re2;              ///< additional regex to support numDiscards matching
    CRegExprMatcher sub_re3;              ///< additional regex to support numDiscards matching
    CRegExprMatcher illuminaOldSuffix2;   ///< additional regex for suffix matching 
    CRegExprMatcher illuminaOldSuffix;    ///< additional regex for suffix matching

public:
    CDefLineMatcherIlluminaOldBase(
        const string& displayName,
        const string& pattern): CDefLineMatcher(displayName, pattern),
        sub_re1(R"(([!-~]*?)(:)(\d+)$)"),
        sub_re2(R"(([!-~]*?)(:)(\d+)(:)(\d+)(\s+|$))"),
        sub_re3(R"((\d+)(:)(\d+)(\s+|$))"),
        illuminaOldSuffix2(R"((-?\d+\.\d+|-?\d+)([^\d\s.][!-~]+))"),
        illuminaOldSuffix(R"((/[12345])([^\d\s][!-~]+))")
    {
        
    }

    uint8_t GetPlatform() const override {
        return SRA_PLATFORM_ILLUMINA;
    };

    //prefix, sep1, lane, sep2, tile, sep3, x, sep4, y, spotGroup, readNum, endSep
    //0       1     2     3     4     5     6  7     8  9          10       11 

    virtual void GetMatch(CFastqRead& read) override
    {
        re2::StringPiece prefix{re.GetMatch()[0]};
        auto& lane = re.GetMatch()[2];
        auto& tile = re.GetMatch()[4];
        auto& x = re.GetMatch()[6];
        auto& y = re.GetMatch()[8];

        auto& readNum = re.GetMatch()[10];

        m_tmp_suffix.set(nullptr, 0);
        if (illuminaOldSuffix2.Matches(y)) {
            y = illuminaOldSuffix2.GetMatch()[0];
            auto& suffix = illuminaOldSuffix2.GetMatch()[1];
            if (suffix.size() >= 3) {
                if (suffix.starts_with("/1") || suffix.starts_with("/2"))
                    suffix.remove_prefix(2);
                m_tmp_suffix = suffix;                    
            }         
        } else if (!readNum.empty() && illuminaOldSuffix.Matches(readNum)) {
            readNum = illuminaOldSuffix.GetMatch()[0];    
            if (illuminaOldSuffix.GetMatch()[1].size() >= 3) 
                m_tmp_suffix = illuminaOldSuffix.GetMatch()[1];
        }

        if (!m_tmp_suffix.empty())
            read.SetSuffix(m_tmp_suffix);

        int numDiscards = 0;
        if (!prefix.empty()) {
            numDiscards = countExtraNumbersInIllumina(prefix, re.GetMatch()[7], x, y);
            if (numDiscards == 2 && x.find('.') != re2::StringPiece::npos) {
                string new_suffix;
                re.GetMatch()[5].AppendToString(&new_suffix); // sep3
                x.AppendToString(&new_suffix); //x 
                re.GetMatch()[7].AppendToString(&new_suffix); // sep4
                y.AppendToString(&new_suffix); //y
                new_suffix.append(read.Suffix());
                read.SetSuffix(new_suffix);
            } else if (numDiscards == 1 && y.find('.') != re2::StringPiece::npos) {
                string new_suffix;
                re.GetMatch()[7].AppendToString(&new_suffix); // sep4
                y.AppendToString(&new_suffix); //y
                new_suffix.append(read.Suffix());
                read.SetSuffix(new_suffix);
            }
        }

        m_tmp_spot.clear();
        if (numDiscards == 1) {
            assert(!prefix.empty());
            if (sub_re1.Matches(prefix)) {
                sub_re1.GetMatch()[0].AppendToString(&m_tmp_spot);
                sub_re1.GetMatch()[1].AppendToString(&m_tmp_spot);
                sub_re1.GetMatch()[2].AppendToString(&m_tmp_spot);
            } else {
                prefix.AppendToString(&m_tmp_spot);
            }
            s_add_sep(m_tmp_spot, re.GetMatch()[1]);
            lane.AppendToString(&m_tmp_spot);
            s_add_sep(m_tmp_spot, re.GetMatch()[3]);
            tile.AppendToString(&m_tmp_spot);
            s_add_sep(m_tmp_spot, re.GetMatch()[5]);
            x.AppendToString(&m_tmp_spot);

        } else if (numDiscards == 2) {
            assert(!prefix.empty());
            if (sub_re2.Matches(prefix)) {
                sub_re2.GetMatch()[0].AppendToString(&m_tmp_spot);
                sub_re2.GetMatch()[1].AppendToString(&m_tmp_spot);
                sub_re2.GetMatch()[2].AppendToString(&m_tmp_spot);
                sub_re2.GetMatch()[3].AppendToString(&m_tmp_spot);
                sub_re2.GetMatch()[4].AppendToString(&m_tmp_spot);
            } else if (sub_re3.Matches(prefix)) {
                sub_re3.GetMatch()[0].AppendToString(&m_tmp_spot);
                sub_re3.GetMatch()[1].AppendToString(&m_tmp_spot);
                sub_re3.GetMatch()[2].AppendToString(&m_tmp_spot);
            } else {
                assert(false);
                throw fastq_error(101, "Unexpected IlluminaOld Defline");
            }
            s_add_sep(m_tmp_spot, re.GetMatch()[1]);
            lane.AppendToString(&m_tmp_spot);
            s_add_sep(m_tmp_spot, re.GetMatch()[3]);
            tile.AppendToString(&m_tmp_spot);

        } else {
            if (!prefix.empty()) {
                prefix.CopyToString(&m_tmp_spot);
                s_add_sep(m_tmp_spot, re.GetMatch()[1]);
            }
            lane.AppendToString(&m_tmp_spot); //lane
            s_add_sep(m_tmp_spot, re.GetMatch()[3]); // sep2
            tile.AppendToString(&m_tmp_spot); //tile
            s_add_sep(m_tmp_spot, re.GetMatch()[5]); // sep3
            x.AppendToString(&m_tmp_spot); //x
            s_add_sep(m_tmp_spot, re.GetMatch()[7]); //sep 4
            y.AppendToString(&m_tmp_spot); //y
        }
        read.MoveSpot(move(m_tmp_spot));

        if (!readNum.empty()) { // readNum
            readNum.remove_prefix(1);
            read.SetReadNum(readNum);
        }

        if (!re.GetMatch()[9].empty()) {
            re.GetMatch()[9].remove_prefix(1); // spotGroup
            read.SetSpotGroup(re.GetMatch()[9]);
        }

    }

    int countExtraNumbersInIllumina(re2::StringPiece& prefix, re2::StringPiece& sep_str, re2::StringPiece& x, re2::StringPiece& y)
    {
        if (sep_str.empty())
            return 0;
        const char sep = (sep_str[0] == '-') ? ':' : sep_str[0];
        // Determine how many numbers at the end of prefix
        // separated by sep 
        sharq::split(prefix, m_tmp_strlist, sep);
        int numCount = 0;

        // Determine if prefix ends in one or two digits
        // delineated with 'sep'
        auto sz = m_tmp_strlist.size();
        if (sz == 0)
            return 0;

        if (s_is_number(m_tmp_strlist[sz - 1]))
            ++numCount;
        if (sz > 1 && s_is_number(m_tmp_strlist[sz - 2]))
            ++numCount;

        // Determine how many numbers to discard (capping at 2 based on what we have seen in the data)
        int discardCount = 0;
        if (numCount) {
            sharq::split(y, m_tmp_strlist, '.');
            if (!m_tmp_strlist.empty() && atoi(m_tmp_strlist.front().data()) < 4) {
                discardCount += 1;
                if (numCount == 2) {
                    sharq::split(x, m_tmp_strlist, '.');
                    if (!m_tmp_strlist.empty() && atoi(m_tmp_strlist.front().data()) < 4) 
                        discardCount += 1;
                }
            }
        }
        return discardCount;
    }

private:
    vector<string_view> m_tmp_strlist;
    re2::StringPiece m_tmp_suffix;

};


class CDefLineMatcherIlluminaNew : public CDefLineMatcherIlluminaNewBase
/// illuminaNew matcher
{
public:
    CDefLineMatcherIlluminaNew() :
        CDefLineMatcherIlluminaNewBase(
            "illuminaNew",
            R"(^[@>+]([!-~]+?)([:_])(\d+)([:_])(\d+)([:_])(-?\d+\.?\d*)([:_])(-?\d+\.\d+|\d+)(\s+|[:_|-])([12345]|):([NY]):(\d+|O):?([!-~]*?)(\s+|$))")
    {}
};


class CDefLineMatcherIlluminaNewNoPrefix : public CDefLineMatcherIlluminaNewBase
/// illuminaNewNoPrefix matcher
{
public:
    CDefLineMatcherIlluminaNewNoPrefix() :
        CDefLineMatcherIlluminaNewBase(
            "illuminaNewNoPrefix",
            R"(^[@>+]([!-~]*?)(:?)(\d+)([:_])(\d+)([:_])(\d+)([:_])(\d+)(\s+|_)([12345]|):([NY]):(\d+|O):?([!-~]*?)(\s+|$))")
    {}

};


class CDefLineMatcherIlluminaNewWithSuffix : public CDefLineMatcherIlluminaNewBase
/// IlluminaNewWithSuffix (aka IlluminaNewWithJunk in fastq-load.py) matcher
{
public:
    CDefLineMatcherIlluminaNewWithSuffix() :
        CDefLineMatcherIlluminaNewBase(
            "illuminaNewWithSuffix",
            R"(^[@>+]([!-~]+)([:_])(\d+)([:_])(\d+)([:_])(-?\d+\.?\d*)([:_])(-?\d+\.\d+|\d+)([!-/:-~][!-~]*?\s+|[!-/:-~][!-~]*?[:_|-])([12345]|):([NY]):(\d+|O):?([!-~]*?)(\s+|$))"),
        mSuffixPattern("(#[!-~]*?|)(/[12345]|\\[12345])?([!-~]*?)(#[!-~]*?|)(/[12345]|\\[12345])?([:_|]?)(\\s+|$)")
    {
    }
    virtual void GetMatch(CFastqRead& read) override
    {
        CDefLineMatcherIlluminaNewBase::GetMatch(read);
        const string m9 = re.GetMatch()[9].as_string();
        if ( m9.size() > 2 ) {
            if ( mSuffixPattern.Matches( m9 ) ) {
                read.SetSuffix( mSuffixPattern.GetMatch()[2] );
            }
        }
    }

private:
    CRegExprMatcher mSuffixPattern;
};

    //         if (re2::RE2::PartialMatchN(re.GetMatch()[9], mSuffixPattern, &mSuffixArgs[0], (int)mSuffixArgs.size())) {
    //             read.SetSuffix(mSuffixMatch[2]);
    //         }
    //     }
    // }

    // re2::RE2 mSuffixPattern{"(#[!-~]*?|)(/[12345]|\\[12345])?([!-~]*?)(#[!-~]*?|)(/[12345]|\\[12345])?([:_|]?)(\\s+|$)"};
    // vector<re2::RE2::Arg> mSuffixArgv;
    // vector<re2::RE2::Arg*> mSuffixArgs;
    // vector<re2::StringPiece> mSuffixMatch;


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

class CDefLineMatcherIlluminaOldColon : public CDefLineMatcherIlluminaOldBase
/// illuminaOldColon
{
public:
    CDefLineMatcherIlluminaOldColon() :
        CDefLineMatcherIlluminaOldBase(
            "IlluminaOldColon",
            R"(^[@>+]?([!-~]+?)(:)(\d+)(:)(\d+)(:)(-?\d+\.?\d*)([-:])(-?\d+\.\d+|-?\d+)_?[012]?(#[!-~]*?|)\s?(/[12345]|\\[12345])?(\s+|$))")
    {}

};

class CDefLineMatcherIlluminaOldUnderscore : public CDefLineMatcherIlluminaOldBase
/// IlluminaOldUnderscore
{
public:
    CDefLineMatcherIlluminaOldUnderscore() :
        CDefLineMatcherIlluminaOldBase(
            "IlluminaOldUnderscore",
            R"(^[@>+]?([!-~]+?)(_)(\d+)(_)(\d+)(_)(-?\d+\.?\d*)(_)(-?\d+\.\d+|-?\d+)(#[!-~]*?|)\s?(/[12345]|\\[12345])?(\s+|$))")
    {}

};

class CDefLineMatcherIlluminaOldNoPrefix : public CDefLineMatcherIlluminaOldBase
/// IlluminaOldUnderscore
{
public:
    CDefLineMatcherIlluminaOldNoPrefix() :
        CDefLineMatcherIlluminaOldBase(
            "IlluminaOldNoPrefix",
            R"(^[@>+]?([!-~]*?)(:?)(\d+)(:)(\d+)(:)(-?\d+\.?\d*)(:)(-?\d+\.\d+|-?\d+)(#[!-~]*?|)\s?(/[12345]|\\[12345])?(\s+|$))")
    {}

};


class CDefLineMatcherIlluminaOldWithSuffix : public CDefLineMatcherIlluminaOldBase
/// IlluminaOldUnderscore
{
public:
    CDefLineMatcherIlluminaOldWithSuffix() :
        CDefLineMatcherIlluminaOldBase(
            "IlluminaOldWithSuffix",
            R"(^[@>+]?([!-~]+?)(:)(\d+)(:)(\d+)(:)(-?\d+\.?\d*)(:)(-?\d+\.\d+|-?\d+)(#[!-~]*?|)(/[12345][!-~]+)(\s+|$))")
    {}
};

class CDefLineMatcherIlluminaOldWithSuffix2 : public CDefLineMatcherIlluminaOldBase
/// IlluminaOldUnderscore
{
public:
    CDefLineMatcherIlluminaOldWithSuffix2() :
        CDefLineMatcherIlluminaOldBase(
            "IlluminaOldWithSuffix2",
            R"(^[@>+]?([!-~]+?)(:)(\d+)(:)(\d+)(:)(-?\d+\.?\d*)(:)(-?\d+\.?\d*[!-~]+?)(#[!-~]*?|)\s?(/[12345]|\\[12345])?(\s+|$))")
    {}

};


class CDefLineMatcherBgiOld : public CDefLineMatcher
/// BgiOld matcher
{
public:
    CDefLineMatcherBgiOld() :
        CDefLineMatcher(
            "BgiOld",
            R"(^[@>+](\S{1,3}\d{9}\S{0,3})(L\d)(C\d{3})(R\d{3})([_]?\d{1,8})(#[!-~]*?|)(/[1234]\S*|)(\s+|$))")
    {
    }
    uint8_t GetPlatform() const override {
        return 0;//SRA_PLATFORM_UNDEFINED
    };

    virtual void GetMatch(CFastqRead& read) override
    {

        // flowcell, lane, column, row, readNo, spotGroup, readNum, endSep
        m_tmp_spot.clear();
        re.GetMatch()[0].AppendToString(&m_tmp_spot); //flowcell
        re.GetMatch()[1].AppendToString(&m_tmp_spot); //lane
        re.GetMatch()[2].AppendToString(&m_tmp_spot); //column
        re.GetMatch()[3].AppendToString(&m_tmp_spot); //row
        re.GetMatch()[4].AppendToString(&m_tmp_spot); // readNo
        read.MoveSpot(move(m_tmp_spot));

        if (!re.GetMatch()[6].empty() && re.GetMatch()[6].starts_with("/"))
            re.GetMatch()[6].remove_prefix(1);
        read.SetReadNum(re.GetMatch()[6]);

        if (!re.GetMatch()[5].empty())
            re.GetMatch()[5].remove_prefix(1); // spotGroup
        read.SetSpotGroup(re.GetMatch()[5]);
    }

};

class CDefLineMatcherBgiNew : public CDefLineMatcher
/// BgiNew matcher
{
public:
    CDefLineMatcherBgiNew() :
        CDefLineMatcher(
            "BgiNew",
            R"(^[@>+](\S{1,3}\d{9}\S{0,3})(L\d)(C\d{3})(R\d{3})([_]?\d{1,8})(\S*)(\s+|[_|-])([12345]|):([NY]):(\d+):?([!-~]*?)(\s+|$))")

    {}

    uint8_t GetPlatform() const override {
        return 0;//SRA_PLATFORM_UNDEFINED
    };

    virtual void GetMatch(CFastqRead& read) override
    {
        //  0         1     2       3    4       5       6     7        8           9         10         11
        //  flowcell, lane, column, row, readNo, suffix, sep1, readNum, filterRead, reserved, spotGroup, endSep
        m_tmp_spot.clear();
        re.GetMatch()[0].AppendToString(&m_tmp_spot); //flowcell
        re.GetMatch()[1].AppendToString(&m_tmp_spot); //lane
        re.GetMatch()[2].AppendToString(&m_tmp_spot); //column
        re.GetMatch()[3].AppendToString(&m_tmp_spot); //row
        re.GetMatch()[4].AppendToString(&m_tmp_spot); // readNo
        read.MoveSpot(move(m_tmp_spot));

        read.SetSuffix(re.GetMatch()[5]);

        read.SetReadNum(re.GetMatch()[7]);

        read.SetSpotGroup(re.GetMatch()[10]);

        read.SetReadFilter(re.GetMatch()[8] == "Y" ? 1 : 0);
    }
};

// NANOPORE

class CDefLineMatcherNanoporeBase : public CDefLineMatcher
{
public:
    CDefLineMatcherNanoporeBase( const string& defLineName, const string& pattern )
    :   CDefLineMatcher( defLineName, pattern ),
        getPorePass( R"(pass[/\\])" ),
        getPoreFail( R"(fail[/\\])" ),
        getPoreBarcode2( R"((NB\d{2}|BC\d{2}|barcode\d{2})([/\\]))" )
    {
    }

    uint8_t GetPlatform() const override {
        return SRA_PLATFORM_OXFORD_NANOPORE;
    };

    void PostProcess( CFastqRead& read )
    {   // Common Nanopore processing (done after parsing the defline)

        if ( read.Channel().empty() )
        {
            read.SetChannel( "0" );
        }
        if ( read.NanoporeReadNo().empty() )
        {
            read.SetNanoporeReadNo( "0" );
        }

        // Set readNum to None if actually a file number or missing
        if ( poreMid == "_file" ||
             read.NanoporeReadNo().empty() )
        {
            read.SetNanoporeReadNo( "0" );
        }

        // Process poreFile if present
        if ( ! poreFile.empty() )
        {
            // Check for 'pass' or 'fail'
            if ( getPorePass.Matches( poreFile ) )
            {
                read.SetReadFilter( 0 );
            }
            else if ( getPoreFail.Matches( poreFile ) )
            {
                read.SetReadFilter( 1 );
            }

            // Check for barcode
            if ( getPoreBarcode2.Matches( poreFile ) )
            {
                re2::StringPiece barcode = getPoreBarcode2.GetMatch()[0];
                const string Barcode = "barcode";
                if ( barcode.find( Barcode ) == 0 )
                {   // self.spotGroup = re.sub(r'barcode(\d+)$',r'BC\1',self.spotGroup,1)
                    read.SetSpotGroup( string( "BC" ) + barcode.substr( Barcode.size() ).as_string() );
                }
                else
                {
                    read.SetSpotGroup( barcode );
                }
            }
        }

    //     # Split poreFile on '/' or '\' if present

    //     poreFileChunks = re.split(r'[/\\]',self.poreFile)
    //     if len ( poreFileChunks) > 1:
    //         self.poreFile = poreFileChunks.pop()
    //TODO: ... and then self.poreFile does not seem to be used anywhere (ask Bob)

    //TODO: where does appendPoreReadToName come from? (ask Bob)
    // if self.poreRead and self.appendPoreReadToName:
    //     self.name += self.suffix + self.poreRead

    // # Check for missing poreRead (from R-based poRe fastq dump) and normalize read type

    // if ( poreRead.empty() )
    // {
    //TODO: where does filename come from? (ask Bob)
        //     if self.filename:
        //         if self.pore2Dpresent.search(self.filename):
        //             self.poreRead = "2D"
        //         elif self.poreTemplatePresent.search(self.filename):
        //             self.poreRead = "template"
        //         elif self.poreComplementPresent.search(self.filename):
        //             self.poreRead = "complement"
        //         else:
        //             self.poreRead = ""

        //         if self.poreRead and self.appendPoreReadToName:
        //             self.name += self.suffix + "_" + self.poreRead //AB: this might happen twice
        //     else:
        //         self.poreRead = ""

    }
    // elif ( self.poreRead == "_twodirections" or
    //        self.poreRead[1:] == "2D" or
    //        self.poreRead[0:3] == "_2d" ):
    //     self.poreRead = "2D"
    // elif ( self.poreRead[0:9] == "_template" or
    //        self.poreRead == "-1D" or
    //        self.poreRead == ".1T" ):
    //     self.poreRead = "template"
    // else:
    //     self.poreRead = "complement"

    // if ( not self.deflineType and
    //      self.saveDeflineType ):
    //     self.deflineType = self.NANOPORE
    // }
    //TODO: ... and then self.poreRead does not seem to be used anywhere (ask Bob)

    re2::StringPiece poreMid;
    string poreFile;
    re2::StringPiece poreRead;
    CRegExprMatcher getPorePass;
    CRegExprMatcher getPoreFail;
    CRegExprMatcher getPoreBarcode2;
};

class CDefLineMatcherNanopore_Basic : public CDefLineMatcherNanoporeBase
{
public:
    CDefLineMatcherNanopore_Basic( const string& defLineName, const string& pattern )
        : CDefLineMatcherNanoporeBase( defLineName, pattern ), getPoreReadNo2( R"(read_?(\d+))" )
    {
    }

    virtual void GetMatch(CFastqRead& read) override
    {
//re.DumpMatch();
        // 0 poreStart
        // 1 self.channel
        // 2 poreMid
        // 3 self.readNo
        // 4 poreEnd
        // 5 self.poreRead
        // 6 self.poreFile
        // 7 endSep

        m_tmp_spot.clear();
        if ( !re.GetMatch()[3].empty() )
        {   // self.name = poreStart + self.channel + poreMid + self.readNo + poreEnd
            re.GetMatch()[0].AppendToString(&m_tmp_spot); //poreStart
            re.GetMatch()[1].AppendToString(&m_tmp_spot); //channel
            re.GetMatch()[2].AppendToString(&m_tmp_spot); //poreMid
            re.GetMatch()[3].AppendToString(&m_tmp_spot); //readNo
            re.GetMatch()[4].AppendToString(&m_tmp_spot); //poreEnd

            read.SetNanoporeReadNo( re.GetMatch()[3] );
        }
        else
        {   // self.name = poreStart + self.channel + poreEnd
            re.GetMatch()[0].AppendToString(&m_tmp_spot); //poreStart
            re.GetMatch()[1].AppendToString(&m_tmp_spot); //channel
            re.GetMatch()[4].AppendToString(&m_tmp_spot); //poreEnd

            if ( getPoreReadNo2.Matches(re.GetLastInput()) )
            {
                read.SetNanoporeReadNo( getPoreReadNo2.GetMatch()[0] );
            }
        }
        read.MoveSpot(move(m_tmp_spot));

        read.SetChannel( re.GetMatch()[1] );

        poreMid = re.GetMatch()[2];
        poreRead = re.GetMatch()[5];
        re.GetMatch()[6].AppendToString( &poreFile );
        PostProcess( read );
    }

    CRegExprMatcher getPoreReadNo2;
};

class CDefLineMatcherNanopore1 : public CDefLineMatcherNanopore_Basic
{
public:
    CDefLineMatcherNanopore1() : CDefLineMatcherNanopore_Basic(
            "Nanopore1",
            R"([@>+]+?(channel_)(\d+)(_read_)?(\d+)?([!-~]*?)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D)?(:[!-~ ]+?_ch\d+_file\d+_strand.fast5)?(\s+|$))"
        ) {}
};

class CDefLineMatcherNanopore2 : public CDefLineMatcherNanopore_Basic
{
public:
    CDefLineMatcherNanopore2() : CDefLineMatcherNanopore_Basic(
            "Nanopore2",
            R"([@>+]([!-~]*?ch)(\d+)(_file)(\d+)([!-~]*?)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D)(:[!-~ ]+?_ch\d+_file\d+_strand.fast5)?(\s+|$))"
        ) {}
};

class CDefLineMatcherNanopore3 : public CDefLineMatcherNanoporeBase
{
public:
    CDefLineMatcherNanopore3() :
        CDefLineMatcherNanoporeBase(
            "Nanopore3",
            R"([@>+]([!-~]*?)[: ]?([!-~]+?Basecall)(_[12]D[_0]*?|_Alignment[_0]*?|_Barcoding[_0]*?|)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D|)[: ]([!-~]*?)[: ]?([!-~ ]+?_ch)_?(\d+)(_read|_file)_?(\d+)(_strand\d*.fast5|_strand\d*.*|)(\s+|$))"
        )
    {}

    virtual void GetMatch(CFastqRead& read) override
    {
//re.DumpMatch();
        // 0 prefix
        // 1 self.name
        // 2 self.suffix
        // 3 self.poreRead
        // 4 discard
        // 5 poreStart
        // 6 self.channel
        // 7 poreMid
        // 8 self.readNo
        // 9 poreEnd
        // 10 endSep

        read.SetSpot( re.GetMatch()[1] );
        read.SetSuffix( re.GetMatch()[2] );
        // For now, poreRead is expected to be "_template", other variants will be passed to the regular fastq-load.py
        read.SetChannel( re.GetMatch()[6] );
        read.SetNanoporeReadNo( re.GetMatch()[8] );

        poreMid = re.GetMatch()[7];
        //self.poreFile = poreStart + self.channel + poreMid + self.readNo + poreEnd
        re.GetMatch()[5].AppendToString( &poreFile ); //poreStart
        re.GetMatch()[6].AppendToString( &poreFile ); //channel
        re.GetMatch()[7].AppendToString( &poreFile ); //poreMid
        re.GetMatch()[8].AppendToString( &poreFile ); //readNo
        re.GetMatch()[9].AppendToString( &poreFile ); //poreEnd

        poreRead = re.GetMatch()[3];
        PostProcess( read );
    }
};

class CDefLineMatcherNanopore3_1 : public CDefLineMatcherNanoporeBase
{
public:
    CDefLineMatcherNanopore3_1() :
        CDefLineMatcherNanoporeBase(
            "Nanopore3_1",
            R"([@>+]([!-~]+?)[: ]?([!-~]+?Basecall)(_[12]D[_0]*?|_Alignment[_0]*?|_Barcoding[_0]*?|)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D|)[: ]([!-~]*?)[: ]?([!-~ ]+?_read_)(\d+)(_ch_)(\d+)(_strand\d*.fast5|_strand\d*.*)(\s+|$))"
        )
    {}

    virtual void GetMatch(CFastqRead& read) override
    {
//re.DumpMatch();
        // 0 prefix
        // 1 self.name
        // 2 self.suffix
        // 3 self.poreRead
        // 4 discard
        // 5 poreStart
        // 6 self.readNo
        // 7 poreMid
        // 8 self.channel
        // 9 poreEnd
        // 10 endSep

        read.SetSpot( re.GetMatch()[1] );
        read.SetSuffix( re.GetMatch()[2] );
        // For now, poreRead is expected to be "_template", other variants will be passed to the regular fastq-load.py
        read.SetNanoporeReadNo( re.GetMatch()[6] );
        read.SetChannel( re.GetMatch()[8] );

        poreMid = re.GetMatch()[7];
        // poreFile = poreStart + self.readNo + poreMid + self.channel + poreEnd
        re.GetMatch()[5].AppendToString( &poreFile ); //poreStart
        re.GetMatch()[6].AppendToString( &poreFile ); //readNo
        re.GetMatch()[7].AppendToString( &poreFile ); //poreMid
        re.GetMatch()[8].AppendToString( &poreFile ); //channel
        re.GetMatch()[9].AppendToString( &poreFile ); //poreEnd

        poreRead = re.GetMatch()[3];
        PostProcess( read );
    }
};

class CDefLineMatcherNanopore4 : public CDefLineMatcherNanoporeBase
{
public:
    CDefLineMatcherNanopore4() :
        CDefLineMatcherNanoporeBase(
            "Nanopore4",
            R"([@>+]([!-~]*?\S{8}-\S{4}-\S{4}-\S{4}-\S{12}\S*[_]?\d?)[\s+[!-~ ]*?|]$)"
        ),
        getPoreReadNo( R"(read[=_]?(\d+))" ),
        getPoreChannel( R"(ch[=_]?(\d+))" ),
        getPoreBarcode( R"(barcode=(\S+))" )
    {}

    virtual void GetMatch(CFastqRead& read) override
    {
        // 0 self.name
        read.SetSpot( re.GetMatch()[0] );

        if ( getPoreReadNo.Matches(re.GetLastInput()) )
        {
            read.SetNanoporeReadNo( getPoreReadNo.GetMatch()[0] );
        }

        if ( getPoreChannel.Matches(re.GetLastInput()) )
        {
            read.SetChannel( getPoreChannel.GetMatch()[0] );
        }

        const string Unclassified = string("unclassified");
        if ( getPoreBarcode.Matches(re.GetLastInput()) &&
             getPoreBarcode.GetMatch()[0].as_string() != Unclassified )
        {
            read.SetSpotGroup( getPoreBarcode.GetMatch()[0] );
        }

        PostProcess( read );
    }

    CRegExprMatcher getPoreReadNo;
    CRegExprMatcher getPoreChannel;
    CRegExprMatcher getPoreBarcode;
};

class CDefLineMatcherNanopore5 : public CDefLineMatcherNanoporeBase
{
public:
    CDefLineMatcherNanopore5() :
        CDefLineMatcherNanoporeBase(
            "Nanopore5",
            R"([@>+]([!-~]*?[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}_Basecall)(_[12]D[_0]*?|_Alignment[_0]*?|_Barcoding[_0]*?)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D)\S*?$)"
        )
    {}

    virtual void GetMatch(CFastqRead& read) override
    {
        // 0 self.name
        // 1 self.suffix
        // 2 self.poreRead    }
        read.SetSpot( re.GetMatch()[0] );
        read.SetSuffix( re.GetMatch()[1] );
        // For now, poreRead is expected to be "_template", other variants will be passed to the regular fastq-load.py

        PostProcess( read );
    }
};

// LS454
class CDefLineMatcherLS454 : public CDefLineMatcher
/// LS454 matcher
{
public:
    CDefLineMatcherLS454() :
        CDefLineMatcher(
            "LS454",
            R"(^[@>+]([!-~]+_|)([A-Z0-9]{7})(\d{2})([A-Z0-9]{5})(/[12345])?(\s+|$))")
    {
    }
    uint8_t GetPlatform() const override {
        return SRA_PLATFORM_454;
    };

    virtual void GetMatch(CFastqRead& read) override
    {
        // prefix, dateAndHash454, region454, xy454, readNum, endSep
        // 0       1               2          3      4        5
        m_tmp_spot.clear();
        re.GetMatch()[0].AppendToString(&m_tmp_spot); //prefix
        re.GetMatch()[1].AppendToString(&m_tmp_spot); //dateAndHash454
        re.GetMatch()[2].AppendToString(&m_tmp_spot); //region454
        re.GetMatch()[3].AppendToString(&m_tmp_spot); //xy454
        read.MoveSpot(move(m_tmp_spot));
        auto& readNo = re.GetMatch()[4];
        if (!readNo.empty()) {
            readNo.remove_prefix(1);
            read.SetReadNum(readNo);
        }
    }
};

/*
    self.ionTorrent = re.compile(r"[@>+]([A-Z0-9]{5})(:)(\d{1,5})(:)(\d{1,5})([^#/\s]*)(#[!-~]*?|)(/[12345]|\\[12345]|[LR])?(\s+|$)")
    self.ionTorrent2 = re.compile(r"[@>+]([A-Z0-9]{5})(:)(\d{1,5})(:)(\d{1,5})([!-~]*)(\s+|[_|])([12345]|):([NY]):(\d+):?([!-~]*?)(\s+|$)")
*/
class CDefLineMatcherIonTorrent : public CDefLineMatcher
/// ION_TORRENT
{
public:
    CDefLineMatcherIonTorrent() :
        CDefLineMatcher(
            "IonTorrent",
            R"(^[@>+]([A-Z0-9]{5})(:)(\d{1,5})(:)(\d{1,5})([^#/\s]*)(#[!-~]*?|)(/[12345]|\\[12345]|[LR])?(\s+|$))")
    {
    }
    uint8_t GetPlatform() const override {
        return SRA_PLATFORM_ION_TORRENT;
    };

    virtual void GetMatch(CFastqRead& read) override
    {
        // runId, sep1, row, sep2, column, readNum, endSep
        // runId, sep1, row, sep2, column, suffix, spotGroup, readNum, endSep
        // 0      1     2    3     4       5       6          7        8 
        m_tmp_spot.clear();

        re.GetMatch()[0].AppendToString(&m_tmp_spot); //runId
        re.GetMatch()[1].AppendToString(&m_tmp_spot); //sep1
        re.GetMatch()[2].AppendToString(&m_tmp_spot); //row
        re.GetMatch()[3].AppendToString(&m_tmp_spot); //sep2
        re.GetMatch()[4].AppendToString(&m_tmp_spot); //column
        read.MoveSpot(move(m_tmp_spot));
        auto& suffix = re.GetMatch()[5];

        auto& spotGroup = re.GetMatch()[6];
        if (!spotGroup.empty()) 
            read.SetSpotGroup(spotGroup);            

        auto& readNum = re.GetMatch()[7];
        static const re2::StringPiece readNum1{"1"};
        static const re2::StringPiece readNum2{"2"};

        if (readNum.empty() && !suffix.empty() && (suffix[0] == 'L' || suffix[0] == 'R')) {
            assert(suffix.size() == 1 && (suffix[0] == 'L' || suffix[0] == 'R'));
            read.SetReadNum( suffix == "L" ? readNum1 : readNum2 );
        } else {
            read.SetSuffix(suffix);
            if (readNum.starts_with("/") || readNum.starts_with("\\")) 
                readNum.remove_prefix(1);
            else if (readNum == "L")
                readNum = "1";
            else if (readNum == "R")
                readNum = "2";
            read.SetReadNum(readNum);
        }
    }
};


class CDefLineMatcherIonTorrent2 : public CDefLineMatcher
/// ION_TORRENT
{
public:
    CDefLineMatcherIonTorrent2() :
        CDefLineMatcher(
            "IonTorrent2",
            R"(^[@>+]([A-Z0-9]{5})(:)(\d{1,5})(:)(\d{1,5})([!-~]*)(\s+|[_|])([12345]|):([NY]):(\d+):?([!-~]*?)(\s+|$))")
    {
    }
    uint8_t GetPlatform() const override {
        return SRA_PLATFORM_ION_TORRENT;
    };

    virtual void GetMatch(CFastqRead& read) override
    {
        // runId, sep1, row, sep2, column, readNum, filterRead, reserved, spotGroup, endSep
        // 0      1     2    3     4       5        6           7         8          9 

        // runId, sep1, row, sep2, column, suffix, sep3, readNum, filterRead, reserved, suffix, spotGroup, endSep
        // 0      1     2    3     4       5       6     7         8          9         10      11         12     


        m_tmp_spot.clear();

        re.GetMatch()[0].AppendToString(&m_tmp_spot); //runId
        re.GetMatch()[1].AppendToString(&m_tmp_spot); //sep1
        re.GetMatch()[2].AppendToString(&m_tmp_spot); //row
        re.GetMatch()[3].AppendToString(&m_tmp_spot); //sep2
        re.GetMatch()[4].AppendToString(&m_tmp_spot); //column
        read.MoveSpot(move(m_tmp_spot));

        read.SetSuffix(re.GetMatch()[5]);

        read.SetReadNum(re.GetMatch()[7]);

        read.SetReadFilter(re.GetMatch()[8] == "Y" ? 1 : 0);

        read.SetSpotGroup(re.GetMatch()[11]);
    }
};


class CDefLineMatcherPacBio : public CDefLineMatcher
/// PacBio
{
public:
    CDefLineMatcherPacBio() :
        CDefLineMatcher(
            "PacBio",
            R"(^[@>+](m\d{5,6}_\d{6}_[!-~]+?_c\d{33}_s\d+_[pX]\d/\d+/?\d*_?\d*|m\d{6}_\d{6}_[!-~]+?_c\d{33}_s\d+_[pX]\d[|/]\d+[|/]ccs[!-~]*?)(\s+|$))")
    {
    }
    CDefLineMatcherPacBio(const string& defLineName, const string& pattern) :
        CDefLineMatcher(defLineName, pattern)
    {
    }

    uint8_t GetPlatform() const override {
        return SRA_PLATFORM_PACBIO_SMRT;
    };

    virtual void GetMatch(CFastqRead& read) override
    {
        // Name
        // 0   

        m_tmp_spot.clear();

        re.GetMatch()[0].AppendToString(&m_tmp_spot); // name
        read.MoveSpot(move(m_tmp_spot));
    }
};

class CDefLineMatcherPacBio2 : public CDefLineMatcherPacBio
/// PacBio2
{
public:
    CDefLineMatcherPacBio2() :
        CDefLineMatcherPacBio(
            "PacBio2",
            R"(^[@>+]([!-~]*?m\d{5,6}\S{0,3}_\d{6}_\d{6}[/_]\d+[!-~]*?)(\s+|$))")
    {
    }
};


class CDefLineMatcherPacBio3 : public CDefLineMatcherPacBio
/// PacBio3
{
public:
    CDefLineMatcherPacBio3() :
        CDefLineMatcherPacBio(
            "PacBio3",
            R"(^[@>+]([!-~]*?m\d{5,6}\S{0,3}_\d{6}_\d{6}[/_]\d+/ccs[!-~]*?)(\s+|$))")
    {
    }
};

class CDefLineMatcherPacBio4 : public CDefLineMatcherPacBio
/// PacBio4
{
public:
    CDefLineMatcherPacBio4() :
        CDefLineMatcherPacBio(
            "PacBio4",
            R"(^[@>+]([!-~]*?m\d{5,6}\S{0,3}_\d{6}_\d{6}[/_]\d+/\d+_\d+[!-~]*?)(\s+|$))")
    {
    }
};


//self.illuminaOldBcRnOnly = re.compile(r"^[@>+]([!-~]+?)(#[!-~]+?)(/[12345]|\\[12345])(\s+|$)")

class CDefLineIlluminaOldBcRn : public CDefLineMatcher
/// IlluminaOld BarCode and ReadNum only 
{
public:
    CDefLineIlluminaOldBcRn() :
        CDefLineMatcher(
            "illuminaOldBcRnOnly",
            R"(^[@>+]([!-~]+?)(#[!-~]+?)(/[1234]|\\[1234])(\s+|$))")
    {
    }
    CDefLineIlluminaOldBcRn(const string& defLineName, const string& pattern) :
        CDefLineMatcher(defLineName, pattern)
    {
    }

    uint8_t GetPlatform() const override {
        return SRA_PLATFORM_UNDEFINED;
    };

    virtual void GetMatch(CFastqRead& read) override
    {
        // Name SpotGroup ReadNum endSep
        // 0    1         2       3

        m_tmp_spot.clear();

        re.GetMatch()[0].AppendToString(&m_tmp_spot); // name
        read.MoveSpot(move(m_tmp_spot));

        auto& spot_group = re.GetMatch()[1];
        if (spot_group.starts_with("#")) {
            spot_group.remove_prefix(1);
            read.SetSpotGroup(spot_group);
            auto& read_num = re.GetMatch()[2];
            auto sz = read_num.size();
            if ((sz > 0 && read_num[0] == '/') || (sz > 1 && read_num[0] == '\\')) {
                read_num.remove_prefix(1);
                read.SetReadNum(read_num);
            }
        } else {
            spot_group.remove_prefix(1);
            read.SetReadNum(spot_group);
        }
    }
};

//self.illuminaOldBcOnly = re.compile(r"^[@>+]([!-~]+?)(#[!-~]+)(\s+|$)(.?)")
class CDefLineIlluminaOldBcOnly : public CDefLineIlluminaOldBcRn
/// IlluminaOld BarCode only 
{
public:
    CDefLineIlluminaOldBcOnly() :
        CDefLineIlluminaOldBcRn(
            "illuminaOldBcOnly",
            R"(^[@>+]([!-~]+?)(#[!-~]+)(\s+|$)(.?))")
    {
    }
};

//self.illuminaOldRnOnly = re.compile(r"^[@>+]([!-~]+?)(/[12345]|\\[12345])(\s+|$)(.?)")
class CDefLineIlluminaOldRnOnly : public CDefLineIlluminaOldBcRn
/// IlluminaOld ReadNum only 
{
public:
    CDefLineIlluminaOldRnOnly() :
        CDefLineIlluminaOldBcRn(
            "illuminaOldRnOnly",
            R"(^[@>+]([!-~]+?)(/[1234]|\\[1234])(\s+|$)(.?))")
    {
    }
};


#endif
