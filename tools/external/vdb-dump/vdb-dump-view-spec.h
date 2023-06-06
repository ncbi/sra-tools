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

#ifndef _h_vdb_dump_view_spec_
#define _h_vdb_dump_view_spec_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/text.h>
#include <klib/vector.h>

struct VCursor;
struct VDatabase;
struct VSchema;
struct VView;

typedef struct view_spec view_spec;
struct view_spec
{
    char *  name;
    Vector  args; /* view_spec*; 0 elements if table */
};

rc_t view_spec_parse ( const char * spec, view_spec ** self, char * error, size_t error_size );
void view_spec_free ( view_spec * self );

rc_t view_spec_open ( view_spec * self, const struct VDatabase * db, const struct VSchema * schema, const struct VView ** view ) ;

#ifdef __cplusplus
}
#endif

#endif
