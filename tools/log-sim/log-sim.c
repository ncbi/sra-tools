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
#include <klib/time.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/defs.h>
#include <kfs/gzip.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define OPTION_INPUT        "input"
#define ALIAS_INPUT         "i"

#define OPTION_OUTPUT       "output"
#define ALIAS_OUTPUT        "o"

#define OPTION_FORCE        "force"
#define ALIAS_FORCE         "f"

#define OPTION_APPEND       "append"
#define ALIAS_APPEND        "a"

#define OPTION_FN           "function"
#define ALIAS_FN            "u"

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

#define OPTION_SPEED        "speed"
#define ALIAS_SPEED         "e"

static const char * input_usage[]  = { "input file to replay (dflt: stdin)", NULL };
static const char * output_usage[] = { "output file to be written to (dflt: stdout)", NULL };
static const char * force_usage[]  = { "overwrite output-file if it already exists", NULL };
static const char * append_usage[] = { "append to output-file if it already exists", NULL };
static const char * funct_usage[]  = { "pick other function: (random|analyze|presort)", NULL };
static const char * gunzip_usage[] = { "gunzip the input-file", NULL };
static const char * skip_usage[]   = { "skip events before current time for replay", NULL };
static const char * count_usage[]  = { "how many records to produce", NULL };
static const char * domain_usage[] = { "domain name to use for random", NULL };
static const char * min_wait_usage[] = { "min wait-time for random (in ms)", NULL };
static const char * max_wait_usage[] = { "max wait-time for random (in ms)", NULL };
static const char * speed_usage[]  = { "speed-up (<1.0) or slow-down (>1.0) replay", NULL };

OptDef MyOptions[] =
{
	{ OPTION_INPUT, 		ALIAS_INPUT, 		NULL, 	input_usage, 		1, 	true, 	false },
	{ OPTION_OUTPUT, 		ALIAS_OUTPUT, 		NULL, 	output_usage, 		1, 	true, 	false },
	{ OPTION_FORCE, 		ALIAS_FORCE, 		NULL, 	force_usage, 		1, 	false, 	false },
	{ OPTION_APPEND, 		ALIAS_APPEND, 		NULL, 	append_usage, 		1, 	false, 	false },
	{ OPTION_FN, 		    ALIAS_FN, 		    NULL, 	funct_usage, 		1, 	true, 	false },
	{ OPTION_GUNZIP, 		ALIAS_GUNZIP, 		NULL, 	gunzip_usage, 		1, 	false, 	false },
	{ OPTION_SKIP, 		    ALIAS_SKIP, 		NULL, 	skip_usage, 		1, 	false, 	false },
	{ OPTION_COUNT, 		ALIAS_COUNT, 		NULL, 	count_usage, 		1, 	true, 	false },
	{ OPTION_DOMAIN, 		ALIAS_DOMAIN, 		NULL, 	domain_usage, 		1, 	true, 	false },
	{ OPTION_MINWAIT, 		ALIAS_MINWAIT, 		NULL, 	min_wait_usage, 	1, 	true, 	false },
	{ OPTION_MAXWAIT, 		ALIAS_MAXWAIT, 		NULL, 	max_wait_usage,		1, 	true, 	false },
	{ OPTION_SPEED, 		ALIAS_SPEED, 		NULL, 	speed_usage,		1, 	true, 	false }
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
            case 'u' : param = "FUNCTION"; break;
            case 'e' : param = "FACTOR"; break;
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
        *res = false;
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

static rc_t get_float_option( const Args *args, const char * option_name, double * res, double dflt )
{
    const char * value; 
    rc_t rc = get_str_option( args, option_name, &value, NULL );
    if ( rc == 0 && value != NULL )
    {
        *res = atof( value );
        if ( *res == 0.0 ) *res = 1.0;
    }
    else 
        *res = dflt;
    return rc;
}

/***************************************************************************/
typedef enum e_fn { fn_replay, fn_random, fn_analyze, fn_presort } e_fn;

static rc_t get_fn_option( const Args *args, e_fn * res )
{
    const char * value;
    rc_t rc = get_str_option( args, OPTION_FN, &( value ), NULL );
    if ( rc == 0 )
    {
        *res = fn_replay;
        if ( value != NULL )
        {
            switch( value[ 0 ] )
            {
                case 'R' :
                case 'r' : *res = fn_random; break;
                case 'A' :
                case 'a' : *res = fn_analyze; break;
                case 'P' :
                case 'p' : *res = fn_presort; break;
            }
        }
    }
    return rc;
}

typedef struct log_sim_ctx_t
{
    /* input parameters */
    const char * src;
    const char * dst;
    const char * domain;
    bool force, append, gunzip, skip;
    e_fn fn;
    unsigned long count, minwait, maxwait;
    double speed;

    /* runtime variables */
    KDirectory * dir;
    const KFile * src_file;
    KFile * dst_file;
    uint64_t dst_pos;
    uint64_t line_nr;
    char buffer[ 4096 * 4 ];
} log_sim_ctx_t;

const char * dflt_domain = "sra-download.ncbi.nlm.nih.gov";

static rc_t gather_log_sim_ctx( log_sim_ctx_t * self, Args * args )
{
    rc_t rc = get_str_option( args, OPTION_INPUT, &( self -> src ), NULL );
    if ( rc == 0 ) rc = get_str_option( args, OPTION_OUTPUT, &( self -> dst ), NULL );
    if ( rc == 0 ) rc = get_str_option( args, OPTION_DOMAIN, &( self -> domain ), dflt_domain );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_FORCE, &( self -> force ) );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_APPEND, &( self -> append ) );
    if ( rc == 0 ) rc = get_fn_option( args, &( self -> fn ) );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_GUNZIP, &( self -> gunzip ) );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_SKIP, &( self -> skip ) );
    if ( rc == 0 ) rc = get_ulong_option( args, OPTION_COUNT, &( self -> count ), 0 );
    if ( rc == 0 ) rc = get_ulong_option( args, OPTION_MINWAIT, &( self -> minwait ), 0 );
    if ( rc == 0 ) rc = get_ulong_option( args, OPTION_MAXWAIT, &( self -> maxwait ), 0 );
    if ( rc == 0 ) rc = get_float_option( args, OPTION_SPEED, &( self -> speed ), 1.0 );

    self -> dir = NULL;
    self -> src_file = NULL;
    self -> dst_file = NULL;
    self -> dst_pos = 0;
    self -> line_nr = 0;

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
        if ( !( self -> fn == fn_random )  ) rc = open_input_file( self );
        if ( rc == 0 ) rc = open_output_file( self );
    }
    return rc;
}

static void destroy_log_sim_ctx( log_sim_ctx_t * self )
{
    if ( NULL != self -> dst_file && NULL != self -> dst ) KFileRelease( self -> dst_file );
    if ( NULL != self -> src_file && NULL != self -> src ) KFileRelease( self -> src_file );
    if ( NULL != self -> dir ) KDirectoryRelease( self -> dir );
}

static rc_t dst_print( log_sim_ctx_t * self, const char * fmt, ... )
{
    rc_t rc;
    va_list args;
    size_t num_writ1;
    va_start( args, fmt );

    rc = string_vprintf ( self -> buffer, sizeof self -> buffer, &num_writ1, fmt, args );
    if ( rc == 0 )
    {
        size_t num_writ2;
        rc = KFileWrite( self -> dst_file, self -> dst_pos, self -> buffer, num_writ1, &num_writ2 );
        if ( rc == 0 )
            self -> dst_pos += num_writ2;
    }
    va_end( args );
    if ( rc == 0 ) self -> line_nr += 1;
    return rc;
}

/***************************************************************************/

#define STATE_ALPHA 0
#define STATE_LF 1
#define STATE_NL 2

typedef struct buffer_range
{
    const char * start;
    uint32_t processed, count, state;
} buffer_range;

static rc_t ProcessFromBuffer( buffer_range * range,
    rc_t ( CC * on_line )( const String * line, void * data ), void * data  )
{
    rc_t rc = 0;
    uint32_t idx;
    const char * p = range->start;
    String S;

    S.addr = p;
    S.len = S.size = range->processed;
    for ( idx = range->processed; idx < range->count && rc == 0; ++idx )
    {
        switch( p[ idx ] )
        {
            case 0x0A : switch( range->state )
                        {
                            case STATE_ALPHA : /* ALPHA --> LF */
                                                rc = on_line( &S, data );
                                                range->state = STATE_LF;
                                                break;

                            case STATE_LF : /* LF --> LF */
                                             break;

                            case STATE_NL : /* NL --> LF */
                                             range->state = STATE_LF;
                                             break;
                        }
                        break;

            case 0x0D : switch( range->state )
                        {
                            case STATE_ALPHA : /* ALPHA --> NL */
                                                rc = on_line( &S, data );
                                                range->state = STATE_NL;
                                                break;

                            case STATE_LF : /* LF --> NL */
                                             range->state = STATE_NL;
                                             break;

                            case STATE_NL : /* NL --> NL */
                                             break;
                        }
                        break;

            default   : switch( range->state )
                        {
                            case STATE_ALPHA : /* ALPHA --> ALPHA */
                                                S.len++; S.size++;
                                                break;

                            case STATE_LF : /* LF --> ALPHA */
                                             S.addr = &p[ idx ]; S.len = S.size = 1;
                                             range->state = STATE_ALPHA;
                                             break;

                            case STATE_NL : /* NL --> ALPHA */
                                             S.addr = &p[ idx ]; S.len = S.size = 1;
                                             range->state = STATE_ALPHA;
                                             break;
                        }
                        break;
        }
    }
    if ( range->state == STATE_ALPHA )
    {
        range->start = S.addr;
        range->count = S.len;
    }
    else
        range->count = 0;
    return rc;
}


static rc_t ProcessLineByLine( struct KFile const * f,
        rc_t ( CC * on_line )( const String * line, void * data ), void * data )
{
    rc_t rc = 0;
    uint64_t pos = 0;
    char buffer[ 4096 * 4 ];
    buffer_range range;
    bool done = false;

    range.start = buffer;
    range.count = 0;
    range.processed = 0;
    range.state = STATE_ALPHA;

    do
    {
        size_t num_read;
        rc = KFileReadAll ( f, pos, ( char * )( range.start + range.processed ),
                        ( sizeof buffer ) - range.processed, &num_read );
        if ( rc == 0 )
        {
            done = ( num_read == 0 );
            if ( !done )
            {
                range.start = buffer;
                range.count = range.processed + num_read;

                rc = ProcessFromBuffer( &range, on_line, data );
                if ( range.count > 0 )
                {
                    memmove ( buffer, range.start, range.count );
                }
                range.start = buffer;
                range.processed = range.count;

                pos += num_read;
            }
            else if ( range.state == STATE_ALPHA )
            {
                String S;
                S.addr = range.start;
                S.len = S.size = range.count;
                rc = on_line( &S, data );
            }
        }
    } while ( rc == 0 && !done );

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

/***************************************************************************/

typedef struct replay_ctx_t
{
    log_sim_ctx_t * lctx;
    bool first;
    uint32_t seconds;
} replay_ctx_t;

static rc_t replay_callback( const String * line, void * data )
{
    rc_t rc = 0;
    replay_ctx_t * rctx = data;
    log_sim_ctx_t * self = rctx -> lctx;
    hms_t hms;

    if ( extract_time( line, &hms ) )
    {
        uint32_t seconds = to_seconds( hms.hour, hms.minute, hms.second );
        if ( rctx -> first  )
        {
            /* the first line in the log: records it's time + print*/
            rctx -> seconds = seconds;
            rctx -> first = false;
            rc = dst_print( self, "%S\n", line );
        }
        else
        {
            if ( seconds < rctx -> seconds )
            {
                /* jumping back in time? print it if no skipping requested */
                if ( !( self -> skip ) )
                    rc = dst_print( self, "%S\n", line );
            }
            else if ( seconds == rctx -> seconds )
            {
                /* same second as last one? print it.... */
                rc = dst_print( self, "%S\n", line );
            }
            else
            {
                /* in the future? wait the difference in seconds then print, update last */
                uint32_t int_sleep_time;
                double float_sleep_time = seconds - rctx -> seconds;
                /* adjust for slow-down/speed-up - factor */
                float_sleep_time *= 1000;
                float_sleep_time *= self -> speed;
                int_sleep_time = ceil( float_sleep_time );
                rc = KSleepMs( int_sleep_time );
                if ( rc == 0 )
                {
                    rc = dst_print( self, "%S\n", line );
                    rctx -> seconds = seconds;
                }
            }
        }

        if ( rc == 0 && self -> count > 0 && self -> line_nr > self -> count )
            rc = SILENT_RC( rcExe, rcNoTarg, rcVisiting, rcRange, rcExhausted );
    }
    return rc;
}

static rc_t replay( log_sim_ctx_t * self )
{
    replay_ctx_t rctx = { self, true, 0 };
    rc_t rc = ProcessLineByLine( self -> src_file,  replay_callback, &rctx );
    if ( rc != 0 )
    {
        rc_t rc1 = SILENT_RC( rcExe, rcNoTarg, rcVisiting, rcRange, rcExhausted );
        if ( rc != rc1 )
            LOGERR ( klogInt, rc, "relpay() failed" );
    }
    return rc;
}

/***************************************************************************/

typedef struct analyze_ctx_t
{
    log_sim_ctx_t * lctx;
    hms_t hms;
    bool empty;
    uint32_t num_events;
    uint64_t size_events;
} analyze_ctx_t;

static bool hms_diff( const hms_t * hms_1, const hms_t * hms_2 )
{
    if ( hms_1 -> second != hms_2 -> second ) return true;
    if ( hms_1 -> minute != hms_2 -> minute ) return true;
    if ( hms_1 -> hour != hms_2 -> hour ) return true;
    return false;
}

static void set_actx( analyze_ctx_t * actx, hms_t *hms, uint32_t len )
{
    actx -> hms . hour = hms -> hour;
    actx -> hms . minute = hms -> minute;
    actx -> hms . second = hms -> second;
    actx -> num_events = 1;
    actx -> size_events = len;
}

static rc_t analyze_print( analyze_ctx_t * actx )
{
    rc_t rc = dst_print( actx -> lctx,
        "%d\t%d\t%d\t%d\t%d\n",
        actx -> hms . hour, actx -> hms . minute, actx -> hms . second, actx -> num_events, actx -> size_events );
    return rc;
}

static rc_t analyze_callback( const String * line, void * data )
{
    rc_t rc = 0;
    analyze_ctx_t * actx = data;
    log_sim_ctx_t * self = actx -> lctx;
    hms_t hms;

    if ( extract_time( line, &hms ) )
    {
        if ( actx -> empty )
        {
            set_actx( actx, &hms, line -> len );
            actx -> empty = false;
        }
        else
        {
            if ( hms_diff( &hms, &( actx -> hms ) ) )
            {
                rc = analyze_print( actx );
                set_actx( actx, &hms, line -> len );
            }
            else
            {
                ( actx -> num_events ) ++;
                ( actx -> size_events ) += line -> len;
            }
        }
    }
    if ( rc == 0 && self -> count > 0 && self -> line_nr > self -> count )
        rc = SILENT_RC( rcExe, rcNoTarg, rcVisiting, rcRange, rcExhausted );
    return rc;
}

static rc_t analyze( log_sim_ctx_t * self )
{
    analyze_ctx_t actx = { self, { 0, 0, 0 }, true, 0, 0 };

    rc_t rc = ProcessLineByLine( self -> src_file,  analyze_callback, &actx );
    if ( rc != 0 )
    {
        rc_t rc1 = SILENT_RC( rcExe, rcNoTarg, rcVisiting, rcRange, rcExhausted );
        if ( rc != rc1 )
            LOGERR ( klogInt, rc, "analyze() failed" );
    }
    else
    {
        if ( !actx . empty )
            rc = analyze_print( &actx );
    }
    return rc;
}

/***************************************************************************/

static rc_t presort_callback( const String * line, void * data )
{
    rc_t rc = 0;
    log_sim_ctx_t * self = data;
    hms_t hms;

    if ( extract_time( line, &hms ) )
        rc = dst_print( self, "%02d%02d%02d\t%S\n", hms . hour, hms . minute, hms . second, line );

    if ( rc == 0 && self -> count > 0 && self -> line_nr > self -> count )
        rc = SILENT_RC( rcExe, rcNoTarg, rcVisiting, rcRange, rcExhausted );

    return rc;
}

static rc_t presort( log_sim_ctx_t * self )
{
    rc_t rc = ProcessLineByLine( self -> src_file,  presort_callback, self );
    if ( rc != 0 )
    {
        rc_t rc1 = SILENT_RC( rcExe, rcNoTarg, rcVisiting, rcRange, rcExhausted );
        if ( rc != rc1 )
            LOGERR ( klogInt, rc, "presort() failed" );
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

typedef uint8_t rand_ip_t[ 4 ];

static void make_rand_ip( unsigned long *x, rand_ip_t * ip )
{
    for ( int i = 0; i < 4; i++ )
        (*ip)[ i ] = (uint8_t)simple_rand_range( x, 0, 255 );
}

static int tzoff( int tzoff_min )
{
    int h = tzoff_min / 60;
    int m = tzoff_min - ( h * 60 );
    return ( h * 100 + m );
}

typedef struct month_t { char name[ 4 ]; } month_t;

typedef struct random_ctx_t
{
    log_sim_ctx_t * lctx;
    unsigned long rnd;
    month_t months[ 12 ];
} random_ctx_t;

static rc_t generate_random_event( random_ctx_t * rctx )
{
    rc_t rc;
    rand_ip_t ip;
    unsigned long acc = simple_rand_range( &( rctx -> rnd ), 1, 1000000 );
    log_sim_ctx_t * self = rctx -> lctx;
    KTime t;
    int tz;
    char sign;

    make_rand_ip( &( rctx -> rnd ), &ip );
    KTimeLocal( &t, KTimeStamp() );
    tz = tzoff( t.tzoff );
    sign = ( tz < 0 ) ? '-' : '+';
    if ( tz < 0 ) tz = -tz;
    rc = dst_print( self,
        "%d.%d.%d.%d - - [%02d/%s/%04d:%02d:%02d:%02d %c%04d] \"%s\" \"GET /path/path/SRR%06d HTTP/1.1\" 206 32768 0.00 \"toolkit.2.9.1\" - port=443 rl=293\n",
        ip[ 0 ], ip[ 1 ], ip[ 2 ], ip[ 3 ],
        t.day + 1, rctx -> months[ t.month ].name, t.year, t.hour, t.minute, t.second, sign, tz,
        self -> domain, acc
        );
    if ( rc == 0 ) rc = Quitting();
    if ( rc == 0 && self -> minwait > 0 && self -> maxwait > 0 )
    {
        unsigned long wait = simple_rand_range( &( rctx -> rnd ), self -> minwait, self -> maxwait );
        rc = KSleepMs( wait );
    }
    return rc;
}

static rc_t generate_random( log_sim_ctx_t * self )
{
    rc_t rc = 0;
    random_ctx_t rctx = { self, KTimeStamp() };
    month_t m[ 12 ] = { {"Jan"}, {"Feb"}, {"Mar"}, {"Apr"}, {"May"}, {"Jun"}, {"Jul"}, {"Aug"}, {"Sep"}, {"Oct"}, {"Nov"}, {"Dec"} };
    
    for ( int i = 0; i < 12; ++i )
        for ( int j = 0; j < 4; ++j )
            rctx . months[ i ].name[ j ] = m[ i ].name[ j ];

    if ( self -> count > 0 )
    {
        unsigned long i;
        for ( i = 0; rc == 0 && i < self -> count; i++ )
        {
            rc = generate_random_event( &rctx );
        }
    }
    else
    {
        while( rc == 0 )
        {
            rc = generate_random_event( &rctx );
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
                switch ( lctx . fn )
                {
                    case fn_replay  : rc = replay( &lctx ); break;
                    case fn_random  : rc = generate_random( &lctx ); break;
                    case fn_analyze : rc = analyze( &lctx ); break;
                    case fn_presort : rc = presort( &lctx ); break;
                }
                /* ======================================== */
                destroy_log_sim_ctx( &lctx );
            }
        }
    }
    return rc;
}
