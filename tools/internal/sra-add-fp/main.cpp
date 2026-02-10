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
* ===========================================================================
*
*/

#include <string>
#include <iostream>
#include <sstream>

#include <klib/out.h>
#include <klib/log.h>

#include <kapp/main.h>
#include <kapp/args.h>

#include <kdb/manager.h>
#include <kdb/meta.h>

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <insdc/insdc.h>

#include <fingerprint.hpp>

#include "../../shared/toolkit.vers.h"

using namespace std;
using namespace VDB;

#define DISP_RC(rc, msg) if (rc != 0) { LOGERR(klogInt, rc, msg), throw msg; }

static
uint32_t
AddColumn( const VCursor * cursor, const char * name )
{
    uint32_t idx = 0;
    rc_t rc = VCursorAddColumn( cursor, &idx, name);
    if ( rc != 0 )
    {
        ostringstream ex;
        ex << "Error adding column " << name << ", rc = " << rc;
        throw logic_error( ex.str() );
    }
    return idx;
}

void
CheckRc( rc_t rc, const string& message )
{
    if ( rc != 0 )
    {
        LOGERR(klogErr, rc, message.c_str() );
        throw logic_error( message );
    }
}

static
void
VTableAddFingerprint( VTable * tbl)
{
    KMetadata * meta = nullptr;
    CheckRc( VTableOpenMetadataUpdate ( tbl, & meta ), "Cannot open metadata" );

    KMDataNode * node = nullptr;
    if ( KMetadataOpenNodeRead( meta, (const KMDataNode **) & node, "QC/current/fingerprint" ) == 0 )
    {
        KMDataNodeRelease( node );
        throw logic_error( string( "Fingerprinting data already exists") );
    }
    KMDataNodeRelease( node );

    CheckRc( KMetadataOpenNodeUpdate( meta, &node, "QC/current/fingerprint" ), "Error opening node QC/current/fingerprint" );
    const VCursor * cur = nullptr;
    CheckRc( VTableCreateCursorRead( tbl, & cur ), "Error creating cursor" );

    uint32_t colReadIdx  = AddColumn( cur, "READ" );
    uint32_t colStartIdx = AddColumn( cur, "READ_START" );
    uint32_t colLenIdx   = AddColumn( cur, "READ_LEN" );
    uint32_t colTypeIdx  = AddColumn( cur, "READ_TYPE" );

    CheckRc( VCursorOpen( cur ), "Error opening cursor" );

    int64_t firstRow = 0;
    uint64_t rowCount = 0;
    CheckRc( VCursorIdRange ( cur, colReadIdx, & firstRow, & rowCount ), "Error calling VCursorIdRange" );

    Fingerprint fp;

    for ( uint64_t i = 0; i < rowCount; ++i )
    {
        int64_t rowId = firstRow + i;

        const char * spot_buffer = nullptr;
        uint32_t spot_len = 0;
        CheckRc( VCursorCellDataDirect( cur, rowId, colReadIdx, nullptr, (const void**)&spot_buffer, nullptr, &spot_len), "Error reading READ" );

        uint32_t* read_start_buffer = nullptr;
        uint32_t read_start_len = 0;
        CheckRc( VCursorCellDataDirect( cur, rowId, colStartIdx, nullptr, (const void**)&read_start_buffer, nullptr, &read_start_len), "Error reading READ_START" );

        uint32_t* read_len_buffer = nullptr;
        uint32_t read_len_len = 0;
        CheckRc( VCursorCellDataDirect( cur, rowId, colLenIdx, nullptr, (const void**)&read_len_buffer, nullptr, &read_len_len), "Error reading READ_LEN" );

        uint8_t* read_type_buffer = nullptr;
        uint32_t read_type_len = 0;
        CheckRc( VCursorCellDataDirect( cur, rowId, colTypeIdx, nullptr, (const void**)&read_type_buffer, nullptr, &read_type_len), "Error reading READ_TYPE" );

        //cout << rowId << ": " << string( spot_buffer, spot_len ) << endl;
        for( uint32_t i = 0; i < read_len_len; ++i )
        {
            // const char * type = "";
            // switch(read_type_buffer[i])
            // {
            // case SRA_READ_TYPE_TECHNICAL: type = "TECH"; break;
            // case SRA_READ_TYPE_BIOLOGICAL: type = "BIO"; break;
            // default:
            //     throw logic_error( string( "Unknown read type" ) );
            // }
            // cout << i << ": START=" << read_start_buffer[i] <<
            //             " LEN=" << read_len_buffer[i] <<
            //             " " << type << endl;

            if ( read_type_buffer[i] == SRA_READ_TYPE_BIOLOGICAL )
            {
                string read = string( spot_buffer + read_start_buffer[i], read_len_buffer[i] );
                // cout << "recording " << read << endl;

                fp.record( read );
            }
        }
    }

    ostringstream out;
    JSON_ostream j ( out, true );
    j << '{';
    fp.canonicalForm( j );
    j << '}';

    //cout << out.str() << endl;

    CheckRc( KMDataNodeWriteCString ( node, out.str().c_str() ), "Error writing fingerprint" );

    KMDataNodeRelease( node );
    VCursorRelease( cur );
    KMetadataRelease( meta );
}

static
void
HandleTable( VDBManager * mgr, const char * path )
{
    VTable * tbl = nullptr;

    CheckRc( VDBManagerOpenTableUpdate ( mgr, & tbl, nullptr, path ), string("Cannot open for update: '") + path + "'" );

    VTableAddFingerprint( tbl );

    VTableRelease( tbl );
}

static
void
HandleDatabase( VDBManager * mgr, const char * path )
{
    VDatabase *db = nullptr;
    CheckRc( VDBManagerOpenDBUpdate ( mgr, &db, nullptr, path ), string("Cannot open for update: '") + path + "'" );

    VTable * tbl = nullptr;
    CheckRc( VDatabaseOpenTableUpdate( db, & tbl, "SEQUENCE" ), string("Cannot open SEQUENCE table for update: '") + path + "'" );

    VTableAddFingerprint( tbl );

    VTableRelease( tbl );
    VDatabaseRelease ( db );
}

static
void
AddFingerprint( const char * run )
{
    VDBManager * mgr = nullptr;
    CheckRc( VDBManagerMakeUpdate( & mgr, nullptr ), "VDBManagerMakeUpdate failed" );

    rc_t rc = VDBManagerWritable( mgr, run );
    if (rc != 0)
    {
        switch ( GetRCState(rc) )
        {
        case rcNotFound:
            throw logic_error( string("Accession not found: '") + run + "'" );
        case rcIncorrect:
            throw logic_error( string("Invalid accession file: '") + run + "'" );
        case rcLocked:
            {
                CheckRc( VDBManagerUnlock( mgr, run ),
                        string( "cannot unlock '" ) + run + "'" );
                CheckRc( VDBManagerWritable( mgr, run ), string( "failed to unlock '" ) + run + "'" );
                break;
            }
        default:
            CheckRc( rc, string( run ) + " is read-only" );
        }
    }

    switch ( VDBManagerPathType( mgr, "%s", run ) & ~ kptAlias )
    {
    case kptDatabase :
        HandleDatabase( mgr, run );
        break;
    case kptTable :
        HandleTable( mgr, run );
        break;
    case kptPrereleaseTbl :
        throw logic_error( string( "Prerelase Tables are not suported" ) );
    default:
        throw logic_error( string( "Invalid accession path" ) );
    }
}

const char UsageDefaultName[] = "sra-add-fp";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s <path> [options]\n"
                    "Summary:\n"
                    "  Add fingerprinting data to the SEQUENCE table in the given accession directory.\n"
                    "\n", progname);
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == nullptr )
    {
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    }
    else
    {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }

    if ( rc )
    {
        progname = fullpath = UsageDefaultName;
    }

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );

    HelpOptionsStandard ();

    HelpVersion ( fullpath, TOOLKIT_VERS );

    return rc;
}

MAIN_DECL(argc, argv)
{
    VDB::Application app(argc, argv);

    try
    {
        SetUsage( Usage );
        SetUsageSummary( UsageSummary );

        Args * args = nullptr;
        CheckRc( ArgsMakeAndHandle( &args, app.getArgC(), app.getArgV(), 0, nullptr, 0 ), "ArgsMakeAndHandle() failed" );

        uint32_t param_count = 0;
        CheckRc( ArgsParamCount( args, &param_count ), "ArgsParamCount() failed" );

        if ( param_count == 0 || param_count > 1 )
        {
            MiniUsage(args);
            app.setRc( 1 );
        }
        else
        {
            const char * accession = nullptr;
            CheckRc( ArgsParamValue( args, 0, ( const void ** )&( accession ) ), "ArgsParamValue() failed" );
            AddFingerprint( accession );
        }

        ArgsRelease( args );
    }
    catch ( exception & x )
    {
        cerr <<  x.what () << endl;
        app.setRc( 1 );
    }

    return app.getExitCode();
}
