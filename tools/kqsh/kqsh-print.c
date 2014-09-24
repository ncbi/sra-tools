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

#include "kqsh-priv.h"

#include <klib/symbol.h>
#include <klib/out.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>


/* printf
 *  understands printing KSymbol
 */
static
int kqsh_print_frame ( const kqsh_stack *frame )
{
    assert ( frame != NULL );

    if ( frame -> prev != NULL )
    {
        int status = kqsh_print_frame ( frame -> prev );
        if ( status != 0 )
            return status;
        return KOutMsg ( ".%u", frame -> cmd_num );
    }

    return KOutMsg ( "%u", frame -> cmd_num );
}

static
int kqsh_vprintf ( const char *format, va_list args )
{
    int status;
    const char *start, *end;

    for ( status = 0, start = end = format; * end != 0; ++ end )
    {
        /* const String *s; */

        switch ( * end )
        {
        case '%':
            if ( end > start )
            {
                status = KOutMsg ( "%.*s", ( uint32_t ) ( end - start ), start );
                if ( status != 0 )
                    break;
            }
            switch ( * ( ++ end ) )
            {
            case 'd':
                status = KOutMsg ( "%d", va_arg ( args, int ) );
                break;
            case 'u':
                status = KOutMsg ( "%u", va_arg ( args, unsigned int ) );
                break;
            case 'x':
                status = KOutMsg ( "%x", va_arg ( args, unsigned int ) );
                break;
            case 'X':
                status = KOutMsg ( "%X", va_arg ( args, unsigned int ) );
                break;
            case 'f':
                status = KOutMsg ( "%f", va_arg ( args, double ) );
                break;
            case 'l':
                switch ( * ( ++ end ) )
                {
                case 'd':
                    status = KOutMsg ( "%ld", va_arg ( args, int64_t ) );
                    break;
                case 'u':
                    status = KOutMsg ( "%lu", va_arg ( args, uint64_t ) );
                    break;
                case 'x':
                    status = KOutMsg ( "%lx", va_arg ( args, uint64_t ) );
                    break;
                case 'X':
                    status = KOutMsg ( "%lX", va_arg ( args, uint64_t ) );
                    break;
                }
                break;
            case 'p':
                status = KOutMsg ( "0x%lX", va_arg ( args, size_t ) );
                break;
            case 's':
                status = KOutMsg ( "%s", va_arg ( args, const char* ) );
                break;
            case '.':
                if ( end [ 1 ] == '*' && end [ 2 ] == 's' )
                {
                    end += 2;
                    status = va_arg ( args, int );
                    status = KOutMsg ( "%.*s", status, va_arg ( args, const char* ) );
                    break;
                }
                /* not handling anything else */
                status = KOutMsg ( "%%." );
                break;
            case 'S':
                status = KOutMsg ( "%S", va_arg ( args, const String* ) );
                break;
            case 'N':
                status = KOutMsg ( "%N", va_arg ( args, const KSymbol* ) );
                break;
            case 'F':
                status = kqsh_print_frame ( va_arg ( args, const kqsh_stack* ) );
                break;
            case '%':
                status = KOutMsg ( "%%" );
                break;
            }

            start = end + 1;
            break;
        }

        if ( status < 0 )
            break;
    }

    if ( status >= 0 && end > start )
        status = KOutMsg ( "%.*s", ( uint32_t ) ( end - start ), start );

    fflush ( stdout );

    return status;
}

int kqsh_printf ( const char *format, ... )
{
    int status;
    va_list args;

    va_start ( args, format );
    status = kqsh_vprintf ( format, args );
    va_end ( args );

    return status;
}
