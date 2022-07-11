#include "ascp-priv.h" /* ascp_path */
#include <klib/printf.h> /* string_printf */
#include <assert.h>
#include <limits.h> /* PATH_MAX */
#include <stdlib.h> /* getenv */

bool ascp_path(const char **cmd, const char **key) {
    static int idx = 0;
    static const char *k[] = {
 "/Applications/Aspera Connect.app/Contents/Resources/asperaweb_id_dsa.openssh",
 "/Applications/Aspera Connect.app/Contents/Resources/asperaweb_id_dsa.putty",
    };
    static const char c[] = "/Applications/Aspera Connect.app/Contents/"
        "Resources/ascp";
    assert(cmd != NULL && key != NULL);
    if (idx == 0 || idx == 1) {
        ++idx;
        *cmd = c;
        *key = k[idx];
        return true;
    }
    else if (idx == 2 || idx == 3) {
        rc_t rc = 0;
        static char k[PATH_MAX] = "";
        static char c[PATH_MAX] = "";
        {
            size_t num_writ = 0;
            const char* home = getenv("HOME");
            if (home == NULL) {
                home = "";
            }
            if (idx == 2) {
                rc = string_printf(k, sizeof k, &num_writ,
"%s/Applications/Aspera Connect.app/Contents/Resources/asperaweb_id_dsa.openssh"
                , home);
            }
            else {
                rc = string_printf(k, sizeof k, &num_writ,
"%s/Applications/Aspera Connect.app/Contents/Resources/asperaweb_id_dsa.putty"
                , home);
            }
            if (rc != 0 || num_writ >= PATH_MAX) {
                assert(0);
                k[0] = '\0';
            }
            else {
                rc = string_printf(c, sizeof c, &num_writ,
 "%s/Applications/Aspera Connect.app/Contents/Resources/ascp"
                   , home);
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
        return true;
    }
    else {
        idx = 0;
        *cmd =  *key = NULL;
        return false;
    }
}
