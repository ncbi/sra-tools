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
 *  Author: Kurt Rodarmer
*
* ===========================================================================
*
*/

#include <ncbi/secure/string.hpp>

#include <climits>

namespace ncbi
{

    long long int decToLongLongInteger ( const String & s )
    {
        // walk across the string
        auto it = s . makeIterator ();

        // must be non-empty
        if ( ! it . isValid () )
            throw InvalidNumeral ( XP ( XLOC ) << "empty numeric string" );

        // detect negation
        bool negate = false;
        if ( * it == '-' )
        {
            negate = true;
            if ( ! ( ++ it ) . isValid () )
                throw InvalidNumeral ( XP ( XLOC ) << "expected digits in numeric string" );
        }
        else if ( * it == '+' )
        {
            if ( ! ( ++ it ) . isValid () )
                throw InvalidNumeral ( XP ( XLOC ) << "expected digits in numeric string" );
        }

        // must have at least 1 digit
        if ( ! iswdigit ( * it ) )
        {
            throw InvalidNumeral (
                XP ( XLOC )
                << "non-numeric string - expected digit but found '"
                << putc ( * it )
                << "' at position "
                << it . charIndex ()
                );
        }

        // initial value
        long long int val = * it - '0';

        // loop over remaining string
        while ( ( ++ it ) . isValid () )
        {
            // non-digits are fatal
            if ( ! iswdigit ( * it ) )
            {
                throw InvalidNumeral (
                    XP ( XLOC )
                    << "non-numeric string - expected digit but found '"
                    << putc ( * it )
                    << "' at position "
                    << it . charIndex ()
                    );
            }

            // detect overflow
            if ( val > ( LLONG_MAX / 10 ) )
            {
                throw OverflowException (
                    XP ( XLOC )
                    << "numeric string of length "
                    << s . length ()
                    << " overflows long long int"
                    << " on mult: val => " << val
                    );
            }

            // shift previous value to next decimal place
            val *= 10;

            // get the value of ones place
            int d = * it - '0';

            // detect overflow
            if ( val > ( LLONG_MAX - d ) )
            {
                // ah, the asymmetry of negative space
                if ( negate )
                {
                    // perform the test again
                    if ( - val >= ( LLONG_MIN + d ) )
                    {
                        // make sure we're at end of string
                        if ( ! ( ++ it ) . isValid () )
                        {
                            // do it now
                            return - val - d;
                        }
                    }
                }

                throw OverflowException (
                    XP ( XLOC )
                    << "numeric string of length "
                    << s . length ()
                    << " overflows long long int"
                    << " on add: val => " << val
                    );
            }

            // complete number
            val += d;
        }

        return negate ? - val : val;
    }

}
