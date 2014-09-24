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

#ifndef _hpp_sratoolkit_exception_
#define _hpp_sratoolkit_exception_

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#include <exception>
#include <string>

namespace sra
{

    /* Exception
     *  create with or without an RC
     *  with or without an accompanying text string
     */
    class Exception : std::exception
    {
    public:

        /* what
         *  implement std::exception::what
         */
        virtual const char *what () const
            throw ();

        /* return_code
         *  get the offending rc
         */
        inline rc_t return_code () const
        { return rc; }

        /* file_name
         * get the filename where error occurred
         */
        inline const char *file_name () const
        { return filename; }

        /* line_number
         *  get the line number where error occurred
         */
        inline uint32_t line_number () const
        { return lineno; }

        /* CONSTRUCTORS */

        /* with only an RC
         *  just records rc, filename and lineno
         *  initializes what with empty or unspecified or something
         */
        Exception ( rc_t rc, const char * filename, uint32_t lineno )
            throw ();

        /* with a printf-style error message
         *  records filename and lineno
         *  uses string_vprintf to build error_msg from fmt + args
         *  initializes rc with an unknown error code
         */
        Exception ( const char * filename, uint32_t lineno, const char *fmt, ... )
            throw ();

        /* with an RC and a printf-style error message
         *  records rc, filename and lineno
         *  uses string_vprintf to build error_msg from fmt + args
         */
        Exception ( rc_t rc, const char * filename, uint32_t lineno, const char *fmt, ... )
            throw ();

        /* destructor
         *  tears down string
         */
        virtual ~ Exception ()
            throw ();

    protected:

        std :: string error_msg;
        const char *filename;
        uint32_t lineno;
        rc_t rc;
    };
}

#endif /* _hpp_sratoolkit_exception_ */
