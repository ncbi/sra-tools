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

#ifndef _h_vdb_redactval_
#define _h_vdb_redactval_

#ifndef _h_vdb_copy_includes_
#include "vdb-copy-includes.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/********************************************************************
    redact_buffer is a pointer and a size...
********************************************************************/
typedef struct redact_buffer
{
    void *   buffer;
    uint32_t buffer_len;
} redact_buffer;
typedef redact_buffer* p_redact_buffer;


void redact_buf_init( redact_buffer * rbuf );
void redact_buf_free( redact_buffer * rbuf );
rc_t redact_buf_resize( redact_buffer * rbuf, const size_t new_size );

/********************************************************************
    redact_val is a mapping of typename to its redaction-value
********************************************************************/
typedef struct redact_val
{
    char *name;             /* the name of the type */
    uint32_t len;           /* the length of the value */
    void * value;           /* pointer to the value of length len */
} redact_val;
typedef redact_val* p_redact_val;


/********************************************************************
    vector of redact-values
********************************************************************/
typedef struct redact_vals
{
    Vector vals;
} redact_vals;
typedef redact_vals* p_redact_vals;


/*
 * fills a buffer with the value found in a redact-val-struct
*/
void redact_val_fill_buffer( const p_redact_val r_val,
                             redact_buffer * rbuf,
                             const size_t buffsize );


/*
 * initializes a redact-val-list
*/
rc_t redact_vals_init( redact_vals** vals );


/*
 * destroys the redact-val-list
*/
rc_t redact_vals_destroy( redact_vals* vals );


/*
 * adds a entry into the redact-val-list
*/
rc_t redact_vals_add( redact_vals* vals, const char* name, 
                      const uint32_t len, const char* value );


/*
 * returns a pointer to a redact-value by name
*/
p_redact_val redact_vals_get_by_name( const redact_vals* vals,
                                      const char * name );


/*
 * returns a pointer to a redact-value by type-cast
*/
p_redact_val redact_vals_get_by_cast( const redact_vals* vals,
                                      const char * cast );

#ifdef __cplusplus
}
#endif

#endif
