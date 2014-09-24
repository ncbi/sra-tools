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

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/database.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/container.h>
#include <klib/vector.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <fmtdef.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define WITH_DNA

/********************************************************************
write procedure:
(1)....KDirectoryNativeDir           ... make a root directory
(2)....KDirectoryOpenDirUpdate       ... chroot to specific directory
(3)....VDBManagerMakeUpdate          ... make manager for update
(4)....VDBManagerMakeSchema          ... make schema
(5)....VSchemaParseFile              ... load a schema from file
(6)....VDBManagerCreateDB            ... let the manager create a DB
(7)....VDatabaseCreateTable          ... create a Table for the DB
(8)....VTableCreateCursorWrite       ... create a writable cursor
(9)....VCursorAddColumn              ... add the column to write
(10)...VCursorOpen                   ... open the cursor
-----LOOP-----
 (11)....VCursorOpenRow              ... open the row
 (12)....VCursorWrite                ... write the data
 (13)....VCursorCommitRow            ... commit the row
 (14)....VCursorCloseRow             ... close the row
-----END LOOP-----
(15)...VCursorCommit                 ... commit the cursor
(16)...VCursorRelease                ... destroy the cursor
(17)...VTableRelease                 ... destroy the table
(18)...VDatabaseRelease              ... destroy the database
(19)...VSchemaRelease                ... destroy the schema
(20)...VDBManagerRelease             ... destroy the manager
(21)...KDirectoryRelease             ... destroy the chroot'ed dir
(22)...KDirectoryRelease             ... destroy the root dir
********************************************************************/


/********************************************************************
helper function needed by schema-dump
********************************************************************/
static
rc_t CC flush ( void *dst, const void *buffer, size_t bsize )
{
    FILE *f = dst;
    fwrite ( buffer, 1, bsize, f );
    return 0;
}

/********************************************************************
helper function to display failure or success message
********************************************************************/
static
void display_rescode( const rc_t res, const char* failure, const char* success )
{
    if ( res != 0 )
    {
        if ( failure )
            LOGERR( klogInt, res, failure );
    }
    else
    {
        if ( success )
            LOGMSG( klogInfo, success );
    }
}

/********************************************************************
helper function to display the manager version
********************************************************************/
static
rc_t show_manager_version( VDBManager *my_manager )
{
    uint32_t version;
    rc_t res = VDBManagerVersion( my_manager, &version );
    if ( res != 0 )
    {
        LOGERR ( klogInt, res, "failed to determine vdb mgr version" );
    }
    else
    {
        PLOGMSG ( klogInfo, ( klogInfo, "manager-version = $(maj).$(min).$(rel)",
                  "vers=0x%X,maj=%u,min=%u,rel=%u"
                              , version
                              , version >> 24
                              , ( version >> 16 ) & 0xFF
                              , version & 0xFFFF ));
    }
    return res;
}

/* write context */
typedef struct write_ctx
{
    uint32_t idx_line;
    uint32_t idx_line_nr;
    uint32_t idx_dna;
    uint32_t idx_quality;
    uint32_t line_nr;
    void *buffer;
    uint32_t count;
} write_ctx;
typedef write_ctx* p_write_ctx;

p_write_ctx make_write_ctx( void )
{
    return calloc( 1, sizeof( write_ctx ) );
}

/********************************************************************
helper function to write one data-cunk into one new row
********************************************************************/
static
rc_t write_data_row( VCursor *my_cursor, p_write_ctx ctx )
{
    rc_t res = VCursorOpenRow( my_cursor );
    /* display_rescode( res, "failed to open row", "row opened" ); */
    if ( res == 0 )
    {
        /* write the ascii-line*/
        res = VCursorWrite( my_cursor, ctx->idx_line, 8, ctx->buffer, 0, ctx->count );
        display_rescode( res, "failed to write ascii-line", NULL );

        /* write the line-number (zero-based) */
        res = VCursorWrite( my_cursor, ctx->idx_line_nr, 32, &(ctx->line_nr), 0, 1 );
        display_rescode( res, "failed to write line-nr", NULL );

#ifdef WITH_DNA
        /* write the ascii-line interpreted as dna-data */
        res = VCursorWrite( my_cursor, ctx->idx_dna, 2, ctx->buffer, 0, ctx->count << 2 );
        display_rescode( res, "failed to write dna-data", NULL );
#endif

        /* write the ascii-line interpreted as quality (array of 4 byte) */
        res = VCursorWrite( my_cursor, ctx->idx_quality, 32, ctx->buffer, 0, ctx->count >> 2 );
        display_rescode( res, "failed to write quality", NULL );
        
        res = VCursorCommitRow( my_cursor );
        display_rescode( res, "failed to commit row", NULL );

        res = VCursorCloseRow( my_cursor );
    }
    return res;
}

#define BUFLEN 120

/********************************************************************
reads the data-src-file in chunks and writes it with "write_data_chunk"
********************************************************************/
static
rc_t write_data_loop( VCursor *my_cursor, p_write_ctx ctx, const KFile *data_src )
{
    char buf[ BUFLEN ];
    char* read_dest = buf;
    uint64_t file_pos = 0;
    size_t num_to_read = BUFLEN - 1;
    size_t num_read;
    size_t chars_in_buf = 0;
    size_t written = 0;
    rc_t res;

    ctx->line_nr = 0;
    ctx->buffer = buf;
    do {
        res = KFileRead( data_src, file_pos, read_dest, num_to_read, &num_read );
        if ( res != 0 )
        {
            LOGERR ( klogInt, res, "failed to read form KFile" );
            num_read = 0;
        }
        else
        {
            if ( num_read > 0 )
            {
                char* lf;
                chars_in_buf+=num_read;
                file_pos+=num_read;
                do {
                    buf[chars_in_buf] = 0;
                    lf = strchr( buf, 0x0A );
                    if ( lf != NULL )
                    {
                        ctx->count = ( lf - buf );
                        res = write_data_row( my_cursor, ctx );
                        if ( res == 0 )
                        {
                            ctx->line_nr++;
                            written += ctx->count;
                            chars_in_buf -= ( ctx->count + 1 );
                            memmove( buf, ++lf, chars_in_buf );
                        }
                        else
                        {
                            num_read = 0; /* terminates the outer do-loop */
                        }
                    }
                } while ( ( lf != NULL )&&( num_read > 0 ) );
                read_dest = buf + chars_in_buf;
                num_to_read = BUFLEN - ( chars_in_buf + 1 );
            }
        }
    } while ( num_read > 0 );

    PLOGMSG ( klogInfo, ( klogInfo, "written $(n_lines) lines = $(n_bytes) bytes",
                          "n_lines=%lu,n_bytes=%lu", ctx->line_nr, written ));
    if ( ctx->line_nr > 0 )
    {
        res = VCursorCommit( my_cursor );
        display_rescode( res, "failed to commit cursor", NULL );
    }
    return res;
}


/********************************************************************
creates a writable cursor, adds 1 column, opens cursor and calls the write-loop
********************************************************************/
static
rc_t create_cursor_and_write( VTable *my_table, const KFile *data_src  )
{
    VCursor *my_cursor;
    p_write_ctx ctx = make_write_ctx();
    uint32_t open_cols = 0, should_open = 0;
    
    rc_t res = VTableCreateCursorWrite( my_table, &my_cursor, kcmInsert );
    display_rescode( res, "failed to create write-cursor", NULL );
    if ( res == 0 )
    {
        res = VCursorAddColumn( my_cursor, &ctx->idx_line, "LINE" );
        display_rescode( res, "failed to add LINE-column", NULL );
        if ( res == 0 ) open_cols++;
        should_open++;
        
        res = VCursorAddColumn( my_cursor, &ctx->idx_line_nr, "LINE_NR" );
        display_rescode( res, "failed to add LINE_NR-column", NULL );
        if ( res == 0 ) open_cols++;
        should_open++;
        
#ifdef WITH_DNA
        res = VCursorAddColumn( my_cursor, &ctx->idx_dna, "READ" );
        display_rescode( res, "failed to add READ-column", NULL );
        if ( res == 0 ) open_cols++;
        should_open++;        
#endif

        res = VCursorAddColumn( my_cursor, &ctx->idx_quality, "QUALITY" );
        display_rescode( res, "failed to add QUALITY-column", NULL );
        if ( res == 0 ) open_cols++;        
        should_open++;

        if ( open_cols == should_open )
        {
            res = VCursorOpen( my_cursor );
            display_rescode( res, "failed to open cursor", NULL );
            if ( res == 0 )
            {
                res = write_data_loop( my_cursor, ctx, data_src );
            }
        }
        res = VCursorRelease( my_cursor );
    }
    free( ctx );
    return res;
}

/********************************************************************
let the manager create a database with a schema, calls cursor-creation and write
********************************************************************/
static
rc_t create_dababase_from_schema( VDBManager *my_manager, VSchema *my_schema, 
                                  const char* db_path, const KFile *data_src  )
{
    VDatabase *my_database;
    VTable *my_table;
    rc_t res = VDBManagerCreateDB( my_manager, &my_database, my_schema, 
                                   "DATA2DUMP:MyDatabase", kcmInit | kcmParents, "%s", db_path );
    display_rescode( res, "failed to create database", "database created" );
    if ( res == 0 )
    {
        res = VDatabaseCreateTable( my_database, &my_table, "test", kcmInit | kcmParents, "Tab1" );
        display_rescode( res, "failed to create table", "table created" );
        if ( res == 0 )
        {
            create_cursor_and_write( my_table, data_src );
            res = VTableRelease( my_table );
            display_rescode( res, "failed to release table", "table released" );
        }
        res = VDatabaseRelease( my_database );
        display_rescode( res, "failed to release database", "database released" );
    }
    return res;
} 

/********************************************************************
let the manager create a schema, calls dababase-creation etc.
********************************************************************/
static
rc_t create_schema( VDBManager *my_manager, const char *schema_path, 
                    const char *db_path, const KFile *data_src  )
{
    VSchema *schema;
    /* empty schema, unless $VDB_ROOT tells it
       where to find include files */
    rc_t res = VDBManagerMakeSchema ( my_manager, & schema );
    display_rescode( res, "failed to create schema", "schema created" );
    if ( res == 0 )
    {
        /* load the schema from text */
        res = VSchemaParseFile ( schema, "%s", schema_path );
        if ( res != 0 )
        {
            PLOGERR ( klogInt,  (klogInt, res, "failed to load schema file '$(path)'",
                                 "path=%s", schema_path ));
        }
        else
        {
            res = VSchemaDump ( schema, sdmPrint, "DATA2DUMP:MyDatabase", flush, stdout );
            if ( res != 0 )
                LOGERR ( klogInt, res, "failed to dump schema" );
            else
            {
                res = create_dababase_from_schema( my_manager, schema, db_path, data_src );
            }
        }
        res = VSchemaRelease ( schema );
        display_rescode( res, "failed to release schema", "schema release" );
    }
    return res;
}

static
rc_t build_test_db ( const char *schema_path, const char *db_path, const char *src_file )
{
    KDirectory *wd;
    rc_t res = KDirectoryNativeDir ( & wd );
    if ( res != 0 )
        LOGERR ( klogSys, res, "failed to determine wd" );
    else
    {
        KDirectory *root;

#if 1 /* SHOULD WORK WHEN DISABLED
         ENABLED FOR NOW AS A WORKAROUND */
        root = wd;
        KDirectoryAddRef ( root );
#else
        res = KDirectoryOpenDirUpdate ( wd, & root, true, "." );
#endif
        if ( res != 0 )
            LOGERR ( klogSys, res, "failed to chroot to wd" );
        else
        {
            VDBManager *my_manager;
            res = VDBManagerMakeUpdate ( & my_manager, root );
            if ( res != 0 )
                LOGERR ( klogInt, res, "failed to open vdb manager" );
            else
            {
                const KFile *data_src;
                res = KDirectoryOpenFileRead ( wd, &data_src, "%s", src_file );
                display_rescode( res, "failed to open src-file", "src-file opened" );
                if ( res == 0 )
                {
                    show_manager_version( my_manager );
                    res = create_schema( my_manager, schema_path, db_path, data_src );
                    VDBManagerRelease ( my_manager );
                    KFileRelease( data_src );
                }
            }
            KDirectoryRelease ( root );
        }
        KDirectoryRelease ( wd );
    }
    return res;
}


ver_t CC KAppVersion ( void )
{
    return 0;
}

rc_t CC Usage (const Args * args)
{
    return 0;
}

rc_t CC Version ( const Args * args )
{
    return 0;
}

/****************************************************************************************

                    argv[1]            argv[2]    argv[3]
usage  : vdb-boot   shema              db_path    source-file
example: vdb-boot   vdb-boot.vschema   test       vdb-boot.c

 ***************************************************************************************/
rc_t CC KMain ( int argc, char *argv [] )
{
    if ( argc != 4 )
    {
        LOGMSG ( klogFatal, "aaaah!" );
        return -1;
    }
    return build_test_db ( argv[1], argv[2], argv[3] );
}
