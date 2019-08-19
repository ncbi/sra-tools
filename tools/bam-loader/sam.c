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
 *
 */

#include <klib/defs.h>
#include <klib/rc.h>
#include <klib/data-buffer.h>
#include <klib/log.h>
#include <sysalloc.h>

#include <math.h>
#include <ctype.h>
#if _DEBUGGING
#include <stdio.h>
#endif

#include "bam-alignment.h"
#include "sam.h"

static inline void bam_alignment_set_u16(uint8_t *const value, uint16_t newvalue) {
    value[0] = newvalue;
    value[1] = newvalue >> 8;
}

static inline void bam_alignment_set_i16(uint8_t *const value, int16_t newvalue) {
    union { uint16_t u; int16_t i; } y;
    y.i = newvalue;
    bam_alignment_set_u16(value, y.u);
}

static inline void bam_alignment_set_u32(uint8_t *const value, uint32_t newvalue) {
    bam_alignment_set_u16(value, newvalue);
    bam_alignment_set_u16(value + 2, newvalue >> 16);
}

static inline void bam_alignment_set_i32(uint8_t *const value, int32_t newvalue) {
    union { uint32_t u; int32_t i; } y;
    y.i = newvalue;
    bam_alignment_set_u32(value, y.u);
}

static inline void bam_alignment_set_f32(uint8_t *const value, double newvalue) {
    union { uint32_t u; float f; } y;
    y.f = (float)newvalue;
    bam_alignment_set_u32(value, y.u);
}

static inline void bam_alignment_inc_u32(uint8_t *const value) {
    bam_alignment_set_u32(value, bam_alignment_get_u32(value) + 1);
}

static inline void bam_alignment_inc_i32(uint8_t *const value) {
    bam_alignment_set_i32(value, bam_alignment_get_i32(value) + 1);
}

static bool number_parser_is_unsigned(number_parser const *const self)
{
    return self->sgn >= 0;
}

static bool number_parser_is_integer(number_parser const *const self)
{
    return self->exponent - self->shift == 0;
}

static bool number_parser_is_u16(number_parser const *const self)
{
    return number_parser_is_integer(self)
        && number_parser_is_unsigned(self)
        && self->mantissa <= UINT16_MAX;
}

static bool number_parser_is_u32(number_parser const *const self)
{
    return number_parser_is_integer(self)
        && number_parser_is_unsigned(self)
        && self->mantissa <= UINT32_MAX;
}

static bool number_parser_get_exact_u16(number_parser const *const self, uint16_t *rslt)
{
    if (number_parser_is_u16(self)) {
        *rslt = (uint16_t)self->mantissa;
        return true;
    }
    return false;
}

#if 0
static bool number_parser_get_exact_i16(number_parser const *const self, int16_t *rslt)
{
    if (number_parser_is_integer(self)) {
        if (self->sgn < 0 && self->mantissa <= (uint64_t)(-INT16_MIN)) {
            *rslt = -(int16_t)self->mantissa;
            return true;
        }
        else if (self->mantissa <= INT16_MAX) {
            *rslt = (int16_t)self->mantissa;
            return true;
        }
    }
    return false;
}
#endif

static bool number_parser_get_exact_u32(number_parser const *const self, uint32_t *rslt)
{
    if (number_parser_is_u32(self)) {
        *rslt = (uint32_t)self->mantissa;
        return true;
    }
    return false;
}

static bool number_parser_get_exact_i32(number_parser const *const self, int32_t *rslt)
{
    if (self->exponent - self->shift == 0) {
        if (self->sgn < 0 && self->mantissa <= (uint64_t)(-(int64_t)INT32_MIN)) {
            *rslt = -(uint32_t)self->mantissa;
            return true;
        }
        else if (self->mantissa <= INT32_MAX) {
            *rslt = (uint32_t)self->mantissa;
            return true;
        }
    }
    return false;
}

static double number_parser_get(number_parser const *const self)
{
    double scale = pow(10, self->exponent - self->shift);
    if (self->sgn < 0) scale = -scale;
    return scale * self->mantissa;
}

static bool number_parser_set_exact_u16(number_parser const *const self, uint8_t *const dst)
{
    uint16_t value;
    if (number_parser_get_exact_u16(self, &value)) {
        bam_alignment_set_u16(dst, value);
        return true;
    }
    return false;
}

#if 0
static bool number_parser_set_exact_i16(number_parser const *const self, uint8_t *const dst)
{
    int16_t value;
    if (number_parser_get_exact_i16(self, &value)) {
        bam_alignment_set_i16(dst, value);
        return true;
    }
    return false;
}
#endif

#if 0
static bool number_parser_set_exact_u32(number_parser const *const self, uint8_t *const dst)
{
    uint32_t value;
    if (number_parser_get_exact_u32(self, &value)) {
        bam_alignment_set_u32(dst, value);
        return true;
    }
    return false;
}
#endif

static bool number_parser_set_exact_i32(number_parser const *const self, uint8_t *const dst)
{
    int32_t value;
    if (number_parser_get_exact_i32(self, &value)) {
        bam_alignment_set_i32(dst, value);
        return true;
    }
    return false;
}

static bool parse_integer(uint64_t *const value, int ch)
{
    if (ch >= '1' && ch <= '9' && *value < (uint64_t)(UINT64_MAX / 10)) {
        *value = (*value * 10) + (ch - '0');
        return true;
    }
    if (ch == '0' && *value <= (uint64_t)(UINT64_MAX / 10)) {
        *value *= 10;
        return true;
    }
    return false;
}

static int64_t packCIGAR(uint64_t length, int const opchar)
{
    if (length <= UINT32_MAX >> 4)
        switch (opchar) {
        case 'M': return (length << 4) | 0;
        case 'I': return (length << 4) | 1;
        case 'D': return (length << 4) | 2;
        case 'N': return (length << 4) | 3;
        case 'S': return (length << 4) | 4;
        case 'H': return (length << 4) | 5;
        case 'P': return (length << 4) | 6;
        case '=': return (length << 4) | 7;
        case 'X': return (length << 4) | 8;
        }
    return -1;
}

/* SAM spec says any unrecognized-but-otherwise-valid character gets mapped to N */
static uint8_t translate_to_4nabin(int const ch)
{
    static uint8_t tr[] = {
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 0, 15, 15,
        15, 1, 14, 2, 13, 15, 15, 4, 11, 15, 15, 12, 15, 3, 15, 15,
        15, 15, 5, 6, 8, 15, 7, 9, 15, 10, 15, 15, 15, 15, 15, 15,
        15, 1, 14, 2, 13, 15, 15, 4, 11, 15, 15, 12, 15, 3, 15, 15,
        15, 15, 5, 6, 8, 15, 7, 9, 15, 10, 15, 15, 15, 15, 15, 15,
    };
    return (0 <= ch && ch < 128) ? tr[ch] : 15;
}

static bool parse_pos(SAM2BAM_Parser *const self, uint8_t *const fld, int ch, rc_t *rc)
{
    uint64_t value = bam_alignment_get_u32(fld);
    if (ch >= 0) {
        if (parse_integer(&value, ch)) {
            if (value <= ((uint64_t)INT32_MAX) + 1) {
                bam_alignment_set_u32(fld, value);
                return true;
            }
            *rc = RC(rcAlign, rcFile, rcReading, rcData, rcTooBig);
        }
        else
            *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        return false;
    }
    bam_alignment_set_i32(fld, ((int32_t)value) - 1);
    return true;
}

static bool parse_refname(SAM2BAM_Parser *const self, uint8_t *const fld, int ch, rc_t *rc)
{
    int32_t result;
    if (!self->lookup(self->lookup_ctx, ch, &result, rc))
        return false;
    if (ch < 0)
        bam_alignment_set_i32(fld, result);
    return true;
}

static bool parse_numeric(number_parser *const self, int const ch)
{
    switch (self->state) {
        case 0:
            memset(self, 0, sizeof(*self));
            self->state = 1;
            if (ch == '-') {
                self->ok = 0;
                self->sgn = -1;
                return true;
            }
            if (ch == '+') {
                self->ok = 0;
                self->sgn = 1;
                return true;
            }
            /* fallthrough */
        case 1:
            self->state = 2;
            if (parse_integer(&self->mantissa, ch)) {
                self->ok = 1;
                return true;
            }
            break;
        case 2:
            if (parse_integer(&self->mantissa, ch)) {
                self->ok = 1;
                return true;
            }
            if (ch == '.') {
                self->ok = 0;
                self->state = 3;
                return true;
            }
            if (ch == 'E' || ch == 'e') {
                self->ok = 0;
                self->state = 4;
                return true;
            }
            break;
        case 3:
            if (parse_integer(&self->mantissa, ch)) {
                self->ok = 1;
                self->shift += 1;
                return true;
            }
            if (ch == 'E' || ch == 'e') {
                self->ok = 0;
                self->state = 4;
                return true;
            }
            break;
        case 4:
            self->state = 5;
            if (ch == '-') {
                self->ok = 0;
                self->state = 6;
                return true;
            }
            if (ch == '+') {
                self->ok = 0;
                return true;
            }
            /* fallthrough */
        case 5:
        {
            uint64_t exponent = self->exponent;
            if (parse_integer(&exponent, ch)) {
                self->ok = 1;
                self->exponent = exponent;
                return true;
            }
        }
            break;
        case 6:
        {
            uint64_t exponent = -self->exponent;
            if (parse_integer(&exponent, ch)) {
                self->ok = 1;
                self->exponent = -exponent;
                return true;
            }
        }
            break;
        default:
            assert(!"reachable");
            abort();
    }
    return false;
}

#define RESULT_FIELD(NAME) self->rslt->cooked.NAME

#define PARSER_FUNCTION(NAME) parse_ ## NAME
#define DEF_PARSER_FUNCTION_INLINE(NAME)                                        \
static inline bool parse_ ## NAME ## _inline (  SAM2BAM_Parser *const self      \
                                              , int ch, rc_t *const rc)
#define DECL_PARSER_FUNCTION(NAME)                                              \
static bool parse_ ## NAME (  SAM2BAM_Parser *const self, int const ch          \
                            , rc_t *const rc, char const *data                  \
                            , char const *const endp)

#define DEF_PARSER_FUNCTION(NAME, NEXT)                                         \
DECL_PARSER_FUNCTION(NAME)                                                      \
{                                                                               \
    char const *const value = data;                                             \
    while ((void)(self->field_in_size = data < endp ? (data - value) + 1 : (endp - value)), parse_ ## NAME ## _inline (self, data < endp ? *data : ch, rc)) {    \
        if (data < endp) { ++data; } else { return true; }                      \
    }                                                                           \
    (void)PLOGERR(klogErr, (klogErr, *rc, "Parsing SAM " # NAME ": $(value)",   \
                                "value=%s", value));                            \
    self->parser = PARSER_FUNCTION(NEXT);                                       \
    return false;                                                               \
}

DECL_PARSER_FUNCTION(QNAME);
DECL_PARSER_FUNCTION(FLAG);
DECL_PARSER_FUNCTION(RNAME);
DECL_PARSER_FUNCTION(POS);
DECL_PARSER_FUNCTION(MAPQ);
DECL_PARSER_FUNCTION(CIGAR);
DECL_PARSER_FUNCTION(RNEXT);
DECL_PARSER_FUNCTION(PNEXT);
DECL_PARSER_FUNCTION(TLEN);
DECL_PARSER_FUNCTION(SEQ);
DECL_PARSER_FUNCTION(QUAL);
DECL_PARSER_FUNCTION(EXTRA);
DECL_PARSER_FUNCTION(EXTRA_A);
DECL_PARSER_FUNCTION(EXTRA_i);
DECL_PARSER_FUNCTION(EXTRA_f);
DECL_PARSER_FUNCTION(EXTRA_H);
DECL_PARSER_FUNCTION(EXTRA_Z);
DECL_PARSER_FUNCTION(EXTRA_B);
DECL_PARSER_FUNCTION(EXTRA_B_u);
DECL_PARSER_FUNCTION(EXTRA_B_i);
DECL_PARSER_FUNCTION(EXTRA_B_f);

static bool parse_refname(SAM2BAM_Parser *const self, uint8_t *const fld, int ch, rc_t *rc);
static bool parse_pos(SAM2BAM_Parser *const self, uint8_t *const fld, int ch, rc_t *rc);

static bool SAM2BAM_Parser_grow(SAM2BAM_Parser *const self, rc_t *rc);

static size_t check_size(SAM2BAM_Parser *const self, size_t const more, rc_t *const rc)
{
    size_t const current = self->rslt_size;
    size_t const need = current + more;
    if (need > self->max_size && !SAM2BAM_Parser_grow(self, rc))
        return 0;
    self->field_out_size += more;
    self->rslt_size = need;
    return current;
}

DEF_PARSER_FUNCTION_INLINE(QNAME)
{
    switch (self->state) {
    case -1:
        if (ch < 0)
            goto DONE;
        else {
            self->state = 1;
            if (!parse_QNAME(self, '*', rc, 0, 0))
                return false;
            return parse_QNAME(self, ch, rc, 0, 0);
        }
        break;
    case 0:
        if (ch == '*') {
            self->state = -1;
            return true;
        }
        self->state += 1;
        /* fallthrough */
    case 1:
        if (ch < 0 || (ch >= '!' && ch <= '~' && ch != '@')) {
            if (self->field_out_size < 255) {
                size_t current;
                if ((current = check_size(self, 1, rc)) > 0) {
                    if (ch >= 0) {
                        self->rslt->raw[current] = ch;
                        return true;
                    }
                    self->rslt->raw[current] = '\0';
                DONE:
                    RESULT_FIELD(read_name_len) = self->field_out_size;
                    self->parser = PARSER_FUNCTION(FLAG);
                    return true;
                }
                return false;
            }
            *rc = RC(rcAlign, rcFile, rcReading, rcData, rcTooBig);
            return false;
        }
        break;
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(QNAME, FLAG)

DEF_PARSER_FUNCTION_INLINE(FLAG)
{
    switch (self->state) {
    case 0:
        self->state += 1;
        self->numeric.state = 0;
        /* fallthrough */
    case 1:
        if (ch >= 0) {
            if (parse_numeric(&self->numeric, ch))
                return true;
            break;
        }
        if (self->numeric.ok && number_parser_set_exact_u16(&self->numeric, RESULT_FIELD(flags))) {
            self->parser = PARSER_FUNCTION(RNAME);
            return true;
        }
        break;
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(FLAG, RNAME)

DEF_PARSER_FUNCTION_INLINE(RNAME)
{
    switch (self->state) {
    case -1:
        if (ch < 0)
            goto DONE;
        *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        return false;
    case 0:
        if (ch == '*') {
            /* set to -1 */
            bam_alignment_set_i32(RESULT_FIELD(rID), -1);
            self->state = -1;
            return true;
        }
        self->state = 1;
        /* fallthrough */
    case 1:
        if (!parse_refname(self, RESULT_FIELD(rID), ch, rc))
            return false;
        if (ch < 0) {
        DONE:
            self->parser = PARSER_FUNCTION(POS);
        }
        return true;
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(RNAME, POS)

DEF_PARSER_FUNCTION_INLINE(POS)
{
    switch (self->state) {
    case 0:
        self->state = 1;
        /* fallthrough */
    case 1:
        if (!parse_pos(self, RESULT_FIELD(pos), ch, rc))
            return false;
        if (ch < 0)
            self->parser = PARSER_FUNCTION(MAPQ);
        return true;
    }
    abort();
}

DEF_PARSER_FUNCTION(POS, MAPQ)

DEF_PARSER_FUNCTION_INLINE(MAPQ)
{
    switch (self->state) {
    case 0:
        RESULT_FIELD(mapQual) = 0;
        self->state = 1;
        /* fallthrough */
    case 1:
        if (ch >= 0) {
            uint64_t value = RESULT_FIELD(mapQual);
            if (parse_integer(&value, ch)) {
                if (value <= UINT8_MAX) {
                    RESULT_FIELD(mapQual) = (uint8_t)value;
                    return true;
                }
            }
        }
        else {
            self->parser = PARSER_FUNCTION(CIGAR);
            return true;
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(MAPQ, CIGAR)

DEF_PARSER_FUNCTION_INLINE(CIGAR)
{
    /* parse \*|([0-9]+[MIDNSHP=X])+ */
    switch (self->state) {
    case -1:
        if (ch < 0)
            goto DONE;
        break;
    case 0:
        if (ch == '*') {
            self->state = -1;
            return true;
        }
        self->state = 1;
        /* fallthrough */
    case 1:
        if (ch < 0)
        DONE:
        {
            size_t const n_cigars = self->field_out_size / 4;
            assert(self->field_out_size % 4 == 0);
            if (n_cigars < UINT16_MAX) {
                bam_alignment_set_u16(RESULT_FIELD(n_cigars), (uint16_t)n_cigars);
                self->parser = PARSER_FUNCTION(RNEXT);
                return true;
            }
            break;
        }
        self->numeric.state = 0;
        self->state += 1;
        /* fallthrough */
    case 2:
        if (parse_numeric(&self->numeric, ch))
            return true;
        else {
            int64_t value;
            if (   self->numeric.ok
                && number_parser_is_integer(&self->numeric)
                && number_parser_is_unsigned(&self->numeric)
                && ((value = packCIGAR(self->numeric.mantissa, ch)) >= 0)
                )
            {
                size_t offset = check_size(self, 4, rc);
                if (offset > 0) {
                    bam_alignment_set_u32(&self->rslt->raw[offset], (uint32_t)value);
                    self->state = 1;
                    return true;
                }
                return false;
            }
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(CIGAR, RNEXT)

DEF_PARSER_FUNCTION_INLINE(RNEXT)
{
    switch (self->state) {
    case -1:
        if (ch < 0)
            goto DONE;
        *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
        return false;
    case 0:
        if (ch == '*') {
            /* set to -1 */
            bam_alignment_set_i32(RESULT_FIELD(mate_rID), -1);
            self->state = -1;
            return true;
        }
        if (ch == '=') {
            /* copy the value from RNAME */
            bam_alignment_set_i32(RESULT_FIELD(mate_rID), bam_alignment_get_i32(RESULT_FIELD(rID)));
            self->state = -1;
            return true;
        }
        self->state = 1;
        /* fallthrough */
    case 1:
        if (!parse_refname(self, RESULT_FIELD(mate_rID), ch, rc))
            return false;
        if (ch < 0) {
        DONE:
            self->parser = PARSER_FUNCTION(PNEXT);
        }
        return true;
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(RNEXT, PNEXT)

DEF_PARSER_FUNCTION_INLINE(PNEXT)
{
    switch (self->state) {
    case 0:
        self->state = 1;
        /* fallthrough */
    case 1:
        if (!parse_pos(self, RESULT_FIELD(mate_pos), ch, rc))
            return false;
        if (ch < 0)
            self->parser = PARSER_FUNCTION(TLEN);
        return true;
    }
    abort(); /**< unreachable **/
}

DEF_PARSER_FUNCTION(PNEXT, TLEN)

DEF_PARSER_FUNCTION_INLINE(TLEN)
{
    switch (self->state) {
    case 0:
        self->numeric.state = 0;
        self->state = 1;
        /* fallthrough */
    case 1:
        if (parse_numeric(&self->numeric, ch))
            return true;
        if (ch < 0 && self->numeric.ok && number_parser_set_exact_i32(&self->numeric, RESULT_FIELD(ins_size))) {
            self->parser = PARSER_FUNCTION(SEQ);
            return true;
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(TLEN, SEQ)

DEF_PARSER_FUNCTION_INLINE(SEQ)
{
    switch (self->state) {
    case -1:
        if (ch < 0) {
            assert(self->field_out_size == 0);
            goto DONE;
        }
        break;
    case 0:
        if (ch == '*') {
            self->state = -1;
            return true;
        }
        self->state = 1;
        /* fallthrough */
    case 1:
        if (ch >= 0) {
            if (   (ch >= 'A' && ch <= 'Z')
                || (ch >= 'a' && ch <= 'z')
                ||  ch == '=' || ch == '.')
            {
                int const basebin = translate_to_4nabin(ch);
                if (self->field_in_size % 2 == 0) {
                    size_t const current = check_size(self, 0, rc);
                    self->rslt->raw[current - 1] |= (uint8_t)(basebin);
                    return true;
                }
                else {
                    size_t const current = check_size(self, 1, rc);
                    if (current > 0) {
                        self->rslt->raw[current] = (uint8_t)(basebin << 4);
                        return true;
                    }
                    return false;
                }
            }
        }
        else if (self->field_in_size <= UINT32_MAX)
    DONE:
        {
            assert(self->field_out_size == 0 || (self->field_in_size + 1) >> 1 == self->field_out_size);
            bam_alignment_set_u32(RESULT_FIELD(read_len), self->field_out_size == 0 ? 0 : (uint32_t)self->field_in_size);
            self->parser = PARSER_FUNCTION(QUAL);
            return true;
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(SEQ, QUAL)

DEF_PARSER_FUNCTION_INLINE(QUAL)
{
    size_t const readlen = bam_alignment_get_u32(RESULT_FIELD(read_len));
    switch (self->state) {
    case -1:
        if (ch < 0) {
            size_t const current = check_size(self, readlen, rc);
            if (current == 0)
                return false;
            if (readlen > 0)
                memset(&self->rslt->raw[current], -1, readlen);
            goto DONE;
        }
        else {
            self->state = 1;
            if (!parse_QUAL(self, '*', rc, 0, 0))
                return false;
            return parse_QUAL(self, ch, rc, 0, 0);
        }
        break;
    case 0:
        if (ch == '*') {
            self->state = -1;
            return true;
        }
        self->state = 1;
        /* fallthrough */
    case 1:
        if (ch >= '!' && ch <= '~' && self->field_in_size <= readlen) {
            size_t const current = check_size(self, 1, rc);
            if (current > 0) {
                self->rslt->raw[current] = (uint32_t)(ch - 33);
                return true;
            }
            return false;
        }
        if (ch < 0) {
        DONE:
            if (self->field_out_size == readlen) {
                self->parser = PARSER_FUNCTION(EXTRA);
                return true;
            }
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(QUAL, EXTRA)

DEF_PARSER_FUNCTION_INLINE(EXTRA)
{
    size_t current;
    switch (self->state) {
    case 0:
        if (ch < 0) return true;
        if (((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')))
            goto COPY_IN;
        break;
    case 1:
        if (((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' || ch <= '9'))) {
        COPY_IN:
            if ((current = check_size(self, 1, rc)) > 0) {
                self->rslt->raw[current] = ch;
                ++self->state;
                return true;
            }
            return false;
        }
        break;
    case 2:
        if (ch == ':') {
            ++self->state;
            return true;
        }
        break;
    case 3:
        if ((current = check_size(self, 1, rc)) > 0) {
            self->state = 1;
            self->rslt->raw[current] = ch;
            switch (ch) {
            case 'A':
                self->parser = PARSER_FUNCTION(EXTRA_A);
                return true;
            case 'i':
                self->parser = PARSER_FUNCTION(EXTRA_i);
                return true;
            case 'f':
                self->parser = PARSER_FUNCTION(EXTRA_f);
                return true;
            case 'Z':
                self->parser = PARSER_FUNCTION(EXTRA_Z);
                return true;
            case 'H':
                self->parser = PARSER_FUNCTION(EXTRA_H);
                return true;
            case 'B':
                self->parser = PARSER_FUNCTION(EXTRA_B);
                return true;
            }
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DECL_PARSER_FUNCTION(EXTRA)
{
    if (data == NULL)
        return parse_EXTRA_inline(self, ch, rc);
    while (data < endp) {
        char const *const value = data;
        if (parse_EXTRA_inline(self, *data, rc)) {
            ++data;
            if (self->parser == PARSER_FUNCTION(EXTRA))
                continue;
            return self->parser(self, ch, rc, data, endp);
        }
        (void)PLOGERR(klogErr, (klogErr, *rc, "Parsing SAM EXTRA: $(value)", "value=%s", value));
        self->parser = PARSER_FUNCTION(EXTRA);
        return false;
    }
    return true;
}

DEF_PARSER_FUNCTION_INLINE(EXTRA_A)
{
    switch (self->state) {
    case 1:
        if (ch == ':') {
            ++self->state;
            return true;
        }
        break;
    case 2:
        if (ch >= '!' && ch <= '~') {
            size_t current;
            if ((current = check_size(self, 1, rc)) > 0) {
                self->rslt->raw[current] = ch;
                ++self->state;
                return true;
            }
            return false;
        }
        break;
    case 3:
        if (ch < 0) {
            self->parser = PARSER_FUNCTION(EXTRA);
            return true;
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(EXTRA_A, EXTRA)

DEF_PARSER_FUNCTION_INLINE(EXTRA_i)
{
    switch (self->state) {
    case 1:
        if (ch == ':') {
            ++self->state;
            self->numeric.state = 0;
            return true;
        }
        break;
    case 2:
        if (parse_numeric(&self->numeric, ch))
            return true;
        else if (ch < 0 && self->numeric.ok) {
            int32_t value;
            if (number_parser_get_exact_i32(&self->numeric, &value)) {
                size_t const cp = check_size(self, 4, rc);
                if (cp > 0) {
                    bam_alignment_set_i32(&self->rslt->raw[cp], value);
                    self->parser = PARSER_FUNCTION(EXTRA);
                    return true;
                }
                return false;
            }
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(EXTRA_i, EXTRA)

DEF_PARSER_FUNCTION_INLINE(EXTRA_f)
{
    switch (self->state) {
    case 1:
        if (ch == ':') {
            ++self->state;
            self->numeric.state = 0;
            return true;
        }
        break;
    case 2:
        if (parse_numeric(&self->numeric, ch))
            return true;
        else if (ch < 0 && self->numeric.ok) {
            size_t const cp = check_size(self, 4, rc);
            if (cp > 0) {
                bam_alignment_set_f32(&self->rslt->raw[cp], number_parser_get(&self->numeric));
                self->parser = PARSER_FUNCTION(EXTRA);
                return true;
            }
            return false;
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(EXTRA_f, EXTRA)

DEF_PARSER_FUNCTION_INLINE(EXTRA_H)
{
    switch (self->state) {
    case 1:
        if (ch == ':') {
            self->state = 2;
            return true;
        }
        break;
    case 2: /**< an even number of chars **/
        if (ch < 0) {
            size_t const cp = check_size(self, 1, rc);
            if (cp > 0) {
                self->rslt->raw[cp] = '\0';
                self->parser = PARSER_FUNCTION(EXTRA);
                return true;
            }
            return false;
        }
        /* fallthrough */
    case 3:
        if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')) {
            size_t const cp = check_size(self, 1, rc);
            if (cp > 0) {
                self->rslt->raw[cp] = ch;
                self->state = 3 - (self->state - 2); /* state toggles between 2 and 3 */
                return true;
            }
            return false;
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(EXTRA_H, EXTRA)

DEF_PARSER_FUNCTION_INLINE(EXTRA_Z)
{
    switch (self->state) {
    case 1:
        if (ch == ':') {
            ++self->state;
            return true;
        }
        break;
    case 2:
        if ((ch >= '!' && ch <= '~') || ch == ' ') {
            size_t const cp = check_size(self, 1, rc);
            if (cp > 0) {
                self->rslt->raw[cp] = ch;
                return true;
            }
            return false;
        }
        if (ch < 0) {
            size_t const cp = check_size(self, 1, rc);
            if (cp > 0) {
                self->rslt->raw[cp] = '\0';
                self->parser = PARSER_FUNCTION(EXTRA);
                return true;
            }
            return false;
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(EXTRA_Z, EXTRA)

DEF_PARSER_FUNCTION_INLINE(EXTRA_B)
{
    switch (self->state) {
    case 1:
        if (ch == ':') {
            ++self->state;
            return true;
        }
        break;
    case 2:
        {
            size_t const cp = check_size(self, 5, rc);
            if (cp == 0)
                return false;
            self->extraArrayCountPos = cp + 1;
            bam_alignment_set_i32(&self->rslt->raw[self->extraArrayCountPos], 0);
            self->state = 1;
            switch (ch) {
            case 'C':
            case 'I':
            case 'S':
                self->rslt->raw[cp] = 'I';
                self->parser = PARSER_FUNCTION(EXTRA_B_u);
                return true;
            case 'c':
            case 'i':
            case 's':
                self->rslt->raw[cp] = 'i';
                self->parser = PARSER_FUNCTION(EXTRA_B_i);
                return true;
            case 'f':
                self->rslt->raw[cp] = 'f';
                self->parser = PARSER_FUNCTION(EXTRA_B_f);
                return true;
            }
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DECL_PARSER_FUNCTION(EXTRA_B)
{
    if (data == NULL)
        return parse_EXTRA_B_inline(self, ch, rc);
    while (data < endp) {
        char const *const value = data;
        if (parse_EXTRA_B_inline(self, *data, rc)) {
            ++data;
            if (self->parser == PARSER_FUNCTION(EXTRA_B))
                continue;
            return self->parser(self, ch, rc, data, endp);
        }
        (void)PLOGERR(klogErr, (klogErr, *rc, "Parsing SAM EXTRA_B: $(value)", "value=%s", value));
        self->parser = PARSER_FUNCTION(EXTRA);
        return false;
    }
    return true;
}

DEF_PARSER_FUNCTION_INLINE(EXTRA_B_u)
{
    switch (self->state) {
    case 1:
        if (ch < 0)
            goto DONE;
        if (ch == ',') {
            self->state = 2;
            return true;
        }
        break;
    case 2:
        self->numeric.state = 0;
        if (parse_numeric(&self->numeric, ch)) {
            self->state = 3;
            return true;
        }
        break;
    case 3:
        if (parse_numeric(&self->numeric, ch))
            return true;
        if ((ch < 0 || ch == ',') && self->numeric.ok) {
            uint32_t value;
            if (number_parser_get_exact_u32(&self->numeric, &value)) {
                size_t const cp = check_size(self, 4, rc);
                if (cp > 0) {
                    bam_alignment_set_u32(&self->rslt->raw[cp], value);
                    bam_alignment_inc_i32(&self->rslt->raw[self->extraArrayCountPos]);

                    if (ch == ',')
                        self->state = 2;
                    else {
                    DONE:
                        self->parser = PARSER_FUNCTION(EXTRA);
                    }
                    return true;
                }
                return false;
            }
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(EXTRA_B_u, EXTRA)

DEF_PARSER_FUNCTION_INLINE(EXTRA_B_i)
{
    switch (self->state) {
    case 1:
        if (ch < 0)
            goto DONE;
        if (ch == ',') {
            self->state = 2;
            return true;
        }
        break;
    case 2:
        self->numeric.state = 0;
        if (parse_numeric(&self->numeric, ch)) {
            self->state = 3;
            return true;
        }
        break;
    case 3:
        if (parse_numeric(&self->numeric, ch))
            return true;
        if ((ch < 0 || ch == ',') && self->numeric.ok) {
            int32_t value;
            if (number_parser_get_exact_i32(&self->numeric, &value)) {
                size_t const cp = check_size(self, 4, rc);
                if (cp > 0) {
                    bam_alignment_set_i32(&self->rslt->raw[cp], value);
                    bam_alignment_inc_i32(&self->rslt->raw[self->extraArrayCountPos]);

                    if (ch == ',')
                        self->state = 2;
                    else {
                    DONE:
                        self->parser = PARSER_FUNCTION(EXTRA);
                    }
                    return true;
                }
                return false;
            }
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(EXTRA_B_i, EXTRA)

DEF_PARSER_FUNCTION_INLINE(EXTRA_B_f)
{
    switch (self->state) {
    case 1:
        if (ch < 0)
            goto DONE;
        if (ch == ',') {
            self->state = 2;
            return true;
        }
        break;
    case 2:
        self->numeric.state = 0;
        if (parse_numeric(&self->numeric, ch)) {
            self->state = 3;
            return true;
        }
        break;
    case 3:
        if (parse_numeric(&self->numeric, ch))
            return true;
        if ((ch < 0 || ch == ',') && self->numeric.ok) {
            size_t const cp = check_size(self, 4, rc);
            if (cp > 0) {
                bam_alignment_set_f32(&self->rslt->raw[cp], number_parser_get(&self->numeric));
                bam_alignment_inc_i32(&self->rslt->raw[self->extraArrayCountPos]);
                
                if (ch == ',')
                    self->state = 2;
                else {
                DONE:
                    self->parser = PARSER_FUNCTION(EXTRA);
                }
                return true;
            }
            return false;
        }
    }
    *rc = RC(rcAlign, rcFile, rcReading, rcData, rcInvalid);
    return false;
}

DEF_PARSER_FUNCTION(EXTRA_B_f, EXTRA)

static bool SAM2BAM_Parser_grow(SAM2BAM_Parser *const self, rc_t *rc)
{
    size_t const size = self->max_size * 2;
    void *tmp;
    if ((void *)self->rslt == self->static_buffer) {
        tmp = malloc(size);
        if (tmp)
            memmove(tmp, self->rslt, self->max_size);
    }
    else
        tmp = realloc(self->rslt, size);

    if (tmp == NULL) {
        *rc = RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
        return false;
    }
    self->rslt = tmp;
    self->max_size = size;
    return true;
}

#if 0
static void DebugPrintSAM(  char const *const data
                          , unsigned const fields
                          , unsigned const *const offsets
                          )
{
#if _DEBUGGING
    static char const *fieldNames[] = {
        "QNAME", "FLAG", "RNAME", "POS", "MAPQ", "CIGAR",
        "RNEXT", "PNEXT", "TLEN", "SEQ", "QUAL" };
    unsigned i;
    for (i = 0; i < fields; ++i) {
        char const *const value = data + ((i == 0) ? 0 : offsets[i - 1]);
        char const *const endp = data + offsets[i] - 1;
        size_t const length = endp - value;
        if (i < 11)
            fprintf(stderr, "%s:\t%.*s\n", fieldNames[i], (int)length, value);
        else
            fprintf(stderr, "EXTRA_%u:\t%.*s\n", i - 10, (int)length, value);
    }
#endif
}
#endif

static void LogSAM(  int level
                   , char const *const data
                   , unsigned const fields
                   , unsigned const *const offsets
                   )
{
    static char const *fieldNames[] = {
        "QNAME", "FLAG", "RNAME", "POS", "MAPQ", "CIGAR",
        "RNEXT", "PNEXT", "TLEN", "SEQ", "QUAL" };
    unsigned i;
    for (i = 0; i < fields; ++i) {
        char const *const value = data + ((i == 0) ? 0 : offsets[i - 1]);
        char const *const endp = data + offsets[i] - 1;
        size_t const length = endp - value;
        if (i < 11)
            (void)PLOGMSG(level, (level, "$(field): $(value)", "field=%s,value=%.*s", fieldNames[i], (int)length, value));
        else
            (void)PLOGMSG(level, (level, "$(field): $(value)", "field=EXTRA_%u,value=%.*s", i - 10, (int)length, value));
    }
}

static SAM2BAM_Parser *init(  SAM2BAM_Parser *const self
                            , void *const static_buffer
                            , size_t const buffer_size
                            , RefNameLookupFunction lookup
                            , RefNameLookupContext *lookup_ctx
                            )
{
    memset(self, 0, sizeof(*self));
    if (static_buffer && buffer_size) {
        self->rslt = self->static_buffer = static_buffer;
        self->max_size = buffer_size;
    }
    else {
        self->rslt = malloc(self->max_size = 64 * 1024);
        if (self->rslt == NULL) return NULL;
    }
    self->lookup = lookup;
    self->lookup_ctx = lookup_ctx;
    self->parser = parse_QNAME;
    self->rslt_size = offsetof(bam_alignment, cooked.read_name[0]);
    memset(self->rslt, 0, self->rslt_size);
    return self;
}

static SAM2BAM_Parser *parse(  char const *const data
                             , unsigned const fields
                             , unsigned const *const offsets
                             , void *const static_buffer
                             , size_t const buffer_size
                             , RefNameLookupFunction lookup
                             , RefNameLookupContext *lookup_ctx
                             , rc_t *rc
                             )
{
    SAM2BAM_Parser *const self = malloc(sizeof(*self));
    
    if (self == NULL)
        goto OUT_OF_MEMORY;

    if (init(self, static_buffer, buffer_size, lookup, lookup_ctx) == NULL)
        goto OUT_OF_MEMORY;
    
    // LogSAM(klogErr, data, fields, offsets);
    self->record_chars = offsets[fields - 1] - 1;
    *rc = 0;
    for (self->field = 0; self->field != fields; ) {
        unsigned const i = self->field;
        char const *const field = data + ((i == 0) ? 0 : offsets[i - 1]);
        char const *const endp = data + offsets[i] - 1;
        rc_t lrc = 0;
        
        self->field += 1;
        self->state = 0;
        self->field_out_size = 0;
        self->field_in_size = 0;
        self->numeric.state = 0;
        if (self->parser(self, -1, &lrc, field, endp))
            continue;
        if (*rc == 0)
            *rc = lrc;
    }
    if (self->parser == PARSER_FUNCTION(EXTRA) && *rc == 0)
        return self;
    
    if (*rc == 0) {
        *rc = RC(rcAlign, rcFile, rcReading, rcData, rcTooShort);
        (void)LOGERR(klogErr, *rc, "Parsing SAM:");
        LogSAM(klogInfo, data, fields, offsets);
    }
    if (self->rslt != static_buffer)
        free(self->rslt);
    free(self);
    return NULL;

OUT_OF_MEMORY:
    free(self);
    *rc = RC(rcAlign, rcFile, rcReading, rcMemory, rcExhausted);
    return NULL;
}

SAM2BAM_Parser *SAM2BAM_Parser_parse(  char const *const data
                                     , unsigned const fields
                                     , unsigned const *const offsets
                                     , void *const static_buffer
                                     , size_t const buffer_size
                                     , RefNameLookupFunction lookup
                                     , RefNameLookupContext *lookup_ctx
                                     , rc_t *rc
                                     )
{
    return parse(data, fields, offsets, static_buffer, buffer_size, lookup, lookup_ctx, rc);
}

SAM2BAM_Parser *SAM2BAM_Parser_initialize(  SAM2BAM_Parser *const self
                                          , void *const static_buffer
                                          , size_t const buffer_size
                                          , RefNameLookupFunction lookup
                                          , RefNameLookupContext *lookup_ctx
                                          )
{
    return init(self, static_buffer, buffer_size, lookup, lookup_ctx);
}
