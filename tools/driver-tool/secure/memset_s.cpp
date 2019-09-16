/*==============================================================================
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
 *  Author: Kurt Rodarmer
 *
 * ===========================================================================
 *
 */

#include "memset-priv.hpp"

#include <cstring>
#include <string.h>

#define PRINT_MEMSET_RESULT 0

#if PRINT_MEMSET_RESULT
#include <stdio.h>
#define PRINTF( ... ) printf ( __VA_ARGS__ )
#else
#define PRINTF( ... ) ( ( void ) 0 )
#endif

namespace ncbi
{
    /** memset
     * hopefully this is a memset that doesn't get easily optimized out
     * our wonderful C/C++ stewards have seen proper the introduction of
     * very clever optimizations that unfortunately change the semantics
     * of the language in ways that can be danced around due to the language
     * not having defined behavior with regard to de-allocated memory.
     */
    void memset_while_respecting_language_semantics ( void * dest, size_t dsize, int ch, size_t count, const char * str )
    {
        if ( count > dsize )
            count = dsize;

        PRINTF ( "memset: contents before: '%s'\n", str );

        memset ( dest, ch, count );

        PRINTF ( "memset: contents after: '%s'\n", str );

#if SEC_TESTING
        for ( size_t i = 0; i < count; ++ i )
        {
            if ( ( int ) str [ i ] != ch )
                throw "error in memset_s replacement code";
        }
#endif
        
    }

}
