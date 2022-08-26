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
#ifndef _h_type_matcher_
#define _h_type_matcher_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_matcher_input_
#include "matcher_input.h"
#endif

typedef struct matcher matcher;

/* initializes the matcher */
rc_t matcher_init( matcher** self );

/* destroys the matcher */
rc_t matcher_destroy( matcher* self );

/* shows the matching-matrix */
rc_t matcher_report( matcher* self, const bool only_copy_columns );

/* makes a typecast-string for the source-table by column-name */
rc_t matcher_src_cast( const matcher* self, const char *name, char **cast );

/* checks if a column with the given name has at least one type
   that is also in the given typelist... */
bool matcher_src_has_type( const matcher* self, const VSchema * s,
                           const char *name, const Vector *id_vector );

/* makes a typecast-string for the destination-table by column-name */
rc_t matcher_dst_cast( const matcher* self, const char *name, char **cast );

/* performs a type-match between src/dst with the given in_struct */
rc_t matcher_execute( matcher* self, const p_matcher_input in );


rc_t matcher_db_execute( matcher* self, const VTable * src_tab, VTable * dst_tab,
                         const VSchema * schema, const char * columns, 
                         const char * kfg_path );

#ifdef __cplusplus
}
#endif

#endif
