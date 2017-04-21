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

#ifndef _h_slice_
#define _h_slice_

#include <klib/rc.h>
#include <klib/text.h>
#include <klib/vector.h>

#include <kapp/args.h>

typedef struct slice
{
    uint64_t start, count, end;
    const String * refname;
} slice;


slice * make_slice( uint64_t start, uint64_t count, const String * refname );

void release_slice( slice * slice );

void print_slice( slice * slice );

slice * make_slice_from_str( const String * S );

bool filter_by_slice( const slice * slice, const String * refname, uint64_t start, uint64_t end );

bool filter_by_slices( const Vector * slices, const String * refname, uint64_t pos, uint32_t len );

rc_t get_slice( const Args * args, const char *option, slice ** slice );

rc_t get_slices( const Args * args, const char *option, Vector * slices );

#endif