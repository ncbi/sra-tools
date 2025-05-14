/*===========================================================================
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
* =========================================================================== */

#include <kapp/main.h>

#include <kfs/file.h> /* KFileRelease */

#include <klib/log.h> /* PLOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/rc.h> /* RC */
#include <klib/status.h> /* STSMSG */

#include <kns/http.h> /* KNSManagerMakeHttpFile */
#include <kns/manager.h> /* KNSManagerRelease */
#include <kns/stream.h> /* KStreamRelease */

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)

#define DEFAULT_BLOCK_SIZE "512K"
#define BLOCK_ALIAS  "s"
#define BLOCK_OPTION "block-size"
static const char * BLOCK_USAGE [] = { "Bytes per block.",
    "Default: " DEFAULT_BLOCK_SIZE, NULL };

#define FUNCT_ALIAS  "K"
#define FUNCT_OPTION "function"
static const char * FUNCT_USAGE [] = {
    "Function to perform: FileRead or StreamRead. Default: StreamRead", NULL };

#define CHUNK_ALIAS  "n"
#define CHUNK_OPTION "chunks"
static const char * CHUNK_USAGE [] = {
    "Split file to specifies number of chunks", NULL };

#define MX_CH_ALIAS  "x"
#define MX_CH_OPTION "max-chunk"
static const char * MX_CH_USAGE [] = {
    "Retrieve specifies number of chunks", NULL };

#define MINIM_ALIAS  "N"
#define MINIM_OPTION "min"
static const char * MINIM_USAGE [] = {
    "Retrieve a byte range from specified value. Default: 0", NULL };

#define MAXIM_ALIAS  "X"
#define MAXIM_OPTION "max"
static const char * MAXIM_USAGE [] = {
    "Retrieve a byte range to specified value. Default: not specified", NULL };

#define PRINT_ALIAS  "p"
#define PRINT_OPTION "print"
static const char * PRINT_USAGE [] = {
    "Print output. Don't print if not specified", NULL };

static OptDef Options [] = {
/*        name         alias     fkt    usage-txt cnt needs value required */
    { FUNCT_OPTION, FUNCT_ALIAS, NULL, FUNCT_USAGE, 1, true     , false },
    { BLOCK_OPTION, BLOCK_ALIAS, NULL, BLOCK_USAGE, 1, true     , false },
    { MINIM_OPTION, MINIM_ALIAS, NULL, MINIM_USAGE, 1, true     , false },
    { MAXIM_OPTION, MAXIM_ALIAS, NULL, MAXIM_USAGE, 1, true     , false },
    { CHUNK_OPTION, CHUNK_ALIAS, NULL, CHUNK_USAGE, 1, true     , false },
    { MX_CH_OPTION, MX_CH_ALIAS, NULL, MX_CH_USAGE, 1, true     , false },
    { PRINT_OPTION, PRINT_ALIAS, NULL, PRINT_USAGE, 1, false    , false },
};

const char UsageDefaultName[] = "download";

rc_t CC UsageSummary ( const char * progname ) {
    return OUTMSG ( (
        "Usage:\n"
        "  %s [options] <URL> [...]\n"
        "  Download URL to stdout\n"
        , progname ) );
}

rc_t CC Usage ( const Args * args ) {
    rc_t rc = 0;
    int i = 0;

    const char *progname = UsageDefaultName;
    const char *fullpath = UsageDefaultName;

    if (args == NULL)
        rc = RC(rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram(args, &fullpath, &progname);

    if (rc != 0)
        progname = fullpath = UsageDefaultName;

    UsageSummary(progname);

    OUTMSG(("\n"));

    OUTMSG(("Options:\n"));

    for ( i = 0; i < sizeof Options / sizeof Options [ 0 ]; ++ i ) {
        const char * param = "value";
        assert ( Options [ i ] . aliases );
        if ( Options [ i ] . aliases [ 0 ] == CHUNK_ALIAS [ 0 ]
          || Options [ i ] . aliases [ 0 ] == MX_CH_ALIAS [ 0 ] )
            param = "number";
        else if ( Options [ i ] . aliases [ 0 ] == PRINT_ALIAS [ 0 ] )
            param = NULL;
        HelpOptionLine ( Options [ i ] . aliases, Options [ i ] . name,
                         param,                   Options [ i ] . help );
    }

    OUTMSG(("\n"));

    HelpOptionsStandard();

    HelpVersion(fullpath, KAppVersion());

    return rc;
}

typedef struct {
    bool useFile;

    char * buffer;
    size_t allocated;
    size_t bufSize;
    size_t fileSize;
    uint64_t chunks;
    uint64_t chunkCount;

    uint64_t min;
    uint64_t max;

    bool print;

    KNSManager * mgr;

    const KFile * file;
} Do;

static size_t _sizeFromString ( const char * val ) {
    size_t s = 0;
    size_t o = 1;

    assert ( val );

    for ( s = 0; * val != '\0'; ++ val ) {
        if ( * val < '0' || * val > '9' )
            break;
        s = s * 10 + * val - '0';
    }

    if ( * val == '.' ) {
        size_t d = 0;
        ++ val;
        for ( d = 0; * val != '\0'; ++ val ) {
            if ( * val < '0' || * val > '9' )
                break;
            d = d * 10 + * val - '0';
            o *= 10;
        }
        s = s * o + d;
    }

    if      ( * val == 'k' || * val == 'K')
        s *= 1024L;
    else if ( * val == 'b' || * val == 'B')
        ;
    else if ( * val == 'm' || * val == 'M')
        s *= 1024L * 1024;
    else if ( * val == 'g' || * val == 'G')
        s *= 1024L * 1024 * 1024;
    else if ( * val == 't' || * val == 'T')
        s *= 1024L * 1024 * 1024 * 1024;

    s /= o;

    return s;
}

static
rc_t DoArgs ( Do * self, Args ** args, int argc, char * argv [] )
{
    rc_t rc = 0;
    assert ( self && args );
    rc = ArgsMakeAndHandle ( args, argc, argv, 1,
                             Options, sizeof Options / sizeof Options [ 0 ] );
    if ( rc == 0 ) do {
        uint32_t pcount = 0;

        assert ( args );

/* FUNCT_OPTION */
        {
            const char * val = "s";
            rc = ArgsOptionCount ( * args, FUNCT_OPTION, & pcount );
            if ( rc != 0 ) {
                LOGERR ( klogInt, rc,
                         "Failure to get '" FUNCT_OPTION "' argument");
                break;
            }
            if ( pcount > 0 ) {
                rc = ArgsOptionValue
                    ( * args, FUNCT_OPTION, 0, ( const void ** ) & val );
                if ( rc != 0 ) {
                    LOGERR ( klogInt, rc, "Failure to get '" FUNCT_OPTION
                                          "' argument value" );
                    break;
                }
            }
            assert ( val && val [ 0 ] );
            self -> useFile = false;
            switch ( val [ 0 ] ) {
                case 'f':
                case 'F': self -> useFile = true;
                          break;
                default:  break;
            }
        }

/* BLOCK_OPTION */
        {
            const char * val = DEFAULT_BLOCK_SIZE;
            rc = ArgsOptionCount ( * args, BLOCK_OPTION, & pcount );
            if ( rc != 0 ) {
                LOGERR ( klogInt, rc,
                         "Failure to get '" BLOCK_OPTION "' argument");
                break;
            }
            if ( pcount > 0 ) {
                rc = ArgsOptionValue
                    ( * args, BLOCK_OPTION, 0, ( const void ** ) & val );
                if ( rc != 0 ) {
                    LOGERR ( klogInt, rc, "Failure to get '" BLOCK_OPTION
                                          "' argument value" );
                    break;
                }
            }
            self -> bufSize = _sizeFromString ( val );
            if ( self -> bufSize == 0) {
                rc = RC ( rcExe, rcArgv, rcParsing, rcParam, rcInvalid );
                LOGERR ( klogErr, rc, "Invalid block size" );
                break;
            }
        }

/* MINIM_OPTION */
        {
            const char * val = NULL;
            rc = ArgsOptionCount ( * args, MINIM_OPTION, & pcount );
            if ( rc != 0 ) {
                LOGERR ( klogInt, rc,
                         "Failure to get '" MINIM_OPTION "' argument");
                break;
            }
            if ( pcount > 0 ) {
                uint64_t n = 0;
                int i = 0;
                rc = ArgsOptionValue
                    ( * args, MINIM_OPTION, 0, ( const void ** ) & val );
                if ( rc != 0 ) {
                    LOGERR ( klogInt, rc, "Failure to get '" MINIM_OPTION
                                          "' argument value" );
                    break;
                }
                assert ( val );
                for ( i = 0; val [ i ] != '\0'; ++ i ) {
                    if ( val [ i ] < '0' || val [ i ] > '9' ) {
                        rc = RC ( rcExe, rcArgv, rcParsing,
                                  rcParam, rcInvalid );
                        LOGERR ( klogErr, rc, "Invalid " MINIM_OPTION
                                              " value" );
                        break;
                    }
                    n = n * 10 + val [ i ] - '0';
                }
                self -> min = n;
            }
        }

/* MAXIM_OPTION */
        {
            const char * val = NULL;
            rc = ArgsOptionCount ( * args, MAXIM_OPTION, & pcount );
            if ( rc != 0 ) {
                LOGERR ( klogInt, rc,
                         "Failure to get '" MAXIM_OPTION "' argument");
                break;
            }
            if ( pcount > 0 ) {
                uint64_t n = 0;
                int i = 0;
                rc = ArgsOptionValue
                    ( * args, MAXIM_OPTION, 0, ( const void ** ) & val );
                if ( rc != 0 ) {
                    LOGERR ( klogInt, rc, "Failure to get '" MINIM_OPTION
                                          "' argument value" );
                    break;
                }
                assert ( val );
                for ( i = 0; val [ i ] != '\0'; ++ i ) {
                    if ( val [ i ] < '0' || val [ i ] > '9' ) {
                        rc = RC ( rcExe, rcArgv, rcParsing,
                                  rcParam, rcInvalid );
                        LOGERR ( klogErr, rc,
                                 "Invalid " MINIM_OPTION " value" );
                        break;
                    }
                    n = n * 10 + val [ i ] - '0';
                }
                self -> max = n;
            }
        }

/* CHUNK_OPTION */
        {
            const char * val = NULL;
            rc = ArgsOptionCount ( * args, CHUNK_OPTION, & pcount );
            if ( rc != 0 ) {
                LOGERR ( klogInt, rc,
                         "Failure to get '" CHUNK_OPTION "' argument");
                break;
            }
            if ( pcount > 0 ) {
                uint64_t n = 0;
                int i = 0;
                rc = ArgsOptionValue
                    ( * args, CHUNK_OPTION, 0, ( const void ** ) & val );
                if ( rc != 0 ) {
                    LOGERR ( klogInt, rc, "Failure to get '" CHUNK_OPTION
                                          "' argument value" );
                    break;
                }
                assert ( val );
                for ( i = 0; val [ i ] != '\0'; ++ i ) {
                    if ( val [ i ] < '0' || val [ i ] > '9' ) {
                        rc = RC ( rcExe, rcArgv, rcParsing,
                                  rcParam, rcInvalid );
                        LOGERR ( klogErr, rc,
                                 "Invalid " MINIM_OPTION " value" );
                        break;
                    }
                    n = n * 10 + val [ i ] - '0';
                }
                self -> chunks = n;
            }
        }

/* MX_CH_OPTION */
        {
            const char * val = NULL;
            rc = ArgsOptionCount ( * args, MX_CH_OPTION, & pcount );
            if ( rc != 0 ) {
                LOGERR ( klogInt, rc,
                         "Failure to get '" MX_CH_OPTION "' argument");
                break;
            }
            if ( pcount > 0 ) {
                uint64_t n = 0;
                int i = 0;
                rc = ArgsOptionValue
                    ( * args, MX_CH_OPTION, 0, ( const void ** ) & val );
                if ( rc != 0 ) {
                    LOGERR ( klogInt, rc, "Failure to get '" MX_CH_OPTION
                                          "' argument value" );
                    break;
                }
                assert ( val );
                for ( i = 0; val [ i ] != '\0'; ++ i ) {
                    if ( val [ i ] < '0' || val [ i ] > '9' ) {
                        rc = RC ( rcExe, rcArgv, rcParsing,
                                  rcParam, rcInvalid );
                        LOGERR ( klogErr, rc,
                                 "Invalid " MX_CH_OPTION " value" );
                        break;
                    }
                    n = n * 10 + val [ i ] - '0';
                }
                self -> chunkCount = n;
            }
        }

/* PRINT_OPTION */
        {
            rc = ArgsOptionCount ( * args, PRINT_OPTION, & pcount );
            if ( rc != 0 ) {
                LOGERR ( klogInt, rc,
                         "Failure to get '" PRINT_OPTION "' argument");
                break;
            }
            if ( pcount > 0 )
                self -> print = true;
        }

    } while ( false );

    return rc;
}

static rc_t DoMakeHttpFileAndSize ( Do * self, const char * url ) {
    rc_t rc = 0;

    ver_t version = 0x01010000;

    assert ( self );

    if ( self -> file != NULL )
        return 0;

    rc = KNSManagerMakeHttpFile ( self -> mgr, & self -> file,
                                  NULL, version, url );
    if ( rc != 0 )
        PLOGERR ( klogErr, ( klogErr, rc,
            "Cannot KNSManagerMakeHttpFile($(url)", "url=%s", url ) );


    rc = KFileSize ( self -> file, & self -> fileSize );
    if ( rc == 0 &&  ( self -> max == 0 || self -> max > self -> fileSize ) )
        self -> max = self -> fileSize;

    return rc;
}

static rc_t DoFile ( Do * self, const char * url ) {
    rc_t rc = 0;

    uint64_t pos = 0;
    uint64_t chunk = 0;

    assert ( self );

    rc = DoMakeHttpFileAndSize ( self, url );

    for ( chunk = 0, pos =  self -> min; rc == 0 ; ++ chunk ) {
        size_t num_read = 0;

        size_t toRead = self -> bufSize;
        size_t to = pos + toRead;
        if ( self -> max > 0 && to > self -> max ) {
            to = self -> max;
            toRead = to - pos;
        }

        if ( self -> chunkCount > 0 && chunk >= self -> chunkCount )
            break;

        rc = Quitting ();
        if ( rc != 0 )
            break;

        rc = KFileRead ( self -> file, pos, self -> buffer,
                         toRead, & num_read );
        STSMSG ( STAT_PWR, ( "KFileRead(%lu/%zu, %zu) = %zu (%R)",
                                pos, self -> fileSize, toRead, num_read, rc ) );
        if ( rc != 0 ) {
            PLOGERR ( klogErr, (klogErr, rc,
                "Cannot KFileRead('$(url)',$(pos)/$fsize,$(size))",
                "url=%s,fsize=%zu,pos=%lu,size=%zu",
                url, self -> fileSize, pos  , toRead ) );
            break;
        }

        if ( num_read == 0 )
            break;

        if ( self -> print )
            OUTMSG ( ( "%.*s", ( int ) num_read, self -> buffer ) );

        pos += num_read;

        if ( self -> max > 0 && pos >= self -> max )
            break;
    }

    return rc;
}

static rc_t DoStream ( const Do * self, const char * url ) {
    rc_t rc = 0;

    ver_t version = 0x01010000;
    uint64_t pos = 0;
    KClientHttpRequest * req = NULL;
    KClientHttpResult * rslt = NULL;
    KStream * stream = NULL;

    assert ( self );

    rc = KNSManagerMakeClientRequest
        ( self -> mgr, & req, version, NULL, url );

    if ( rc == 0 )
        rc = KClientHttpRequestGET ( req, & rslt );

    if ( rc == 0 )
        rc = KClientHttpResultGetInputStream ( rslt, & stream );

    while ( rc == 0 ) {
        size_t num_read = 0;
        size_t toRead = self -> bufSize;
        size_t to = pos + toRead;
        if ( self -> max > 0 && to > self -> max ) {
            to = self -> max;
            toRead = to - pos;
        }

        rc = Quitting ();
        if ( rc != 0 )
            break;

        rc = KStreamRead ( stream, self -> buffer, toRead, & num_read );
        STSMSG ( STAT_PWR, ( "KStreamRead(%lu, %zu) = %zu (%R)",
                                        pos, toRead, num_read, rc ) );
        if ( rc != 0 ) {
            PLOGERR ( klogErr, (klogErr, rc,
                "Cannot KStreamRead('$(url)',$(pos),$(size))",
                "url=%s,pos=%lu,size=%zu", url, pos, toRead ) );
            break;
        }

        if ( num_read == 0 )
            break;

        if ( pos + num_read >= self -> min && self -> min < to ) {
            uint64_t from = 0;
            uint64_t size = num_read;
            if ( self -> min > pos ) {
                from = self -> min - pos;
                size -= from;
            }
            if ( self -> print )
                OUTMSG ( ( "%.*s", ( int ) size, self -> buffer + from ) );
        }

        pos += num_read;

        if ( self -> max > 0 && pos >= self -> max )
            break;
    }

    RELEASE ( KStream, stream );
    RELEASE ( KClientHttpResult, rslt );
    RELEASE ( KClientHttpRequest, req );

    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE( argc, argv, VDB_INIT_FAILED );

    rc_t rc = 0;
    Args * args = NULL;
    uint32_t argCount = 0, i = 0;
    const char * url = NULL;
    Do data;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    memset ( & data, 0, sizeof data );

    KStsHandlerSetStdErr    ();
    KStsLibHandlerSetStdErr ();

    rc = DoArgs ( & data, & args, argc, argv );
    if ( rc == 0 )
        rc = ArgsParamCount ( args, & argCount );

    if ( argCount > 0 ) {
        if ( rc == 0 )
            rc = KNSManagerMake ( & data . mgr );

        if ( rc == 0 && ( data . chunks > 0 || data . useFile ) ) {
            rc_t r2 = ArgsParamValue ( args, i, ( const void ** ) & url );
            if ( r2 == 0 ) {
                rc = DoMakeHttpFileAndSize ( & data, url );
                if ( rc == 0 && data . chunks > 0 ) {
                    data . bufSize = data . fileSize / data . chunks + 1;
                    if ( data . bufSize > data . fileSize )
                        data . bufSize = data . fileSize;
                }
            }
        }

        data . allocated = data . bufSize;
        data . buffer = calloc ( 1, data . allocated );
        if ( data . buffer == NULL )
            return RC ( rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted );

        if ( rc != 0 )
            return rc;
    }

    for ( i = 0; i < argCount; ++ i ) {
        rc_t r = 0;
        const char * multiple = "";
        size_t chunk = data . bufSize;

        rc_t r2 = ArgsParamValue ( args, i, ( const void ** ) & url );
        if ( r2 != 0 )
            continue;

        if ( chunk >= 1024 ) {
            chunk /= 1024;
            multiple = "K";
        }
        if ( chunk >= 1024 ) {
            chunk /= 1024;
            multiple = "M";
        }

        if ( data . chunks > 0 )
            STSMSG ( STAT_USR, ( "Downloading '%s' (%zu) via %s "
                "using %lu %zu%s byte chunks", url,
                data . fileSize, data . useFile ? "KFileRead" : "KStreamRead",
                data . chunks, chunk, multiple ) );
        else if ( data . fileSize == 0 )
            STSMSG ( STAT_USR, ( "Downloading '%s' via %s using %zu%s bytes"
                " chunks", url, data . useFile ? "KFileRead" : "KStreamRead",
                                          chunk, multiple ) );
        else {
            uint64_t n = 0;
            uint64_t s = data . bufSize;
            assert ( s );
            if ( s > data . fileSize )
                s = data . fileSize;
            n = data . fileSize / s + ( data . fileSize % s > 0 ? 1 : 0 );
            STSMSG ( STAT_USR, ( "Downloading '%s' (%zu) via %s "
                "using (%lu) %zu%s byte chunks", url,
                data . fileSize, data . useFile ? "KFileRead" : "KStreamRead",
                n, chunk, multiple ) );
        }

        r2 = data . useFile ? DoFile ( & data, url ) : DoStream ( & data, url );

        r = KFileRelease ( data . file );
        if ( r != 0 && r2 == 0 )
            r2 = r;

        if ( r2 != 0 && rc == 0 )
            rc = r2;
    }

    RELEASE ( KNSManager, data . mgr );
    RELEASE ( Args, args );
    free ( data . buffer );

    STSMSG ( STAT_USR, ( "Done with %R", rc ) );

    return VDB_TERMINATE( rc );
}
