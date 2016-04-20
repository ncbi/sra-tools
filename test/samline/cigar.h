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

#ifndef _h_cigar_
#define _h_cigar_

#ifdef __cplusplus
extern "C" {
#endif

struct cigar_t;

struct cigar_t * make_cigar_t( const char * cigar_str );
void free_cigar_t( struct cigar_t * c );

int cigar_t_reflen( const struct cigar_t * c );
int cigar_t_readlen( const struct cigar_t * c );
int cigar_t_inslen( const struct cigar_t * c );

size_t cigar_t_string( char * buffer, size_t buf_len, const struct cigar_t * c );

struct cigar_t * merge_cigar_t( const struct cigar_t * c );

size_t md_tag( char * buffer, size_t buf_len,
               const struct cigar_t * c, const char * read, const char * reference );

void debug_cigar_t( const struct cigar_t * c );

size_t cigar_t_2_read( char * buffer, size_t buf_len,
                       const struct cigar_t * c, const char * ref_bases, const char * ins_bases );

#ifdef __cplusplus
}
#endif

#endif
