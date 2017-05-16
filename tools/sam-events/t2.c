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
#include <kapp/args.h>

#include <klib/out.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/printf.h>

#include <kproc/thread.h>
#include <kproc/queue.h>
#include <kproc/timeout.h>

#include <stdio.h>
#include <ctype.h>

/* use the allele-dictionary to reduce the alleles per position */
#include "common.h"

/* use the alignment-iterator to read from a csra-file */
#include "alignment_iter.h"

#include "expandCIGAR.h"
#include "alig_consumer.h"

const char UsageDefaultName[] = "t2";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [<path> ...] [options]\n"
                     "\n", progname );
}

#define OPTION_CACHE  "cache"
#define ALIAS_CACHE   "c"
static const char * cache_usage[]     = { "the lookup-cache to use", NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_CACHE,     ALIAS_CACHE,    NULL, cache_usage,     1,   true,        false }
};

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    uint32_t i;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg( "Options:\n" );

    HelpOptionsStandard ();
    for ( i = 0; i < sizeof ( ToolOptions ) / sizeof ( ToolOptions[ 0 ] ); ++i )
        HelpOptionLine ( ToolOptions[ i ].aliases, ToolOptions[ i ].name, NULL, ToolOptions[ i ].help );

    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

typedef struct tool_ctx
{
    const char * cache_file;
} tool_ctx;


static rc_t fill_out_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc = get_charptr( args, OPTION_CACHE, &ctx->cache_file );
    if ( rc == 0 )
    {

    }
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

typedef struct consumer_thread_data
{
    KQueue * payload;
    const char * acc;
} consumer_thread_data;


static rc_t CC consumer_thread_function( const KThread * self, void * data )
{
    rc_t rc = 0;
    consumer_thread_data * ctd = data;
    struct alig_consumer * consumer;    
    alig_consumer_data ac_data;
    
    ac_data.fasta = loadcSRA( ctd->acc, 1024 * 1024 );
    memset( &ac_data.limits, 0, sizeof ac_data.limits );
    ac_data.lookup = NULL;
    ac_data.slice = NULL;
    ac_data.purge = 5000;
    ac_data.dict_strategy = 0;
    
    rc = alig_consumer_make( &consumer, &ac_data );
    if ( rc == 0 )
    {
        bool running = true;    
        uint64_t processed = 0;
        KQueue * q_in  = ctd->payload;
        while ( running )
        {
            AlignmentT * al;
            timeout_t t;
            TimeoutInit( &t, 100 );
            if ( KQueuePop( q_in, ( void ** )&al, &t ) != 0 )
                running = !KQueueSealed( q_in );
            else
            {
                /* now let us process that aligment... */
                rc = alig_consumer_consume_alignment( consumer, al );

                free_alignment_copy( al );
                processed++;
            }
        }
        alig_consumer_release( consumer );
    }
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

static rc_t run( Args * args, uint32_t count, tool_ctx * ctx )
{
    rc_t rc = 0;
    uint32_t idx;
    const char * acc;
    for ( idx = 0; rc == 0 && idx < count; ++idx )
    {
        rc = ArgsParamValue( args, idx, ( const void ** )&acc );
        if ( rc != 0 )
            log_err( "ArgsParamValue( %d ) failed %R", idx, rc );
        else
        {
            struct alig_iter * ai;
            rc = alig_iter_csra_make( &ai, acc, 1024 * 1024 * 32, NULL );
            if ( rc == 0 )
            {
                consumer_thread_data ctd;
                rc = KQueueMake( &ctd.payload, 32 );
                if ( rc == 0 )
                {
                    KThread * consumer_thread;
                    ctd.acc = acc;
                    rc = KThreadMake( &consumer_thread, consumer_thread_function, &ctd );
                    if ( rc == 0 )
                    {
                        uint32_t t_quit = 0;
                        bool running = true;
                        while ( running )
                        {
                            AlignmentT alignment;
                            running = alig_iter_get( ai, &alignment );
                            if ( running )
                            {
                                /* we have to make a copy to throw the alignment over the 'fence'
                                into a Queue and be processed inside a thread... */
                                AlignmentT * A = copy_alignment( &alignment );
                                if ( A != NULL )
                                {
                                    bool pushing = true;
                                    timeout_t t;
                                    while ( running && pushing )
                                    {
                                        TimeoutInit( &t, 100 );
                                        pushing = ( KQueuePush( ctd.payload, A, &t ) != 0 );
                                    }
                                }
                            }
                            if ( ++t_quit > 1000 )
                            {
                                running = ( Quitting() == 0 );
                                t_quit = 0;
                            }
                        }
                        KQueueSeal( ctd.payload );
                        KThreadWait( consumer_thread, NULL );
                        KThreadRelease( consumer_thread );
                    }
                    KQueueRelease ( ctd.payload );
                }
                alig_iter_release( ai );
            }
        }
    }
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                ToolOptions, sizeof ( ToolOptions ) / sizeof ( OptDef ) );
    if ( rc != 0 )
        log_err( "ArgsMakeAndHandle() failed %R", rc );
    else
    {
        tool_ctx ctx;
        rc = fill_out_tool_ctx( args, &ctx );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = ArgsParamCount( args, &count );
            if ( rc != 0 )
                log_err( "ArgsParamCount() failed %R", rc );
            else
                rc = run( args, count, &ctx );
        }
        ArgsWhack ( args );
    }
    return rc;
}
