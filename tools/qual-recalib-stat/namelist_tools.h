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
#ifndef _h_namelist_tools_
#define _h_namelist_tools_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/namelist.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/rc.h>

int nlt_strcmp( const char* s1, const char* s2 );
rc_t nlt_make_namelist_from_string( const KNamelist **list, const char * src );
bool nlt_is_name_in_namelist( const KNamelist *list, const char *name_to_find );
bool nlt_namelist_intersect( const KNamelist *list1, const KNamelist *list2 );

rc_t nlt_remove_names_from_namelist( const KNamelist *source,
            const KNamelist **dest, const KNamelist *to_remove );

rc_t nlt_remove_strings_from_namelist( const KNamelist *source,
            const KNamelist **dest, const char *items_to_remove );

#ifdef __cplusplus
}
#endif

#endif
