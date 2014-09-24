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

#include <os-native.h>
#include <sysalloc.h>

#include "last_rowid.h"
#include "line_token_iter.h"
#include <klib/log.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/namelist.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <stdlib.h>


typedef struct sg_entry
{
    BSTNode node;

    const String * spot_group;
    const String * full_filename;
    uint32_t batch;
} sg_entry;


static int CC String_entry_cmp ( const void * item, const BSTNode * n )
{
    const String * name = ( const String * ) item;
    const sg_entry * sg = ( const sg_entry * ) n;
    return StringCompare ( sg->spot_group, name );
}


static int CC entry_entry_cmp ( const BSTNode * item, const BSTNode * n )
{
    const sg_entry * sg1 = ( const sg_entry * ) item;
    const sg_entry * sg2 = ( const sg_entry * ) n;
    return StringCompare ( sg2->spot_group, sg1->spot_group );
}


static rc_t make_sg_entry( const String * spot_group, const String * full_filename, uint32_t batch, sg_entry ** entry )
{
    rc_t rc = 0;
    ( *entry ) = malloc( sizeof ** entry );
    if ( *entry == NULL )
    {
        rc = RC( rcExe, rcDatabase, rcReading, rcMemory, rcExhausted );
        (void)LOGERR( klogErr, rc, "memory exhausted when creating new spotgroup-lookup-entry" );
    }
    else
    {
        rc = StringCopy ( &( ( *entry )->spot_group ), spot_group );
        if ( rc == 0 )
            rc = StringCopy ( &( (*entry)->full_filename ), full_filename );
        if ( rc == 0 )
            (*entry)->batch = batch;
    }
    return rc;
}


rc_t insert_file( BSTree * entries, const char * filename )
{
    rc_t rc;
    String S;
    struct token_iter ti;

    StringInitCString( &S, filename );
    rc = token_iter_init( &ti, &S, '.' );
    if ( rc == 0 )
    {
        bool valid;
        String basename;

        rc = token_iter_get( &ti, &basename, &valid, NULL );
        if ( rc == 0 && valid )
        {
            String sg;
            const char * ptr;
            char s_batch[ 4 ];
            uint32_t batch;
            sg_entry * entry;

            StringInit( &sg, basename.addr, basename.size - 4, basename.size - 4 );
            ptr = ( basename.addr + sg.size + 1 );
            s_batch[ 0 ] = *ptr++;
            s_batch[ 1 ] = *ptr++;
            s_batch[ 2 ] = *ptr++;
            s_batch[ 3 ] = 0;
            batch = atoi( s_batch );

            entry = ( sg_entry * ) BSTreeFind ( entries, ( void * )&sg, String_entry_cmp );
            if ( entry == NULL )
            {
                rc = make_sg_entry( &sg, &S, batch, &entry );
                if ( rc == 0 )
                    rc = BSTreeInsert ( entries, ( BSTNode * )entry, entry_entry_cmp );
            }
            else
            {
                if ( batch > entry->batch )
                    entry->batch = batch;
            }
        }
    }
    return rc;
}


static void CC whack_entry( BSTNode * n, void * data )
{
    const sg_entry * sg = ( const sg_entry * ) n;
    StringWhack ( sg->spot_group );
    StringWhack ( sg->full_filename );
    free( ( void * ) sg );
}


typedef struct walk_ctx
{
    rc_t rc;
    int64_t last_row_id;
    const KDirectory *dir;
} walk_ctx;


enum ft
{
    ft_none = 0,    /* it is uncompressed tvs */
    ft_gzip,        /* use gzip */
    ft_bzip,        /* use bzip2 */
    ft_unknown      /* unknown extension */
};


enum ft get_compression( const String * filename )
{
    enum ft res = ft_unknown;
    const char * ptr = filename->addr + ( filename->size - 3 );
    if ( ptr[ 0 ] == 'b' )
    {
        if ( ptr[ 1 ] == 'z' && ptr[ 2 ] == '2' )
            res = ft_bzip;
    }
    else if ( ptr[ 0 ] == 'g' )
    {
        if ( ptr[ 1 ] == 'z' )
            res = ft_gzip;
    }
    else if ( ptr[ 0 ] == 't' )
    {
        if ( ptr[ 1 ] == 's' && ptr[ 2 ] == 'v' )
            res = ft_none;
    }
    return res;
}


static rc_t raw_extract_last_rowid( const KFile * f, int64_t * row_id )
{
    rc_t rc = 0;
    KOutMsg( "uncompressed tsv-file\n" );
    return rc;
}


static rc_t gzip_extract_last_rowid( const KFile * f, int64_t * row_id )
{
    rc_t rc = 0;
    KOutMsg( "compressed with gzip\n" );
    return rc;
}


static rc_t bzip_extract_last_rowid( const KFile * f, int64_t * row_id )
{
    const KFile *bz;
    rc_t rc = KFileMakeBzip2ForRead ( &bz, f );
    if ( rc != 0 )
    {

    }
    else
    {

    }
    return rc;
}


static rc_t extract_last_rowid( const KDirectory *dir, const String * filename, int64_t * row_id )
{
    const KFile * f;
    rc_t rc = KDirectoryOpenFileRead ( dir, &f, "%.*s", filename->size, filename->addr );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot open $(fn)", "fn=%.*s", filename->size, filename->addr ) );
    }
    else
    {
        enum ft comp = get_compression( filename );
        KOutMsg( "file '%S' opened :", filename );

        switch ( comp )
        {
            case ft_none : raw_extract_last_rowid( f, row_id ); break;
            case ft_gzip : gzip_extract_last_rowid( f, row_id ); break;
            case ft_bzip : bzip_extract_last_rowid( f, row_id ); break;
            default      : KOutMsg( "unknown file-type\n" ); break;
        }
        rc = KFileRelease ( f );
    }
    return rc;
}


static bool CC on_file ( BSTNode *n, void *data )
{
    walk_ctx * wctx = ( walk_ctx * )data;
    if ( wctx != NULL && wctx->rc == 0 )
    {
        const sg_entry * sg = ( const sg_entry * ) n;
        if ( sg != NULL )
        {
            size_t num_writ, l;
            l = sg->spot_group->size + 1;
            wctx->rc = string_printf ( ( char * )( sg->full_filename->addr + l ), 4, &num_writ, "%.03u", sg->batch );
            if ( wctx->rc == 0 )
            {
                int64_t row_id = 0;
                ( ( char * ) sg->full_filename->addr )[ l + 3 ] = '.';
                wctx->rc = extract_last_rowid( wctx->dir, sg->full_filename, &row_id );
                if ( wctx->rc == 0 )
                {
                    if ( row_id > wctx->last_row_id )
                        wctx->last_row_id = row_id;
                    return false;
                }
            }
        }
    }
    return true;
}


rc_t discover_last_rowid( const char * src, int64_t * last_row_id )
{
    KDirectory *dir;
    rc_t rc = KDirectoryNativeDir ( &dir );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot create native directory" );
    }
    else
    {
        const KDirectory * src_dir;
        rc = KDirectoryOpenDirRead ( dir, &src_dir, false, "%s", src );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot open directory #$(d)", "d=%s", src ) );
        }
        else
        {
            KNamelist *files;
            rc = KDirectoryList ( src_dir, &files, NULL, NULL, "." );
            if ( rc != 0 )
            {
                (void)LOGERR( klogErr, rc, "cannot list files in output directory" );
            }
            else
            {
                uint32_t count;
                rc = KNamelistCount ( files, &count );
                if ( rc != 0 )
                {
                    (void)LOGERR( klogErr, rc, "cannot count the files in the output directory" );
                }
                else
                {
                    rc = KOutMsg( "%u files in '%s' found\n", count, src );
                    if ( rc == 0 )
                    {
                        uint32_t idx;
                        BSTree entries;

                        BSTreeInit( &entries );
                        for ( idx = 0; idx < count && rc == 0; ++idx )
                        {
                            const char * filename;
                            rc = KNamelistGet ( files, idx, &filename );
                            if ( rc != 0 )
                            {
                                (void)PLOGERR( klogErr, ( klogErr, rc, "cannot retrieve filename #$(idx)", "idx=%u", idx ) );
                            }
                            else
                                rc = insert_file( &entries, filename );
                        }

                        if ( rc == 0 )
                        {
                            walk_ctx wctx;

                            wctx.rc = rc;
                            wctx.last_row_id = *last_row_id;
                            wctx.dir = src_dir;
                            BSTreeDoUntil ( &entries, false, on_file, &wctx );
                            if ( wctx.rc == 0 )
                            {
                                *last_row_id = wctx.last_row_id;
                            }
                            else
                                rc = wctx.rc;
                        }
                        BSTreeWhack ( &entries, whack_entry, NULL );
                    }
                }
                KNamelistRelease ( files );
            }
            KDirectoryRelease( src_dir );
        }
        KDirectoryRelease( dir );
    }
    return rc;
}