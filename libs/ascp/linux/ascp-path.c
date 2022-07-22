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
* ==============================================================================
*
*/

#include "ascp-priv.h" /* ascp_path */

#include <klib/printf.h> /* string_printf */

#include <assert.h>
#include <limits.h> /* PATH_MAX */
#include <stdlib.h> /* getenv */

static int size_of(const char **array) {
    int i = 0;
    while (*(array++) != NULL) {
        ++i;
    }
    return i;
}

bool ascp_path(const char **cmd, const char **key) {
    static int idx = 0;
    static const char *k[] = {
        "/opt/aspera/etc/asperaweb_id_dsa.openssh",
        "/opt/aspera/etc/asperaweb_id_dsa.putty",

        "/opt/aspera/etc/asperaweb_id_dsa.openssh",
        "/opt/aspera/etc/asperaweb_id_dsa.putty",

        "/opt/aspera/etc/asperaweb_id_dsa.openssh",
        "/opt/aspera/etc/asperaweb_id_dsa.putty",
        NULL
    };
    static const char *c[] = {
                            "ascp",                 "ascp",
                   "/usr/bin/ascp",        "/usr/bin/ascp",
            "/opt/aspera/bin/ascp", "/opt/aspera/bin/ascp",
            NULL
        };
    int size = size_of(c);
    assert(cmd != NULL && key != NULL);
    assert(size_of(c) == size_of(k));
    if (idx < size) {
        *cmd = c[idx];
        *key = k[idx];
        ++idx;
        return true;
    }
    else {
        rc_t rc = 0;
        static char k[PATH_MAX] = "";
        static char c[PATH_MAX] = "";
        if (idx > size + 1) {
            *cmd = *key = NULL;
            idx = 0;
            return false;
        }
        {
            size_t num_writ = 0;
            const char* home = getenv("HOME");
            if (home == NULL) {
                home = "";
            }
            if (idx == size) {
                rc = string_printf(k, sizeof k, &num_writ,
                    "%s/.aspera/connect/etc/asperaweb_id_dsa.openssh", home);
            }
            else {
                rc = string_printf(k, sizeof k, &num_writ,
                    "%s/.aspera/connect/etc/asperaweb_id_dsa.putty"  , home);
            }
            if (rc != 0 || num_writ >= PATH_MAX) {
                assert(0);
                k[0] = '\0';
            }
            else {
                rc = string_printf(c, sizeof c, &num_writ,
                    "%s/.aspera/connect/bin/ascp", home);
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
}
