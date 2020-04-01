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

static const char * input_usage[]  = { "input file to replay (dflt: random data", NULL };
static const char * output_usage[] = { "output file to be written to (dflt: stdout)", NULL };
static const char * force_usage[]  = { "overwrite output-file if it already exists", NULL };
static const char * append_usage[] = { "append to output-file if it already exists", NULL };
static const char * random_usage[] = { "generate random events - ignore input", NULL };
static const char * gunzip_usage[] = { "gunzip the input-file", NULL };

OptDef MyOptions[] =
{
	{ OPTION_INPUT, 		ALIAS_INPUT, 		NULL, 	input_usage, 		1, 	true, 	false },
	{ OPTION_OUTPUT, 		ALIAS_OUTPUT, 		NULL, 	output_usage, 		1, 	true, 	false },
	{ OPTION_FORCE, 		ALIAS_FORCE, 		NULL, 	force_usage, 		1, 	false, 	false },
	{ OPTION_APPEND, 		ALIAS_APPEND, 		NULL, 	append_usage, 		1, 	false, 	false },
	{ OPTION_RANDOM, 		ALIAS_RANDOM, 		NULL, 	random_usage, 		1, 	false, 	false },
	{ OPTION_GUNZIP, 		ALIAS_GUNZIP, 		NULL, 	gunzip_usage, 		1, 	false, 	false }
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

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );
    HelpOptionLine ( ALIAS_INPUT,	OPTION_INPUT, 	"filename",	input_usage );
    HelpOptionLine ( ALIAS_OUTPUT, 	OPTION_OUTPUT, 	"filename",	output_usage );
    HelpOptionLine ( ALIAS_FORCE, 	OPTION_FORCE, 	NULL,	force_usage );
    HelpOptionLine ( ALIAS_APPEND, 	OPTION_APPEND, 	NULL,	append_usage );
    HelpOptionLine ( ALIAS_RANDOM, 	OPTION_RANDOM, 	NULL,	random_usage );
    HelpOptionLine ( ALIAS_GUNZIP, 	OPTION_GUNZIP, 	NULL,	gunzip_usage );
    KOutMsg ( "\n" );
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

/***************************************************************************/
typedef struct log_sim_ctx_t
{
    /* input parameters */
    const char * src;
    const char * dst;
    bool force, append, random, gunzip;

    /* */
    KDirectory * dir;
    const KFile * src_file;
    KFile * dst_file;
    uint64_t dst_pos;
    uint64_t line_nr;
    //char buffer[ 4096 ];
    KDataBuffer data_buffer;
} log_sim_ctx_t;

static rc_t get_str_option( const Args *args, const char * option_name, const char ** value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
	if ( rc != 0 )
	{
		PLOGERR ( klogInt, ( klogInt, rc, "ArgsOptionCount( '$(key)' ) failed", "key=%s", option_name ) );
	}
	else
    {
        if ( count > 0 )
        {
            rc = ArgsOptionValue( args, option_name, 0, (const void **)value );
            if ( rc != 0 )
            {
                PLOGERR ( klogInt, ( klogInt, rc, "ArgsOptionValue( 0, '$(key)' ) failed", "key=%s", option_name ) );
            }
        }
        else
        {
            *value = NULL;
        }
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
    {
        *res = ( count > 0 );
    }
    return rc;
}

static rc_t gather_log_sim_ctx( log_sim_ctx_t * self, Args * args )
{
    rc_t rc = get_str_option( args, OPTION_INPUT, &( self -> src ) );
    if ( rc == 0 ) rc = get_str_option( args, OPTION_OUTPUT, &( self -> dst ) );
    self -> force = false;
    self -> append = false;
    self -> random = false;
    self -> gunzip = false;
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_FORCE, &( self -> force ) );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_APPEND, &( self -> append ) );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_RANDOM, &( self -> random ) );
    if ( rc == 0 ) rc = get_bool_option( args, OPTION_GUNZIP, &( self -> gunzip ) );

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

/***************************************************************************/

static rc_t replay_callback( const String * line, void * data )
{
    log_sim_ctx_t * self = data;
    rc_t rc = KDataBufferWipe( &( self -> data_buffer ) );
    if ( rc == 0 )
    {
        rc = KDataBufferPrintf( &( self -> data_buffer ), "[%lu] %S\n", self -> line_nr, line );
        if ( rc == 0 )
        {
            size_t num_writ;
            self -> line_nr += 1;
            rc = KFileWrite( self -> dst_file, self -> dst_pos, self -> data_buffer . base, self -> data_buffer . elem_count, &num_writ );
            if ( rc == 0 )
                self -> dst_pos += num_writ;
        }
    }
    return rc;
}

static rc_t replay( log_sim_ctx_t * self )
{
    rc_t rc = ProcessFileLineByLine( self -> src_file,  replay_callback, self );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "relpay() failed" );
    return rc;
}

/***************************************************************************/

static rc_t generate_random( log_sim_ctx_t * self )
{
    rc_t rc = 0;
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
