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
#include <limits.h> /* PATH_MAX */
#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

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

rc_t ErrMsg( const char * fmt, ... ) {
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;

    va_list list;
    va_start( list, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
    if ( 0 == rc ) {
        rc = pLogMsg( klogErr, "$(E)", "E=%s", buffer );
    }
    va_end( list );
    return rc;
} 

static const char * dflt_seq_defline_fastq_use_name_ri = "@$ac.$si/$ri $sn length=$rl";
static const char * dflt_seq_defline_fastq_syn_name_ri = "@$ac.$si/$ri $si length=$rl";
static const char * dflt_seq_defline_fasta_use_name_ri = ">$ac.$si/$ri $sn length=$rl";
static const char * dflt_seq_defline_fasta_syn_name_ri = ">$ac.$si/$ri $si length=$rl";

static const char * dflt_seq_defline_fastq_use_name = "@$ac.$si $sn length=$rl";
static const char * dflt_seq_defline_fastq_syn_name = "@$ac.$si $si length=$rl";
static const char * dflt_seq_defline_fasta_use_name = ">$ac.$si $sn length=$rl";
static const char * dflt_seq_defline_fasta_syn_name = ">$ac.$si $si length=$rl";

const char * dflt_seq_defline( bool use_name, bool use_read_id, bool fasta ) {
    if ( use_read_id ) {
        if ( fasta ) {
            return use_name ? dflt_seq_defline_fasta_use_name_ri : dflt_seq_defline_fasta_syn_name_ri;
        } else {
            return use_name ? dflt_seq_defline_fastq_use_name_ri : dflt_seq_defline_fastq_syn_name_ri;
        }
    } else {
        if ( fasta ) {
            return use_name ? dflt_seq_defline_fasta_use_name : dflt_seq_defline_fasta_syn_name;
        } else {
            return use_name ? dflt_seq_defline_fastq_use_name : dflt_seq_defline_fastq_syn_name;
        }
    }
    return NULL;
}

static const char * dflt_qual_defline_use_name_ri = "+$ac.$si/$ri $sn length=$rl";
static const char * dflt_qual_defline_syn_name_ri = "+$ac.$si/$ri $si length=$rl";

static const char * dflt_qual_defline_use_name = "+$ac.$si $sn length=$rl";
static const char * dflt_qual_defline_syn_name = "+$ac.$si $si length=$rl";

const char * dflt_qual_defline( bool use_name, bool use_read_id ) {
    if ( use_read_id ) {
        return use_name ? dflt_qual_defline_use_name_ri : dflt_qual_defline_syn_name_ri;
    } else {
        return use_name ? dflt_qual_defline_use_name : dflt_qual_defline_syn_name;
    }
    return NULL;
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

rc_t pack_4na( const String * unpacked, SBuffer_t * packed ) {
    rc_t rc = 0;
    if ( unpacked -> len < 1 ) {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcNull );
    } else {
        if ( unpacked -> len > 0xFFFF ) {
            rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcExcessive );
        } else {
            uint32_t i;
            uint8_t * src = ( uint8_t * )unpacked -> addr;
            uint8_t * dst = ( uint8_t * )packed -> S . addr;
            uint16_t dna_len = ( unpacked -> len & 0xFFFF );
            uint32_t len = 0;
            dst[ len++ ] = ( dna_len >> 8 );
            dst[ len++ ] = ( dna_len & 0xFF );
            for ( i = 0; i < unpacked -> len; ++i ) {
                if ( len < packed -> buffer_size ) {
                    uint8_t base = ( src[ i ] & 0x0F );
                    if ( 0 == ( i & 0x01 ) ) {
                        dst[ len ] = ( base << 4 );
                    } else {
                        dst[ len++ ] |= base;
                    }
                }
            }
            if ( unpacked -> len & 0x01 ) {
                len++;
            }
            packed -> S . size = packed -> S . len = len;
        }
    }
    return rc;
}

static const char xASCII_to_4na[ 256 ] = {
    /* 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x10 0x11 0x12 0x13 0x14 0x15 0x16 0x17 0x18 0x19 0x1A 0x1B 0x1C 0x1D 0x1E 0x1F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x20 0x21 0x22 0x23 0x24 0x25 0x26 0x27 0x28 0x29 0x2A 0x2B 0x2C 0x2D 0x2E 0x2F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x30 0x31 0x32 0x33 0x34 0x35 0x36 0x37 0x38 0x39 0x3A 0x3B 0x3C 0x3D 0x3E 0x3F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x40 0x41 0x42 0x43 0x44 0x45 0x46 0x47 0x48 0x49 0x4A 0x4B 0x4C 0x4D 0x4E 0x4F */
    /* @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O */
       0,   1,   0,   2,   0,   0,   0,   4,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x50 0x51 0x52 0x53 0x54 0x55 0x56 0x57 0x58 0x59 0x5A 0x5B 0x5C 0x5D 0x5E 0x5F */
    /* P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _ */
       0,   0,   0,   0,   8,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x60 0x61 0x62 0x63 0x64 0x65 0x66 0x67 0x68 0x69 0x6A 0x6B 0x6C 0x6D 0x6E 0x6F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x70 0x71 0x72 0x73 0x74 0x75 0x76 0x77 0x78 0x79 0x7A 0x7B 0x7C 0x7D 0x7E 0x7F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x80 0x81 0x82 0x83 0x84 0x85 0x86 0x87 0x88 0x89 0x8A 0x8B 0x8C 0x8D 0x8E 0x8F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0x90 0x91 0x92 0x93 0x94 0x95 0x96 0x97 0x98 0x99 0x9A 0x9B 0x9C 0x9D 0x9E 0x9F */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xA0 0xA1 0xA2 0xA3 0xA4 0xA5 0xA6 0xA7 0xA8 0xA9 0xAA 0xAB 0xAC 0xAD 0xAE 0xAF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xB0 0xB1 0xB2 0xB3 0xB4 0xB5 0xB6 0xB7 0xB8 0xB9 0xBA 0xBB 0xBC 0xBD 0xBE 0xBF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xC0 0xC1 0xC2 0xC3 0xC4 0xC5 0xC6 0xC7 0xC8 0xC9 0xCA 0xCB 0xCC 0xCD 0xCE 0xCF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xD0 0xD1 0xD2 0xD3 0xD4 0xD5 0xD6 0xD7 0xD8 0xD9 0xDA 0xDB 0xDC 0xDD 0xDE 0xDF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xE0 0xE1 0xE2 0xE3 0xE4 0xE5 0xE6 0xE7 0xE8 0xE9 0xEA 0xEB 0xEC 0xED 0xEE 0xEF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,

    /* 0xF0 0xF1 0xF2 0xF3 0xF4 0xF5 0xF6 0xF7 0xF8 0xF9 0xFA 0xFB 0xFC 0xFD 0xFE 0xFF */
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

rc_t pack_read_2_4na( const String * read, SBuffer_t * packed ) {
    rc_t rc = 0;
    if ( read -> len < 1 ) {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcNull );
    } else {
        if ( read -> len > 0xFFFF ) {
            rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcExcessive );
        } else {
            uint32_t i;
            uint8_t * src = ( uint8_t * )read -> addr;
            uint8_t * dst = ( uint8_t * )packed -> S . addr;
            uint16_t dna_len = ( read -> len & 0xFFFF );
            uint32_t len = 0;
            dst[ len++ ] = ( dna_len >> 8 );
            dst[ len++ ] = ( dna_len & 0xFF );
            for ( i = 0; i < read -> len; ++i ) {
                if ( len < packed -> buffer_size ) {
                    uint8_t base = ( xASCII_to_4na[ src[ i ] ] & 0x0F );
                    if ( 0 == ( i & 0x01 ) ) {
                        dst[ len ] = ( base << 4 );
                    } else {
                        dst[ len++ ] |= base;
                    }
                }
            }
            if ( read -> len & 0x01 ) {
                len++;
            }
            packed -> S . size = packed -> S . len = len;
        }
    }
    return rc;
}

static const char x4na_to_ASCII_fwd[ 16 ] = {
    /* 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
       'N', 'A', 'C', 'N', 'G', 'N', 'N', 'N', 'T', 'N', 'N', 'N', 'N', 'N', 'N', 'N'
};

static const char x4na_to_ASCII_rev[ 16 ] = {
    /* 0x00 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
       'N', 'T', 'G', 'N', 'C', 'N', 'N', 'N', 'A', 'N', 'N', 'N', 'N', 'N', 'N', 'N'
};

rc_t unpack_4na( const String * packed, SBuffer_t * unpacked, bool reverse ) {
    rc_t rc = 0;
    uint8_t * src = ( uint8_t * )packed -> addr;
    uint16_t dna_len;

    /* the first 2 bytes are the 16-bit dna-length */
    dna_len = src[ 0 ];
    dna_len <<= 8;
    dna_len |= src[ 1 ];

    if ( dna_len > unpacked -> buffer_size ) {
        rc = increase_SBuffer( unpacked, dna_len - unpacked -> buffer_size );
    }
    if ( 0 == rc ) {
        uint8_t * dst = ( uint8_t * )unpacked -> S . addr;
        uint32_t dst_idx;
        uint32_t i;

        /* use the complement-lookup-table in case of reverse */
        const char * lookup = reverse ? x4na_to_ASCII_rev : x4na_to_ASCII_fwd;

        dst_idx = reverse ? dna_len - 1 : 0;

        for ( i = 2; i < packed -> len; ++i ) {
            /* get the packed byte out of the packed input */
            uint8_t packed_byte = src[ i ];

            /* write the first unpacked byte */
            if ( dst_idx < unpacked -> buffer_size ) {
                dst[ dst_idx ] = lookup[ ( packed_byte >> 4 ) & 0x0F ];
                dst_idx += reverse ? -1 : 1;
            }

            /* write the second unpacked byte */
            if ( dst_idx < unpacked -> buffer_size ) {
                dst[ dst_idx ] = lookup[ packed_byte & 0x0F ];
                dst_idx += reverse ? -1 : 1;
            }
        }

        /* set the dna-length in the output-string */
        unpacked -> S . size = dna_len;
        unpacked -> S . len = ( uint32_t )unpacked -> S . size;

        /* terminated the output-string, just in case */
        dst[ dna_len ] = 0;
    }
    return rc;
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

static bool ends_in_sra( const char * s ) {
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

const char * extract_acc( const char * s ) {
    const char * res = NULL;
    if ( ( NULL != s ) && ( !ends_in_slash( s ) ) ) {
        size_t size = string_size ( s );
        char * slash = string_rchr ( s, size, '/' );
        if ( NULL == slash ) {
            if ( ends_in_sra( s ) ) {
                res = string_dup ( s, size - 4 );
            } else {
                res = string_dup ( s, size );
            }
        } else {
            char * tmp = slash + 1;
            if ( ends_in_sra( tmp ) ) {
                res = string_dup ( tmp, string_size ( tmp ) - 4 );
            } else {
                res = string_dup ( tmp, string_size ( tmp ) );
            }
        }
    }
    return res;
}

const char * extract_acc2( const char * s ) {
    const char * res = NULL;
    VFSManager * mgr;
    rc_t rc = VFSManagerMake ( &mgr );
    if ( 0 != rc ) {
        ErrMsg( "extract_acc2( '%s' ).VFSManagerMake() -> %R", s, rc );
    } else {
        VPath * orig;
        rc = VFSManagerMakePath ( mgr, &orig, "%s", s );
        if ( 0 != rc ) {
            ErrMsg( "extract_acc2( '%s' ).VFSManagerMakePath() -> %R", s, rc );
        } else {
            VPath * acc_or_oid = NULL;
            rc = VFSManagerExtractAccessionOrOID( mgr, &acc_or_oid, orig );
            if ( 0 != rc ) { /* remove trailing slash[es] and try again */
                char P_option_buffer[ PATH_MAX ] = "";
                size_t l = string_copy_measure( P_option_buffer, sizeof P_option_buffer, s );
                char * basename = P_option_buffer;
                while ( l > 0 && strchr( "\\/", basename[ l - 1 ]) != NULL ) {
                    basename[ --l ] = '\0';
                }
                VPath * orig = NULL;
                rc = VFSManagerMakePath ( mgr, &orig, "%s", P_option_buffer );
                if ( 0 != rc ) {
                    ErrMsg( "extract_acc2( '%s' ).VFSManagerMakePath() -> %R", P_option_buffer, rc );
                } else {
                    rc = VFSManagerExtractAccessionOrOID( mgr, &acc_or_oid, orig );
                    if ( 0 != rc ) {
                        ErrMsg( "extract_acc2( '%s' ).VFSManagerExtractAccessionOrOID() -> %R", s, rc );
                    }
                    {
                        rc_t r2 = VPathRelease ( orig );
                        if ( 0 != r2 ) {
                            ErrMsg( "extract_acc2( '%s' ).VPathRelease().2 -> %R", P_option_buffer, rc );
                            if ( 0 == rc ) { rc = r2; }
                        }
                    }
                }
            }
            if ( 0 == rc ) {
                char buffer[ 1024 ];
                size_t num_read;
                rc = VPathReadPath ( acc_or_oid, buffer, sizeof buffer, &num_read );
                if ( 0 != rc ) {
                    ErrMsg( "extract_acc2( '%s' ).VPathReadPath() -> %R", s, rc );
                } else {
                    res = string_dup ( buffer, num_read );
                }
                rc = VPathRelease ( acc_or_oid );
                if ( 0 != rc ) {
                    ErrMsg( "extract_acc2( '%s' ).VPathRelease().1 -> %R", s, rc );
                }
            }

            rc = VPathRelease ( orig );
            if ( 0 != rc ) {
                ErrMsg( "extract_acc2( '%s' ).VPathRelease().2 -> %R", s, rc );
            }
        }

        rc = VFSManagerRelease ( mgr );
        if ( 0 != rc ) {
            ErrMsg( "extract_acc2( '%s' ).VFSManagerRelease() -> %R", s, rc );
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
    dst -> terminate_on_invalid = src -> terminate_on_invalid;
}
