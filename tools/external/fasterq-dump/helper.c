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

#include "kfc/defs.h"
#if _POSIX_C_SOURCE < 200112L
#define _POSIX_C_SOURCE 200112L /* lstat */
#endif

#include "helper.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
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

/*
#ifndef _h_sra_sradb_priv_
#include <sra/sradb-priv.h>
#endif
*/

#ifndef _h_vfs_manager_
#include <vfs/manager.h>
#endif

#ifndef _h_vfs_path_
#include <vfs/path.h>
#endif

#include "../../../libs/inc/search/nucstrstr.h"

#include <atomic32.h>

/* -------------------------------------------------------------------------------- */

bool hlp_is_format_fasta( format_t fmt ){
    bool res;
    switch( fmt ) {
        case ft_fasta_whole_spot : res = true; break;
        case ft_fasta_split_spot : res = true; break;
        case ft_fasta_split_file : res = true; break;
        case ft_fasta_split_3    : res = true; break;
        case ft_fasta_us_split_spot : res = true; break;
        case ft_fasta_ref_tbl : res = true; break;
        case ft_fasta_concat : res = true; break;
        default : res = false; break;
    }
    return res;
}

static format_t format_cmp( const String * Format, const char * test, format_t test_fmt ) {
    String TestFormat;
    StringInitCString( &TestFormat, test );
    if ( 0 == StringCaseCompare ( Format, &TestFormat ) )  {
        return test_fmt;
    }
    return ft_unknown;
}

format_t hlp_get_format_t( const char * format,
            bool split_spot, bool split_file, bool split_3, bool whole_spot,
            bool fasta, bool fasta_us, bool fasta_ref_tbl, bool fasta_concat, 
            bool ref_report ) {
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
        if ( ft_unknown == res ) {
            res = format_cmp( &Format, "fasta-ref-tbl", ft_fasta_ref_tbl );
        }
        if ( ft_unknown == res ) {
            res = format_cmp( &Format, "fasta-concat_all", ft_fasta_concat );
        }
        
    } else {
        /* the format option has not been used, let us see if some of the legacy-options
            have been used */
        if ( fasta_concat ) {
            res = ft_fasta_concat;
        } else if ( fasta_ref_tbl ) {
            res = ft_fasta_ref_tbl;
        } else if ( ref_report ) {
            res = ft_ref_report;
        } else {
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

static const char * FMT_UNKNOWN             = "unknown format";
static const char * FMT_FASTQ_WHOLE_SPOT    = "FASTQ whole spot";
static const char * FMT_FASTQ_SPLIT_SPOT    = "FASTQ split spot";
static const char * FMT_FASTQ_SPLIT_FILE    = "FASTQ split file";
static const char * FMT_FASTQ_SPLIT_3       = "FASTQ split 3";
static const char * FMT_FASTA_WHOLE_SPOT    = "FASTA whole spot";
static const char * FMT_FASTA_SPLIT_SPOT    = "FASTA split spot";
static const char * FMT_FASTA_UNSORTED      = "FASTA-unsorted split spot";
static const char * FMT_FASTA_REF_TBL       = "FASTA-ref-table";
static const char * FMT_FASTA_CONCAT        = "FASTA-concat";
static const char * FMT_FASTA_SPLIT_FILE    = "FASTA split file";
static const char * FMT_FASTA_SPLIT_3       = "FASTA split 3";
static const char * FMT_REF_REPORT          = "ref-report";

const char * hlp_fmt_2_string( format_t fmt ) {
    const char * res = FMT_UNKNOWN;
    switch ( fmt ) {
        case ft_unknown             : res = FMT_UNKNOWN; break;
        case ft_fastq_whole_spot    : res = FMT_FASTQ_WHOLE_SPOT; break;
        case ft_fastq_split_spot    : res = FMT_FASTQ_SPLIT_SPOT; break;
        case ft_fastq_split_file    : res = FMT_FASTQ_SPLIT_FILE; break;
        case ft_fastq_split_3       : res = FMT_FASTQ_SPLIT_3; break;
        case ft_fasta_whole_spot    : res = FMT_FASTA_WHOLE_SPOT; break;
        case ft_fasta_split_spot    : res = FMT_FASTA_SPLIT_SPOT; break;
        case ft_fasta_us_split_spot : res = FMT_FASTA_UNSORTED; break;
        case ft_fasta_ref_tbl       : res = FMT_FASTA_REF_TBL; break;
        case ft_fasta_concat        : res = FMT_FASTA_CONCAT; break;
        case ft_fasta_split_file    : res = FMT_FASTA_SPLIT_FILE; break;
        case ft_fasta_split_3       : res = FMT_FASTA_SPLIT_3; break;
        case ft_ref_report          : res = FMT_REF_REPORT; break;        
    }
    return res;
}

/* -------------------------------------------------------------------------------- */

static const char * FASTA_EXT = ".fasta";
static const char * FASTQ_EXT = ".fastq";

const char * hlp_out_ext( bool fasta ) {
    if ( fasta ) return FASTA_EXT;
    return FASTQ_EXT;
}

/* -------------------------------------------------------------------------------- */

static const char * B_YES = "YES";
static const char * B_NO  = "NO";

const char * hlp_yes_or_no( bool b ) {
    if ( b ) return B_YES;
    return B_NO;
}

/* -------------------------------------------------------------------------------- */

static check_mode_t check_mode_cmp( const String * Mode, const char * test, check_mode_t test_mode ) {
    String STestMode;
    StringInitCString( &STestMode, test );
    if ( 0 == StringCaseCompare ( Mode, &STestMode ) )  {
        return test_mode;
    }
    return cmt_unknown;
}

check_mode_t hlp_get_check_mode_t( const char * mode ) {
    check_mode_t res = cmt_on;
    if ( NULL != mode ) {
        String Mode;
        StringInitCString( &Mode, mode );

        res = check_mode_cmp( &Mode, "on", cmt_on );
        if ( cmt_unknown == res ) {
            res = check_mode_cmp( &Mode, "off", cmt_off );
        }
        if ( cmt_unknown == res ) {
            res = check_mode_cmp( &Mode, "only", cmt_only );
        }
    }
    return res;
}

bool hlp_is_perform_check( check_mode_t mode ) {
    switch ( mode ) {
        case cmt_unknown : return true; break;
        case cmt_on      : return true; break;
        case cmt_off     : return false; break;
        case cmt_only    : return true; break;
    }
    return true;
}

static const char * CM_UNKNOWN    = "unknown";
static const char * CM_ON         = "on";
static const char * CM_OFF        = "off";
static const char * CM_ONLY       = "only";

const char * hlp_check_mode_2_string( check_mode_t cm ) {
    const char * res = CM_UNKNOWN;
    switch ( cm ) {
        case cmt_unknown    : res = CM_UNKNOWN; break;
        case cmt_on         : res = CM_ON; break;
        case cmt_off        : res = CM_OFF; break;
        case cmt_only       : res = CM_ONLY; break;
    }
    return res;
}

/* -------------------------------------------------------------------------------- */

static atomic32_t quit_flag;

rc_t hlp_get_quitting( void ) {
    rc_t rc = Quitting();
    if ( 0 == rc ) {
        if ( 0 != atomic32_read ( & quit_flag ) )
            rc = RC ( rcExe, rcProcess, rcExecuting, rcProcess, rcCanceled );
    }
    return rc;
}

void hlp_set_quitting( void ) {
    atomic32_inc ( & quit_flag );
}

/* -------------------------------------------------------------------------------- */

void hlp_correct_join_options( join_options_t * dst, const join_options_t * src, bool name_column_present ) {
    dst -> rowid_as_name = name_column_present ? src -> rowid_as_name : true;
    dst -> skip_tech = src -> skip_tech;
    dst -> print_spotgroup = src -> print_spotgroup;
    dst -> min_read_len = src -> min_read_len;
    dst -> filter_bases = src -> filter_bases;
}

/* -------------------------------------------------------------------------------- */

uint64_t hlp_make_key( int64_t seq_spot_id, uint32_t seq_read_id ) {
    uint64_t key = seq_spot_id;
    key <<= 1;
    key |= ( 2 == seq_read_id ) ? 1 : 0;
    return key;
}

/* -------------------------------------------------------------------------------- */

const String * hlp_make_string_copy( const char * src )
{
    const String * res = NULL;
    if ( NULL != src ) {
        String tmp;
        StringInitCString( &tmp, src );
        StringCopy( &res, &tmp );
    }
    return res;
}

static void hlp_split_at_loc( String * in, String * out_0, String * out_1, char* loc ) {
    out_0 -> addr = in -> addr;
    out_0 -> size = ( loc - out_0 -> addr );
    out_0 -> len  = ( uint32_t ) out_0 -> size;
    out_1 -> addr = loc + 1;
    out_1 -> size = in -> len - ( out_0 -> len + 1 );
    out_1 -> len  = ( uint32_t ) out_1 -> size;
}

rc_t hlp_split_string( String * in, String * p0, String * p1, uint32_t ch ) {
    rc_t rc = 0;
    char * ch_ptr = string_chr( in -> addr, in -> size, ch );
    if ( NULL == ch_ptr ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTransfer, rcInvalid );
    } else {
        hlp_split_at_loc( in, p0, p1, ch_ptr );
    }
    return rc;
}

rc_t hlp_split_string_r( String * in, String * p0, String * p1, uint32_t ch ) {
    rc_t rc = 0;
    char * ch_ptr = string_rchr( in -> addr, in -> size, ch );
    if ( NULL == ch_ptr ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTransfer, rcInvalid );
    } else {
        hlp_split_at_loc( in, p0, p1, ch_ptr );
    }
    return rc;
}

/* new function to split a path into directory-filename aka stem and extension (Dec 2024) */
rc_t hlp_split_path_into_stem_and_extension( String *in, String* out_stem, String* out_ext ) {
    rc_t rc = 0;
    char * dot_location = string_rchr( in -> addr, in -> size, '.' );
    if ( NULL == dot_location ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTransfer, rcInvalid );
    } else {
        char * slash_location = string_rchr( in -> addr, in -> size, '/' );
        if ( NULL == slash_location ) {
            /* we do not have a slash -> split at the dot_location */
            hlp_split_at_loc( in, out_stem, out_ext, dot_location );
        } else {
            if ( dot_location < slash_location ) {
                /* the dot is part of the path - we do not have an extension */
                out_stem -> addr = in -> addr;
                out_stem -> size = in -> size;
                out_stem -> len = in -> len;
                out_ext -> addr = in -> addr;
                out_ext -> size = 0;
                out_ext -> len = 0;
            } else {
                /* the dot is for the extension */
                hlp_split_at_loc( in, out_stem, out_ext, dot_location );
            }
        }
    }
    return rc;
}

bool hlp_ends_in_slash( const char * s ) {
    bool res = false;
    if ( NULL != s ) {
        uint32_t len = string_measure( s, NULL );
        if ( len > 0 ) {
            res = ( '/' == s[ len - 1 ] );
        }
    }
    return res;
}

bool hlp_ends_in_sra( const char * s ) {
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

bool hlp_extract_path( const char * s, String * path ) {
    bool res = false;
    if ( NULL != s && NULL != path ) {
        path -> addr = s;
        res = hlp_ends_in_slash( s );
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

/* -------------------------------------------------------------------------------- */

void hlp_clear_join_stats( join_stats_t * stats ) {
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

void hlp_add_join_stats( join_stats_t * stats, const join_stats_t * to_add ) {
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

rc_t hlp_print_stats( const join_stats_t * stats, rc_t rc_in ) {
    KOutHandlerSetStdErr();
    rc_t rc = KOutMsg( "spots read      : %,lu\n", stats -> spots_read );
    if ( 0 == rc ) {
        rc = KOutMsg( "reads read      : %,lu\n", stats -> reads_read );
    }
    if ( 0 == rc && 0 == rc_in ) {
        rc = KOutMsg( "reads written   : %,lu\n", stats -> reads_written );
    }
    if ( 0 == rc && stats -> reads_zero_length > 0 ) {
        rc = KOutMsg( "reads 0-length  : %,lu\n", stats -> reads_zero_length );
    }
    if ( 0 == rc && stats -> reads_technical > 0 ) {
        rc = KOutMsg( "technical reads : %,lu\n", stats -> reads_technical );
    }
    if ( 0 == rc && stats -> reads_too_short > 0 ) {
        rc = KOutMsg( "reads too short : %,lu\n", stats -> reads_too_short );
    }
    if ( 0 == rc && stats -> reads_invalid > 0 ) {
        rc = KOutMsg( "reads invalid   : %,lu\n", stats -> reads_invalid );
    }
    KOutHandlerSetStdOut();
    return rc;
}

/* -------------------------------------------------------------------------------- */

typedef struct Buf2NA_t {
    unsigned char map [ 1 << ( sizeof ( char ) * 8 ) ];
    size_t shiftLeft[ 4 ];
    NucStrstr * nss;
    uint8_t * buffer;
    size_t allocated;
} Buf2NA_t;

static rc_t hlp_make_Buf2NA( Buf2NA_t ** self, size_t size, const char * pattern ) {
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

static void hlp_release_Buf2NA( Buf2NA_t * self ) {
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

static bool hlp_match_Buf2NA( Buf2NA_t * self, const String * ascii ) {
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

typedef struct filter_2na_t {
    struct Buf2NA_t * filter_buf2na;
} filter_2na_t;

filter_2na_t * hlp_make_2na_filter( const char * filter_bases ) {
    filter_2na_t * res = NULL;
    if ( NULL != filter_bases ) {
        res = calloc( 1, sizeof * res );
        if ( NULL != res ) {
            rc_t rc = hlp_make_Buf2NA( &( res -> filter_buf2na ), 512, filter_bases );
            if ( 0 != rc ) {
                ErrMsg( "make_2na_filter().error creating nucstrstr-filter from ( %s ) -> %R", filter_bases, rc );
                free( ( void * )res );
                res = NULL;
            }
        }
    }
    return res;
}

void hlp_release_2na_filter( filter_2na_t * self ) {
    if ( NULL != self ) {
        if ( NULL != self -> filter_buf2na ) {
            hlp_release_Buf2NA( self -> filter_buf2na );
        }
    }
}

bool hlp_filter_2na_1( filter_2na_t * self, const String * bases ) {
    bool res = true;
    if ( NULL != self && NULL != bases ) {
        res = hlp_match_Buf2NA( self -> filter_buf2na, bases );
    }
    return res;
}

bool hlp_filter_2na_2( filter_2na_t * self, const String * bases1, const String * bases2 ) {
    bool res = true;
    if ( NULL != self && NULL != bases1 && NULL != bases2 ) {
        res = ( hlp_match_Buf2NA( self -> filter_buf2na, bases1 ) || 
        hlp_match_Buf2NA( self -> filter_buf2na, bases2 ) );
    }
    return res;
}

/* -------------------------------------------------------------------------------- */

rc_t hlp_make_thread( KThread ** self,
                      rc_t ( CC * run_thread ) ( const KThread * self, void * data ),
                      void * data,
                      size_t stacksize ) {
    rc_t rc = KThreadMakeStackSize( self, run_thread, data, stacksize );
    return rc;
}
                      
rc_t hlp_join_and_release_threads( Vector * threads ) {
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

uint64_t hlp_calculate_rows_per_thread( uint32_t * num_threads, uint64_t row_count ) {
    uint64_t res = row_count;
    uint64_t limit = 100 * ( *num_threads );
    if ( row_count < limit ) {
        *num_threads = 1;
    } else {
        res = ( row_count / ( *num_threads ) ) + 1;
    }
    return res;
}

/* -------------------------------------------------------------------------------- */

void hlp_unread_rc_info( bool show ) {
    rc_t rc;
    const char *filename;
    const char * funcname;
    uint32_t lineno;
    while ( GetUnreadRCInfo ( &rc, &filename, &funcname, &lineno ) ) {
        if ( show ) {
            KOutMsg( "unread: %R %s %s %u\n", rc, filename, funcname, lineno );
        }
    }
}

/* -------------------------------------------------------------------------------- */

void hlp_init_qual_to_ascii_lut( char * lut, size_t size ) {
    uint32_t idx;
    memset( lut, '~', size );
    for ( idx = 0; idx < 256; idx++ ) {
        lut[ idx ] = idx + 33;
        if ( '~' == lut[ idx ] ) {
            break;
        }
    }
}

/* -------------------------------------------------------------------------------- */

#ifdef WINDOWS
/* do nothing for WINDOWS... */
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

bool hlp_paths_on_same_filesystem( const char * path1, const char * path2 ) {
#ifdef WINDOWS
    /* do nothing for WINDOWS... */
#else
    if (path1 != NULL && path2 != NULL && path1 != path2 && strcmp(path1, path2) != 0)
    {
        struct stat st1, st2;
        if (stat(path1, &st1) == 0 && stat(path2, &st2) == 0)
            return st1.st_dev == st2.st_dev;
    }
#endif
    return true; /* assume they are the same */
}
