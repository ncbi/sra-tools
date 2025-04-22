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

#include <kapp/main.h>
#include <kns/manager.h>

#include <stdio.h>

ver_t CC KAppVersion ( void )
{
    return 0x01020003;
}

rc_t CC UsageSummary ( const char *progname )
{
    return 0;
}

rc_t CC Usage ( const Args *args )
{
    return 0;
}


rc_t CC KMain ( int argc, char *argv [] )
{
    rc_t rc;
    const char * user_agent;

    rc = KNSManagerGetUserAgent ( &user_agent );
    if ( rc == 0 )
        printf("%s\n", user_agent);

    return rc;
}
