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

#include "sratoolkit-exception.hpp"
#include <klib/printf.h>
#include <klib/rc.h>

namespace sra 
{

    /* Exception
     *  create with or without an RC
     *  with or without an accompanying text string
     */

    /* what
     *  implement std::exception::what
     */
    const char * Exception :: what () const
        throw ()
    {
        try
        {
            return error_msg . c_str ();
        }
        catch ( ... )
        {
            return "INTERNAL ERROR";
        }
    }

    /* with only an RC
     *  just records rc, filename and lineno
     *  initializes what with empty or unspecified or something
     */
    Exception :: Exception ( rc_t _rc, const char * _filename, uint32_t _lineno )
            throw ()
        : filename ( _filename )
        , lineno ( _lineno )
        , rc ( _rc )
    {
        try
        {
            error_msg = std :: string ( "unspecified" );
        }
        catch ( ... )
        {
        }
    }

    /* with a printf-style error message
     *  records filename and lineno
     *  uses string_vprintf to build error_msg from fmt + args
     *  initializes rc with an unknown error code
     */
    Exception :: Exception ( const char * _filename, uint32_t _lineno, const char *fmt, ... )
            throw ()
        : filename ( _filename )
        , lineno ( _lineno )
        , rc ( SILENT_RC ( rcExe, rcNoTarg, rcExecuting, rcNoObj, rcUnknown ) )
    {
    }

    /* with an RC and a printf-style error message
     *  records rc, filename and lineno
     *  uses string_vprintf to build error_msg from fmt + args
     */
    Exception :: Exception ( rc_t _rc, const char * _filename, uint32_t _lineno, const char *fmt, ... )
            throw ()
        : filename ( _filename )
        , lineno ( _lineno )
        , rc ( _rc )
    {
        try
        {
            va_list args;
            va_start ( args, fmt );

            size_t msg_size;
            char msg [ 4096 ];
            rc_t print_rc = string_vprintf ( msg, sizeof msg, & msg_size, fmt, args );

            va_end ( args );

            if ( print_rc == 0 )
                error_msg = std :: string ( msg, msg_size );
            else
                error_msg = std :: string ( "INTERNAL ERROR" );
        }
        catch ( ... )
        {
        }
    }

    /* destructor
     *  tears down string
     */
    Exception :: ~ Exception ()
        throw ()
    {
    }

}
