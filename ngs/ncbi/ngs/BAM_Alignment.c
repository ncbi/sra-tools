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

#include "CSRA1_Alignment.h"

typedef struct BAM_Alignment BAM_Alignment;
#define NGS_ALIGNMENT void

#include "NGS_Alignment.h"

#include "NGS_ReadCollection.h"

#include <sysalloc.h>

#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>

#include <klib/printf.h>
#include <klib/rc.h>

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include "NGS_Refcount.h"
#include "NGS_String.h"

#include <string.h>
#include <limits.h>

#include "BAM_Record.h"

struct BAM_Alignment {
    NGS_Alignment super;
    struct BAM_Record *(*provider_f)(struct NGS_ReadCollection *, ctx_t);
    struct NGS_ReadCollection *provider;
    BAM_Record *cur;
    bool primary;
    bool secondary;
};

static void BAM_AlignmentWhack(void * const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    free(self->cur);
    NGS_RefcountRelease(&self->provider->dad, ctx);
}

static NGS_String *BAM_AlignmentAlignmentId(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    (void)self;
    UNIMPLEMENTED();
    return NULL;
}

static NGS_String * BAM_AlignmentReferenceSpec(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return NGS_StringMake(ctx, self->cur->RNAME, strlen(self->cur->RNAME));

    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


static int BAM_AlignmentMappingQuality(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return self->cur->MAPQ;

    USER_ERROR(xcRowNotFound, "no current row");
    return 0;
}


static NGS_String * BAM_AlignmentReferenceBases(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    (void)self;
    UNIMPLEMENTED();
    return NULL;
}


static bool FindRG(void *const vp, ctx_t ctx, unsigned const ord, BAM_Record_Extra_Field const *const fld)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);

    if (fld->tag[0] == 'R' && fld->tag[1] == 'G' && fld->val_type == 'Z') {
        NGS_String **const prslt = vp;

        *prslt = NGS_StringMakeCopy(ctx, fld->value->string, fld->elemcount);
        return false; /* done */
    }
    return true; /* keep going */
}

static NGS_String * BAM_AlignmentReadGroup(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur) {
        NGS_String *rslt = NULL;

        BAM_Record_ForEachExtra(self->cur, ctx, FindRG, &rslt);
        return rslt;
    }
    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


static NGS_String * BAM_AlignmentReadId(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return self->cur->QNAME ? NGS_StringMake(ctx, self->cur->QNAME, strlen(self->cur->QNAME)) : NULL;

    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


typedef struct {
    unsigned start;
    unsigned length;
} clipped_t;

static clipped_t const get_clipping(BAM_Record const *rec)
{
    clipped_t rslt = { 0, rec->seqlen };

    if (rec->ncigar > 0 && (rec->cigar[0] & 0x0F) == 4) {
        unsigned const length = rec->cigar[0] >> 4;

        rslt.start += length;
        rslt.length -= length;
    }
    if (rec->ncigar > 1 && (rec->cigar[rec->ncigar - 1] & 0x0F) == 4) {
        unsigned const length = rec->cigar[rec->ncigar - 1] >> 4;

        rslt.length -= length;
    }
    return rslt;
}

static NGS_String * BAM_AlignmentClippedFragmentBases(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur) {
        if (self->cur->seqlen) {
            clipped_t const clipping = get_clipping(self->cur);
            return NGS_StringMake(ctx, self->cur->SEQ + clipping.start, clipping.length);
        }
        return NULL;
    }
    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


static NGS_String * BAM_AlignmentClippedFragmentQualities(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur) {
        if (self->cur->seqlen && self->cur->QUAL[0] != -1) {
            clipped_t const clipping = get_clipping(self->cur);
            return NGS_StringMake(ctx, self->cur->QUAL + clipping.start, clipping.length);
        }
        return NULL;
    }
    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


static bool BAM_AlignmentIsPrimary(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return (self->cur->FLAG & 0x0900) == 0 ? true : false;

    USER_ERROR(xcRowNotFound, "no current row");
    return false;
}


static int64_t BAM_AlignmentAlignmentPosition(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return self->cur->POS - 1;

    USER_ERROR(xcRowNotFound, "no current row");
    return -1;
}


static unsigned ComputeRefLen(size_t const count, uint32_t const cigar[])
{
    unsigned rslt = 0;
    unsigned i;

    for (i = 0; i < count; ++i) {
        uint32_t const op = cigar[i];
        unsigned const len = op >> 4;
        int const code = op & 0x0F;

        switch (code) {
        case 0: /* M */
        case 2: /* D */
        case 3: /* N */
        case 7: /* = */
        case 8: /* X */
            rslt += len;
            break;
        }
    }
    return rslt;
}

static uint64_t BAM_AlignmentAlignmentLength(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return ComputeRefLen(self->cur->ncigar, self->cur->cigar);

    USER_ERROR(xcRowNotFound, "no current row");
    return 0;
}


static bool BAM_AlignmentIsReversedOrientation(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return (self->cur->FLAG & 0x0010) == 0 ? false : true;

    USER_ERROR(xcRowNotFound, "no current row");
    return false;
}


static int BAM_AlignmentSoftClip(void *const vp, ctx_t ctx, bool left)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur) {
        unsigned const end = left ? 0 : (self->cur->ncigar - 1);
        uint32_t const op = self->cur->cigar[end];
        int const code = op & 0x0F;
        int const len = op >> 4;

        return code == 4 ? len : 0;
    }
    USER_ERROR(xcRowNotFound, "no current row");
    return false;
}


static uint64_t BAM_AlignmentTemplateLength(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return self->cur->TLEN;

    USER_ERROR(xcRowNotFound, "no current row");
    return 0;
}


static char const *FormatCIGAR(char *dst, char const OPCODE[], size_t const count, uint32_t const cigar[])
{
    uint32_t const *src = cigar + count;
    unsigned i;
    char *last_out;
    char last_code = 0;
    uint32_t last_len = 0;

    last_out = dst;
    for (i = 0; i < count; ++i) {
        uint32_t const op = *--src;
        char const code = OPCODE[op & 0x0F];
        uint32_t const len1 = op >> 4;
        uint32_t len = code == last_code ? ((dst = last_out), last_len + len1) : len1;

        last_len = len;
        last_code = code;
        last_out = dst;
        *--dst = code;
        for ( ; ; ) {
            *--dst = len % 10 + '0';
            if ((len /= 10) == 0)
                break;
        }
    }
    return dst;
}

static NGS_String *CIGAR(ctx_t ctx, char const OPCODE[], size_t const count, uint32_t const cigar[])
{
    size_t const max = 10 * count;
#if DEBUG
    char buffer[max + 1];
    buffer[max] = '\0';
#else
    char buffer[max];
#endif
    char *const endp = buffer + max;
    char const *const rslt = FormatCIGAR(endp, OPCODE, count, cigar);
    size_t const len = endp - rslt;

    return NGS_StringMakeCopy(ctx, rslt, len);
}

static NGS_String *CIGAR_clipped(ctx_t ctx, char const OPCODE[], bool const clipped, size_t const count, uint32_t const cigar[])
{
    if (!clipped)
        return CIGAR(ctx, OPCODE, count, cigar);
    else {
        char const first = cigar[0] & 0x0F;
        char const last  = cigar[count - 1] & 0x0F;
        unsigned const cfirst = (first == 4 || first == 5) ? 1 : 0;
        unsigned const clast  = (last  == 4 || last  == 5) ? 1 : 0;

        return CIGAR(ctx, OPCODE, count - cfirst - clast, &cigar[cfirst]);
    }
}

static NGS_String * BAM_AlignmentShortCigar(void *const vp, ctx_t ctx, bool clipped)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return CIGAR_clipped(ctx, "MIDNSHPMM???????", clipped, self->cur->ncigar, self->cur->cigar);

    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


static NGS_String * BAM_AlignmentLongCigar(void *const vp, ctx_t ctx, bool clipped)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return CIGAR_clipped(ctx, "MIDNSHP=X???????", clipped, self->cur->ncigar, self->cur->cigar);

    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


static bool BAM_AlignmentHasMate(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return (self->cur->FLAG & 0x0001) == 0 ? false : true;

    USER_ERROR(xcRowNotFound, "no current row");
    return false;
}


static NGS_String * BAM_AlignmentMateAlignmentId(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    (void)self;
    UNIMPLEMENTED();
    return 0;
}


static void * BAM_AlignmentMateAlignment(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    (void)self;
    UNIMPLEMENTED();
    return NULL;
}


static NGS_String * BAM_AlignmentMateReferenceSpec(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return self->cur->RNEXT ? NGS_StringMake(ctx, self->cur->RNEXT, strlen(self->cur->RNEXT)) : NULL;

    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


static bool BAM_AlignmentMateIsReversedOrientation(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur)
        return (self->cur->FLAG & 0x0020) == 0 ? false : true;

    USER_ERROR(xcRowNotFound, "no current row");
    return false;
}

static bool BAM_AlignmentIsFirst(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur) {
        bool const isMated = (self->cur->FLAG & 0x001) == 0 ? false : true;
        bool const isFirst = (self->cur->FLAG & 0x040) == 0 ? false : true;
        bool const isLast  = (self->cur->FLAG & 0x080) == 0 ? false : true;
        return (isMated && isFirst && !isLast);
    }
    USER_ERROR(xcRowNotFound, "no current row");
    return false;
}


/*--------------------------------------------------------------------------
 * NGS_AlignmentIterator
 */

static bool ShouldSkip(BAM_Alignment const *const self)
{
    if (!self->cur->RNAME) /* not aligned */
        return true;

    if ((self->cur->FLAG & 0x0900) == 0 && !self->primary) /* is primary and don't want primary */
        return true;

    if ((self->cur->FLAG & 0x0900) != 0 && !self->secondary) /* is secondary and don't want secondary */
        return true;

    return false;
}

static bool BAM_AlignmentIteratorNext(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    do {
        if (self->cur)
            free(self->cur);
        self->cur = self->provider_f(self->provider, ctx);
        if (FAILED() || !self->cur)
            break;
    } while (ShouldSkip(self));

    return self->cur != NULL;
}


static NGS_String * BAM_AlignmentFragmentGetId(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur) {
        return self->cur->QNAME ? NGS_StringMake(ctx, self->cur->QNAME, strlen(self->cur->QNAME)) : NGS_StringMake(ctx, "", 0);
    }
    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


static NGS_String * BAM_AlignmentFragmentGetBases(void *const vp, ctx_t ctx,
                                                  uint64_t offset, uint64_t length)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur) {
        if (offset + length < self->cur->seqlen) {
            return NGS_StringMake(ctx, self->cur->SEQ + offset, length);
        }
        USER_ERROR(xcRowNotFound, "invalid offset or length");
        return NULL;
    }
    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


static NGS_String * BAM_AlignmentFragmentGetQualities(void *const vp, ctx_t ctx,
                                                      uint64_t offset, uint64_t length)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    if (self->cur) {
        if (offset + length < self->cur->seqlen) {
            return NGS_StringMake(ctx, self->cur->QUAL + offset, length);
        }
        USER_ERROR(xcRowNotFound, "invalid offset or length");
        return NULL;
    }
    USER_ERROR(xcRowNotFound, "no current row");
    return NULL;
}


static bool BAM_AlignmentFragmentNext(void *const vp, ctx_t ctx)
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcAccessing);
    BAM_Alignment *const self = (BAM_Alignment *)vp;

    (void)self;
    UNIMPLEMENTED();
    return NULL;
}



static NGS_Alignment_vt const vt =
{
    {
        {
            /* NGS_Refcount */
            BAM_AlignmentWhack
        },

        /* NGS_Fragment */
        BAM_AlignmentFragmentGetId,
        BAM_AlignmentFragmentGetBases,
        BAM_AlignmentFragmentGetQualities,
        BAM_AlignmentFragmentNext
    },

    BAM_AlignmentAlignmentId,
    BAM_AlignmentReferenceSpec,
    BAM_AlignmentMappingQuality,
    BAM_AlignmentReferenceBases,
    BAM_AlignmentReadGroup,
    BAM_AlignmentReadId,
    BAM_AlignmentClippedFragmentBases,
    BAM_AlignmentClippedFragmentQualities,
    NULL,
    BAM_AlignmentIsPrimary,
    BAM_AlignmentAlignmentPosition,
    BAM_AlignmentAlignmentLength,
    BAM_AlignmentIsReversedOrientation,
    BAM_AlignmentSoftClip,
    BAM_AlignmentTemplateLength,
    BAM_AlignmentShortCigar,
    BAM_AlignmentLongCigar,
    BAM_AlignmentHasMate,
    BAM_AlignmentMateAlignmentId,
    BAM_AlignmentMateAlignment,
    BAM_AlignmentMateReferenceSpec,
    BAM_AlignmentMateIsReversedOrientation,
    BAM_AlignmentIsFirst,

    /* Iterator */
    BAM_AlignmentIteratorNext
};

static void BAM_AlignmentInit(BAM_Alignment *const self, ctx_t ctx, bool const primary, bool const secondary,
                              struct BAM_Record *(*const provider_f)(struct NGS_ReadCollection *, ctx_t),
                              struct NGS_ReadCollection *const provider)
{
    self->provider = provider;
    self->provider_f = provider_f;
    self->primary = primary;
    self->secondary = secondary;
}

struct NGS_Alignment *BAM_AlignmentMake(ctx_t ctx, bool const primary, bool const secondary,
                                        struct BAM_Record *(*const provider_f)(struct NGS_ReadCollection *, ctx_t),
                                        struct NGS_ReadCollection *const provider,
                                        char const name[])
{
    FUNC_ENTRY(ctx, rcSRA, rcFile, rcReading);
    void *self = calloc(1, sizeof(BAM_Alignment));
    if (self) {
        NGS_Alignment *const super = &((BAM_Alignment *)self)->super;

        TRY(NGS_AlignmentInit(ctx, super, &vt, "BAM_Alignment", name)) {
            TRY(BAM_AlignmentInit(self, ctx, primary, secondary, provider_f, provider)) {
                return self;
            }
        }
        free(self);
    }
    else {
        SYSTEM_ABORT(xcNoMemory, "allocating BAM_Alignment ( '%s' )", name);
    }
    return NULL;
}
