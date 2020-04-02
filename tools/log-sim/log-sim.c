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
* ===========================================================================
*
*/

#include <kapp/main.h>

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/data-buffer.h>
#include <klib/time.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/defs.h>
#include <kfs/filetools.h>
#include <kfs/gzip.h>

#include <stdlib.h>
#include <string.h>

#define OPTION_INPUT        "input"
#define ALIAS_INPUT         "i"

#define OPTION_OUTPUT       "output"
#define ALIAS_OUTPUT        "o"

#define OPTION_FORCE        "force"
#define ALIAS_FORCE         "f"

#define OPTION_APPEND       "append"
#define ALIAS_APPEND        "a"

#define OPTION_RANDOM       "random"
#define ALIAS_RANDOM        "r"

#define OPTION_GUNZIP       "gunzip"
#define ALIAS_GUNZIP        "g"

#define OPTION_SKIP         "skip"
#define ALIAS_SKIP          "s"

#define OPTION_COUNT        "count"
#define ALIAS_COUNT         "c"

#define OPTION_DOMAIN       "domain"
#define ALIAS_DOMAIN        "d"

#define OPTION_MINWAIT      "min-wait"
#define ALIAS_MINWAIT       "m"

#define OPTION_MAXWAIT      "max-wait"
#define ALIAS_MAXWAIT       "x"

#define OPTION_CLOCKOFS     "clock-offset"
#define ALIAS_CLOCKOFS      "l"

static const char * input_usage[]  = { "input file to replay (dflt: random data", NULL };
static const char * output_usage[] = { "output file to be written to (dflt: stdout)", NULL };
static const char * force_usage[]  = { "overwrite output-file if it already exists", NULL };
static const char * append_usage[] = { "append to output-file if it already exists", NULL };
static const char * random_usage[] = { "generate random events - ignore input", NULL };
static const char * gunzip_usage[] = { "gunzip the input-file", NULL };
static const char * skip_usage[]   = { "skip events before current time", NULL };
static const char * count_usage[]  = { "how many records to produce", NULL };
static const char * domain_usage[] = { "domain name to use", NULL };
static const char * min_wait_usage[] = { "min wait-time for random (in ms)", NULL };
static const char * max_wait_usage[] = { "max wait-time for random (in ms)", NULL };
static const char * clockofs_usage[] = { "offset in minutes to clock (can be negative!)", NULL };

OptDef MyOptions[] =
{
	{ OPTION_INPUT, 		ALIAS_INPUT, 		NULL, 	input_usage, 		1, 	true, 	false },
	{ OPTION_OUTPUT, 		ALIAS_OUTPUT, 		NULL, 	output_usage, 		1, 	true, 	false },
	{ OPTION_FORCE, 		ALIAS_FORCE, 		NULL, 	force_usage, 		1, 	false, 	false },
	{ OPTION_APPEND, 		ALIAS_APPEND, 		NULL, 	append_usage, 		1, 	false, 	false },
	{ OPTION_RANDOM, 		ALIAS_RANDOM, 		NULL, 	random_usage, 		1, 	false, 	false },
	{ OPTION_GUNZIP, 		ALIAS_GUNZIP, 		NULL, 	gunzip_usage, 		1, 	false, 	false },
	{ OPTION_SKIP, 		    ALIAS_SKIP, 		NULL, 	skip_usage, 		1, 	false, 	false },
	{ OPTION_COUNT, 		ALIAS_COUNT, 		NULL, 	count_usage, 		1, 	true, 	false },
	{ OPTION_DOMAIN, 		ALIAS_DOMAIN, 		NULL, 	domain_usage, 		1, 	true, 	false },
	{ OPTION_MINWAIT, 		ALIAS_MINWAIT, 		NULL, 	min_wait_usage, 	1, 	true, 	false },
	{ OPTION_MAXWAIT, 		ALIAS_MAXWAIT, 		NULL, 	max_wait_usage,		1, 	true, 	false },
	{ OPTION_CLOCKOFS, 		ALIAS_CLOCKOFS,     NULL, 	clockofs_usage,		1, 	true, 	false }
};

const char UsageDefaultName[] = "log-sim";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s [options]\n"
        "\n", progname );
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;
    uint32_t idx, count = ( sizeof MyOptions ) / ( sizeof MyOptions[ 0 ] );
    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );
    for ( idx = 0; idx < count; ++idx )
    {
        const OptDef * opt = &MyOptions[ idx ];
        char * param = NULL;
        switch( opt->aliases[ 0 ] )
        {
            case 'i' :
            case 'o' : param = "PATH"; break;
            case 'c' : param = "NUMBER"; break;
            case 'd' : param = "NAME"; break;
            case 'm' :
            case 'x' : param = "mSec."; break;
            case 'l' : param = "minutes"; break;
        }
        HelpOptionLine ( opt->aliases, opt->name, param, opt->help );
    }

    KOutMsg ( "\n" );
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

/***************************************************************************/

static rc_t get_str_option( const Args *args, const char * option_name, const char ** value, const char * dflt )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
	if ( rc != 0 )
	{
		PLOGERR ( klogInt, ( klogInt, rc, "ArgsOptionCount( '$(key)' ) failed", "key=%s", option_name ) );
        *value = dflt;
	}
	else
    {
        if ( count > 0 )
        {
            rc = ArgsOptionValue( args, option_name, 0, (const void **)value );
            if ( rc != 0 )
            {
                PLOGERR ( klogInt, ( klogInt, rc, "ArgsOptionValue( 0, '$(key)' ) failed", "key=%s", option_name ) );
                *value = NULL;
            }
        }
        else
            *value = dflt;
    }
    return rc;
}

static rc_t get_bool_option( const Args *args, const char * option_name, bool * res )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
    if ( rc != 0 )
    {
        PLOGERR ( klogInt, ( klogInt, rc, "ArgsOptionCount( '$(key)' ) failed", "key=%s", option_name ) );
    }
    else
        *res = ( count > 0 );
    return rc;
}

static rc_t get_ulong_option( const Args *args, const char * option_name, unsigned long * res, unsigned long dflt )
{
    const char * value; 
    rc_t rc = get_str_option( args, option_name, &value, NULL );
    if ( rc == 0 && value != NULL )
        *res = atol( value );
    else 
        *res = dflt;
    return rc;
}

static rc_t get_long_option( const Args *args, const char * option_name, signed long * res, signed long dflt )
{
    const char * value; 
    rc_t rc = get_str_option( args, option_name, &value, NULL );
    if ( rc == 0 && value != NULL )
        *res = atol( value );
    else 
        *res = dflt;
    return rc;
}

/***************************************************************************/
typedef struct month_t { char name[ 4 ]; } month_t;

typedef struct log_sim_ctx_t
{
    /* input parameters */
    const char * src;
    const char * dst;
    const char * domain;
    bool force, append, random, gunzip, skip;
    unsigned long count, minwait, maxwait;
    signed long clock_offset;

    /* runtime variables */
    KDirectory * dir;
    const KFile * src_file;
    KFile * dst_file;
    uint64_t dst_pos;
    uint64_t line_nr;
    KDataBuffer data_buffer;
    unsigned long rnd;
    month_t months[ 12 ];
} log_sim_ctx_t;

const char * dflt_domain = "sra-download.ncbi.nlm.nih.gov";

static rc_t gather_log_sim_ctx( log_sim_ctx_t * self, Args * args )
{
    month_t m[ 12 ] = { {"Jan"}, {"Feb"}, {"Mar"}, {"Apr"}, {"May"}, {"Jun"}, {"Jul"}, {"Aug"}, {"Sep"}, {"Oct"}, {"Nov"}, {"Dec"} };
    
    rc_t rc = get_str_option( args, OPTION_INPUT, &( self -> src ), NULL );
    if ( rc == 0 ) rc = get_str_option( args, OPTION_OUTPUT, &( self -> dst ), NULL );
    if ( rc == 0 ) rc = get_str_option( args, OPTION_DOMAIN, &( self -> domain ), dflt_domain );
    self -> force = false;
    self -> append = false;
    self -> random = false;
    self -> gunzip = false;
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_FORCE, &( self -> force ) );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_APPEND, &( self -> append ) );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_RANDOM, &( self -> random ) );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_GUNZIP, &( self -> gunzip ) );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_SKIP, &( self -> skip ) );
    if ( rc == 0 ) rc = get_ulong_option( args, OPTION_COUNT, &( self -> count ), 0 );
    if ( rc == 0 ) rc = get_ulong_option( args, OPTION_MINWAIT, &( self -> minwait ), 0 );
    if ( rc == 0 ) rc = get_ulong_option( args, OPTION_MAXWAIT, &( self -> maxwait ), 0 );
    if ( rc == 0 ) rc = get_long_option( args, OPTION_CLOCKOFS, &( self -> clock_offset ), 0 );
    if ( rc == 0 )
    {
        rc = KDataBufferMake( &( self -> data_buffer ), 8, 1024 );
        if ( rc != 0 )
        {
            LOGERR ( klogInt, rc, "KDataBufferMake() failed" );
        }
    }

    self -> dir = NULL;
    self -> src_file = NULL;
    self -> dst_file = NULL;
    self -> dst_pos = 0;
    self -> line_nr = 0;
    self -> rnd = KTimeStamp();

    for ( int i = 0; i < 12; ++i )
        for ( int j = 0; j < 4; ++j )
            self -> months[ i ].name[ j ] = m[ i ].name[ j ];
    return rc;
}

static rc_t open_input_file( log_sim_ctx_t * self )
{
    rc_t rc = 0;
    if ( NULL != self -> src )
    {
        uint32_t pt = KDirectoryPathType( self -> dir, "%s", self -> src );
        if ( kptFile != pt )
        {
            rc = RC( rcExe, rcNoTarg, rcComparing, rcFile, rcInvalid );
            PLOGERR ( klogInt, ( klogInt, rc, "KDirectoryPathType( '$(file)' ) = $(value)", "file=%s,value=%u", self -> src, pt ) );
        }
        else
        {
            rc = KDirectoryOpenFileRead ( self -> dir, &( self -> src_file ), "%s", self -> src );
            if ( rc != 0 )
            {
                PLOGERR ( klogInt, ( klogInt, rc, "cannot open input-file '$(file)'", "file=%s", self -> src ) );
            }
        }
    }
    else
    {
        /* if no input-file given use stdin */
        rc = KFileMakeStdIn( &( self -> src_file ) );
        if ( rc != 0 )
        {
            LOGERR ( klogInt, rc, "cannot open stdin" );
        }
    }
    if ( rc == 0 && self -> gunzip )
    {
        const KFile * tmp;
        rc = KFileMakeGzipForRead ( &tmp, self -> src_file );
        if ( rc != 0 )
        {
            LOGERR ( klogInt, rc, "cannot create GZIP reader" );
        }
        else
        {
            KFileRelease( self -> src_file );
            self -> src_file = tmp;
        }
    }
    return rc;
}

static rc_t open_output_file( log_sim_ctx_t * self )
{
    rc_t rc = 0;
    if ( NULL != self -> dst )
    {
        KCreateMode cm = kcmParents;
        if ( self -> force ) cm |= kcmInit; else cm |= kcmCreate;
        rc = KDirectoryCreateFile( self -> dir, &( self -> dst_file ), false, 0664, cm, "%s", self -> dst );
        if ( rc != 0 )
        {
            PLOGERR ( klogInt, ( klogInt, rc, "cannot open output-file '$(file)'", "file=%s", self -> dst ) );
        }
        else
        {
            /* seek to end if --append */
            if ( self -> append )
            {
                rc = KFileSize( ( const KFile * )self -> dst, &( self -> dst_pos ) );
                if ( rc != 0 )
                    PLOGERR ( klogInt, ( klogInt, rc, "getting size of output-file '$(file)' failed", "file=%s", self -> dst ) );
            }
        }
    }
    else
    {
        /* if not output-file given use stdout */
        rc = KFileMakeStdOut( &( self -> dst_file ) );
        if ( rc != 0 )
        {
            LOGERR ( klogInt, rc, "cannot open stdout" );
        }
    }
    return rc;
}

static rc_t prepare_log_sim_ctx( log_sim_ctx_t * self )
{
    rc_t rc = KDirectoryNativeDir( &( self -> dir ) );
    if ( rc != 0 )
    {
        LOGERR ( klogInt, rc, "KDirectoryNativeDir() failed" );
    }
    else
    {
        if ( !self -> random ) rc = open_input_file( self );
        if ( rc == 0 ) rc = open_output_file( self );
    }
    return rc;
}

static void destroy_log_sim_ctx( log_sim_ctx_t * self )
{
    KDataBufferWhack ( &( self -> data_buffer ) );
    if ( NULL != self -> dst_file && NULL != self -> dst ) KFileRelease( self -> dst_file );
    if ( NULL != self -> src_file && NULL != self -> src ) KFileRelease( self -> src_file );
    if ( NULL != self -> dir ) KDirectoryRelease( self -> dir );
}

static rc_t dst_print( log_sim_ctx_t * self, const char * fmt, ... )
{
    rc_t rc = KDataBufferWipe( &( self -> data_buffer ) );
    if ( rc == 0 )
    {
        va_list args;
        va_start( args, fmt );
        rc = KDataBufferVPrintf( &( self -> data_buffer ), fmt, args );
        if ( rc == 0 )
        {
            size_t num_writ;
            rc = KFileWrite( self -> dst_file, self -> dst_pos, self -> data_buffer . base, self -> data_buffer . elem_count, &num_writ );
            if ( rc == 0 )
                self -> dst_pos += num_writ;
        }
        va_end( args );
    }
    if ( rc == 0 ) self -> line_nr += 1;
    return rc;
}

/***************************************************************************/
typedef struct hms_t
{
    uint32_t hour, minute, second;
} hms_t;

typedef struct buff_t
{
    char tmp[ 8 ];
    uint32_t idx;
} buff_t;

static void handle_char( char c, char ss, buff_t * buf, uint32_t * dst, uint32_t * state )
{
    if ( c == ss )
    {
        buf -> tmp[ buf -> idx ] = 0;
        *dst = atoi( buf -> tmp );
        buf -> idx = 0;
        ( *state )++;
    }
    else
    {
        if ( buf -> idx < ( sizeof( buf -> tmp ) - 1 ) )
            buf -> tmp[ buf -> idx++ ] = c; 
    }
}

static bool extract_time( const String * line, hms_t * t )
{
    uint32_t idx, state = 0;
    bool res = false;
    buff_t buf;
    buf.idx = 0;
    for ( idx = 0; !res && idx < line -> len; ++idx )
    {
        char c = line -> addr[ idx ];
        switch( state )
        {
            case 0 : if ( c == '[' ) state++; break;
            case 1 : if ( c == ':' ) state++; break;
            case 2 : handle_char( c, ':', &buf, &( t -> hour ), &state ); break;
            case 3 : handle_char( c, ':', &buf, &( t -> minute ), &state ); break;
            case 4 : handle_char( c, ' ', &buf, &( t -> second ), &state ); break;
            case 5 : res = true; break;
        }
    }
    return res;
}

static uint32_t to_seconds( uint32_t hours, uint32_t minutes, uint32_t seconds )
{
    uint32_t res = hours * 60 * 60;
    res += minutes * 60;
    return res + seconds;
}

static rc_t replay_callback( const String * line, void * data )
{
    rc_t rc = 0;
    log_sim_ctx_t * self = data;
    hms_t hms;

    if ( extract_time( line, &hms ) )
    {
        KTime t;
        uint32_t log_sec = to_seconds( hms.hour, hms.minute, hms.second );
        uint32_t cur_sec;
        bool print_it = false;
        
        KTimeLocal( &t, KTimeStamp() );
        cur_sec = to_seconds( t.hour, t.minute, t.second ) + ( self -> clock_offset * 60 );

        if ( log_sec < cur_sec )
        {
            /* time has passed, should we print or skip? */
            print_it = !( self -> skip );
        }
        else if ( log_sec == cur_sec )
        {
            /* time matches: print now: */
            print_it = true;
        }
        else
        {
            /* wait for time to match... */
            while ( !print_it )
            {
                rc = KSleepMs( 250 );
                KTimeLocal( &t, KTimeStamp() );
                cur_sec = to_seconds( t.hour, t.minute, t.second ) + ( self -> clock_offset * 60 );
                if ( log_sec == cur_sec )
                    print_it = true;
            }
        }
        
        if ( print_it )
        {
            rc = dst_print( self, "%S\n", line );
            if ( self -> count > 0 && ( self -> line_nr > self -> count ) )
                rc = SILENT_RC( rcExe, rcNoTarg, rcVisiting, rcRange, rcExhausted );
        }
    }
    return rc;
}

static rc_t replay( log_sim_ctx_t * self )
{
    rc_t rc = ProcessFileLineByLine( self -> src_file,  replay_callback, self );
    if ( rc != 0 )
    {
        rc_t rc1 = SILENT_RC( rcExe, rcNoTarg, rcVisiting, rcRange, rcExhausted );
        if ( rc != rc1 )
            LOGERR ( klogInt, rc, "relpay() failed" );
    }
    return rc;
}

/***************************************************************************/

static unsigned long simple_rand( unsigned long *x )
{
    unsigned long r = *x;
    r ^= ( r << 21 );
    r ^= ( r >> 35 );
    r ^= ( r << 4 );
    *x = r;
    return r;
}

static unsigned long simple_rand_range( unsigned long *x, unsigned long lower, unsigned long upper )
{
    unsigned long r = simple_rand( x );
    return lower + ( r % ( upper - lower + 1 ) );
}

typedef struct rand_ip_t
{
    unsigned int addr[ 4 ];
} rand_ip_t;

static void make_rand_ip( unsigned long *x, rand_ip_t * ip )
{
    for ( int i = 0; i < 4; i++ )
        ip -> addr[ i ] = (unsigned int)simple_rand_range( x, 0, 255 );
}

static int tzoff( int tzoff_min )
{
    int h = tzoff_min / 60;
    int m = tzoff_min - ( h * 60 );
    return ( h * 100 + m );
}

static rc_t generate_random_event( log_sim_ctx_t * self )
{
    rc_t rc;
    rand_ip_t ip;
    unsigned long acc = simple_rand_range( &( self -> rnd ), 1, 1000000 );
    KTime t;
    int tz;
    char sign;

    make_rand_ip( &( self -> rnd ), &ip );
    KTimeLocal( &t, KTimeStamp() );
    tz = tzoff( t.tzoff );
    sign = ( tz < 0 ) ? '-' : '+';
    if ( tz < 0 ) tz = -tz;
    rc = dst_print( self,
        "%d.%d.%d.%d - - [%02d/%s/%04d:%02d:%02d:%02d %c%04d] \"%s\" \"GET /path/path/SRR%06d HTTP/1.1\" 206 32768 0.00 \"toolkit.2.9.1\" - port=443 rl=293\n",
        ip.addr[ 0 ], ip.addr[ 1 ], ip.addr[ 2 ], ip.addr[ 3 ],
        t.day + 1, self -> months[ t.month ].name, t.year, t.hour, t.minute, t.second, sign, tz,
        self -> domain, acc
        );
    if ( rc == 0 ) rc = Quitting();
    if ( rc == 0 && self -> minwait > 0 && self -> maxwait > 0 )
    {
        unsigned long wait = simple_rand_range( &( self -> rnd ), self -> minwait, self -> maxwait );
        rc = KSleepMs( wait );
    }
    return rc;
}

static rc_t generate_random( log_sim_ctx_t * self )
{
    rc_t rc = 0;

    if ( self -> count > 0 )
    {
        unsigned long i;
        for ( i = 0; rc == 0 && i < self -> count; i++ )
        {
            rc = generate_random_event( self );
        }
    }
    else
    {
        while( rc == 0 )
        {
            rc = generate_random_event( self );
        }
    }
    return rc;
}


/***************************************************************************
    Main:
***************************************************************************/
rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                                  MyOptions, sizeof MyOptions / sizeof ( OptDef ) );
    if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "ArgsMakeAndHandle failed()" );
	}
    else
    {
        log_sim_ctx_t lctx;

        rc = gather_log_sim_ctx( &lctx, args );
        if ( rc == 0 )
        {
            rc = prepare_log_sim_ctx( &lctx );
            if ( rc == 0 )
            {
                /* ======================================== */
                if ( lctx . random )
                    rc = generate_random( &lctx );
                else
                    rc = replay( &lctx );
                /* ======================================== */
                destroy_log_sim_ctx( &lctx );
            }
        }
    }
    return rc;
}
