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

#ifndef _h_allele_lookup_
#define _h_allele_lookup_

#include <klib/rc.h>
#include <klib/text.h>

struct Allele_Lookup;

/* construct an allele-lookup from a path */
rc_t allele_lookup_make( struct Allele_Lookup ** al, const char * path );

/* releae an allele-lookup */
rc_t allele_lookup_release( struct Allele_Lookup * al );

/* perform an allele-lookup */
bool allele_lookup_perform( const struct Allele_Lookup * al, const String * key, uint64_t * values );


struct Lookup_Cursor;

/* create a lookup-cursor from a allel-lookup */
rc_t lookup_cursor_make( const struct Allele_Lookup * al, struct Lookup_Cursor ** curs );

/* release a lookup-cursor */
rc_t lookup_cursor_release( struct Lookup_Cursor * curs );

/* get key/value from the cursor and position to next... */
bool lookup_cursor_next( struct Lookup_Cursor * curs, String * key, uint64_t * values );

#endif