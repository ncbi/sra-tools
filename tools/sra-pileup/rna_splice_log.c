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

#include <klib/rc.h>
#include <klib/printf.h>
#include <klib/vector.h>

#include <kfs/directory.h>
#include <kfs/file.h>

#include <string.h>

#include "rna_splice_log.h"

typedef struct rna_splice_dict rna_splice_dict;
struct rna_splice_dict
{
    KVector * v;
};


struct rna_splice_dict * make_rna_splice_dict( void )
{
    struct rna_splice_dict * res = NULL;
    KVector * v;
    rc_t rc = KVectorMake ( &v );
    if ( rc == 0 )
    {
        res = calloc( 1, sizeof * res );
        if ( res != NULL )
        {
            res->v = v;
        }
        else
        {
            KVectorRelease ( v );
        }
    }
    return res;
}


void free_rna_splice_dict( struct rna_splice_dict * dict )
{
    if ( dict != NULL )
    {
        KVectorRelease ( dict->v );
        free( dict );
    }
}


typedef struct splice_dict_key splice_dict_key;
struct splice_dict_key
{
    uint32_t len;
    uint32_t pos;
};

union dict_key_union
{
    uint64_t key;
    splice_dict_key key_struct;
};

union dict_value_union
{
    uint64_t value;
    splice_dict_entry entry;
};


bool rna_splice_dict_get( struct rna_splice_dict * dict,
                          uint32_t pos, uint32_t len, splice_dict_entry * entry )
{
    bool res = false;
    if ( dict != NULL )
    {
        rc_t rc;
        union dict_key_union ku;
        union dict_value_union vu;

        ku.key_struct.pos = pos;
        ku.key_struct.len = len;
        rc = KVectorGetU64 ( dict->v, ku.key, &(vu.value) );
        res = ( rc == 0 );
        if ( res && entry != NULL )
        {
            entry->count = vu.entry.count;
            entry->intron_type = vu.entry.intron_type;
        }
    }
    return res;
}


void rna_splice_dict_set( struct rna_splice_dict * dict,
                          uint32_t pos, uint32_t len, const splice_dict_entry * entry )
{
    if ( dict != NULL && entry != NULL )
    {
        union dict_key_union ku;
        union dict_value_union vu;

        ku.key_struct.pos = pos;
        ku.key_struct.len = len;
        vu.entry.count = entry->count;
        vu.entry.intron_type = entry->intron_type;
        KVectorSetU64 ( dict->v, ku.key, vu.value );
    }
}


/* --------------------------------------------------------------------------- */


typedef struct rna_splice_log rna_splice_log;
struct rna_splice_log
{
    KFile * log_file;
    const char * tool_name;
    struct ReferenceObj const * ref_obj;

    char ref_name[ 1024 ];
    uint64_t log_file_pos;
};


struct rna_splice_log * make_rna_splice_log( const char * filename, const char * toolname )
{
    struct rna_splice_log * res = NULL;
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir ( &dir );
    if ( rc == 0 )
    {
        KFile * f;
        rc = KDirectoryCreateFile ( dir, &f, false, 0664, kcmInit, "%s", filename );
        if ( rc == 0 )
        {
            res = calloc( 1, sizeof * res );
            if ( res != NULL )
            {
                res->log_file = f;
                if ( toolname != NULL )
                    res->tool_name = string_dup_measure ( toolname, NULL );
            }
            else
                KFileRelease ( f );
        }
        KDirectoryRelease ( dir );
    }
    return res;
}


void free_rna_splice_log( struct rna_splice_log * sl )
{
    if ( sl != NULL )
    {
        KFileRelease ( sl->log_file );
        if ( sl->tool_name != NULL ) free( ( void * )sl->tool_name );
        free( ( void * ) sl );
    }
}


void rna_splice_log_enter_ref( struct rna_splice_log * sl,
                               const char * ref_name,
                               struct ReferenceObj const * ref_obj )
{
    if ( sl != NULL )
    {
        if ( ref_name != NULL )
            string_copy_measure ( sl->ref_name, sizeof( sl->ref_name ), ref_name );
        else
            sl->ref_name[ 0 ] = 0;

        sl->ref_obj = ref_obj;
    }
}


static void copy_read_and_reverse_complement( uint8_t * dst, const uint8_t * const src, INSDC_coord_len const count )
{
    static char const compl[] = {
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 , '.',  0 , 
        '0', '1', '2', '3',  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C', 
        'D',  0 ,  0 , 'M',  0 , 'K', 'N',  0 , 
         0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W', 
         0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C', 
        'D',  0 ,  0 , 'M',  0 , 'K', 'N',  0 , 
         0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W', 
         0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 , 
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0
    };

    INSDC_coord_len i, j;
    
    for ( i = 0, j = count - 1; i != count; ++i, --j )
    {
        dst[ i ] = compl[ src[ j ] ];
    }
}


#define PRE_POST_LEN 10
#define EDGE_LEN ( ( PRE_POST_LEN * 2 ) + 2 )


static rc_t write_to_file( struct rna_splice_log * sl, const uint8_t * src, size_t len )
{
    size_t num_writ;
    rc_t rc = KFileWriteAll( sl->log_file, sl->log_file_pos, src, len, &num_writ );
    if ( rc == 0 )
        sl->log_file_pos += num_writ;
    return rc;
}

static rc_t print_edge( struct rna_splice_log * sl,
                        INSDC_coord_zero pos,
                        bool const reverse_complement,
                        bool const add_newline )
{
    rc_t rc;
    INSDC_coord_len from_ref_obj, to_read;
    uint8_t buffer[ EDGE_LEN + 1 ];
    INSDC_coord_zero rd_pos = 0;
    uint32_t pre_len = PRE_POST_LEN;
    uint32_t post_len = PRE_POST_LEN;

    if ( pos >= PRE_POST_LEN )
        rd_pos = ( pos - PRE_POST_LEN ); /* in the rare case the delete is at the very beginning of the alignment */
    else
        pre_len = pos; /* rd_pos is still 0, what we want*/

    to_read = pre_len + post_len + 2;
    rc = ReferenceObj_Read( sl->ref_obj, rd_pos, to_read, buffer, &from_ref_obj );
    if ( rc == 0 )
    {
        uint8_t complement[ EDGE_LEN + 1 ];
        uint8_t to_write[ EDGE_LEN + 5 ];
        uint8_t * ref_bytes = buffer;

        if ( from_ref_obj < to_read )
            post_len -= ( to_read - from_ref_obj );

        if ( reverse_complement )
        {
            copy_read_and_reverse_complement( complement, buffer, from_ref_obj );
            ref_bytes = complement;
        }
        memcpy( to_write, ref_bytes, pre_len );
        to_write[ pre_len ] = '\t';
        to_write[ pre_len + 1 ] = ref_bytes[ pre_len ];
        to_write[ pre_len + 2 ] = ref_bytes[ pre_len + 1 ];
        to_write[ pre_len + 3 ] = '\t';
        memcpy( &( to_write[ pre_len + 4 ] ), &( ref_bytes[ pre_len + 2 ] ), post_len );

        if ( add_newline )
            to_write[ pre_len + post_len + 4 ] = '\n';
        else
            to_write[ pre_len + post_len + 4 ] = '\t';

        rc = write_to_file( sl, to_write, pre_len + post_len + 5 );
    }
    return rc;
}


/*
#define INTRON_UNKNOWN 0
#define INTRON_FWD 1
#define INTRON_REV 2
*/

static const char intron_type_to_ascii[] = { 'u', '+', '-', 'u' };

static rc_t CC on_dict_key_value( uint64_t key, uint64_t value, void * user_data )
{
    rc_t rc = 0;
    struct rna_splice_log * sl = ( struct rna_splice_log * )user_data;
    if ( sl != NULL )
    {
        char tmp[ 512 ];
        size_t num_writ;
        union dict_key_union ku;
        union dict_value_union vu;
        char intron;
        bool reverse_complement;

        ku.key = key;
        vu.value = value;
        intron = intron_type_to_ascii[ vu.entry.intron_type & 0x03 ];
        reverse_complement = ( ( vu.entry.intron_type & 0x03 ) == INTRON_REV );

        rc = string_printf ( tmp, sizeof tmp, &num_writ,
                             "%s\t%u\t%u\t%u\t%c\t",
                             sl->ref_name, ku.key_struct.pos + 1, ku.key_struct.len, vu.entry.count, intron );
        if ( rc == 0 )
            rc = write_to_file( sl, ( uint8_t * )tmp, num_writ );

        if ( reverse_complement )
        {
            if ( rc == 0 )
                rc = print_edge( sl, ku.key_struct.pos + ku.key_struct.len - 2, true, false );
            if ( rc == 0 )
                rc = print_edge( sl, ku.key_struct.pos, true, true );
        }
        else
        {
            if ( rc == 0 )
                rc = print_edge( sl, ku.key_struct.pos, false, false );
            if ( rc == 0 )
                rc = print_edge( sl, ku.key_struct.pos + ku.key_struct.len - 2, false, true );
        }
    }
    return rc;
}


void rna_splice_log_exit_ref( struct rna_splice_log * sl, struct rna_splice_dict * dict )
{
    if ( sl != NULL && dict != NULL )
    {
        KVectorVisitU64 ( dict->v, false, on_dict_key_value, sl );
    }
}

