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

#include "../syspass-priv.h" /* get_pass */

#include <klib/rc.h>

#include <conio.h> /* _getch */
#include <stdio.h>

rc_t get_pass(const char *prompt, char *buf, size_t bufsiz) {
    if (prompt == NULL || buf == NULL) {
        return RC(rcExe, rcFunction, rcEvaluating, rcParam, rcNull);
    }

    fprintf(stderr, "%s", prompt);

    {
        size_t i = 0;
        for (i = 0; i < bufsiz; ++i) {
            buf[i] = _getch();
            if (buf[i] == '\r') {
                fprintf(stderr, "\r\n");
                buf[i] = '\0';
                return 0;
            }
        }
        return RC(rcExe, rcString, rcReading, rcBuffer, rcInsufficient);
    }
}
