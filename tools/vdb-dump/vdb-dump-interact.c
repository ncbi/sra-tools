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

#include "vdb-dump-interact.h"
#include "vdb-dump-helper.h"

#include <klib/vector.h>
#include <klib/namelist.h>
#include <kfs/file.h>
#include <kfs/filetools.h>
#include <kfg/config.h>
#include <kfg/repository.h>
#include <va_copy.h>

rc_t Quitting();

typedef struct ictx
{
    const dump_context * ctx;
    const Args * args;
    const KFile * std_in;
    bool interactive, done;
    
    String PROMPT;
} ictx;


static rc_t init_ictx( struct ictx * ictx, const dump_context * ctx, const Args * args )
{
    rc_t rc = KFileMakeStdIn ( &( ictx->std_in ) );
    DISP_RC( rc, "KFileMakeStdIn() failed" );
    if ( rc == 0 )
    {
        ictx->ctx = ctx;
        ictx->args = args;
        ictx->interactive = ( KFileType ( ictx->std_in ) == kfdCharDev );
        ictx->done = false;
        
        CONST_STRING( &(ictx->PROMPT), "\nvdb $" );
    }
    return rc;
}


static void release_ictx( ictx * ctx )
{
    KFileRelease( ctx->std_in );
}


static bool matches( const String * cmd, const String * pattern )
{
    char buffer[ 256 ];
    String match;
    uint32_t matching;
    
    StringInit( &match, buffer, sizeof buffer, 0 );
    matching = StringMatch( &match, cmd, pattern );
    return ( matching == pattern->len && matching == cmd->len );
}


static int32_t index_of_match( const String * word, uint32_t num, ... )
{
    int32_t res = -1;
    if ( word != NULL )
    {
        uint32_t idx;
        va_list args;
        
        va_start ( args, num );
        for ( idx = 0; idx < num && res < 0; ++idx )
        {
            const char * arg = va_arg ( args, const char * );
            if ( arg != NULL )
            {
                String S;
                StringInitCString( &S, arg );
                if ( matches( word, &S ) ) res = idx;
            }
        }
        va_end ( args );
    }
    return res;
}


static uint32_t copy_String_2_vector( Vector * v, const String * S )
{
    uint32_t res = 0;
    if ( S->len > 0 && S->addr != NULL )
    {
        String * S1 = malloc( sizeof * S1 );
        if ( S1 != NULL )
        {
            rc_t rc;
            StringInit( S1, S->addr, S->size, S->len );
            rc = VectorAppend( v, NULL, S1 );
            if ( rc == 0 ) res++; else free( S1 );
        }
    }
    return res;
}

static uint32_t split_buffer( Vector * v, const char * buffer, size_t len, const char * delim )
{
    uint32_t i, res = 0;
    size_t delim_len = string_size( delim );
    String S;
    
    StringInit( &S, NULL, 0, 0 );
    VectorInit( v, 0, 10 );    
    for( i = 0; i < len; ++i )
    {
        if ( string_chr( delim, delim_len, buffer[ i ] ) != NULL )
        {
            /* delimiter found */
            res += copy_String_2_vector( v, &S );
            StringInit( &S, NULL, 0, 0 );
        }
        else
        {
            /* normal char in line */
            if ( S.addr == NULL ) S.addr = &( buffer[ i ] );
            S.size++;
            S.len++;
        }
    }
    res += copy_String_2_vector( v, &S );
    return res;
}


static void CC destroy_String( void * item, void * data ) { free( item ); }
static void destroy_String_vector( Vector * v ) { VectorWhack( v, destroy_String, NULL ); }

/* ------------------------------------------------------------------------------------------------- */

static const char * s_BadCategory       = "BadCategory";
static const char * s_UserCategory      = "UserCategory";
static const char * s_SiteCategory      = "SiteCategory";
static const char * s_RemoteCategory    = "RemoteCategory";
static const char * s_UnknownCategory   = "unknow category";

static const char * KRepCategory_to_str( const KRepCategory c )
{
    switch( c )
    {
        case krepBadCategory        : return s_BadCategory; break;
        case krepUserCategory       : return s_UserCategory; break;
        case krepSiteCategory       : return s_SiteCategory; break;
        case krepRemoteCategory     : return s_RemoteCategory; break;
    };
    return s_UnknownCategory;
}

static const char * s_Prefix_bad        = "bad";
static const char * s_Prefix_user       = "user";
static const char * s_Prefix_site       = "site";
static const char * s_Prefix_remote     = "remote";
static const char * s_Prefix_unknown    = "unknow";

static const char * KRepCategory_to_prefix( const KRepCategory c )
{
    switch( c )
    {
        case krepBadCategory        : return s_Prefix_bad; break;
        case krepUserCategory       : return s_Prefix_user; break;
        case krepSiteCategory       : return s_Prefix_site; break;
        case krepRemoteCategory     : return s_Prefix_remote; break;
    };
    return s_Prefix_unknown;
}


static const char * s_BadSubCategory            = "BadSubCategory";
static const char * s_MainSubCategory           = "MainSubCategory";
static const char * s_AuxSubCategory            = "AuxSubCategory";
static const char * s_ProtectedSubCategory      = "ProtectedSubCategory";

static const char * KRepSubCategory_to_str( const KRepSubCategory c )
{
    switch( c )
    {
        case krepBadSubCategory         : return s_BadSubCategory; break;
        case krepMainSubCategory        : return s_MainSubCategory; break;
        case krepAuxSubCategory         : return s_AuxSubCategory; break;
        case krepProtectedSubCategory   : return s_ProtectedSubCategory; break;
    }
    return s_UnknownCategory;
}

static const char * s_yes = "yes";
static const char * s_no  = "no";

static const char * yes_or_no( const bool flag )
{
    return flag ? s_yes : s_no;
}

typedef rc_t ( CC * repofunc )( const KRepository *self, char *buffer, size_t bsize, size_t * size );

static rc_t vdi_report_repo_str( const KRepository * repo, const char * elem_name, repofunc f )
{
    if ( f != NULL )
    {
        char buffer[ 4096 ];
        rc_t rc = f( repo, buffer, sizeof buffer, NULL );
        if ( rc == 0 ) return KOutMsg( "     - %s : %s\n", elem_name, buffer );
    }
    return 0;
}

static rc_t vdi_report_repository( const KRepository * repo, const char * prefix, int idx, bool full )
{
    KRepCategory cat = KRepositoryCategory( repo );
    KRepSubCategory subcat = KRepositorySubCategory( repo );
    rc_t rc = KOutMsg( "  repo.%s #%d:\n", prefix, idx );
    if ( rc == 0 )
        rc = KOutMsg( "     - category : %s.%s\n", KRepCategory_to_str( cat ), KRepSubCategory_to_str( subcat ) );
    if ( rc == 0 ) rc = vdi_report_repo_str( repo, "name", KRepositoryName );
    if ( full )
    {
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "displayname", KRepositoryDisplayName );
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "root", KRepositoryRoot );
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "resolver", KRepositoryResolver );
        
        if ( rc == 0 )
            rc = KOutMsg( "     - disabled : %s\n", yes_or_no( KRepositoryDisabled( repo ) ) );
        if ( rc == 0 )
            rc = KOutMsg( "     - cached : %s\n", yes_or_no( KRepositoryCacheEnabled( repo ) ) );
            
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "ticket", KRepositoryDownloadTicket );
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "key", KRepositoryEncryptionKey );
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "keyfile", KRepositoryEncryptionKeyFile );
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "desc", KRepositoryDescription );    
        
        if ( rc == 0 )
        {
            uint32_t prj_id;
            rc_t rc1 = KRepositoryProjectId( repo, &prj_id );
            if ( rc1 == 0 ) rc = KOutMsg( "     - prj-id : %d\n", prj_id );
        }
    }
    
    if ( rc == 0 ) rc = KOutMsg( "\n" );
    return rc;
}

typedef rc_t ( CC * catfunc )( const KRepositoryMgr *self, KRepositoryVector *user_repositories );

static catfunc vdi_get_catfunc( const KRepCategory cat )
{
    switch( cat )
    {
        case krepUserCategory       : return KRepositoryMgrUserRepositories; break;
        case krepSiteCategory       : return KRepositoryMgrSiteRepositories; break;
        case krepRemoteCategory     : return KRepositoryMgrRemoteRepositories; break;
    };
    return NULL;
}

static rc_t vdi_report_repo_vector( const KRepositoryMgr * repomgr, const KRepCategory cat,
                    int32_t select, bool full )
                                    
{
    rc_t rc = 0;
    catfunc f = vdi_get_catfunc( cat );
    if ( f != NULL )
    {
        KRepositoryVector v;
        rc = f( repomgr, &v );
        if ( rc == 0 )
        {
            const char * prefix = KRepCategory_to_prefix( cat );
            uint32_t idx, len = VectorLength( &v );
            bool disabled = KRepositoryMgrCategoryDisabled( repomgr, cat );
            rc = KOutMsg( "repo.%s --> disabled: %s, %d subrepositories )\n", prefix, yes_or_no( disabled ), len );
            for ( idx = 0; rc == 0 && idx < len; ++idx )
            {
                if ( select == idx || !full )
                    rc = vdi_report_repository( VectorGet( &v, idx ), prefix, idx, full );
            }
            
            KRepositoryVectorWhack( &v );
        }
    }
    return rc;
}


static rc_t vdi_repo_all( const KRepositoryMgr * repomgr, bool full )
{
    rc_t rc = vdi_report_repo_vector( repomgr, krepUserCategory, -1, full );
    if ( rc == 0 )
        rc = vdi_report_repo_vector( repomgr, krepSiteCategory, -1, full );
    if ( rc == 0 )
        rc = vdi_report_repo_vector( repomgr, krepRemoteCategory, -1, full );
    return rc;
}

static rc_t vdi_repo( const Vector * v )
{
    KConfig * cfg;
    rc_t rc = KConfigMake( &cfg, NULL );
    if ( rc == 0 )
    {
        const KRepositoryMgr * repomgr;
        rc = KConfigMakeRepositoryMgrRead( cfg, &repomgr );
        {
            if ( VectorLength( v ) < 2 )
                rc = vdi_repo_all( repomgr, true );
            else
            {
                String * which_repo = VectorGet( v, 1 );
                if ( which_repo != NULL )
                {
                    int32_t idx1 = index_of_match( which_repo, 4, "user", "site", "remote", "all" );
                    if ( VectorLength( v ) > 2 )
                    {
                        String * S2 = VectorGet( v, 2 );
                        int32_t select = ( int32_t )string_to_I64( S2->addr, S2->len, NULL );
                        switch( idx1 )
                        {
                            case 0 : rc = vdi_report_repo_vector( repomgr, krepUserCategory, select, true ); break;
                            case 1 : rc = vdi_report_repo_vector( repomgr, krepSiteCategory, select, true ); break;
                            case 2 : rc = vdi_report_repo_vector( repomgr, krepRemoteCategory, select, true ); break;
                            default : rc = KOutMsg( "unknow repository '%S'", which_repo ); break;
                        }
                        
                    }
                    else
                    {
                        switch( idx1 )
                        {
                            case 0 : rc = vdi_report_repo_vector( repomgr, krepUserCategory, -1, false ); break;
                            case 1 : rc = vdi_report_repo_vector( repomgr, krepSiteCategory, -1, false ); break;
                            case 2 : rc = vdi_report_repo_vector( repomgr, krepRemoteCategory, -1, false ); break;
                            case 3 : rc = vdi_repo_all( repomgr, false );
                            default : rc = KOutMsg( "unknow repository '%S'", which_repo ); break;
                        }
                    }
                }
            }
            
            KRepositoryMgrRelease( repomgr );
        }
        KConfigRelease ( cfg );
    }
    return rc;
}


static rc_t vdi_test( const Vector * v )
{
    rc_t rc = 0;
    uint32_t start = VectorStart( v );
    uint32_t len = VectorLength( v );
    uint32_t idx;
    for ( idx = start; rc == 0 && idx < ( start + len ); ++idx )
        rc = KOutMsg( "{%S} ", VectorGet( v, idx ) );
    return rc;
}


static rc_t vdi_top_help()
{
    rc_t rc = KOutMsg( "help:\n" );
    if ( rc == 0 )
        rc = KOutMsg( "quit / exit : terminate program\n" );
    if ( rc == 0 )
        rc = KOutMsg( "help [cmd]  : print help [ for this topic ]\n" );
    if ( rc == 0 )
        rc = KOutMsg( "repo        : manage repositories\n" );
    return rc;
}


static rc_t vdi_help_on_help()
{
    rc_t rc = KOutMsg( "help: [help]\n" );
    return rc;
}

static rc_t vdi_help_on_repo()
{
    rc_t rc = KOutMsg( "help: [repo]\n" );
    return rc;
}

static rc_t vdi_help( const Vector * v )
{
    rc_t rc = 0;
    if ( VectorLength( v ) < 2 )
        rc = vdi_top_help();
    else
    {
        int32_t cmd_idx = index_of_match( VectorGet( v, 1 ), 2,
            "help", "repo" );
        switch( cmd_idx )
        {
            case 0 : rc = vdi_help_on_help(); break;
            case 1 : rc = vdi_help_on_repo(); break;
        }
    }
    return rc;
}


static rc_t vdi_handle_buffer( ictx * ctx, const char * buffer, size_t buffer_len )
{
    rc_t rc = 0;
    Vector v;
    uint32_t args = split_buffer( &v, buffer, buffer_len, " \t" );
    if ( args > 0 )
    {
        const String * S = VectorGet( &v, 0 );
        if ( S != NULL )
        {
            int32_t cmd_idx = index_of_match( S, 6,
                "quit", "exit", "help", "repo", "test" );
                
            ctx->done = ( cmd_idx == 0 || cmd_idx == 1 );
            if ( !ctx->done )
            {
                switch( cmd_idx )
                {
                    case 2  : rc = vdi_help( &v ); break;
                    case 3  : rc = vdi_repo( &v ); break;
                    case 4  : rc = vdi_test( &v ); break;
                    default : rc = KOutMsg( "??? {%S}", S ); break;
                }
                if ( rc == 0 )
                {
                    if ( ctx->interactive )
                        rc = KOutMsg( "%S", &( ctx->PROMPT ) );
                    else
                        rc = KOutMsg( "\n" );
                }
            }
        }
    }
    destroy_String_vector( &v );
    return rc;
}


static rc_t vdi_interactive_loop( ictx * ctx )
{
    char one_char[ 4 ], buffer[ 4096 ];
    uint64_t pos = 0;
    size_t buffer_idx = 0;
    rc_t rc = KOutMsg( "%S", &( ctx->PROMPT ) );    

    while ( rc == 0 && !ctx->done && ( 0 == Quitting() ) )
    {
        size_t num_read;
        rc = KFileRead( ctx->std_in, pos, one_char, 1, &num_read );
        if ( rc != 0 )
            LOGERR ( klogErr, rc, "failed to read stdin" );
        else
        {
            pos += num_read;
            if ( one_char[ 0 ] == '\n' )
            {
                rc = vdi_handle_buffer( ctx, buffer, buffer_idx ); /* <---- */
                buffer_idx = 0;
            }
            else
            {
                if ( buffer_idx < ( ( sizeof buffer ) - 1 ) )
                {
                    buffer[ buffer_idx++ ] = one_char[ 0 ];
                }
                else
                {
                    rc = KOutMsg( "\ntoo long!%s", &( ctx->PROMPT ) );
                    buffer_idx = 0;
                }
            }
        }
    }
    KOutMsg( "\n" );
    return rc;
}


static rc_t on_line( const String * line, void * data )
{
    ictx * ctx = data;
    rc_t rc = Quitting();
    if ( rc == 0 ) rc = vdi_handle_buffer( ctx, line->addr, line->size );
    return rc;
}


rc_t vdi_main( const dump_context * ctx, const Args * args )
{
    ictx ictx;
    rc_t rc = init_ictx( &ictx, ctx, args );
    if ( rc == 0 )
    {
        if ( ictx.interactive )
            rc = vdi_interactive_loop( &ictx );
        else
            rc = ProcessFileLineByLine( ictx.std_in, on_line, &ictx ); /* from kfs/filetools.h */
            
        release_ictx( &ictx );
    }
    return rc;
}