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

#ifndef _h_idx_to_name_
#define _h_idx_to_name_

#ifndef _h_klib_container_
#include <klib/container.h>
#endif

typedef struct idx_to_name_t {
    SLList items;
} idx_to_name_t;

void idx_to_name_init( idx_to_name_t * self );
void idx_to_name_release( idx_to_name_t * self );

void idx_to_name_add( idx_to_name_t * self, const char * name, uint32_t idx );
const char * idx_to_name_get( const idx_to_name_t * self, uint32_t idx );

#endif
