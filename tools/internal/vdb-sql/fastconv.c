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

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/log.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/time.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include "sqlite3.h"

static const char * format_usage[] = { "format ( fastq, fasta, default = fastq )", NULL };
#define OPTION_FORMAT   "format"
#define ALIAS_FORMAT     "f"

OptDef ToolOptions[] =
{
    { OPTION_FORMAT,    ALIAS_FORMAT,    NULL, format_usage,     1, true,   false }
};

const char UsageDefaultName[] = "fastconv";

rc_t CC UsageSummary( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [options]\n"
                     "\n", progname );
}

rc_t ErrMsg( const char * fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;

    va_list list;
    va_start( list, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
    if ( rc == 0 )
        /* rc = KOutMsg( "%s\n", buffer ); */
        /* rc = pLogErr( klogErr, 1, "$(E)", "E=%s", buffer ); */
        rc = pLogMsg( klogErr, "$(E)", "E=%s", buffer );
    va_end( list );
    return rc;
}

rc_t CC Usage ( const Args * args )
{
    rc_t rc;
    uint32_t idx, count = ( sizeof ToolOptions ) / ( sizeof ToolOptions[ 0 ] );
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;

    if ( args == NULL )
        rc = RC( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram( args, &fullpath, &progname );

    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary( progname );

    KOutMsg( "Options:\n" );
    for ( idx = 1; idx < count; ++idx ) /* start with 1, do not advertize row-range-option*/
        HelpOptionLine( ToolOptions[ idx ].aliases, ToolOptions[ idx ].name, NULL, ToolOptions[ idx ].help );

    HelpOptionsStandard();
    HelpVersion( fullpath, KAppVersion() );
    return rc;
}

typedef struct writer
{
    char buffer [ 4096 ];
    KFile * f;
    uint64_t pos;
} writer;

static rc_t make_writer( writer ** wrt, const char * filename, ... )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc != 0 )
        ErrMsg( "KDirectoryNativeDir() -> %R", rc );
    else
    {
        KFile * f;
        va_list list;

        va_start( list, filename );
        rc = KDirectoryVCreateFile ( dir, &f, false, 0664, kcmInit, filename, list );
        if ( rc == 0 )
        {
            writer * w = malloc( sizeof * w );
            if ( w == NULL )
            {
                rc = RC( rcApp, rcNoTarg, rcOpening, rcMemory, rcExhausted );
                KFileRelease( f );
            }
            else
            {
                w -> f = f;
                w -> pos = 0;
                *wrt = w;
            }
        }
        va_end( list );
        KDirectoryRelease( dir );
    }
    return rc;
}

static void release_writer( writer * self )
{
    if ( self != NULL )
    {
        KFileRelease( self -> f );
        free( ( void * ) self );
    }
}


static rc_t write_writer( writer * self, const char * fmt, ... )
{
    rc_t rc;
    size_t num_writ;
    va_list list;

    va_start( list, fmt );
    rc = string_vprintf( self -> buffer, sizeof self -> buffer, &num_writ, fmt, list );
    va_end( list );
    if ( rc == 0 )
    {
        size_t num_writ2;
        rc = KFileWriteAll( self -> f, self -> pos, self -> buffer, num_writ, &num_writ2 );
        if ( rc == 0 )
            self -> pos += num_writ2;
    }
    return rc;

}

/* {"a":[123, 456]} */
void split_prim_ids( const char * s, uint64_t * ids )
{
    uint32_t id = 0;
    uint32_t state = 0;
    String S;
    while ( s[ id ] != 0 )
    {
        switch( s[ id ] )
        {
            case '[' : state = 1; break;
            case ',' : ids[ 0 ] = StringToU64( &S, NULL ); state = 3; break;
            case ']' : ids[ 1 ] = StringToU64( &S, NULL ); state = 5; break;
            default : switch( state )
                       {
                            case 1 : S.addr = &s[ id ]; S.len = 1; S.size = 1; state = 2; break;
                            case 2 : S.len += 1; S.size += 1; break;
                            case 3 : S.addr = &s[ id ]; S.len = 1; S.size = 1; state = 4; break;
                            case 4 : S.len += 1; S.size += 1; break;
                       }
        }
        id++;
    }
}

static rc_t activate_vdb_module( void )
{
    /* prototype for the extension in sqlite3vdb.c ( which has no header-file... ) */
    int sqlite3_vdbsqlite_init( sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi );
    typedef void ( *entrypoint )( void );

    rc_t rc = 0;
    int res = sqlite3_auto_extension( ( entrypoint )sqlite3_vdbsqlite_init );
    if ( res != SQLITE_OK )
    {
        ErrMsg( "cannot load the vdb-module" );
        rc = RC( rcApp, rcNoTarg, rcOpening, rcItem, rcInvalid );
    }
    return rc;
}

static rc_t execute_stm( sqlite3 * db, const char * sql, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;
    va_list list;

    va_start( list, sql );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, sql, list );
    va_end( list );
    if ( rc == 0 )
    {
        char *err = NULL;
        int res = sqlite3_exec( db, buffer, NULL, NULL, &err );
        if ( res != SQLITE_OK )
        {
            ErrMsg( "sqlite3_exec( %s ) -> %s", sql, err );
            sqlite3_free( err );
            rc = RC( rcApp, rcNoTarg, rcOpening, rcItem, rcInvalid );
        }
    }
    return rc;
}

static rc_t prepare_stm( sqlite3 * db, const char * sql, sqlite3_stmt ** stm )
{
    rc_t rc = 0;
    char *err = NULL;
    int res = sqlite3_prepare_v2( db, sql, -1, stm, NULL );
    if ( res != SQLITE_OK )
    {
        ErrMsg( "sqlite3_prepare_v2( %s ) -> %s", sql, err );
        sqlite3_free( err );
        rc = RC( rcApp, rcNoTarg, rcOpening, rcItem, rcInvalid );
    }
    return rc;
}

static void get_ALIG_READ( sqlite3 * db, sqlite3_stmt * ALIG, int parameter_index, uint64_t PRIM_ID, const char ** alig_read )
{
    int res = sqlite3_reset( ALIG );
    *alig_read = NULL;
    if ( res != SQLITE_OK )
        ErrMsg( "get_ALIG_values(): sqlite3_reset() failed %s", sqlite3_errmsg( db ) );
    else
    {
        res = sqlite3_bind_int64( ALIG, parameter_index, PRIM_ID );
        if ( res != SQLITE_OK )
            ErrMsg( "get_ALIG_values(): sqlite3_bind_int64( %ld ) failed %s", PRIM_ID, sqlite3_errmsg( db ) );
        else
        {
            res = sqlite3_step( ALIG );
            if ( res != SQLITE_ROW )
                ErrMsg( "get_ALIG_values(): sqlite3_step( %ld ) failed %s", PRIM_ID, sqlite3_errmsg( db ) );
            else
                *alig_read = ( const char * )sqlite3_column_text( ALIG, 1 );
        }
    }

}

static rc_t iterate_SRC_SEQ( sqlite3 * db, const char * acc, writer * wrt )
{
    sqlite3_stmt * SEQ;
    sqlite3_stmt * ALIG;
    uint64_t prim_id[ 2 ];

    rc_t rc = prepare_stm( db, "select * from SRC_SEQ", &SEQ );
    if ( rc == 0 )
    {
        rc = prepare_stm( db, "select * from ALIG where ALIGN_ID = @PARAM", &ALIG );
        if ( rc == 0 )
        {
            int resSEQ  = sqlite3_step( SEQ );
            int parameterIndex = sqlite3_bind_parameter_index( ALIG, "@PARAM" );

            while ( rc == 0 && resSEQ == SQLITE_ROW )
            {
                sqlite3_int64 SPOT_ID = sqlite3_column_int64( SEQ, 0 );
                const char * PRIMARY_ALIGNMENT_ID = ( const char * )sqlite3_column_text( SEQ, 1 );
                split_prim_ids( PRIMARY_ALIGNMENT_ID, prim_id );

                /* rc = write_writer( wrt, "%ld [ %ld, %ld ]\n", SPOT_ID, prim_id[ 0 ], prim_id[ 1 ] ); */

                if ( prim_id[ 0 ] == 0 && prim_id[ 1 ] == 0 )
                {
                    const char * CMP_READ = ( const char * )sqlite3_column_text( SEQ, 2 );
                    write_writer( wrt, "[%ld] 0.0 %s\n", SPOT_ID, CMP_READ );
                }
                else
                {
                    if ( prim_id[ 0 ] > 0 && prim_id[ 1 ] > 0 )
                    {
                        const char * ALIG_READ = NULL;
                        write_writer( wrt, "[%ld] 1.1 ", SPOT_ID );
                        get_ALIG_READ( db, ALIG, parameterIndex, prim_id[ 0 ], &ALIG_READ );
                        write_writer( wrt, "%s ", ALIG_READ );
                        get_ALIG_READ( db, ALIG, parameterIndex, prim_id[ 1 ], &ALIG_READ );
                        write_writer( wrt, "%s\n", ALIG_READ );
                    }
                    else
                    {
                        const char * ALIG_READ = NULL;
                        const char * CMP_READ = ( const char * )sqlite3_column_text( SEQ, 2 );
                        if ( prim_id[ 0 ] > 0 )
                        {
                            get_ALIG_READ( db, ALIG, parameterIndex, prim_id[ 0 ], &ALIG_READ );
                            write_writer( wrt, "[%ld] 1.0 %s %s\n", SPOT_ID, ALIG_READ, CMP_READ );
                        }
                        else
                        {
                            get_ALIG_READ( db, ALIG, parameterIndex, prim_id[ 1 ], &ALIG_READ );
                            write_writer( wrt, "[%ld] 1.0 %s %s\n", SPOT_ID, CMP_READ, ALIG_READ );
                        }
                    }
                }
                resSEQ  = sqlite3_step( SEQ );
            }

            if ( resSEQ != SQLITE_DONE )
                ErrMsg( "iterate_SRC_SEQ() -> %s", sqlite3_errmsg( db ) );

            sqlite3_finalize( ALIG );
        }
        sqlite3_finalize( SEQ );
    }
    return rc;
}


static rc_t convert( const char * acc, writer * wrt )
{
    rc_t rc = activate_vdb_module();
    if ( rc == 0 )
    {
        char buffer[ 4096 ];
        size_t num_writ;

        rc = string_printf( buffer, sizeof buffer, &num_writ, "%s.db", acc );
        if ( rc == 0 )
        {
            sqlite3 * db;
            int res = sqlite3_open( buffer, &db );
            if ( res != SQLITE_OK )
            {
                rc = RC( rcApp, rcNoTarg, rcOpening, rcItem, rcInvalid );
                ErrMsg( "cant open %s", buffer );
            }
            else
            {
                KTime_t t;

                rc = execute_stm( db,
                    "create virtual table SRC_ALIG using vdb( %s, table = PRIMARY_ALIGNMENT, columns = ALIGN_ID;READ )",
                    acc );

                if ( rc == 0 )
                    rc = execute_stm( db, "create table ALIG( ALIGN_ID INTEGER PRIMARY KEY, READ )" );

                KOutMsg( "start copy\n" );
                t = KTimeStamp();

                if ( rc == 0 )
                    rc = execute_stm( db, "insert into ALIG select * from SRC_ALIG" );

                KOutMsg( "done copy ( t = %ld )\n", KTimeStamp() - t );
                t = KTimeStamp();

                if ( rc == 0 )
                    rc = execute_stm( db,
                        "create virtual table SRC_SEQ using vdb( %s, table = SEQUENCE, columns = SPOT_ID;PRIMARY_ALIGNMENT_ID;CMP_READ;(INSDC:quality:text:phred_33)QUALITY )",
                        acc );

                KOutMsg( "start iterating\n" );
                t = KTimeStamp();

                if ( rc == 0 )
                    rc = iterate_SRC_SEQ( db, acc, wrt );

                KOutMsg( "done iterating ( t = %ld )\n", KTimeStamp() - t );

                sqlite3_close( db );
            }
        }
    }
    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    rc_t rc;
    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    uint32_t num_options = sizeof ToolOptions / sizeof ToolOptions [ 0 ];

    rc = ArgsMakeAndHandle ( &args, argc, argv, 1, ToolOptions, num_options );
    if ( rc != 0 )
        ErrMsg( "ArgsMakeAndHandle() -> %R", rc );
    if ( rc == 0 )
    {
        const char * acc;
        rc = ArgsParamValue( args, 0, (const void **)&acc );
        if ( rc != 0 )
            ErrMsg( "ArgsParamValue() -> %R", rc );
        else
        {
            writer * wrt;
            rc = make_writer( &wrt, "%s.fastq", acc );
            if ( rc == 0 )
            {
                rc = convert( acc, wrt );
                release_writer( wrt );
            }
        }
        ArgsWhack( args );
    }

    return VDB_TERMINATE( rc );
}
