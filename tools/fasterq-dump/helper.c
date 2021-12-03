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

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_klib_log_
#include <klib/log.h>
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_kfs_buffile_
#include <kfs/buffile.h>
#endif

#ifndef _h_search_nucstrstr_
#include <search/nucstrstr.h>
#endif

#ifndef _h_vfs_manager_
#include <vfs/manager.h>
#endif

#ifndef _h_vfs_path_
#include <vfs/path.h>
#endif

#include <atomic32.h>

bool is_format_fasta( format_t fmt ){
    bool res;
    switch( fmt ) {
        case ft_fasta_whole_spot : res = true; break;
        case ft_fasta_split_spot : res = true; break;
        case ft_fasta_split_file : res = true; break;
        case ft_fasta_split_3    : res = true; break;
        case ft_fasta_us_split_spot : res = true; break;
        default : res = false; break;
    }
    return res;
}

const String * make_string_copy( const char * src )
{
    const String * res = NULL;
    if ( NULL != src ) {
        String tmp;
        StringInitCString( &tmp, src );
        StringCopy( &res, &tmp );
    }
    return res;
}

static format_t format_cmp( String * Format, const char * test, format_t test_fmt ) {
    String TestFormat;
    StringInitCString( &TestFormat, test );
    if ( 0 == StringCaseCompare ( Format, &TestFormat ) )  {
        return test_fmt;
    }
    return ft_unknown;
}

format_t get_format_t( const char * format,
            bool split_spot, bool split_file, bool split_3, bool whole_spot,
            bool fasta, bool fasta_us ) {
    format_t res = ft_unknown;
    if ( NULL != format && 0 != format[ 0 ] ) {
        /* the format option has been used, try to recognize one of the options,
           the legacy options will be ignored now */

        String Format;
        StringInitCString( &Format, format );

        res = format_cmp( &Format, "fastq-whole-spot", ft_fastq_whole_spot );
        if ( ft_unknown == res ) {
            res = format_cmp( &Format, "fastq-split-spot", ft_fastq_split_spot );
        }
        if ( ft_unknown == res ) {
            res = format_cmp( &Format, "fastq-split-file", ft_fastq_split_file );
        }
        if ( ft_unknown == res ) {
            res = format_cmp( &Format, "fastq-split-3", ft_fastq_split_3 );
        }
        if ( ft_unknown == res ) {
            res = format_cmp( &Format, "fasta-whole-spot", ft_fasta_whole_spot );
        }
        if ( ft_unknown == res ) {
            res = format_cmp( &Format, "fasta-split-spot", ft_fasta_split_spot );
        }
        if ( ft_unknown == res ) {
            res = format_cmp( &Format, "fasta-split-file", ft_fasta_split_file );
        }
        if ( ft_unknown == res ) {
            res = format_cmp( &Format, "fasta-split-3", ft_fasta_split_3 );
        }
        if ( ft_unknown == res ) {
            res = format_cmp( &Format, "fasta-us-split-spot", ft_fasta_us_split_spot );
        }
    } else {
        /* the format option has not been used, let us see if some of the legacy-options
            have been used */
        if ( split_3 ) {
            res = ( fasta || fasta_us ) ? ft_fasta_split_3 : ft_fastq_split_3;
            /* split-file, split-spot and whole-spot are ignored! */
        } else if ( split_file ) {
            res = ( fasta || fasta_us ) ? ft_fasta_split_file : ft_fastq_split_file;
            /* split-3, split-spot and whole-spot are ignored! */
        } else if ( split_spot ) {
            /* split-3, split-file and whole-spot are ignored! */
            if ( fasta_us ) {
                res = ft_fasta_us_split_spot;
            } else if ( fasta ) {
                res = ft_fasta_split_spot;
            } else {
                res = ft_fastq_split_spot;
            }
        } else if ( whole_spot ) {
            /* split-3, split-file and split-spot are ignored! */
            res = ( fasta || fasta_us ) ? ft_fasta_whole_spot : ft_fastq_whole_spot;
        }
    }
    /* default to split_3 if no format has been given at all */
    if ( ft_unknown == res ) {
        if ( fasta_us ) {
            res = ft_fasta_us_split_spot;
        } else if ( fasta ) {
            res = ft_fasta_split_spot;
        } else {
            res = ft_fastq_split_3;
        }
    }
    return res;
}

rc_t split_string( String * in, String * p0, String * p1, uint32_t ch ) {
    rc_t rc = 0;
    char * ch_ptr = string_chr( in -> addr, in -> size, ch );
    if ( NULL == ch_ptr ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTransfer, rcInvalid );
    } else {
        p0 -> addr = in -> addr;
        p0 -> size = ( ch_ptr - p0 -> addr );
        p0 -> len  = ( uint32_t ) p0 -> size;
        p1 -> addr = ch_ptr + 1;
        p1 -> size = in -> len - ( p0 -> len + 1 );
        p1 -> len  = ( uint32_t ) p1 -> size;
    }
    return rc;
}

rc_t split_string_r( String * in, String * p0, String * p1, uint32_t ch ) {
    rc_t rc = 0;
    char * ch_ptr = string_rchr( in -> addr, in -> size, ch );
    if ( NULL == ch_ptr ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTransfer, rcInvalid );
    } else {
        p0 -> addr = in -> addr;
        p0 -> size = ( ch_ptr - p0 -> addr );
        p0 -> len  = ( uint32_t ) p0 -> size;
        p1 -> addr = ch_ptr + 1;
        p1 -> size = in -> len - ( p0 -> len + 1 );
        p1 -> len  = ( uint32_t ) p1 -> size;
    }
    return rc;
}

uint64_t make_key( int64_t seq_spot_id, uint32_t seq_read_id ) {
    uint64_t key = seq_spot_id;
    key <<= 1;
    key |= ( 2 == seq_read_id ) ? 1 : 0;
    return key;
}

rc_t join_and_release_threads( Vector * threads ) {
    rc_t rc = 0;
    uint32_t i, n = VectorLength( threads );
    for ( i = VectorStart( threads ); i < n; ++i ) {
        KThread * thread = VectorGet( threads, i );
        if ( NULL != thread ) {
            rc_t rc1;
            KThreadWait( thread, &rc1 );
            if ( 0 == rc && rc1 != 0 ) {
                rc = rc1;
            }
            KThreadRelease( thread );
        }
    }
    VectorWhack ( threads, NULL, NULL );
    return rc;
}

void clear_join_stats( join_stats_t * stats ) {
    if ( stats != NULL )
    {
        stats -> spots_read = 0;
        stats -> reads_read = 0;
        stats -> reads_written = 0;
        stats -> reads_zero_length = 0;
        stats -> reads_technical = 0;
        stats -> reads_too_short = 0;
        stats -> reads_invalid = 0;
    }
}

void add_join_stats( join_stats_t * stats, const join_stats_t * to_add ) {
    if ( NULL != stats && NULL != to_add )
    {
        stats -> spots_read += to_add -> spots_read;
        stats -> reads_read += to_add -> reads_read;
        stats -> reads_written += to_add -> reads_written;
        stats -> reads_zero_length += to_add -> reads_zero_length;
        stats -> reads_technical += to_add -> reads_technical;
        stats -> reads_too_short += to_add -> reads_too_short;
        stats -> reads_invalid += to_add -> reads_invalid;
    }
}

/*
int get_vdb_pathtype( KDirectory * dir, const VDBManager * vdb_mgr, const char * accession ) {
    int res = kptAny;
    rc_t rc = 0;
    bool release_mgr = false;
    const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
    if ( mgr == NULL ) {
        rc = VDBManagerMakeRead( &mgr, dir );
        if ( rc != 0 ) {
            ErrMsg( "get_vdb_pathtype().VDBManagerMakeRead() -> %R\n", rc );
        } else {
            release_mgr = true;
        }
    }
    if ( rc == 0 ) {
        res = ( VDBManagerPathType ( mgr, "%s", accession ) & ~ kptAlias );
        if ( release_mgr ) {
            VDBManagerRelease( mgr );
        }
    }
    return res;
}
*/

/* ===================================================================================== */

bool ends_in_slash( const char * s ) {
    bool res = false;
    if ( NULL != s ) {
        uint32_t len = string_measure( s, NULL );
        if ( len > 0 ) {
            res = ( '/' == s[ len - 1 ] );
        }
    }
    return res;
}

bool ends_in_sra( const char * s ) {
    bool res = false;
    if ( NULL != s ) {
        uint32_t len = string_measure( s, NULL );
        if ( len > 4 ) {
            res = ( ( 'a' == s[ len - 1 ] ) &&
                    ( 'r' == s[ len - 2 ] ) &&
                    ( 's' == s[ len - 3 ] ) &&
                    ( '.' == s[ len - 4 ] ) );
        }
    }
    return res;
}

bool extract_path( const char * s, String * path ) {
    bool res = false;
    if ( NULL != s && NULL != path ) {
        path -> addr = s;
        res = ends_in_slash( s );
        if ( res ) {
            path -> len = string_measure( s, & path -> size );
        } else {
            size_t size = string_size ( s );
            char * slash = string_rchr ( s, size, '/' );
            res = ( NULL != slash );
            if ( res ) {
                path -> len = ( slash - s );
                path -> size = ( size_t ) path -> len;
            }
        }
    }
    return res;
}

/* ===================================================================================== */

typedef struct Buf2NA_t {
    unsigned char map [ 1 << ( sizeof ( char ) * 8 ) ];
    size_t shiftLeft[ 4 ];
    NucStrstr * nss;
    uint8_t * buffer;
    size_t allocated;
} Buf2NA_t;

rc_t make_Buf2NA( Buf2NA_t ** self, size_t size, const char * pattern ) {
    rc_t rc = 0;
    NucStrstr * nss;
    int res = NucStrstrMake ( &nss, 0, pattern, string_size ( pattern ) );
    if ( 0 != res ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "make_Buf2NA().NucStrstrMake() -> %R", rc );
    } else {
        uint8_t * buffer = calloc( size, sizeof * buffer );
        if ( NULL == buffer ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "make_Buf2NA().calloc().1() -> %R", rc );
            NucStrstrWhack ( nss );
        } else {
            Buf2NA_t * res = calloc( 1, sizeof * res );
            if ( NULL == res ) {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                ErrMsg( "make_Buf2NA().calloc().2() -> %R", rc );
                NucStrstrWhack ( nss );
                free( ( void * ) buffer );
            } else {
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

void release_Buf2NA( Buf2NA_t * self ) {
    if ( self != NULL ) {
        if ( self -> buffer != NULL ) {
            free( ( void * ) self -> buffer );
        }
        if ( self -> nss != NULL ) {
            NucStrstrWhack ( self -> nss );
        }
        free( ( void * ) self );
    }
}

bool match_Buf2NA( Buf2NA_t * self, const String * ascii ) {
    bool res = false;
    if ( self != NULL && ascii != NULL ) {
        int i;
        size_t needed = ( ( ascii -> len + 3 ) / 4 );
        if ( needed > self -> allocated ) {
            free( ( void * )self -> buffer );
            self -> buffer = calloc( needed, sizeof *( self -> buffer ) );
        }
        else {
            memset( self -> buffer, 0, needed );
        }
        if ( self -> buffer != NULL ) {
            unsigned int selflen;
            int dst = 0;
            int src = 0;
            i = ascii -> len;
            while ( i >= 4 ) {
                self -> buffer[ dst++ ] =
                    self -> map[ ( unsigned char )ascii -> addr[ src ] ] << 6 |
                    self -> map[ ( unsigned char )ascii -> addr[ src + 1 ] ] << 4 |
                    self -> map[ ( unsigned char )ascii -> addr[ src + 2 ] ] << 2 |
                    self -> map[ ( unsigned char )ascii -> addr[ src + 3 ] ];
                src += 4;
                i -= 4;
            }
            switch( i ) {
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

rc_t helper_make_thread( KThread ** self,
                         rc_t ( CC * run_thread ) ( const KThread * self, void * data ),
                         void * data,
                         size_t stacksize ) {
    rc_t rc = KThreadMakeStackSize( self, run_thread, data, stacksize );
    return rc;
}

/* ===================================================================================== */

static atomic32_t quit_flag;

rc_t get_quitting( void ) {
    rc_t rc = Quitting();
    if ( 0 == rc ) {
        if ( 0 != atomic32_read ( & quit_flag ) )
        rc = RC ( rcExe, rcProcess, rcExecuting, rcProcess, rcCanceled );
    }
    return rc;
}

void set_quitting( void ) {
    atomic32_inc ( & quit_flag );
}

/* ===================================================================================== */

uint64_t calculate_rows_per_thread( uint32_t * num_threads, uint64_t row_count ) {
    uint64_t res = row_count;
    uint64_t limit = 100 * ( *num_threads );
    if ( row_count < limit ) {
        *num_threads = 1;
    } else {
        res = ( row_count / ( *num_threads ) ) + 1;
    }
    return res;
}

void correct_join_options( join_options_t * dst, const join_options_t * src, bool name_column_present ) {
    dst -> rowid_as_name = name_column_present ? src -> rowid_as_name : true;
    dst -> skip_tech = src -> skip_tech;
    dst -> print_spotgroup = src -> print_spotgroup;
    dst -> min_read_len = src -> min_read_len;
    dst -> filter_bases = src -> filter_bases;
}
