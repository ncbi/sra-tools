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
/* #include "sampart.vers.h" */

#include <kapp/main.h>
#include <kapp/args.h>

#include <klib/out.h>
#include <klib/rc.h>
#include <klib/printf.h>
#include <klib/text.h>

#include <os-native.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "refbases.h"
#include "cigar.h"

#define DFLT_FUNCTION       "read"
#define DFLT_REFNAME        "NC_011752.1"
#define DFLT_REFPOS         10000
#define DFLT_CIGAR          "50M"
#define DFLT_INSBASES       "ACGTACGTACGT"
#define DFLT_LEN            50
#define DFLT_SEED           7

static const char * function_usage[]       = { "which function to execute ( read, qual, cigar, rlen )", NULL };
static const char * refname_usage[]        = { "the ref-seq-id to use 'NC_011752.1'", NULL };
static const char * refpos_usage[]         = { "the position on the reference 0-based", NULL };
static const char * cigar_usage[]          = { "the cigar-string to use", NULL };
static const char * insbases_usage[]       = { "what bases to insert ( if needed )", NULL };
static const char * len_usage[]            = { "length of random quality", NULL };
static const char * seed_usage[]           = { "seed for random", NULL };

#define OPTION_FUNCTION       "function"
#define OPTION_REFNAME        "refname"
#define OPTION_REFPOS         "refpos"
#define OPTION_CIGAR          "cigar"
#define OPTION_INSBASES       "insbases"
#define OPTION_LEN            "len"
#define OPTION_SEED           "seed"

#define ALIAS_FUNCTION       "f"
#define ALIAS_REFNAME        "r"
#define ALIAS_REFPOS         "p"
#define ALIAS_CIGAR          "c"
#define ALIAS_INSBASES       "i"
#define ALIAS_LEN            "l"
#define ALIAS_SEED           "s"

OptDef Options[] =
{
    { OPTION_FUNCTION,  ALIAS_FUNCTION, NULL, function_usage,  1,    true,     false },
    { OPTION_REFNAME,   ALIAS_REFNAME,  NULL, refname_usage,   1,    true,     false },
    { OPTION_REFPOS,    ALIAS_REFPOS,   NULL, refpos_usage,    1,    true,     false },
    { OPTION_CIGAR,     ALIAS_CIGAR,    NULL, cigar_usage,     1,    true,     false },
    { OPTION_INSBASES,  ALIAS_INSBASES, NULL, insbases_usage,  1,    true,     false },
    { OPTION_LEN,       ALIAS_LEN,      NULL, len_usage,       1,    true,     false },
    { OPTION_SEED,      ALIAS_SEED,     NULL, seed_usage,      1,    true,     false }
};

const char UsageDefaultName[] = "sampart";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg( "\nUsage:\n %s [options]\n\n", progname );
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    int i, n_options;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );
    KOutMsg ( "Options:\n" );

    n_options = sizeof Options / sizeof Options[ 0 ];
    for ( i = 0; i < n_options; ++i )
    {
        OptDef * o = &Options[ i ];
        HelpOptionLine( o->aliases, o->name, NULL, o->help );
    }

    KOutMsg ( "\n" );
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}


/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
/*
ver_t CC KAppVersion ( void )
{
    return 0x030000001;
}
*/

static const char * get_str_option( const Args * args, const char * name, uint32_t idx, const char * dflt )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( ( rc == 0 )&&( count > idx ) )
    {
        const char * res = NULL;
        ArgsOptionValue( args, name, idx, (const void **)&res );
        return res;
    }
    else
        return dflt;
}

static uint32_t get_uint32_option( const Args * args, const char * name, uint32_t idx, const uint32_t dflt )
{
    const char * s = get_str_option( args, name, idx, NULL );
    if ( s == NULL )
        return dflt;
    return atoi( s );
}

static rc_t CC write_to_FILE ( void *f, const char *buffer, size_t bytes, size_t *num_writ )
{
    * num_writ = fwrite ( buffer, 1, bytes, f );
    if ( * num_writ != bytes )
        return RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    return 0;
}

static rc_t make_read( const Args * args )
{
    rc_t rc = 0;
    const char * cigar_str = get_str_option( args, OPTION_CIGAR, 0, DFLT_CIGAR );
    struct cigar_t * cigar = make_cigar_t( cigar_str );
    if ( cigar != NULL )
    {
        uint32_t reflen        = cigar_t_reflen( cigar );
        uint32_t refpos        = get_uint32_option( args, OPTION_REFPOS, 0, DFLT_REFPOS );
        const char * refname   = get_str_option( args, OPTION_REFNAME, 0, DFLT_REFNAME );
        const char * refbases  = read_refbases( refname, refpos, reflen, NULL );
        if ( refbases != NULL )
        {
            int read_len = cigar_t_readlen( cigar );
            char * buffer = malloc( read_len + 1 );
            if ( buffer != NULL )
            {
                const char * ins_bases  = get_str_option( args, OPTION_INSBASES, 0, DFLT_INSBASES );
                cigar_t_2_read( buffer, read_len + 1, cigar, refbases, ins_bases );
                rc = KOutMsg( "%s", buffer );
                free( buffer );
            }
            free( ( void * )refbases );
        }
        free_cigar_t( cigar );
    }
    return rc;
}


static size_t random_string( char * buffer, size_t buflen, const char * char_set, size_t length )
{
    size_t res = 0;
    if ( buffer != NULL && buflen > 0 )
    {
        const char dflt_charset[] = "0123456789"
                                    "abcdefghijklmnopqrstuvwxyz"
                                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const char * cs = ( char_set == NULL ) ? dflt_charset : char_set;
        size_t charset_len = strlen( cs ) - 1;
        while ( res < length && res < ( buflen - 1 ) )
        {
            size_t rand_idx = ( double ) rand() / RAND_MAX * charset_len;
            buffer[ res++ ] = cs[ rand_idx ];
        }
        buffer[ res ] = 0;
    }
    return res;
}


static size_t random_quality( char * buffer, size_t buflen, size_t length )
{
    const char qualities[] = "!\"#$%&'()*+,-./0123456789:;<=>?"
                             "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                             "`abcdefghijklmnopqrstuvwxyz{|}~";
    return random_string( buffer, buflen, qualities, length );
}


static rc_t make_quality( const Args * args )
{
    rc_t rc = 0;
    uint32_t len  = get_uint32_option( args, OPTION_LEN, 0, DFLT_LEN );
    uint32_t seed = get_uint32_option( args, OPTION_SEED, 0, DFLT_SEED );
    char * buffer = malloc( len + 1 );
    if ( buffer != NULL )
    {
        if ( seed > 0 ) srand( seed );
        random_quality( buffer, len + 1, len );
        rc = KOutMsg( "%s", buffer );
        free( buffer );
    }
    return rc;
}

static rc_t make_cigar( const Args * args )
{
    rc_t rc = 0;
    const char * cigar_str = get_str_option( args, OPTION_CIGAR, 0, DFLT_CIGAR );
    struct cigar_t * cigar = make_cigar_t( cigar_str );
    if ( cigar != NULL )
    {
        char buffer[ 4096 ];
        struct cigar_t * merged_cigar = merge_cigar_t( cigar );
        cigar_t_string( buffer, sizeof buffer, merged_cigar );
        rc = KOutMsg( "%s", buffer );
        free_cigar_t( merged_cigar );
        free_cigar_t( cigar );
    }
    return rc;
}

static rc_t get_ref_len( const Args * args )
{
    rc_t rc = 0;
    const char * refname = get_str_option( args, OPTION_REFNAME, 0, DFLT_REFNAME );
    if ( refname != NULL )
        rc = KOutMsg( "%d", ref_len( refname ) );
    return rc;
}

static const char function_read[] = "read";
static const char function_qual[] = "qual";
static const char function_cigar[] = "cigar";
static const char function_rlen[] = "rlen";

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE( argc, argv, VDB_INIT_FAILED );

    rc_t rc = KOutHandlerSet ( write_to_FILE, stdout );
    if ( rc == 0 )
    {
        Args * args;

        int n_options = sizeof Options / sizeof Options[ 0 ];
        rc = ArgsMakeAndHandle( &args, argc, argv, 1,  Options, n_options );
        if ( rc == 0 )
        {
            const char * function = get_str_option( args, OPTION_FUNCTION, 0, DFLT_FUNCTION );

            if ( 0 == strcase_cmp ( function, string_size( function ), function_read, 4, 4 ) )
                rc = make_read( args );
            else if ( 0 == strcase_cmp ( function, string_size( function ), function_qual, 4, 4 ) )
                rc = make_quality( args );
            else if ( 0 == strcase_cmp ( function, string_size( function ), function_cigar, 5, 5 ) )
                rc = make_cigar( args );
            else if ( 0 == strcase_cmp ( function, string_size( function ), function_rlen, 4, 4 ) )
                rc = get_ref_len( args );
            else
                rc = KOutMsg( "unknown function '%s'\n", function );
            ArgsWhack( args );
        }
    }
    return VDB_TERMINATE( rc );
}
