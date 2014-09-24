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
#include "namelist_tools.h"

#include <sysalloc.h>

#include <stdlib.h>
#include <string.h>


int nlt_strcmp( const char* s1, const char* s2 )
{
    size_t n1 = string_size ( s1 );
    size_t n2 = string_size ( s2 );
    return string_cmp ( s1, n1, s2, n2, ( n1 < n2 ) ? n2 : n1 );
}


rc_t nlt_make_namelist_from_string( const KNamelist **list, const char * src )
{
    VNamelist *v_names;
    rc_t rc = VNamelistMake ( &v_names, 5 );
    if ( rc == 0 )
    {
        char * s = string_dup_measure ( src, NULL );
        if ( s )
        {
            uint32_t str_begin = 0;
            uint32_t str_end = 0;
            char c;
            do
            {
                c = s[ str_end ];
                if ( c == ',' || c == 0 )
                {
                    if ( str_begin < str_end )
                    {
                        char c_temp = c;
                        s[ str_end ] = 0;
                        rc = VNamelistAppend ( v_names, &(s[str_begin]) );
                        s[ str_end ] = c_temp;
                    }
                    str_begin = str_end + 1;
                }
                str_end++;
            } while ( c != 0 && rc == 0 );
            free( s );
        }
        rc = VNamelistToConstNamelist ( v_names, list );
        VNamelistRelease( v_names );
    }
    return rc;
}

bool nlt_is_name_in_namelist( const KNamelist *list,
                             const char *name_to_find )
{
    uint32_t count, idx;
    bool res = false;
    if ( list == NULL || name_to_find == NULL )
        return res;
    if ( KNamelistCount( list, &count ) == 0 )
    {
        for ( idx = 0; idx < count && res == false; ++idx )
        {
            const char *item_name;
            if ( KNamelistGet( list, idx, &item_name ) == 0 )
            {
                if ( nlt_strcmp( item_name, name_to_find ) == 0 )
                    res = true;
            }
        }
    }
    return res;
}

/*
    - list1 and list2 containts strings
    - if one of the strings in list2 is contained ( partial match, strstr() )
      in one of the strings in list1 the function returns true...
*/
bool nlt_namelist_intersect( const KNamelist *list1, const KNamelist *list2 )
{
    uint32_t count1;
    bool res = false;
    if ( list1 == NULL || list2 == NULL )
        return res;
    if ( KNamelistCount( list1, &count1 ) == 0 )
    {
        uint32_t idx1;
        for ( idx1 = 0; idx1 < count1 && res == false; ++idx1 )
        {
            const char *string1;
            if ( KNamelistGet( list1, idx1, &string1 ) == 0 )
            {
                uint32_t count2;
                if ( KNamelistCount( list2, &count2 ) == 0 )
                {
                    uint32_t idx2;
                    for ( idx2 = 0; idx2 < count2 && res == false; ++idx2 )
                    {
                        const char *string2;
                        if ( KNamelistGet( list2, idx2, &string2 ) == 0 )
                        {
                            if ( strstr( string1, string2 ) != NULL )
                                res = true;
                        }
                    }
                }
            }
        }
    }
    return res;
}

rc_t nlt_remove_names_from_namelist( const KNamelist *source,
            const KNamelist **dest, const KNamelist *to_remove )
{
    rc_t rc = 0;
    uint32_t count;
    
    if ( source == NULL || dest == NULL || to_remove == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    *dest = NULL;
    rc = KNamelistCount( source, &count );
    if ( rc == 0 && count > 0 )
    {
        VNamelist *cleaned;
        rc = VNamelistMake ( &cleaned, count );
        if ( rc == 0 )
        {
            uint32_t idx;
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                const char *s;
                rc = KNamelistGet( source, idx, &s );
                if ( rc == 0 )
                {
                    if ( !nlt_is_name_in_namelist( to_remove, s ) )
                        rc = VNamelistAppend ( cleaned, s );
                }
                rc = VNamelistToConstNamelist ( cleaned, dest );
            }
        }
    }
    return rc;
}

rc_t nlt_remove_strings_from_namelist( const KNamelist *source,
            const KNamelist **dest, const char *items_to_remove )
{
    rc_t rc = 0;
    const KNamelist *to_remove;
    
    if ( source == NULL || dest == NULL || items_to_remove == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    rc = nlt_make_namelist_from_string( &to_remove, items_to_remove );
    if ( rc == 0 )
    {
        rc = nlt_remove_names_from_namelist( source, dest, to_remove );
        KNamelistRelease( to_remove );
    }
    return rc;
}
