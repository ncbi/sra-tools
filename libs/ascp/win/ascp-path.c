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

#include "ascp-priv.h" /* ascp_path */
#include <assert.h>

static int size_of(const char **array) {
    int i = 0;
    while (*(array++) != NULL) {
        ++i;
    }
    return i;
}

bool ascp_path(const char **cmd, const char **key) {
    static int idx = 0;

    static const char *c[] = {
        "C:\\Program Files (x86)\\Aspera\\Aspera Connect\\bin\\ascp.exe",
        "C:\\Program Files (x86)\\Aspera\\Aspera Connect\\bin\\ascp.exe",

              "C:\\Program Files\\Aspera\\Aspera Connect\\bin\\ascp.exe",
              "C:\\Program Files\\Aspera\\Aspera Connect\\bin\\ascp.exe",
    };

    static const char *k[] = {
"C:\\Program Files (x86)\\Aspera\\Aspera Connect\\etc\\asperaweb_id_dsa.openssh"
                                                                               ,
"C:\\Program Files (x86)\\Aspera\\Aspera Connect\\etc\\asperaweb_id_dsa.putty",

      "C:\\Program Files\\Aspera\\Aspera Connect\\etc\\asperaweb_id_dsa.openssh"
                                                                               ,
      "C:\\Program Files\\Aspera\\Aspera Connect\\etc\\asperaweb_id_dsa.putty",
    };

    int size = 4;

    assert(cmd != NULL && key != NULL);
//  assert(size_of(c) == size_of(k));

    if (idx < size) {
        *cmd = c[idx];
        *key = k[idx];
        ++idx;
        return true;
    }
    else {
        *cmd =  *key = NULL;
        idx = 0;
        return false;
    }
}
