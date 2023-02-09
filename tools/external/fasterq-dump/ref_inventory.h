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

#ifndef _h_ref_inventory_
#define _h_ref_inventory_

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_cmn_iter_
#include "cmn_iter.h"
#endif

#ifndef _h_tool_ctx_
#include "tool_ctx.h"
#endif

/* ------------------------------------------------------------------------------------------------------------- */

struct ref_inventory_entry_t;

const String * ref_inventory_entry_name( const struct ref_inventory_entry_t * self );
const String * ref_inventory_entry_seq_id( const struct ref_inventory_entry_t * self );
int64_t ref_inventory_entry_first_row( const struct ref_inventory_entry_t * self );
uint64_t ref_inventory_entry_row_count( const struct ref_inventory_entry_t * self );
bool ref_inventory_entry_is_internal( const struct ref_inventory_entry_t * self );
bool ref_inventory_entry_is_circular( const struct ref_inventory_entry_t * self );

/* ------------------------------------------------------------------------------------------------------------- */

struct ref_inventory_t;

struct ref_inventory_t * make_ref_inventory( const tool_ctx_t * tool_ctx );
void destroy_ref_inventory( struct ref_inventory_t * self );
uint32_t ref_inventory_count( const struct ref_inventory_t * self );
void ref_inventory_report( const struct ref_inventory_t * self );
const struct ref_entry_t * get_ref_entry_by_name( struct ref_inventory_t * self, const String * name );
const struct ref_entry_t * get_ref_entry_by_seq_id( struct ref_inventory_t * self, const String * seq_id );

/* ------------------------------------------------------------------------------------------------------------- */

struct ref_inventory_filter_t;

typedef enum ref_inventory_location_t {
    ri_all, ri_intern, ri_extern } ref_inventory_location_t;

struct ref_inventory_filter_t * make_ref_inventory_filter( ref_inventory_location_t location );
void destroy_ref_inventory_filter( struct ref_inventory_filter_t * self );
bool add_ref_inventory_filter_name( struct ref_inventory_filter_t * self, const char * name );

/* ------------------------------------------------------------------------------------------------------------- */

struct ref_inventory_iter_t;

struct ref_inventory_iter_t * make_ref_inventory_iter( const struct ref_inventory_t * src,
                                                       const struct ref_inventory_filter_t * filter );
void destroy_ref_inventory_iter( struct ref_inventory_iter_t * self );
const struct ref_inventory_entry_t * ref_inventory_iter_next( struct ref_inventory_iter_t * self );

/* ------------------------------------------------------------------------------------------------------------- */

struct ref_bases_t;

struct ref_bases_t * make_ref_bases( const struct ref_inventory_t * inventory, 
                                     const struct ref_inventory_filter_t * filter,
                                     const tool_ctx_t * tool_ctx );
void destroy_ref_bases( struct ref_bases_t * self );
const struct ref_inventory_entry_t * ref_bases_next_ref( struct ref_bases_t * self );
bool ref_bases_next_chunk( struct ref_bases_t * self, String * dst );
    
/* ------------------------------------------------------------------------------------------------------------- */

struct ref_printer_t;

struct ref_printer_t * make_ref_printer( uint32_t limit );
void destroy_ref_printer( struct ref_printer_t * self );
bool ref_printer_add( struct ref_printer_t * self, const String * S );
bool ref_printer_flush( struct ref_printer_t * self, bool all );
    
/* ------------------------------------------------------------------------------------------------------------- */

bool test_ref_inventory( const tool_ctx_t * tool_ctx );
bool test_ref_inventory_bases( const tool_ctx_t * tool_ctx );
bool test_ref_inventory_print( const tool_ctx_t * tool_ctx );
    
/* ------------------------------------------------------------------------------------------------------------- */

#endif
