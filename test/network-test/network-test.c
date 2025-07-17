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

#include "network-test.vers.h"

#include <klib/out.h>
#include <klib/rc.h>

#include <kapp/main.h>
#include <kapp/args.h>

#include <kfs/file.h>

#include <os-native.h>
#include <sysalloc.h>

#include <stdio.h>

#include "network-testing.h"

const char UsageDefaultName[] = "network-test";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg( "\nUsage:\n %s [accession]\n\n", progname );
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}


/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
ver_t CC KAppVersion ( void )
{
    return NETWORK_TEST_VERS;
}

static rc_t CC write_to_FILE ( void *f, const char *buffer, size_t bytes, size_t *num_writ )
{
    * num_writ = fwrite ( buffer, 1, bytes, f );
    if ( * num_writ != bytes )
        return RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    return 0;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE( argc, argv, VDB_INIT_FAILED );

    rc_t rc = KOutHandlerSet ( write_to_FILE, stdout );
    if ( rc == 0 )
	{
		if ( argc < 2 )
			rc = perfrom_network_test( "SRR000001" );
		else
			rc = perfrom_network_test( argv[ 1 ] );
	}

    return VDB_TERMINATE( rc );
}

