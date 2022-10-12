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

#include "test-sra-priv.h" /* PrintOS */

#include <klib/debug.h> /* DBGMSG */
#include <klib/out.h> /* OUTMSG */

#include <errno.h>
#include <string.h> /* memset */
#include <stdio.h> /* perror */

#include <sys/utsname.h> /* uname */

rc_t PrintOS(bool xml) {
    int ret = 1;

    struct utsname unameData;
    memset(&unameData, 0, sizeof unameData);

    errno = 0;
    ret = uname(&unameData);
    if (ret != 0) {
        if (xml) {
            OUTMSG(("  <Os>"));
            perror("uname returned : ");
            OUTMSG(("</Os>\n"));
        }
        else {
            perror("uname returned : ");
        }

        return 0;
    }
    else {
        const char *b = xml ? "  <Os>"  : "Operating system: '";
        const char *e = xml ? "</Os>" :                   "'\n";

        return OUTMSG(("%s%s %s %s %s %s%s\n", b,
            unameData.sysname, unameData.nodename, unameData.release,
            unameData.version, unameData.machine, e));
    }
}
