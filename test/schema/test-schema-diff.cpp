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

/**
* VSchema diff test.
* Parses input files with 2 schema parsers (old/new), matches the dumps of resulting VSchema objects, reports discrepancies
*/

#include "../../libs/schema/SchemaParser.hpp"
#include "../../libs/schema/ParseTree.hpp"

using namespace std;
using namespace ncbi::SchemaParser;

//////////////////////////////////////////// Main
#include <kfc/except.h>
#include <kapp/args.h>
#include <kfg/config.h>
#include <klib/out.h>
#include <kfs/directory.h>
#include <vdb/schema.h>
#include <vdb/manager.h>
#include <vfs/manager.h>
#include <vfs/path.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

#include "../libs/schema/ASTBuilder.hpp"

rc_t CC FlushSchema ( void *fd, const void * buffer, size_t size )
{
    ostream & out = * static_cast < ostream * > ( fd );
    out . write ( static_cast < const char * > ( buffer ), size );
    out . flush ();
    return 0;
}

void
DumpSchema ( const VSchema * p_schema, ostream & p_out )
{
    if ( VSchemaDump ( p_schema, sdmPrint, 0, FlushSchema, & p_out ) != 0 )
    {
        throw runtime_error ( "DumpSchema failed" );
    }
}

extern "C"
{

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}

const char UsageDefaultName[] = "test-schema-diff";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ( "Usage:\n" "\t%s [options] schema-file ... \n\n", progname );
}

rc_t CC Usage( const Args* args )
{
    return 0;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    KConfigDisableUserSettings();
    
    int failed = 0;
    if ( argc < 2 )
    {
        cout << "Usage:\n\t" << argv[0] << " [-Ipath ... ] schema-file ..." << endl;
        return 1;
    }
    try
    {
        struct KDirectory * wd;
        if ( KDirectoryNativeDir ( & wd ) != 0 )
        {
            throw runtime_error ( "KDirectoryNativeDir failed" );
        }

        struct VDBManager const * vdb;
        if ( VDBManagerMakeRead ( & vdb, wd ) != 0 )
        {
            throw runtime_error ( "VDBManagerMakeRead failed" );
        }

        string outputDir = "./";
        uint32_t fileCount = 0;

        for ( int i = 0 ; i < argc - 1; ++i )
        {
            const char * arg = argv [ i + 1 ];
            if ( arg [ 0 ] == '-' )
            {
                switch ( arg [ 1 ] )
                {
                    case 'I':
                        if ( VDBManagerAddSchemaIncludePath ( vdb, arg + 2 ) )
                        {
                            throw runtime_error ( string ( "VDBManagerAddSchemaIncludePath(" ) + ( arg + 2 ) + ") failed" );
                        }
                        break;
                    case 'o':
                        outputDir = arg + 2;
                        outputDir += "/";
                        break;
                    default:
                        cout << "Unknown option " << arg << endl;
                        break;
                }
                continue;
            }

            VFSManager * vfs;
            if ( VFSManagerMake ( & vfs ) != 0 )
            {
                throw runtime_error ( "VFSManagerMake failed" );
            }
            struct VPath * path;
            if ( VFSManagerMakeSysPath ( vfs, & path, arg ) != 0 )
            {
                throw runtime_error ( "VFSManagerMakeSysPath failed" );
            }

            cout << " Parsing " << arg;
            ++ fileCount;

            string oldSchemaStr;
            string newSchemaStr;
            bool compare = true;

            // old parser
            {
                VSchema * schema;
                if ( VDBManagerMakeSchema ( vdb, & schema ) != 0 )
                {
                    throw runtime_error ( "VDBManagerMakeSchema failed" );
                }
                if ( VSchemaParseFile ( schema, "%s", arg ) != 0 )
                {
                    throw runtime_error ( "VSchemaParseFile (old) failed" );
                }

                ostringstream out;
                DumpSchema ( schema, out );
                oldSchemaStr = out . str ();

                VSchemaRelease ( schema );
            }

            // new parser
            {
                HYBRID_FUNC_ENTRY( rcSRA, rcSchema, rcParsing );

                struct KFile const * file;
                if ( VFSManagerOpenFileRead ( vfs, & file, path )  != 0 )
                {
                    throw runtime_error ( "VFSManagerOpenFileRead failed" );
                }

                SchemaParser parser;
                if ( ! parser . ParseFile ( ctx, file, arg ) )
                {
                    cout << endl;
                    const ErrorReport & errors = parser . GetErrors ();
                    uint32_t count = errors . GetCount ();
                    for ( uint32_t i = 0; i < count; ++i )
                    {
                        const ErrorReport :: Error * err = errors . GetError ( i );
                        cerr << err -> m_file << ":" << err -> m_line << ":" << err -> m_column << ":" << err -> m_message << endl;
                    }
                    cout << endl << string ( "ParseFile failed! " ) << endl;
                    ++ failed;
                    compare = false;
                }
                else if ( FAILED () )
                {
                    cerr << WHAT () << endl;
                    cout << string ( "ParseFile failed! " ) << endl;
                    ++ failed;
                    compare = false;
                }
                else if ( FAILED () )
                {
                    cerr << WHAT () << endl;
                    cout << string ( "ParseFile failed! " ) << endl;
                    ++ failed;
                    compare = false;
                }
                else
                {
                    VSchema * schema;
                    if ( VDBManagerMakeSchema ( vdb, & schema ) != 0 )
                    {
                        throw runtime_error ( "VDBManagerMakeSchema failed" );
                    }

                    ASTBuilder b ( ctx, schema );
                    AST * root = b . Build ( ctx, * parser . GetParseTree (), arg );

                    uint32_t count = b . GetErrorCount ();
                    if ( count > 0 )
                    {
                        cout << endl << string ( "AST build failed: " ) << endl;
                        for ( uint32_t i = 0 ; i < count; ++ i )
                        {
                            const ErrorReport :: Error * err = b . GetErrors () . GetError ( i );
                            if ( err -> m_file != 0 && * err -> m_file != 0)
                            {
                                cout << err -> m_file << ":";
                                if ( err -> m_line != 0 )
                                {
                                    cout << err -> m_line << ":" << err -> m_column << ":";
                                }
                                cout << " error:";
                            }
                            cout << err -> m_message << endl;
                        }
                        ++ failed;
                        compare = false;
                    }

                    AST :: Destroy ( root );

                    ostringstream out;
                    DumpSchema ( schema, out );
                    newSchemaStr = out . str ();

                    VSchemaRelease ( schema );
                }

                KFileRelease ( file );
            }

            if ( compare )
            {
                if ( oldSchemaStr != newSchemaStr )
                {
                    string filename = arg;
                    replace ( filename . begin (), filename . end (), '/', '_' );

                    ofstream oldfile ( ( outputDir + filename + ".old" ) . c_str () );
                    oldfile << oldSchemaStr;
                    oldfile . close();

                    ofstream newfile ( ( outputDir + filename + ".new" ) . c_str () );
                    newfile << newSchemaStr;
                    newfile . close();

                    cout << " ... schema dump mismatch, see " << filename << ".old/.new" << endl;
                    ++ failed;
                }
                else
                {
                    cout << " ... OK" << endl;
                }
            }

            VPathRelease ( path );
            VFSManagerRelease ( vfs );
        }
        cout << "Failed: " << failed << " out of " << fileCount << endl;

        VDBManagerRelease ( vdb );
        KDirectoryRelease ( wd );
    }
    catch ( exception& ex)
    {
        cerr << " Exception: " << ex . what () << endl;
        return 2;
    }
    catch ( ... )
    {
        cerr << " Unknown exception" << endl;
        return 3;
    }
    return failed == 0 ? 0 : 4;
}

}

