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


#ifndef _h_vdb_dump_print_
#define _h_vdb_dump_print_

#include <vdb/schema.h> /* for VTypedesc */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vdp_opts
{
    bool print_dna_bases;
    bool in_hex;
    bool without_sra_types;
    char c_boolean; /* how boolean is printed '1' ... 0/1, 'T' ... T/F, /0 ... true/false */
} vdp_opts;


/* vdp_print_cell
 *  prints the content of a cursor-cell to stdout
 *
 *
 *  "elem_bits" [ IN ] - element size in bits
 *
 *  "base" [ IN ] - pointer to cell starting bit
 *
 *  "boff" [ IN ] - bit offset in BITS of first bit to be printed
 *
 *  "row_len" [ IN ] - the number of elements in cell
 *
 *  "type_desc" [ IN ] - type description of the cell ( from schema )
 */

rc_t vdp_print_cell_2_buffer( char * buf, size_t buf_size, size_t *num_written,
                              const uint32_t elem_bits, const void * base, uint32_t boff, uint32_t row_len,
                              const VTypedesc * type_desc, vdp_opts * opts );

rc_t vdp_print_cell( const uint32_t elem_bits, const void * base, uint32_t boff, uint32_t row_len,
                     const VTypedesc * type_desc, vdp_opts * opts );


#ifdef __cplusplus
}
#endif

#endif