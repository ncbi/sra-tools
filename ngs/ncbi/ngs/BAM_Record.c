/* ===========================================================================
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
 */

#include <klib/rc.h>
#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <stddef.h>
#include <assert.h>
#include <string.h>

#include <sysalloc.h>

#include "NGS_ReadGroup.h"
#include "NGS_Reference.h"
#include "NGS_Alignment.h"
#include "NGS_Read.h"
#include "NGS_String.h"

#include "BAM_Record.h"

static int GetSize(int const type)
{
    switch (type) {
        case 'A':
        case 'C':
        case 'c':
            return 1;
        case 'S':
        case 's':
            return 2;
        case 'F':
        case 'I':
        case 'i':
            return 4;
        default:
            return -1;
    }
}

static void NextArrayField(BAM_Record_Extra_Field *const rslt)
{
    if (rslt->fieldsize >= 8) {
        char const *const value = rslt->value->string;
        int const size  = GetSize(value[0]);
        
        if (size > 0) {
            rslt->val_type  = value[0];
            rslt->elemcount = (value[1] + (value[2] << 8) + (value[3] << 16) + (value[4] << 24));
            rslt->value = (void const *)(value + 5);
            rslt->fieldsize = size * rslt->elemcount;
        }
        else
            rslt->fieldsize = -1;
        return;
    }
    rslt->value = NULL;
}

static void NextStringField(BAM_Record_Extra_Field *const rslt)
{
    unsigned const max = rslt->fieldsize - 3;
    for (rslt->elemcount = 0; rslt->elemcount < max; ++rslt->elemcount) {
        if (rslt->value->string[rslt->elemcount] == '\0') {
            rslt->fieldsize = rslt->elemcount + 1;
            return;
        }
    }
    rslt->value = NULL;
}

static void NextField(BAM_Record_Extra_Field *const rslt)
{
    if (rslt->fieldsize >= 4) {
        if (rslt->val_type == 'B')
            NextArrayField(rslt);
        else if (rslt->val_type == 'Z' || rslt->val_type == 'H')
            NextStringField(rslt);
        else
            rslt->fieldsize = GetSize(rslt->val_type);
        return;
    }
    rslt->value = NULL;
}

unsigned BAM_Record_ForEachExtra(BAM_Record const *const self, ctx_t ctx,
                                 BAM_Record_ForEachExtra_cb const cb,
                                 void *const usr_ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcRow, rcReading);
    char const *extra = (char const *)self->extra;
    char const *const endp = extra + self->extralen;
    unsigned cnt = 0;
    
    while (extra < endp) {
        BAM_Record_Extra_Field fld = {
            extra,
            (void const *)(extra + 3),
            1,
            (int)(endp - extra),
            extra[2]
        };
        
        NextField(&fld);
        if (fld.fieldsize <= 0) {
            USER_WARNING(xcUnexpected, "unexpected type code '%c'", fld.val_type);
            break;
        }
        extra = fld.tag + fld.fieldsize;
        if (fld.value == NULL || extra > endp) {
            USER_WARNING(xcUnexpected, "record is truncated");
            break;
        }
        if (!cb(usr_ctx, ctx, cnt++, &fld))
            break;
    }
    return cnt;
}
