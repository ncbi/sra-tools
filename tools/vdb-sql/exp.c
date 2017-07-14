

#include <kapp/main.h>
#include <kapp/args.h>

#include <klib/log.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/rc.h>

#include <kfc/xcdefs.h>
#include <kfc/except.h>
#include <kfc/ctx.h>
#include <kfc/rsrc.h>

#include <stdlib.h>

/*
#include <../libs/ngs/NCBI-NGS.h>
*/
#include <../libs/ngs/NGS_ReadCollection.h>
#include <../libs/ngs/NGS_Read.h>
#include <../libs/ngs/NGS_String.h>
/*
#include <ngs/itf/ErrBlock.h>
*/

#define OPTION_A "A"
#define ALIAS_A "A"
static const char * usage_A[] = { "A", NULL };

OptDef ToolOptions[] =
{
    { OPTION_A, ALIAS_A, NULL, usage_A, 1, false,  false }
};

const char UsageDefaultName[] = "exp";

static rc_t ErrMsg( const char * fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;

    va_list list;
    va_start( list, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
    if ( rc == 0 )
        rc = pLogMsg( klogErr, "$(E)", "E=%s", buffer );
    va_end( list );
    return rc;
} 

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s <path> [<path> ...] [options]\n"
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

    if ( rc )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    HelpOptionsStandard ();

    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}


static void the_function( const char * acc )
{
    NGS_ReadCollection * m_coll;
    
    HYBRID_FUNC_ENTRY ( rcSRA, rcRow, rcAccessing );

    m_coll = NGS_ReadCollectionMake( ctx, acc );
    if ( !FAILED() )
    {
        NGS_Read * m_read;
        
        KOutMsg( "our read-collection is open!\n" );
        m_read = NGS_ReadCollectionGetReads( m_coll, ctx, true, true, true );
        if ( !FAILED() )
        {
            uint64_t count = 0;
            KOutMsg( "we have reads!\n" );
        
            while ( NGS_ReadIteratorNext( m_read, ctx ) && count < 10 )
            {
                NGS_String * m_seq = NGS_ReadGetReadSequence( m_read, ctx, 0, ( size_t ) - 1 );
                if ( !FAILED() )
                {
                    const char * s = NGS_StringData( m_seq, ctx );
                    size_t n = NGS_StringSize( m_seq, ctx );
                    KOutMsg( "%.*s\n", n, s );
                    NGS_StringRelease( m_seq, ctx );
                }
                ++count;
            }
            NGS_ReadRelease ( m_read, ctx );
        }
        else
        {
            KOutMsg( "we have no reads!\n" );
            CLEAR();
        }
        NGS_RefcountRelease( ( NGS_Refcount * ) m_coll, ctx );
    }
    else
    {
        KOutMsg( "accession failed to open!\n" );
        CLEAR();
    }
}

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle( &args, argc, argv,
            1, ToolOptions, sizeof ToolOptions / sizeof ToolOptions [ 0 ] );
    if ( rc != 0 )
        ErrMsg( "ArgsMakeAndHandle() -> %R", rc );
    else
    {
        the_function( "SRR341578" );
        ArgsWhack( args );
    }
    return rc;
}
