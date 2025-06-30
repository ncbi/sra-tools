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
* ============================================================================*/

#include "ascp-priv.h" /* ascp_path */
#include <klib/printf.h> /* string_printf */
#include <assert.h>
#include <limits.h> /* PATH_MAX */
#include <stdlib.h> /* getenv */

#define OLD "/Applications/Aspera Connect.app/Contents/Resources"
#define NEW "/Applications/IBM Aspera Connect.app/Contents/Resources"
#define BIN "ascp"
#define KEY "aspera_tokenauth_id_rsa"

bool ascp_path(const char **cmd, const char **key) {
    static int idx = 0;
    assert(cmd != NULL && key != NULL);
    if (idx == 0) {
        static const char k[] = NEW "/" KEY;
        static const char c[] = NEW "/" BIN;
        *cmd = c;
        *key = k;
        idx++;
        return true;
    }
    else if (idx == 1) {
        static const char k[] = OLD "/" KEY;
        static const char c[] = OLD "/" BIN;
        *cmd = c;
        *key = k;
        idx++;
        return true;
    }
    else if (idx == 2 || idx == 3) {
        rc_t rc = 0;
        static char k[PATH_MAX] = "";
        static char c[PATH_MAX] = "";
        {
            size_t num_writ = 0;
            const char *home = getenv("HOME");
            if (home == NULL) {
                home = "";
            }
            rc = string_printf(k, sizeof k, &num_writ, "%s%s/" KEY,
                    home, idx == 2 ? NEW : OLD);
            if (rc != 0 || num_writ >= PATH_MAX) {
                assert(0);
                k[0] = '\0';
            }
            else {
                rc = string_printf(c, sizeof c, &num_writ, "%s%s/" BIN,
                    home, idx == 2 ? NEW : OLD);
                if (rc != 0 || num_writ >= PATH_MAX) {
                    assert(0);
                    c[0] = '\0';
                }
            }
        }
        if (rc != 0) {
            *cmd = *key = NULL;
            idx = 0;
            return false;
        }
        else {
            *cmd = c;
            *key = k;
            ++idx;
            return true;
        }
    }
    else {
        idx = 0;
        *cmd =  *key = NULL;
        return false;
    }
}
