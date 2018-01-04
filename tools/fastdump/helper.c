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

#include "helper.h"

#include <klib/log.h>
#include <klib/printf.h>
#include <klib/out.h>
#include <kfs/defs.h>
#include <kfs/file.h>
#include <kfs/buffile.h>
#include <search/nucstrstr.h>

#include <kdb/manager.h>
#include <vdb/manager.h>

rc_t ErrMsg( const char * fmt, ... )
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

rc_t CC ArgsOptionCount( const struct Args * self, const char * option_name, uint32_t * count );
rc_t CC ArgsOptionValue( const struct Args * self, const char * option_name, uint32_t iteration, const void ** value );

const char * get_str_option( const struct Args *args, const char *name, const char * dflt )
{
    const char* res = dflt;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc == 0 && count > 0 )
    {
        rc = ArgsOptionValue( args, name, 0, (const void**)&res );
        if ( rc != 0 ) res = dflt;
    }
    return res;
}

bool get_bool_option( const struct Args *args, const char *name )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    return ( rc == 0 && count > 0 );
}


uint64_t get_uint64_t_option( const struct Args * args, const char *name, uint64_t dflt )
{
    uint64_t res = dflt;
    const char * s = get_str_option( args, name, NULL );
    if ( s != NULL )
    {
        size_t l = string_size( s );
        if ( l > 0 )
        {
            char * endptr;
            res = strtol( s, &endptr, 0 );
        }
    }
    return res;
}

uint32_t get_uint32_t_option( const struct Args * args, const char *name, uint32_t dflt )
{
    uint32_t res = dflt;
    const char * s = get_str_option( args, name, NULL );
    if ( s != NULL )
    {
        size_t l = string_size( s );
        if ( l > 0 )
        {
            char * endptr;
            res = ( uint32_t )strtol( s, &endptr, 0 );
        }
    }
    return res;

}

size_t get_size_t_option( const struct Args * args, const char *name, size_t dflt )
{
    size_t res = dflt;
    const char * s = get_str_option( args, name, NULL );
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

static format_t format_cmp( String * Format, const char * test, format_t test_fmt )
{
    String TestFormat;
    StringInitCString( &TestFormat, test );
    if ( 0 == StringCaseCompare ( Format, &TestFormat ) )
        return test_fmt;
    return ft_unknown;
}

format_t get_format_t( const char * format, bool split_spot, bool split_file, bool split_3 )
{
    format_t res = ft_unknown;
    if ( format != NULL && format[ 0 ] != 0 )
    {
        String Format;
        StringInitCString( &Format, format );
        
        res = format_cmp( &Format, "special", ft_special );
        if ( res == ft_unknown )
            res = format_cmp( &Format, "fastq", ft_fastq );
        if ( res == ft_unknown )
            res = format_cmp( &Format, "fastq-split-spot", ft_fastq_split_spot );
        if ( res == ft_unknown )
            res = format_cmp( &Format, "fastq-split-file", ft_fastq_split_file );
        if ( res == ft_unknown )
            res = format_cmp( &Format, "fastq-split-3", ft_fastq_split_3 );
    }
    
    if ( res == ft_unknown )
        res = ft_fastq;
    if ( res == ft_fastq )
    {
        if ( split_3 )
            res = ft_fastq_split_3;
        else if ( split_file )
            res = ft_fastq_split_file;
        else if ( split_spot )
            res = ft_fastq_split_spot;
    }
    return res;
}

rc_t make_SBuffer( SBuffer * buffer, size_t len )
{
    rc_t rc = 0;
    String * S = &buffer -> S;
    S -> addr = malloc( len );
    if ( S -> addr == NULL )
    {
        S -> size = S -> len = 0;
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "malloc( %d ) -> %R", ( len ), rc );
    }
    else
    {
        S -> size = 0;
        S -> len = 0;
        buffer -> buffer_size = len;
    }
    return rc;
}

void release_SBuffer( SBuffer * self )
{
    if ( self != NULL )
    {
        String * S = &self -> S;
        if ( S -> addr != NULL )
            free( ( void * ) S -> addr );
    }
}

rc_t increase_SBuffer( SBuffer * self, size_t by )
{
    rc_t rc;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else
    {
        size_t new_size = self -> buffer_size + by;
        release_SBuffer( self );
        rc = make_SBuffer( self, new_size );
    }
    return rc;
}

rc_t print_to_SBufferV( SBuffer * self, const char * fmt, va_list args )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else
    {
        char * dst = ( char * )( self -> S . addr );
        size_t num_writ = 0;
            
        rc = string_vprintf( dst, self -> buffer_size, &num_writ, fmt, args );
        if ( rc == 0 )
        {
            self -> S . size = num_writ;
            self -> S . len = ( uint32_t )self -> S . size;
        }
    }
    return rc;
}

rc_t try_to_enlarge_SBuffer( SBuffer * self, rc_t rc_err )
{
    rc_t rc = rc_err;
    if ( ( GetRCObject( rc ) == ( enum RCObject )rcBuffer ) && ( GetRCState( rc ) == rcInsufficient ) )
    {
        rc = increase_SBuffer( self, self -> buffer_size ); /* double it's size */
        if ( rc != 0 )
            ErrMsg( "increase_SBuffer() -> %R", rc );
    }
    return rc;
}

rc_t print_to_SBuffer( SBuffer * self, const char * fmt, ... )
{
    rc_t rc = 0;
    bool done = false;
    while ( rc == 0 && !done )
    {
        va_list args;
        
        va_start( args, fmt );
        rc = print_to_SBufferV( self, fmt, args );
        va_end( args );
        
        done = ( rc == 0 );
        if ( !done )
            rc = try_to_enlarge_SBuffer( self, rc );
    }
    return rc;
}


rc_t make_and_print_to_SBuffer( SBuffer * self, size_t len, const char * fmt, ... )
{
    rc_t rc = make_SBuffer( self, len );
    if ( rc == 0 )
    {
        va_list args;

        va_start( args, fmt );
        rc = print_to_SBufferV( self, fmt, args );
        va_end( args );
    }
    return rc;
}

rc_t split_string( String * in, String * p0, String * p1, uint32_t ch )
{
    rc_t rc = 0;
    char * ch_ptr = string_chr( in -> addr, in -> size, ch );
    if ( ch_ptr == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTransfer, rcInvalid );
    else
    {
        p0 -> addr = in -> addr;
        p0 -> size = ( ch_ptr - p0 -> addr );
        p0 -> len  = ( uint32_t ) p0 -> size;
        p1 -> addr = ch_ptr + 1;
        p1 -> size = in -> len - ( p0 -> len + 1 );
        p1 -> len  = ( uint32_t ) p1 -> size;
    }
    return rc;
}

compress_t get_compress_t( bool gzip, bool bzip2 )
{
    if ( gzip && bzip2 )
        return ct_bzip2;
    else if ( gzip )
        return ct_gzip;
    else if ( bzip2 )
        return ct_bzip2;
    return ct_none;
}

uint64_t make_key( int64_t seq_spot_id, uint32_t seq_read_id )
{
    uint64_t key = seq_spot_id;
    key <<= 1;
    key |= ( seq_read_id == 2 ) ? 1 : 0;
    return key;
}

rc_t pack_4na( const String * unpacked, SBuffer * packed )
{
    rc_t rc = 0;
    if ( unpacked -> len < 1 )
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcNull );
    else
    {
        if ( unpacked -> len > 0xFFFF )
            rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcExcessive );
        else
        {
            uint32_t i;
            uint8_t * src = ( uint8_t * )unpacked -> addr;
            uint8_t * dst = ( uint8_t * )packed -> S . addr;
            uint16_t dna_len = ( unpacked -> len & 0xFFFF );
            uint32_t len = 0;
            dst[ len++ ] = ( dna_len >> 8 );
            dst[ len++ ] = ( dna_len & 0xFF );
            for ( i = 0; i < unpacked -> len; ++i )
            {
                if ( len < packed -> buffer_size )
                {
                    uint8_t base = ( src[ i ] & 0x0F );
                    if ( 0 == ( i & 0x01 ) )
                        dst[ len ] = ( base << 4 );
                    else
                        dst[ len++ ] |= base;
                }
            }
            if ( unpacked -> len & 0x01 )
                len++;
            packed -> S . size = packed -> S . len = len;
        }
    }
    return rc;
}


static char x4na_to_ASCII[ 16 ] =
{
    /* 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
       'N', 'A', 'C', 'N', 'G', 'N', 'N', 'N', 'T', 'N', 'N', 'N', 'N', 'N', 'N', 'N'
};


void unpack_4na( const String * packed, SBuffer * unpacked )
{
    uint32_t i;
    uint8_t * src = ( uint8_t * )packed -> addr;
    uint8_t * dst = ( uint8_t * )unpacked -> S . addr;
    uint32_t dst_idx = 0;
    uint16_t dna_len = src[ 0 ];
    dna_len <<= 8;
    dna_len |= src[ 1 ];
    for ( i = 2; i < packed -> len; ++i )
    {
        uint8_t packed_byte = src[ i ];
        if ( dst_idx < unpacked -> buffer_size )
            dst[ dst_idx++ ] = x4na_to_ASCII[ ( packed_byte >> 4 ) & 0x0F ];
        if ( dst_idx < unpacked -> buffer_size )
            dst[ dst_idx++ ] = x4na_to_ASCII[ packed_byte & 0x0F ];
    }
    unpacked -> S . size = dna_len;
    unpacked -> S .len = ( uint32_t )unpacked -> S . size;
    dst[ dna_len + 2 ] = 0;
}

bool file_exists( const KDirectory * dir, const char * fmt, ... )
{
    uint32_t pt;
    va_list list;
    
    va_start( list, fmt );
    pt = KDirectoryVPathType( dir, fmt, list );
    va_end( list );

    return ( pt == kptFile ) ;
}

bool dir_exists( const KDirectory * dir, const char * fmt, ... )
{
    uint32_t pt;
    va_list list;
    
    va_start( list, fmt );
    pt = KDirectoryVPathType( dir, fmt, list );
    va_end( list );

    return ( pt == kptDir ) ;
}

void join_and_release_threads( Vector * threads )
{
    uint32_t i, n = VectorLength( threads );
    for ( i = VectorStart( threads ); i < n; ++i )
    {
        KThread * thread = VectorGet( threads, i );
        if ( thread != NULL )
        {
            KThreadWait( thread, NULL );
            KThreadRelease( thread );
        }
    }
    VectorWhack ( threads, NULL, NULL );
}


void clear_join_stats( join_stats * stats )
{
    if ( stats != NULL )
    {
        stats -> spots_read = 0;
        stats -> fragments_read = 0;
        stats -> fragments_written = 0;
        stats -> fragments_zero_length = 0;
        stats -> fragments_technical = 0;
        stats -> fragments_too_short = 0;
    }
}

void add_join_stats( join_stats * stats, const join_stats * to_add )
{
    if ( stats != NULL && to_add != NULL )
    {
        stats -> spots_read += to_add -> spots_read;
        stats -> fragments_read += to_add -> fragments_read;
        stats -> fragments_written += to_add -> fragments_written;
        stats -> fragments_zero_length += to_add -> fragments_zero_length;
        stats -> fragments_technical += to_add -> fragments_technical;
        stats -> fragments_too_short += to_add -> fragments_too_short;
    }
}

rc_t delete_files( KDirectory * dir, const VNamelist * files )
{
    uint32_t count;
    rc_t rc = VNameListCount( files, &count );
    if ( rc != 0 )
        ErrMsg( "VNameListCount() -> %R", rc );
    else
    {
        uint32_t idx;
        for ( idx = 0; rc == 0 && idx < count; ++idx )
        {
            const char * filename;
            rc = VNameListGet( files, idx, &filename );
            if ( rc != 0 )
                ErrMsg( "VNameListGet( #%d) -> %R", idx, rc );
            else
            {
                if ( file_exists( dir, "%s", filename ) )
                {
                    rc = KDirectoryRemove( dir, true, "%s", filename );
                    if ( rc != 0 )
                        ErrMsg( "KDirectoryRemove( '%s' ) -> %R", filename, rc );
                }
            }
        }
    }
    return rc;
}

uint64_t total_size_of_files_in_list( KDirectory * dir, const VNamelist * files )
{
    uint64_t res = 0;
    if ( dir != NULL && files != NULL )
    {
        uint32_t idx, count;
        rc_t rc = VNameListCount( files, &count );
        for ( idx = 0; rc == 0 && idx < count; ++idx )
        {
            const char * filename;
            rc = VNameListGet( files, idx, &filename );
            if ( rc == 0 )
            {
                uint64_t size;
                rc_t rc1 = KDirectoryFileSize( dir, &size, "%s", filename );
                if ( rc1 == 0 )
                    res += size;
            }
        }
    }
    return res;
}

int get_vdb_pathtype( KDirectory * dir, const char * accession )
{
    int res = kptAny;
    const VDBManager * vdb_mgr;
    rc_t rc = VDBManagerMakeRead( &vdb_mgr, dir );
    if ( rc != 0 )
        ErrMsg( "get_vdb_pathtype().VDBManagerMakeRead() -> %R\n", rc );
    else
    {
        res = ( VDBManagerPathType ( vdb_mgr, "%s", accession ) & ~ kptAlias );
        VDBManagerRelease( vdb_mgr );
    }
    return res;
}

/* ===================================================================================== */

bool ends_in_slash( const char * s )
{
    bool res = false;
    if ( s != NULL )
    {
        uint32_t len = string_measure( s, NULL );
        if ( len > 0 )
            res = ( s[ len - 1 ] == '/' );
    }
    return res;
}

bool extract_path( const char * s, String * path )
{
    bool res = false;
    if ( s != NULL && path != NULL )
    {
        path -> addr = s;
        res = ends_in_slash( s );
        if ( res )
            path -> len = string_measure( s, & path -> size );
        else
        {
            size_t size = string_size ( s );
            char * slash = string_rchr ( s, size, '/' );
            res = ( slash != NULL );
            if ( res )
            {
                path -> len = ( slash - s );
                path -> size = ( size_t ) path -> len;
            }
        }
    }
    return res;
}

const char * extract_acc( const char * s )
{
    const char * res = NULL;
    if ( s != NULL )
    {
        if ( !ends_in_slash( s ) )
        {
            size_t size = string_size ( s );
            char * slash = string_rchr ( s, size, '/' );
            if ( slash == NULL )
                res = s;
            else
                res = slash + 1;
        }
    }
    return res;
}

rc_t create_this_file( KDirectory * dir, const char * filename, bool force )
{
    struct KFile * f;
    KCreateMode create_mode = force ? kcmInit : kcmCreate;
    rc_t rc = KDirectoryCreateFile( dir, &f, false, 0664, create_mode | kcmParents, "%s", filename );
    if ( rc != 0 )
        ErrMsg( "KDirectoryCreateFile( '%s' ) -> %R", filename, rc );
    else
        KFileRelease( f );
    return rc;
}

rc_t create_this_dir( KDirectory * dir, const String * dir_name, bool force )
{
    KCreateMode create_mode = force ? kcmInit : kcmCreate;
    rc_t rc = KDirectoryCreateDir( dir, 0774, create_mode | kcmParents, "%.*s", dir_name -> len, dir_name -> addr );
    if ( rc != 0 )
        ErrMsg( "KDirectoryCreateDir( '%.*s' ) -> %R", dir_name -> len, dir_name -> addr, rc );
    return rc;
}

rc_t make_joined_filename( char * buffer, size_t bufsize,
                           const char * accession,
                           const tmp_id * tmp_id,
                           uint32_t id )
{
    size_t num_writ;
    rc_t rc = string_printf( buffer, bufsize, &num_writ,
                             tmp_id -> temp_path_ends_in_slash ? "%s%s.%s.%u.%u" : "%s/%s.%s.%u.%u",
                             tmp_id -> temp_path,
                             accession,
                             tmp_id -> hostname,
                             tmp_id -> pid,
                             id );
    if ( rc != 0 )
        ErrMsg( "make_joined_filename.string_printf() -> %R", rc );
    return rc;

}

rc_t make_buffered_for_read( KDirectory * dir, const struct KFile ** f,
                             const char * filename, size_t buf_size )
{
    const struct KFile * fr;
    rc_t rc = KDirectoryOpenFileRead( dir, &fr, "%s", filename );
    if ( rc == 0 )
    {
        if ( buf_size > 0 )
        {
            const struct KFile * fb;
            rc = KBufFileMakeRead( &fb, fr, buf_size );
            if ( rc == 0 )
            {
                KFileRelease( fr );
                fr = fb;
            }
        }
        if ( rc == 0 )
            *f = fr;
        else
            KFileRelease( fr );
    }
    return rc;
}

/* ===================================================================================== */

rc_t locked_file_list_init( locked_file_list * self, uint32_t alloc_blocksize )
{
    rc_t rc;
    if ( self == NULL || alloc_blocksize == 0 )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        rc = KLockMake ( &( self -> lock ) );
        if ( rc == 0 )
            rc = VNamelistMake ( & self -> files, alloc_blocksize );
    }
    return rc;
}

rc_t locked_file_list_release( locked_file_list * self, KDirectory * dir )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        KLockRelease ( self -> lock );
        if ( dir != NULL )
            rc = delete_files( dir, self -> files );
        VNamelistRelease ( self -> files );
    }
    return rc;
}

rc_t locked_file_list_append( const locked_file_list * self, const char * filename )
{
    rc_t rc = 0;
    if ( self == NULL || filename == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            rc = VNamelistAppend ( self -> files, filename );
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

rc_t locked_file_list_delete_all( KDirectory * dir, locked_file_list * self )
{
    rc_t rc = 0;
    if ( self == NULL || dir == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            rc = delete_files( dir, self -> files );
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

rc_t locked_file_list_count( const locked_file_list * self, uint32_t * count )
{
    rc_t rc = 0;
    if ( self == NULL || count == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            rc = VNameListCount( self -> files, count );
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

rc_t locked_file_list_pop( locked_file_list * self, const String ** item )
{
    rc_t rc = 0;
    if ( self == NULL || item == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        *item = NULL;
        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            const char * s;
            rc = VNameListGet ( self -> files, 0, &s );
            if ( rc == 0 )
            {
                String S;
                StringInitCString( &S, s );
                rc = StringCopy ( item, &S );
                if ( rc == 0 )
                    rc = VNamelistRemoveIdx( self -> files, 0 );
            }
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

/* ===================================================================================== */

rc_t locked_vector_init( locked_vector * self, uint32_t alloc_blocksize )
{
    rc_t rc;
    if ( self == NULL || alloc_blocksize == 0 )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        rc = KLockMake ( &( self -> lock ) );
        if ( rc == 0 )
        {
            VectorInit ( &( self -> vector ), 0, alloc_blocksize );
            self -> sealed = false;
        }
    }
    return rc;
}

void locked_vector_release( locked_vector * self,
                            void ( CC * whack ) ( void *item, void *data ), void *data )
{
    if ( self == NULL )
    {
        rc_t rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            VectorWhack ( &( self -> vector ), whack, data );
            KLockUnlock ( self -> lock );    
        }
        KLockRelease ( self -> lock );
    }
}

rc_t locked_vector_push( locked_vector * self, const void * item, bool seal )
{
    rc_t rc;
    if ( self == NULL || item == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            rc = VectorAppend ( &( self -> vector ), NULL, item );
            if ( seal )
                self -> sealed = true;
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

rc_t locked_vector_pop( locked_vector * self, void ** item, bool * sealed )
{
    rc_t rc;
    if ( self == NULL || item == NULL || sealed == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            if ( VectorLength( &( self -> vector ) ) == 0 )
            {
                rc = 0;
                *sealed = self -> sealed;
                *item = NULL;
            }
            else
            {
                *sealed = false;
                rc = VectorRemove ( &( self -> vector ), 0, item );
            }
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

/* ===================================================================================== */
rc_t locked_value_init( locked_value * self, uint64_t init_value )
{
    rc_t rc;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        rc = KLockMake ( &( self -> lock ) );
        if ( rc == 0 )
            self -> value = init_value;
    }
    return rc;
}

void locked_value_release( locked_value * self )
{
    if ( self == NULL )
        KLockRelease ( self -> lock );
}

rc_t locked_value_get( locked_value * self, uint64_t * value )
{
    rc_t rc;
    if ( self == NULL || value == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            *value = self -> value;
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

rc_t locked_value_set( locked_value * self, uint64_t value )
{
    rc_t rc;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            self -> value = value;
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

/* ===================================================================================== */
typedef struct Buf2NA
{
    unsigned char map [ 1 << ( sizeof ( char ) * 8 ) ];
    size_t shiftLeft[ 4 ];
    NucStrstr * nss;
    uint8_t * buffer;
    size_t allocated;
} Buf2NA;

rc_t make_Buf2NA( Buf2NA ** self, size_t size, const char * pattern )
{
    rc_t rc = 0;
    NucStrstr * nss;
    int res = NucStrstrMake ( &nss, 0, pattern, string_size ( pattern ) );
    if ( res != 0 )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    else
    {
        uint8_t * buffer = calloc( size, sizeof * buffer );
        if ( buffer == NULL )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            NucStrstrWhack ( nss );
        }
        else
        {
            Buf2NA * res = calloc( 1, sizeof * res );
            if ( res == NULL )
            {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                NucStrstrWhack ( nss );
                free( ( void * ) buffer );
            }
            else
            {
                res -> nss = nss;
                res -> buffer = buffer;
                res -> allocated = size;
                res -> map[ 'A' ] = res -> map[ 'a' ] = 0;
                res -> map[ 'C' ] = res -> map[ 'c' ] = 1;
                res -> map[ 'G' ] = res -> map[ 'g' ] = 2;
                res -> map[ 'T' ] = res -> map[ 't' ] = 3;
                res -> shiftLeft [ 0 ] = 6;
                res -> shiftLeft [ 1 ] = 4;
                res -> shiftLeft [ 2 ] = 2;
                res -> shiftLeft [ 3 ] = 0;
                *self = res;
            }
        }
    }
    return rc;
}

void release_Buf2NA( Buf2NA * self )
{
    if ( self != NULL )
    {
        if ( self -> buffer != NULL )
            free( ( void * ) self -> buffer );
        if ( self -> nss != NULL )
            NucStrstrWhack ( self -> nss );
        free( ( void * ) self );
    }
}

bool match_Buf2NA( Buf2NA * self, const String * ascii )
{
    bool res = false;
    if ( self != NULL && ascii != NULL )
    {
        int i;
        size_t needed = ( ( ascii -> len + 3 ) / 4 );
        if ( needed > self -> allocated )
        {
            free( ( void * )self -> buffer );
            self -> buffer = calloc( needed, sizeof *( self -> buffer ) );
        }
        else
            memset( self -> buffer, 0, needed );

        if ( self -> buffer != NULL )
        {
            unsigned int selflen;
            int dst = 0;
            int src = 0;
            i = ascii -> len;
            while ( i >= 4 )
            {
                self -> buffer[ dst++ ] =
                    self -> map[ ( unsigned char )ascii -> addr[ src ] ] << 6 |
                    self -> map[ ( unsigned char )ascii -> addr[ src + 1 ] ] << 4 |
                    self -> map[ ( unsigned char )ascii -> addr[ src + 2 ] ] << 2 |
                    self -> map[ ( unsigned char )ascii -> addr[ src + 3 ] ];
                src += 4;
                i -= 4;
            }
            switch( i )
            {
                case 3 : self -> buffer[ dst ] = 
                            self -> map[ ( unsigned char )ascii -> addr[ src ] ] << 6 |
                            self -> map[ ( unsigned char )ascii -> addr[ src + 1 ] ] << 4 |
                            self -> map[ ( unsigned char )ascii -> addr[ src + 2 ] ] << 2; break;
                case 2 : self -> buffer[ dst ] = 
                            self -> map[ ( unsigned char )ascii -> addr[ src ] ] << 6 |
                            self -> map[ ( unsigned char )ascii -> addr[ src + 1 ] ] << 4; break;
                case 1 : self -> buffer[ dst ] = 
                            self -> map[ ( unsigned char )ascii -> addr[ src ] ] << 6; break;
            }
            res = ( 0 != NucStrstrSearch ( self -> nss, self -> buffer, 0, ascii -> len, & selflen ) );
        }
    }
    return res;
}
