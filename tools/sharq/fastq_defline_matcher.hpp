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
        if (!re.GetMatch()[0].empty()) {
            re.GetMatch()[0].CopyToString(&spot);
            spot.append(1, (re.GetMatch()[1][0] == '_') ? ':' : re.GetMatch()[1][0]);
        }
        re.GetMatch()[2].AppendToString(&spot); //lane
        spot.append(1, (re.GetMatch()[3][0] == '_') ? ':' : re.GetMatch()[3][0]);
        re.GetMatch()[4].AppendToString(&spot); //tile
        spot.append(1, (re.GetMatch()[5][0] == '_') ? ':' : re.GetMatch()[5][0]);
        re.GetMatch()[6].AppendToString(&spot); //x
        spot.append(1, (re.GetMatch()[7][0] == '_') ? ':' : re.GetMatch()[7][0]);
        re.GetMatch()[8].AppendToString(&spot); //y
        read.SetSpot(spot);

        read.SetReadNum(re.GetMatch()[10]);

        read.SetReadFilter(re.GetMatch()[11] == "Y" ? 1 : 0);

        read.SetSpotGroup(re.GetMatch()[13]);
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
            "^[@>+]([!-~]+)([:_])(\\d+)([:_])(\\d+)([:_])(-?\\d+\\.?\\d*)([:_])(-?\\d+\\.\\d+|\\d+)([!-~]+?\\s+|[!-~]+?[:_|-])([12345]|):([NY]):(\\d+|O):?([!-~]*?)(\\s+|$)"),
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
        re.GetMatch()[0].AppendToString(&spot); //flowcell
        re.GetMatch()[1].AppendToString(&spot); //lane
        re.GetMatch()[2].AppendToString(&spot); //column
        re.GetMatch()[3].AppendToString(&spot); //row
        re.GetMatch()[4].AppendToString(&spot); // readNo
        read.SetSpot(spot);

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
        re.GetMatch()[0].AppendToString(&spot); //flowcell
        re.GetMatch()[1].AppendToString(&spot); //lane
        re.GetMatch()[2].AppendToString(&spot); //column
        re.GetMatch()[3].AppendToString(&spot); //row
        re.GetMatch()[4].AppendToString(&spot); // readNo
        read.SetSpot(spot);

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

        string spot;
        if ( !re.GetMatch()[3].empty() )
        {   // self.name = poreStart + self.channel + poreMid + self.readNo + poreEnd
            re.GetMatch()[0].AppendToString(&spot); //poreStart
            re.GetMatch()[1].AppendToString(&spot); //channel
            re.GetMatch()[2].AppendToString(&spot); //poreMid
            re.GetMatch()[3].AppendToString(&spot); //readNo
            re.GetMatch()[4].AppendToString(&spot); //poreEnd

            read.SetNanoporeReadNo( re.GetMatch()[3] );
        }
        else
        {   // self.name = poreStart + self.channel + poreEnd
            re.GetMatch()[0].AppendToString(&spot); //poreStart
            re.GetMatch()[1].AppendToString(&spot); //channel
            re.GetMatch()[4].AppendToString(&spot); //poreEnd

            if ( getPoreReadNo2.Matches(re.GetLastInput()) )
            {
                read.SetNanoporeReadNo( getPoreReadNo2.GetMatch()[0] );
            }
        }
        read.SetSpot(spot);

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
        read.SetNanoporeReadNo( re.GetMatch()[2] );
    }
};

#endif
