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

#pragma once

#include <ncbi/secure/string.hpp>

#include <vector>
#include <map>

namespace ncbi
{
    /* EnvImport
     *  a structure for importing startup parameters
     */
    struct EnvImport
    {
        template < class T >
        void addParam ( T & value, const String & name );

        template < class T >
        void addList ( std :: vector < T > & list, const char separator,
            U32 max, const String & name );

        void import ();

        EnvImport ();
        EnvImport ( const String & prefix );
        ~ EnvImport ();

        struct Import
        {
            virtual void handleParam ( const String & text ) const = 0;

            Import ( const String & name );
            virtual ~ Import ();

            String name;
        };

        void addParam ( Import * import );

        std :: vector < Import * > imports;
        std :: map < String, Import * > name_map;
        String prefix;
    };

    /* EnvExport
     *  a structure representing environment for child process
     */
    struct EnvExport
    {
        size_t count () const;
        void set ( const String & name, const String & val );
        void add ( const String & name, const String & val, char sep = ',' );
        void unset ( const String & name );

        // import existing values with the given prefix
        // followed by a '_' character
        void import ( const String & prefix );

        size_t populate ( char ** envp, size_t size );
        size_t populateKids ( char ** envp, size_t size );

        void link ( EnvExport * dad );

        EnvExport ( const String & scope );
        ~ EnvExport ();

        std :: map < String, String > env;
        std :: vector < EnvExport * > kids;
        String scope;
        EnvExport * dad;
    };
}
