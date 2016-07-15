#include <ngs/ncbi/NGS.hpp>
#include <ngs/ReadCollection.hpp>

#include <klib/out.h>
#include <kfc/defs.h>
#include <kapp/main.h>

#include <klib/rc.h>
#include <kfc/rc.h>

#include <sysalloc.h>
#include <algorithm>        /* min */

#include <string.h>         /* strcmp () */

#include <algorithm>

#include "args.hpp"
#include "filters.hpp"

#include "koutstream"

namespace ngs {


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/* Arguments                                                   */
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
class DumpArgs : public AArgs {
public :
    static const char * _sM_minSpotIdName;
    static const char * _sM_maxSpotIdName;
    static const char * _sM_spotIdName;
    static const char * _sM_minReadLengthName;
    static const char * _sM_categoryName;
    static const char * _sM_fastaName;
    static const char * _sM_legacyReportName;

    static const int64_t _sM_minSpotIdDefValue = 1;
    static const int64_t _sM_maxSpotIdDefValue = 0;

public :
    typedef AArgs PAPAHEN;
    typedef Read :: ReadCategory ReadCategory;

public :
    DumpArgs ();
    ~DumpArgs ();

    inline const String & accession () const
                { return _M_accession; };

    inline int64_t minSpotId () const
                { return _M_minSpotId; };
    inline int64_t maxSpotId () const
                { return _M_maxSpotId; };

    inline uint64_t minReadLength () const
                { return _M_minReadLength; };

    inline ReadCategory category () const
                { return _M_category; };

    inline bool fastaDump () const
                { return _M_fasta != 0; };

    inline uint64_t fastaDumpWidth () const
                { return _M_fasta; };

    inline bool legacyReport () const
                { return _M_legacyReport; };

protected :
    void __customInit ();
    void __customParse ();
    void __customDispose ();

private :
    String _M_accession;

        /*) Full Spot Fulters
         (*/
    int64_t _M_minSpotId;      /* -N | --minSpotId < rowid > */
    int64_t _M_maxSpotId;      /* -X | --maxSpotId < rowid > */
    uint64_t _M_minReadLength;  /* -M | --minReadLen <len> */
    ReadCategory _M_category;   /* -Y | --category */
    uint64_t _M_fasta;          /* -A | --fasta */
    bool _M_legacyReport;       /* -L | --legacy-report */
};

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/* DumpArgs                                                    */
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

const char * DumpArgs :: _sM_minSpotIdName = "minSpotId";
const char * DumpArgs :: _sM_maxSpotIdName = "maxSpotId";
const char * DumpArgs :: _sM_spotIdName = "spotId";
const char * DumpArgs :: _sM_minReadLengthName = "minReadLength";
const char * DumpArgs :: _sM_categoryName = "category";
const char * DumpArgs :: _sM_fastaName = "fasta";
const char * DumpArgs :: _sM_legacyReportName = "legacy-report";

DumpArgs :: DumpArgs ()
:   AArgs ()
,   _M_accession ( "" )
,   _M_minSpotId ( _sM_minSpotIdDefValue )
,   _M_maxSpotId ( _sM_maxSpotIdDefValue )
,   _M_minReadLength ( 0 )
,   _M_category ( Read :: all )
,   _M_fasta ( 0 )
,   _M_legacyReport ( false )
{
}   /* DumpArgs :: DumpArgs () */

DumpArgs :: ~DumpArgs ()
{
}   /* DumpArgs :: ~DumpArgs () */

void
DumpArgs :: __customInit ()
{
        /*) Here we are adding some extra options 
         (*/
    {
        AOptDef TheOpt;

        TheOpt . setName ( _sM_minSpotIdName );
        TheOpt . setAliases ( "N" );
        TheOpt . setParam ( "rowid" );
        TheOpt . setNeedValue ( true );
        TheOpt . setRequired ( false );
        TheOpt . setHlp ( "Minimum spot id" );
        TheOpt . setMaxCount ( 1 );

        addOpt ( TheOpt );
    }

    {
        AOptDef TheOpt;

        TheOpt . setName ( _sM_maxSpotIdName );
        TheOpt . setAliases ( "X" );
        TheOpt . setParam ( "rowid" );
        TheOpt . setNeedValue ( true );
        TheOpt . setRequired ( false );
        TheOpt . setHlp ( "Maximum spot id" );
        TheOpt . setMaxCount ( 1 );

        addOpt ( TheOpt );
    }

    {
        AOptDef TheOpt;

        TheOpt . setName ( _sM_spotIdName );
        TheOpt . setAliases ( "S" );
        TheOpt . setParam ( "rowid" );
        TheOpt . setNeedValue ( true );
        TheOpt . setRequired ( false );
        TheOpt . setHlp ( "Spot id" );
        TheOpt . setMaxCount ( 1 );

        addOpt ( TheOpt );
    }

    {
        AOptDef TheOpt;

        TheOpt . setName ( _sM_minReadLengthName );
        TheOpt . setAliases ( "M" );
        TheOpt . setParam ( "len" );
        TheOpt . setNeedValue ( true );
        TheOpt . setRequired ( false );
        TheOpt . setHlp ( "Filter by sequence length >= <len>" );
        TheOpt . setMaxCount ( 1 );

        addOpt ( TheOpt );
    }

    {
        AOptDef TheOpt;

        TheOpt . setName ( _sM_categoryName );
        TheOpt . setAliases ( "Y" );
        TheOpt . setParam ( "alignment" );
        TheOpt . setNeedValue ( true );
        TheOpt . setRequired ( false );
        TheOpt . setHlp ( "Reads to dump. Accepts these values : <fullyAligned>, <partiallyAligned>, <aligned>, <unaligned>, <all>. Optional, default value <all> " );
        TheOpt . setMaxCount ( 1 );

        addOpt ( TheOpt );
    }
    
    {
        AOptDef TheOpt;

        TheOpt . setName ( _sM_fastaName );
        TheOpt . setAliases ( "A" );
        TheOpt . setParam ( "width" );
        TheOpt . setNeedValue ( true );
        TheOpt . setRequired ( false );
        TheOpt . setHlp ( "FASTA only, no qualities, optional line wrap width (set to zero for no wrapping)" );
        TheOpt . setMaxCount ( 1 );

        addOpt ( TheOpt );
    }

    {
        AOptDef TheOpt;

        TheOpt . setName ( _sM_legacyReportName );
        TheOpt . setAliases ( "R" );
        TheOpt . setNeedValue ( false );
        TheOpt . setRequired ( false );
        TheOpt . setHlp ( "Use legacy style 'Written spots' for tool" );
        TheOpt . setMaxCount ( 1 );

        addOpt ( TheOpt );
    }
}   /* DumpArgs :: __customInit () */

void
DumpArgs :: __customDispose ()
{
    _M_accession = "";
    _M_minSpotId = _sM_minSpotIdDefValue;
    _M_maxSpotId = _sM_maxSpotIdDefValue;
    _M_minReadLength = 0;
    _M_category = Read :: all;
    _M_fasta = 0;
    _M_legacyReport = false;
}   /* DumpArgs :: __customDispose () */

void
DumpArgs :: __customParse ()
{
    if ( ! good () ) {
        throw ErrorMsg ( "__customParse::reset: Not good" );
    }

    uint32_t cnt = parVal () . valCount ();
    if ( cnt == 0 ) {
        throw ErrorMsg ( "__customParse: Too few paramters" );
    }

    _M_accession = parVal () . val ( 0 );

    if ( _M_accession . empty () ) {
        std :: cerr << "ERROR: <accession> is not defined" << std :: endl;
        throw ErrorMsg ( "__custromParse: ERROR: <accession> is not defined" );
    }

    AOptVal optV = optVal ( _sM_spotIdName );
    int64_t __spotId = 0;

    if ( optV . exist () ) {
        if ( optVal ( _sM_minSpotIdName ) . exist () ) {
            throw ErrorMsg ( String ( "__custromParse: ERROR: parameter \"" ) + _sM_spotIdName + "\" can not coexists with parameter \"" + _sM_minSpotIdName + "\"");
        }

        if ( optVal ( _sM_maxSpotIdName ) . exist () ) {
            throw ErrorMsg ( String ( "__custromParse: ERROR: parameter \"" ) + _sM_spotIdName + "\" can not coexists with parameter \"" + _sM_maxSpotIdName + "\"");
        }

        if ( optV . valCount () != 1 ) {
            throw ErrorMsg ( String ( "__custromParse: ERROR: Too many \"" ) + _sM_spotIdName + "\" values");
        }

        __spotId = optV . int64Val ();
    }

    optV = optVal ( _sM_minSpotIdName );
    if ( optV . exist () ) {
        if ( optV . valCount () != 1 ) {
            throw ErrorMsg ( String ( "__custromParse: ERROR: Too many \"" ) + _sM_minSpotIdName + "\" values");
        }

        _M_minSpotId = optV . int64Val ();
    }

    optV = optVal ( _sM_maxSpotIdName );
    if ( optV . exist () ) {
        if ( optV . valCount () != 1 ) {
            throw ErrorMsg ( String ( "__custromParse: ERROR: Too many \"" ) + _sM_maxSpotIdName + "\" values");
        }

        _M_maxSpotId = optV . int64Val ();
    }

    if ( __spotId != 0 ) {
        _M_minSpotId = __spotId;
        _M_maxSpotId = __spotId;
    }

    optV = optVal ( _sM_minReadLengthName );
    if ( optV . exist () ) {
        if ( optV . valCount () != 1 ) {
            throw ErrorMsg ( String ( "__custromParse: ERROR: Too many \"" ) + _sM_minReadLengthName + "\" values");
        }

        _M_minReadLength = optV . uint64Val ();
    }

    _M_category = Read :: all;
    optV = optVal ( _sM_categoryName );
    if ( optV . exist () ) {
        if ( optV . valCount () != 1 ) {
            throw ErrorMsg ( String ( "__custromParse: ERROR: Too many \"" ) + _sM_categoryName + "\" values");
        }

        String __v = optV . val ();

        while ( true ) {
            if ( __v == "fullyAligned" ) {
                _M_category = Read :: fullyAligned;
                break;
            }

            if ( __v == "partiallyAligned" ) {
                _M_category = Read :: partiallyAligned;
                break;
            }

            if ( __v == "aligned" ) {
                _M_category = Read :: aligned;
                break;
            }

            if ( __v == "unaligned" ) {
                _M_category = Read :: unaligned;
                break;
            }

            if ( __v == "all" ) {
                _M_category = Read :: all;
                break;
            }

            throw ErrorMsg ( String ( "__customParse: ERROR: Invalid value \"" + __v + "\" for option \"" + _sM_categoryName + "\"" ) );
        }
    }

    _M_fasta = 0;
    optV = optVal ( _sM_fastaName );
    if ( optV . exist () ) {
        if ( optV . valCount () != 1 ) {
            throw ErrorMsg ( String ( "__custromParse: ERROR: Too many \"" ) + _sM_fastaName + "\" values");
        }

        _M_fasta = optV . uint64Val ();
    }

    _M_legacyReport = optVal ( _sM_legacyReportName ) . exist ();

}   /* DumpArgs :: __customParse () */

}; /* namespace ngs */

/*))
 //  KMain, and all other
((*/

using namespace ngs;

const char UsageDefaultName[] = "fq-d";

rc_t CC
UsageSummary ( const char * progname )
{
        /*) Standard usage of that program
         (*/
    return KOutMsg (
                "\n"
                "Usage:\n"
                "  %s [Options] <Accession>\n"
                "\n"
                "\n"
                "Use option --help for more information\n"
                "\n",
                progname
                );
}

rc_t CC
Usage ( const Args * a )
{
    if ( a == NULL ) {
        throw ngs :: ErrorMsg ( "Usage: NULL args passed" );
    }

    const char * prog;

    if ( ArgsProgram ( a, NULL, & prog ) != 0 ) {
        throw ngs :: ErrorMsg ( "Usage: Infalid args passed" );
    }

    UsageSummary ( prog );

    KOutMsg ( "OPTIONS:\n" );

    HelpOptionsStandard ();

    AArgs * TheArgs = AArgs :: __getArgs ( const_cast < Args * > ( a ) );

    if ( TheArgs == NULL ) {
        throw ngs :: ErrorMsg ( "Usage: Using unregistered usage" );
    }

    size_t c = TheArgs -> optDefCount ();
    for ( size_t i = 0; i < c; i ++ ) {
        const AOptDef & o = TheArgs -> optDef ( i );

        const char * c [] = { NULL, NULL };
        c [ 0 ] = o . getHlp ();

        HelpOptionLine (
                        o . getAliases (),
                        o . getName (),
                        o . getParam (),
                        c
                        );

    }

    return 0;
}

static void run ( const DumpArgs & TheArgs );

rc_t
KMain ( int argc, char * argv [] )
{
    try {
        DumpArgs TheArgs;

        try {
            TheArgs . init ( true );
            TheArgs . parse ( argc, argv );
        }
        catch ( ... ) {
            UsageSummary ( TheArgs . prog () . c_str () );
            throw;
        }

        if ( ! TheArgs . good () ) {
            TheArgs . usage ();

            return 1;
        }

        run ( TheArgs );

        TheArgs . dispose ();
    }
    catch ( std :: exception & E ) {
        std :: cerr << "Exception handled '" << E.what () << "'" << std :: endl;

        return 1;
    }
    catch ( ... ) {
        std :: cerr << "UNKNOWN exception handled" << std :: endl;

        return 2;
    }

    return 0;
}   /* KMain () */

static
void
setupFilters ( AFilters & Filters, const DumpArgs & TheArgs )
{
    if ( TheArgs . minReadLength () != 0 ) {
        Filters . addLengthFilter ( TheArgs . minReadLength () );
    }
}   /* setupFilters () */

static
void
dumpFastQ (
        int64_t SpotId,
        const ngs :: String & CollectionName,
        const ReadIterator & Iterator
)
{
        /*)  We do not check values for arguments validity!
         (*/
    StringRef ReadName = Iterator . getReadName ();
    StringRef Bases = Iterator . getReadBases ();
    StringRef Qualities = Iterator . getReadQualities ();

        /*)  First, we are doint base header
         (*/
    kout << "@"
        << CollectionName
        << '.'
        << SpotId
        << ' '
        << ReadName
        << " length="
        << Bases . size ()
        << '\n'
        ;

        /*)  Second is going base itsefl
         (*/
    kout << Bases
        << '\n'
        ;

        /*)  Third, header for qualities
         (*/
    kout << '+'
        << CollectionName
        << '.'
        << SpotId
        << ' '
        << ReadName
        << " length="
        << Qualities . size ()
        << '\n'
        ;

        /*)  Finally there are qualities
         (*/
    kout << Qualities
        << '\n'
        ;
}   /* dumpFastQ () */

static
void
dumpFastA (
        int64_t SpotId,
        const ngs :: String & CollectionName,
        const ReadIterator & Iterator,
        uint64_t Width
)
{
        /*)  We do not check values for arguments validity!
         (*/
    StringRef ReadName = Iterator . getReadName ();
    StringRef Bases = Iterator . getReadBases ();

    uint64_t __l = Bases . size ();

        /*)  First, we are doing base header
         (*/
    kout << '>'
        << CollectionName
        << '.'
        << SpotId
        << ' '
        << ReadName
        << " length="
        << __l
        << '\n'
        ;

        /*)  Second is going base itself by width
         (*/

    uint64_t __p = 0;
    const char * __s = Bases . data ();


    if ( 0 < Width ) {
        while ( __p < __l ) {
            uint64_t __t = std :: min ( Width, __l - __p );

            kout << std :: string ( __s, ( std :: string :: size_type ) __p, ( std :: string :: size_type ) __t ) << "\n" ;

            __p += __t;
        }
    }
    else {
        kout << __s << "\n" ;
    }

}   /* dumpFastA () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

static
void
run ( const DumpArgs & TheArgs )
{
    if ( ! TheArgs . good () ) {
        throw ErrorMsg ( "Invalid arguments" );
    }

    ngs :: String Acc ( TheArgs . accession () . c_str () );
    ReadCollection RCol = ncbi :: NGS :: openReadCollection ( Acc );

    int64_t minSpot = TheArgs . minSpotId ();
    int64_t maxSpot = TheArgs . maxSpotId ();

    if ( minSpot == 0 ) {
        minSpot = 1;
    }

    if ( maxSpot == 0 ) {
        maxSpot = RCol . getReadCount ();
    }

    if ( maxSpot < minSpot ) {
        int64_t Id = minSpot;
        minSpot = maxSpot;
        maxSpot = Id;
    }

    ReadIterator Iterator = RCol.getReadRange (
                                            minSpot,
                                            maxSpot - minSpot + 1,
                                            TheArgs . category ()
                                            );

    ngs :: String ReadCollectionName = RCol.getName ();

    AFilters Filters ( TheArgs . accession () );
    setupFilters ( Filters, TheArgs );

    for ( int64_t llp = TheArgs . minSpotId () ; Iterator.nextRead (); llp ++ ) {

        if ( Filters . checkIt ( Iterator ) ) {
            if ( TheArgs . fastaDump () ) {
                dumpFastA ( llp, ReadCollectionName, Iterator, TheArgs . fastaDumpWidth () );
            }
            else { 
                dumpFastQ ( llp, ReadCollectionName, Iterator );
            }
        }
    }

    kout.flush ();

    std :: cerr << Filters . report ( TheArgs . legacyReport () );

}   /* run () */

