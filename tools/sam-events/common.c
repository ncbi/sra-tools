/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnologmsgy Information
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
#include "common.h"

#include <klib/printf.h>
#include <klib/log.h>
#include <klib/out.h>

#include <kfs/directory.h>
#include <kfs/file.h>

rc_t log_err( const char * t_fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;
    
    va_list args;
    va_start( args, t_fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, t_fmt, args );
    va_end( args );
    if ( rc == 0 )
        rc = LogMsg( klogErr, buffer );
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */


rc_t add_cols_to_cursor( const VCursor *curs, uint32_t * idx_array,
        const char * tbl_name, const char * acc, uint32_t n, ... )
{
    rc_t rc = 0;
    uint32_t i;
    va_list args;
    
    va_start( args, n );
    
    for ( i = 0; i < n; i++ )
    {
        const char * colname = va_arg( args, const char * );
        rc = VCursorAddColumn( curs, &( idx_array[ i ] ), colname );
        if ( rc != 0 )
            log_err( "cannot add '%s' to cursor for '%s'.%s %R", colname, acc, tbl_name, rc );
    }
    
    va_end( args );
    
    if ( rc == 0 )
    {
        rc = VCursorOpen( curs );
        if ( rc != 0 )
            log_err( "cannot open cursor for '%s'.%s %R", acc, tbl_name, rc );
    }
    
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */


void inspect_sam_flags( AlignmentT * al, uint32_t sam_flags )
{
    al->fwd = ( ( sam_flags & 0x10 ) == 0 );
    if ( ( sam_flags & 0x01 ) == 0x01 )
        al->first = ( ( sam_flags & 0x40 ) == 0x40 );
    else
        al->first = true;
}


/* ----------------------------------------------------------------------------------------------- */


rc_t get_bool( const Args * args, const char *option, bool *value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option, &count );
    *value = ( rc == 0 && count > 0 );
    return rc;
}


rc_t get_charptr( const Args * args, const char *option, const char ** value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option, &count );
    if ( rc == 0 && count > 0 )
    {
        rc = ArgsOptionValue( args, option, 0, ( const void ** )value );
        if ( rc != 0 )
            *value = NULL;
    }
    else
        *value = NULL;
    return rc;
}


rc_t get_uint32( const Args * args, const char *option, uint32_t * value, uint32_t dflt )
{
    const char * svalue;
    rc_t rc = get_charptr( args, option, &svalue );
    if ( rc == 0 && svalue != NULL )
        *value = atoi( svalue );
    else
        *value = dflt;
    return 0;
}

size_t string_2_size_t( const char * s, size_t dflt )
{
    size_t res = dflt;
    if ( s != NULL )
    {
        size_t l = string_size( s );
        if ( l > 0 )
        {
            size_t multipl = 1;
            switch( s[ l - 1 ] )
            {
                case 'k' :
                case 'K' : multipl = 1024; break;
                case 'm' :
                case 'M' : multipl = 1024 * 1024; break;
                case 'g' :
                case 'G' : multipl = 1024 * 1024 * 1024; break;
            }
            
            if ( multipl > 1 )
            {
                char * src = string_dup( s, l - 1 );
                if ( src != NULL )
                {
                    char * endptr;
                    res = strtol( src, &endptr, 0 ) * multipl;
                    free( src );
                }
            }
            else
            {
                char * endptr;
                res = strtol( s, &endptr, 0 );
            }
        }
    }
    return res;
}


rc_t get_size_t( const Args * args, const char *option, size_t * value, size_t dflt )
{

    const char * svalue;
    rc_t rc = get_charptr( args, option, &svalue );
    if ( rc == 0 && svalue != NULL )
        *value = string_2_size_t( svalue, dflt );
    else
        *value = dflt;
    return 0;
}

/* ----------------------------------------------------------------------------------------------- */

typedef struct Writer
{
    KFile * f;
    uint64_t pos;
} Writer;


rc_t writer_release( struct Writer * wr )
{
    rc_t rc = 0;
    if ( wr != NULL )
    {
        if ( wr->f != NULL )
            rc = KFileRelease( wr->f );
        free( ( void * ) wr );
    }
    return rc;
}


rc_t writer_make( struct Writer ** wr, const char * filename )
{
    rc_t rc = 0;
    if ( wr == NULL || filename == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "writer_make() given a NULL-ptr" );
    }
    else
    {
        KDirectory * dir;
        rc = KDirectoryNativeDir( &dir );
        if ( rc != 0 )
            log_err( "writer_make() : KDirectoryNativeDir() failed %R", rc );
        else
        {
            KFile * f;
            rc = KDirectoryCreateFile( dir, &f, false, 0664, kcmInit, "%s", filename );
            if ( rc != 0 )
                log_err( "writer_make() : KDirectoryCreateFile( '%s' ) failed %R", filename, rc );
            else
            {
                Writer * w = calloc( 1, sizeof *w );
                if ( w == NULL )
                {
                    log_err( "writer_make() memory exhausted" );
                    rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
                }
                else
                {
                    w->f = f;
                    *wr = w;
                }
            }
            KDirectoryRelease( dir );
        }
    }
    return rc;
}


rc_t writer_write( struct Writer * wr, const char * fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ_printf;
    
    va_list args;
    va_start( args, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ_printf, fmt, args );
    va_end( args );
    if ( rc != 0 )
        log_err( "writer_write() : string_vprintf() failed %R", rc );
    else
    {
        size_t num_writ_file_write;
        rc = KFileWriteAll( wr->f, wr->pos, buffer, num_writ_printf, &num_writ_file_write );
        if ( rc != 0 )
            log_err( "writer_write() : KFileWriteAll() failed %R", rc );
        else
            wr->pos += num_writ_file_write;
    }
    return rc;

}


/* ----------------------------------------------------------------------------------------------- */


AlignmentT * copy_alignment( const AlignmentT * src )
{
    AlignmentT * res = malloc( sizeof * res );
    if ( res != NULL )
    {
        size_t total = src->rname.size + src->cigar.size + src->read.size;
        res->rname.addr = malloc( total );
        if ( res->rname.addr != NULL )
        {
            char * dst = ( char * )res->rname.addr;
            
            string_copy( dst, src->rname.size, src->rname.addr, src->rname.size );
            res->rname.size = src->rname.size;
            res->rname.len = src->rname.len;
            dst += src->rname.len;
            
            string_copy( dst, src->cigar.size, src->cigar.addr, src->cigar.size );
            res->cigar.addr = dst;
            res->cigar.size = src->cigar.size;
            res->cigar.len = src->cigar.len;
            dst += src->cigar.len;
            
            string_copy( dst, src->read.size, src->read.addr, src->read.size );
            res->read.addr = dst;
            res->read.size = src->read.size;
            res->read.len = src->read.len;
        }
        
        res->pos = src->pos;
        res->fwd = src->fwd;
        res->first = src->first;
    }
    return res;
}


void free_alignment_copy( AlignmentT * src )
{
    if ( src != NULL )
    {
        /* the pointer is in the rname-String! */
        if ( src->rname.addr != NULL ) free( ( void * ) src->rname.addr );
        free( ( void * ) src );
    }
}


rc_t print_alignment( AlignmentT * a )
{
    return KOutMsg( "%S\t%lu\t%S\n", &a->rname, a->pos, &a->cigar );
}

/* ----------------------------------------------------------------------------------------------- */

void clear_recorded_errors( void )
{
    rc_t rc;
    const char * filename;
    const char * funcname;
    uint32_t line_nr;
    while ( GetUnreadRCInfo ( &rc, &filename, &funcname, &line_nr ) )
    {
    }
}
