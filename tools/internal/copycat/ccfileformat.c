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

#define _POSIX_C_SOURCE 200809L
#include <string.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <klib/rc.h>
#include <klib/debug.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <kfs/file.h>
#include <zlib.h>
#include <kff/fileformat.h>
#include <kff/ffext.h>
#include <kff/ffmagic.h>
#include <krypto/wgaencrypt.h>
#include <kfg/config.h>
#include <atomic32.h>
#include <stddef.h>
#include "copycat-priv.h"
#include "debug.h"

#define MIN_FASTA_SEQUENCE_LENGTH 25

static const uint8_t defline_allowed_chars[256] =
{
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0, /*   */  0, /* ! */  0, /* " */  0, /* # */  0, /* $ */  0, /* % */  0, /* & */  0, /* ' */  0, /* ( */  0, /* ) */  0, /* * */  0, /* + */  1, /* , */  1, /* - */  1, /* . */  0, /* / */
  1, /* 0 */  1, /* 1 */  1, /* 2 */  1, /* 3 */  1, /* 4 */  1, /* 5 */  1, /* 6 */  1, /* 7 */  1, /* 8 */  1, /* 9 */  1, /* : */  0, /* ; */  0, /* < */  0, /* = */  0, /* > */  0, /* ? */
  0, /* @ */  1, /* A */  1, /* B */  1, /* C */  1, /* D */  1, /* E */  1, /* F */  1, /* G */  1, /* H */  1, /* I */  1, /* J */  1, /* K */  1, /* L */  1, /* M */  1, /* N */  1, /* O */
  1, /* P */  1, /* Q */  1, /* R */  1, /* S */  1, /* T */  1, /* U */  1, /* V */  1, /* W */  1, /* X */  1, /* Y */  1, /* Z */  0, /* [ */  0, /* \ */  0, /* ] */  0, /* ^ */  1, /* _ */
  0, /* ` */  1, /* a */  1, /* b */  1, /* c */  1, /* d */  1, /* e */  1, /* f */  1, /* g */  1, /* h */  1, /* i */  1, /* j */  1, /* k */  1, /* l */  1, /* m */  1, /* n */  1, /* o */
  1, /* p */  1, /* q */  1, /* r */  1, /* s */  1, /* t */  1, /* u */  1, /* v */  1, /* w */  1, /* x */  1, /* y */  1, /* z */  0, /* { */  0, /* | */  0, /* } */  0, /* ~ */  0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
};

static const uint8_t fastq_sequence_chars[256] =
{
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0, /*   */  0, /* ! */  0, /* " */  0, /* # */  0, /* $ */  0, /* % */  0, /* & */  0, /* ' */  0, /* ( */  0, /* ) */  0, /* * */  0, /* + */  0, /* , */  0, /* - */  0, /* . */  0, /* / */
  0, /* 0 */  0, /* 1 */  0, /* 2 */  0, /* 3 */  0, /* 4 */  0, /* 5 */  0, /* 6 */  0, /* 7 */  0, /* 8 */  0, /* 9 */  0, /* : */  0, /* ; */  0, /* < */  0, /* = */  0, /* > */  0, /* ? */
  0, /* @ */  1, /* A */  0, /* B */  1, /* C */  0, /* D */  0, /* E */  0, /* F */  1, /* G */  0, /* H */  0, /* I */  0, /* J */  0, /* K */  0, /* L */  0, /* M */  1, /* N */  0, /* O */
  0, /* P */  0, /* Q */  0, /* R */  0, /* S */  1, /* T */  0, /* U */  0, /* V */  0, /* W */  0, /* X */  0, /* Y */  0, /* Z */  0, /* [ */  0, /* \ */  0, /* ] */  0, /* ^ */  0, /* _ */
  0, /* ` */  1, /* a */  0, /* b */  1, /* c */  0, /* d */  0, /* e */  0, /* f */  1, /* g */  0, /* h */  0, /* i */  0, /* j */  0, /* k */  0, /* l */  0, /* m */  1, /* n */  0, /* o */
  0, /* p */  0, /* q */  0, /* r */  0, /* s */  1, /* t */  0, /* u */  0, /* v */  0, /* w */  0, /* x */  0, /* y */  0, /* z */  0, /* { */  0, /* | */  0, /* } */  0, /* ~ */  0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
};

static const uint8_t fasta_sequence_chars[256] =
{
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0, /*   */  0, /* ! */  0, /* " */  0, /* # */  0, /* $ */  0, /* % */  0, /* & */  0, /* ' */  0, /* ( */  0, /* ) */  1, /* * */  0, /* + */  0, /* , */  1, /* - */  0, /* . */  0, /* / */
  0, /* 0 */  0, /* 1 */  0, /* 2 */  0, /* 3 */  0, /* 4 */  0, /* 5 */  0, /* 6 */  0, /* 7 */  0, /* 8 */  0, /* 9 */  0, /* : */  0, /* ; */  0, /* < */  0, /* = */  0, /* > */  0, /* ? */
  0, /* @ */  1, /* A */  1, /* B */  1, /* C */  1, /* D */  1, /* E */  1, /* F */  1, /* G */  1, /* H */  1, /* I */  0, /* J */  1, /* K */  1, /* L */  1, /* M */  1, /* N */  0, /* O */
  1, /* P */  1, /* Q */  1, /* R */  1, /* S */  1, /* T */  1, /* U */  1, /* V */  1, /* W */  1, /* X */  1, /* Y */  0, /* Z */  0, /* [ */  0, /* \ */  0, /* ] */  0, /* ^ */  0, /* _ */
  0, /* ` */  1, /* a */  1, /* b */  1, /* c */  1, /* d */  1, /* e */  1, /* f */  1, /* g */  1, /* h */  1, /* i */  0, /* j */  1, /* k */  1, /* l */  1, /* m */  1, /* n */  0, /* o */
  1, /* p */  1, /* q */  1, /* r */  1, /* s */  1, /* t */  1, /* u */  1, /* v */  1, /* w */  1, /* x */  1, /* y */  0, /* z */  0, /* { */  0, /* | */  0, /* } */  0, /* ~ */  0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
  0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0
};

/** Note: buffer must be at least 8 bytes
 */
bool CCFileFormatIsNCBIEncrypted ( void  * buffer )
{
    static const char file_sig[] = "NCBInenc";

    return (memcmp (buffer, file_sig, 8) == 0);
}

/** Note: buffer must be at least 8 bytes
 */
bool CCFileFormatIsKar ( void  * buffer )
{
    static const char file_sig[] = "NCBI.sra";

    return (memcmp (buffer, file_sig, 8) == 0);
}

/** Note: buffer must be at least 4 bytes
 */
bool CCFileFormatIsSff ( void  * buffer )
{
    static const char file_sig[] = ".sff";

    return (memcmp (buffer, file_sig, 4) == 0);
}

/* Validates BGZF structure and decompresses the first block into caller-supplied buffer.
   Returns number of decompressed bytes on success, 0 on failure. */
size_t CCFileFormatDecompressFirstBGZFBlock ( const void * buffer, size_t buffer_size, uint8_t * out, size_t out_size )
{
    const uint8_t *data = (const uint8_t *)buffer;
    z_stream zs;
    int zr;

    if (buffer_size < 18)
        return 0;
    if (data[0] != 0x1f || data[1] != 0x8b)
        return 0;
    if (data[2] != 0x08)
        return 0;
    if ((data[3] & 0x04) == 0)
        return 0;

    uint16_t xlen = ((uint16_t)data[10] | ((uint16_t)data[11] << 8));
    if (xlen < 6)
        return 0;
    if (buffer_size < ((size_t)12 + xlen))
        return 0;
    if (data[12] != 'B' || data[13] != 'C' || data[14] != 0x02 || data[15] != 0x00)
        return 0;
    if (((uint16_t)data[16] | ((uint16_t)data[17] << 8)) == 0)
        return 0;

    memset(&zs, 0, sizeof(zs));
    zr = inflateInit2(&zs, 15 + 16);
    if (zr != Z_OK)
        return 0;

    zs.next_in   = (Bytef *)data;
    zs.avail_in  = (uInt)buffer_size;
    zs.next_out  = (Bytef *)out;
    zs.avail_out = (uInt)out_size;

    zr = inflate(&zs, Z_SYNC_FLUSH);
    inflateEnd(&zs);

    if (zr != Z_STREAM_END && zr != Z_OK)
        return 0;

    DEBUG_STATUS(("%s: Valid BGZF header detected, first block decompressed\n", __func__));

    return (size_t)zs.total_out;
}

void CCFileFormatExtractDefline(const char * line, size_t line_len, CCFileNode *node)
{
    char buff[1024];
    char fields[3][1024];
    size_t len, field_count=0;
    char* str;
    char* tmp = buff;
    char* rest = buff;
    bool done=false;

    node->defline_name[0] = '\0';
    node->defline_pair = 0;

    if(sizeof(buff) <= line_len)
    {
        strncpy(buff, line, sizeof(buff)-1);
        buff[sizeof(buff)-1]='\0';
    }
    else
    {
        strncpy(buff, line, line_len);
        buff[line_len]='\0';
    }
    /* Splitting line into fields, first 3 is enough, rest could be ignored */
    while (field_count<3 && (str = strtok_r(tmp, " \t", &rest)))
    {
        tmp = NULL;
        strncpy(fields[field_count],str,sizeof(fields[0])-1);
        fields[field_count][sizeof(fields[0])-1]='\0';
        /* Truncating tail after semicolon */
        if((str = strchr(fields[field_count],';')) != NULL) str[0]='\0';
        DEBUG_STATUS(("%s: field%d='%s'\n",__func__,field_count+1,fields[field_count]));
        field_count++;
    }
    /* Seaching field with '/' - it should be defline_name and pair */
    for(int i=0; i<field_count; i++)
    {
        len = strlen(fields[i]);
        if(len>2 && fields[i][len-2]=='/' && fields[i][len-1]>='0' && fields[i][len-1]<='9')
        {
            /* Truncating tail with pair # */
            fields[i][len-2]='\0';
            /* Truncating tail after '#' */
            if((str = strchr(fields[i],'#')) != NULL) str[0]='\0';
            strncpy(node->defline_name, fields[i], sizeof(node->defline_name));
            node->defline_name[sizeof(node->defline_name)-1] = '\0';
            node->defline_pair = fields[i][len-1]-'0';
            done=true;
        }
    }
    /* Seaching field with digit at first pos and colon on second - it should be defline_pair, defline name must be in previous field */
    if(!done) for(int i=1; i<field_count; i++)
    {
        len = strlen(fields[i]);
        if(len>1 && fields[i][1]==':' && fields[i][0]>='0' && fields[i][0]<='9')
        {
            /* Truncating defline name tail after '#' */
            if((str = strchr(fields[i-1],'#')) != NULL) str[0]='\0';
            strncpy(node->defline_name, fields[i-1], sizeof(node->defline_name));
            node->defline_name[sizeof(node->defline_name)-1] = '\0';
            node->defline_pair = fields[i][0]-'0';
            done=true;
        }
    }
    /* By default taking first field as the defline_name */
    if(!done && fields[0][0] != '\0')
    {
        /* Truncating defline name tail after '#' */
        if((str = strchr(fields[0],'#')) != NULL) str[0]='\0';
        strncpy(node->defline_name, fields[0], sizeof(node->defline_name));
        node->defline_name[sizeof(node->defline_name)-1] = '\0';
        done=true;
    }
    /* If defline was extracted - clean all unwanted chars from it to prevent code injection */
    if(done)
    {
        int pos=0;
        for(int i=0; i<sizeof(node->defline_name); i++)
        {
            if(defline_allowed_chars[(uint8_t)node->defline_name[i]]==1)
                node->defline_name[pos++] = node->defline_name[i];
            else if(node->defline_name[i]=='\0')
            {
                node->defline_name[pos]='\0';
                break;
            }
        }
        DEBUG_STATUS(("%s: defline_name='%s', defline_pair=%d\n",__func__,node->defline_name,node->defline_pair));
    }
}

/* Scans buffer for FASTQ format compliance.
   Supports both classic 4-line-per-record FASTQ and multi-line FASTQ
   where the sequence and quality blocks may be wrapped across several lines. */
bool CCFileFormatIsFastq (void * buffer, size_t buffer_size, CCFileNode *node)
{
    const char *data = (const char *)buffer;
    const char * newline;
    const char * line;
    const char * limit;
    const char * seq_id;
    size_t len = buffer_size;
    size_t seq_len = 0;
    size_t quality_values_len = 0;
    size_t seq_id_len;
    size_t seqCount = 0;
    size_t firstline_len = 0;

    CCFastqFormatLineType curLineType = ccfqfltIdentifier;

    limit = data + len;
    for (newline = line = data; line < limit && seqCount < 100; line = newline)
    {
        if (*line != '\n')
            for (newline = line+1; newline < limit && *newline != '\n' && *newline != '\r'; newline++);
        else
            newline = line;
        if (line == data) firstline_len = (newline - line);
        DEBUG_STATUS(("%s: line at pos %d, new line at pos %d\n", __func__, line-data, newline-data));
        switch (curLineType)
        {
            case ccfqfltIdentifier:
                if (*line != '@')
                {
                    DEBUG_STATUS(("%s: no identifier\n", __func__));
                    return false;
                }
                if (newline == limit)
                {
                    DEBUG_STATUS(("%s: identifier line truncated at buffer boundary, ignoring incomplete record\n", __func__));
                    break;
                }
                if ((newline - line) < 2)
                {
                    DEBUG_STATUS(("%s: identifier is empty\n", __func__));
                    return false;
                }
                seq_id = line + 1;
                seq_id_len = newline - line - 1;
                seq_len = 0;
                curLineType = ccfqfltSeqLetters;
                break;

            case ccfqfltSeqLetters:
                /* Multi-line FASTQ: sequence may span several lines.
                 * A '+' character at the start of a line signals the end of the
                 * sequence block and the beginning of the quality header line.
                 * Any other line is validated as sequence data and accumulated. */
                if (*line == '+')
                {
                    /* Optionally the '+' line may repeat the sequence identifier */
                    if ((line + 1) != newline)
                    {
                        if (limit > (line + seq_id_len) &&
                            ((newline - line - 1) != seq_id_len ||
                             strncmp(seq_id, line+1, seq_id_len) != 0))
                        {
                            DEBUG_STATUS(("%s: sequence identifier after '+' doesn't match\n", __func__));
                            return false;
                        }
                    }
                    DEBUG_STATUS(("%s: '+' terminator found, extracted sequence length is %d\n", __func__, seq_len));
                    quality_values_len = 0; /* reset accumulated quality length */
                    curLineType = ccfqfltQualityValues;
                }
                else
                {
                    /* Validate sequence characters and accumulate length */
                    for (const char *c = line; c < newline; c++)
                    {
                        if (fastq_sequence_chars[(uint8_t)*c] == 0)
                        {
                            DEBUG_STATUS(("%s: wrong letter '0x%x' at pos %d\n", __func__, *c, c-(char*)data));
                            return false;
                        }
                    }
                    seq_len += (newline - line);
                    DEBUG_STATUS(("%s: sequence length so far=%d\n", __func__, seq_len));
                    /* Stay in ccfqfltSeqLetters to read more sequence lines */
                }
                break;

            case ccfqfltPlus:
                if (*line != '+')
                {
                    DEBUG_STATUS(("%s: no '+' char\n", __func__));
                    return false;
                }
                if ((line + 1) != newline)
                {
                    if (limit > (line + seq_id_len) &&
                        ((newline - line - 1) != seq_id_len ||
                         strncmp(seq_id, line+1, seq_id_len) != 0))
                    {
                        DEBUG_STATUS(("%s: sequence identifier after '+' doesn't match\n", __func__));
                        return false;
                    }
                }
                quality_values_len = 0;
                curLineType = ccfqfltQualityValues;
                break;

            case ccfqfltQualityValues:
                /* Handling case when there are no quality values but single 0x85 instead */
                if ((newline - line) == 1 && (uint8_t)*line == 0x85)
                {
                    curLineType = ccfqfltIdentifier;
                    seqCount++;
                }
                else
                {
                    /* Multi-line FASTQ: quality may span several lines.
                     * Validate characters on this line and add to the running total.
                     * Only advance to the next record once the accumulated quality
                     * length equals the accumulated sequence length. */
                    size_t line_quality_len = 0;
                    for (const char *c = line; c < newline; c++, line_quality_len++)
                    {
                        if (*c < 0x21 || *c > 0x7e)
                        {
                            DEBUG_STATUS(("%s: wrong quality value '0x%02x'\n", __func__, (uint8_t)*c));
                            return false;
                        }
                    }
                    quality_values_len += line_quality_len;

                    if (quality_values_len > seq_len)
                    {
                        /* Quality block is longer than the sequence - malformed record */
                        DEBUG_STATUS(("%s: quality values longer than sequence: %d > %d\n",
                                      __func__, quality_values_len, seq_len));
                        return false;
                    }
                    else if (quality_values_len == seq_len)
                    {
                        /* Accumulated exactly the right number of quality values */
                        DEBUG_STATUS(("%s: extracted quality values length is %d\n", __func__, quality_values_len));
                        curLineType = ccfqfltIdentifier;
                        seqCount++;
                    }
                    else if (newline >= limit)
                    {
                        /* The quality block is incomplete only because we ran out of buffer - not because the file is malformed.
                         * Accept the record and stop scanning. */
                        DEBUG_STATUS(("%s: quality values continue past end of buffer, assuming rest is OK\n", __func__));
                        seqCount++;
                    }
                    /* else: quality_values_len < seq_len and more buffer remains - stay in ccfqfltQualityValues to read the next quality line */
                }
                DEBUG_STATUS(("%s: sequence #%d extracted\n", __func__, seqCount));
                break;

            default:
                return false;
        }
        if (limit > newline)
            len = limit - newline;
        else
            break;
        if (curLineType == ccfqfltIdentifier)
            for (; len > 0 && isspace(*newline); len--, newline++);
        else
        {
            if (*newline == '\r')
            {
                newline++;
                len--;
            }
            if (len > 0 && *newline == '\n')
                newline++;
            else
                break;
        }
    }
    if (seqCount > 0 && firstline_len > 1 && (curLineType == ccfqfltIdentifier || newline == limit))
    {
        CCFileFormatExtractDefline(data+1, firstline_len-1, node);
        DEBUG_STATUS(("%s: Valid FASTQ content detected\n", __func__));
        return true;
    }
    return false;
}

/* Scans buffer for FASTA format compliance */
bool CCFileFormatIsFasta (void * buffer, size_t buffer_size)
{
    const char *data = (const char *)buffer;
    const char *limit = data + buffer_size;
    const char *line_start;
    const char *line_end;
    size_t line_len;
    bool expecting_header = true;         /* state: expecting header line starting with '>' */
    bool found_complete_sequence = false; /* found at least one header + sequence start */
    size_t seq_line_len = 0;              /* accumulated sequence length for current record */


    DEBUG_STATUS(("%s: buffer size = %zu\n", __func__, buffer_size));

    if (buffer_size < (3 + MIN_FASTA_SEQUENCE_LENGTH))  /* Minimum FASTA content is 3 characters plus MIN_FASTA_SEQUENCE_LENGTH like: ">A\n<seQ>" */
        return false;

    line_start = data;

    /* Process buffer line by line */
    while (line_start < limit)
    {
        /* Find end of current line */
        line_end = line_start;
        while (line_end < limit && *line_end != '\n' && *line_end != '\r')
            line_end++;

        bool line_truncated = (line_end == limit);

        line_len = line_end - line_start;

        /* Skip empty lines */
        if (line_len == 0)
        {
            /* Skip line ending characters */
            if (line_end < limit && *line_end == '\r') line_end++;
            if (line_end < limit && *line_end == '\n') line_end++;
            line_start = line_end;
            continue;
        }

        if (expecting_header)
        {
            /* Header line must start with '>' */
            if (*line_start != '>')
            {
                DEBUG_STATUS(("%s: expected header line starting with '>'\n", __func__));
                return false;
            }

            /* Header line truncated at buffer boundary — the record is incomplete.
               If we already confirmed at least one valid record, that's sufficient. */
            if (line_truncated)
            {
                DEBUG_STATUS(("%s: header line truncated at buffer boundary, ignoring incomplete record\n", __func__));
                break;
            }

            /* Header must have a name (more than just '>') */
            if (line_len < 2 || line_start[1] == ' ')
            {
                DEBUG_STATUS(("%s: header '>' must be followed by a non-space character\n", __func__));
                return false;
            }

            /* Check that rest of header contains only printable characters */
            for (size_t i = 1; i < line_len; i++)
            {
                unsigned char ch = (unsigned char)line_start[i];
                if (ch < 0x20 || ch == 0x7F)
                {
                    DEBUG_STATUS(("%s: Non-printable control character in header (0x%02x)\n",
                                __func__, ch));
                    return false;
                }
            }

            DEBUG_STATUS(("%s: header line found, len=%d\n", __func__, (int)line_len-1));
            expecting_header = false;
        }
        else
        {
            /* Sequence line: either continues current sequence or starts new one */
            if (*line_start == '>')
            {
                DEBUG_STATUS(("%s: extracted FASTA sequence line, length=%zu\n", __func__, seq_line_len));
                if (!found_complete_sequence && seq_line_len >= MIN_FASTA_SEQUENCE_LENGTH)
                    found_complete_sequence = true;
                expecting_header = true;
                seq_line_len = 0;
                continue;
            }

            for (size_t i = 0; i < line_len; i++)
            {
                if (fasta_sequence_chars[(uint8_t)line_start[i]] == 0)
                {
                    DEBUG_STATUS(("%s: invalid character in sequence: 0x%02x at pos: %zu\n",
                                  __func__, (unsigned char)line_start[i], i+(line_start-data)));
                    return false;
                }
            }

            seq_line_len += line_len;
        }

        /* Move to next line */
        if (line_end < limit && *line_end == '\r') line_end++;
        if (line_end < limit && *line_end == '\n') line_end++;

        line_start = line_end;
    }

    /* Must have found at least one complete sequence (header + sequence start) */
    if (!found_complete_sequence && seq_line_len >= MIN_FASTA_SEQUENCE_LENGTH)
        found_complete_sequence = true;

    if (!found_complete_sequence)
    {
        DEBUG_STATUS(("%s: no complete FASTA sequence found in buffer\n", __func__));
        return false;
    }

    DEBUG_STATUS(("%s: Valid FASTA content detected\n", __func__));
    return true;
}

CCFileFormat *filefmt;

/* ======================================================================
 * Process does up the copy portion of the copycat tool's functionality
 * and sets up ProcessOne that does the Catalog portion.
 */
struct CCFileFormat
{
    KFileFormat * magic;
    KFileFormat * ext;
    atomic32_t	  refcount;
};

static const char magictable [] =
{
    "Binary Alignment Map Index\tBAMIndex\n"
    "bzip2 compressed data\tBzip\n"
    "Compressed Reference-oriented Alignment Map\tCompressedReferenceOrientedAlignment\n"
    "XML document\tExtensibleMarkupLanguage\n"
    "XML 1.0 document\tExtensibleMarkupLanguage\n"
    "gzip compressed data\tGnuZip\n"
    "Hierarchical Data Format (version 5) data\tHD5\n"
    "NCBI kar sequence read archive\tSequenceReadArchive\n"
    "Generic Format for Sequence Data (SRF)\tSequenceReadFormat\n"
    "Standard Flowgram Format (SFF)\tStandardFlowgramFormat\n"
    "GNU tar archive\tTapeArchive\n"
    "POSIX tar archive\tTapeArchive\n"
    "POSIX tar archive (GNU)\tTapeArchive\n"
    "tar archive\tTapeArchive\n"
    "Zip archive data\tWinZip\n"
};

static const char exttable [] =
{
    "Unknown\tUnknown\n"
    "bam\tBinaryAlignmentMap\n"
    "bai\tBAMIndex\n"
    "bz2\tBzip\n"
    "cram\tCompressedReferenceOrientedAlignment\n"
    "crai\tCRAMIndex\n"
    "csi\tCoordinateSortedIndex\n"
    "fa\tFASTA\n"
    "fai\tFASTAIndex\n"
    "fasta\tFASTA\n"
    "fastq\tFASTQ\n"
    "fq\tFASTQ\n"
    "gz\tGnuZip\n"
    "h5\tHD5\n"
    "pbi\tPacBioBAMIndex\n"
    "sam\tSequenceAlignmentMap\n"
    "sff\tStandardFlowgramFormat\n"
    "sra\tSequenceReadArchive\n"
    "srf\tSequenceReadFormat\n"
    "tar\tTapeArchive\n"
    "tgz\tGnuZip\n"
    "xml\tExtensibleMarkupLanguage\n"
};

static const char formattable [] =
{
    "BAMIndex\tRead\n"
    "BinaryAlignmentMap\tRead\n"
    "Bzip\tCompressed\n"
    "CompressedReferenceOrientedAlignment\tRead\n"
    "CRAMIndex\tRead\n"
    "CoordinateSortedIndex\tRead\n"    
    "ExtensibleMarkupLanguage\tCached\n"
    "FASTA\tRead\n"
    "FASTAIndex\tRead\n"
    "FASTQ\tRead\n"
    "GnuZip\tCompressed\n"
    "HD5\tRead\n"
    "PacBioBAMIndex\tRead\n"
    "SequenceAlignmentMap\tRead\n"
    "SequenceReadArchive\tArchive\n"
    "SequenceReadFormat\tRead\n"
    "StandardFlowgramFormat\tRead\n"
    "TapeArchive\tArchive\n"
    "WinZip\tRead\n"
};

rc_t CCFileFormatAddRef (const CCFileFormat * self)
{
    if (self != NULL)
        atomic32_inc (&((CCFileFormat*)self)->refcount);
    return 0;
}

rc_t CCFileFormatRelease (const CCFileFormat * cself)
{
    rc_t rc = 0;
    CCFileFormat *self;

    self = (CCFileFormat *)cself; /* mutable field is ref count */
    if (self != NULL)
    {
        if (atomic32_dec_and_test (&self->refcount))
        {
            DEBUG_STATUS(("%s: call KFileFormatRelease for extentions\n", __func__));
            rc = KFileFormatRelease (self->ext);
            if (rc == 0)
            {
                DEBUG_STATUS(("%s: call KFileFormatRelease for magic\n", __func__));
                rc = KFileFormatRelease (self->magic);
                if (rc == 0)
                {
                    free (self);
                }
            }
        }
    }
    return rc;
}

rc_t CCFileFormatMake (CCFileFormat ** p)
{
    rc_t rc;
    CCFileFormat * self;

    DEBUG_ENTRY();

    self = malloc (sizeof *self);

    if (self == NULL)
    {
        rc = RC (rcExe, rcFileFormat, rcCreating, rcMemory, rcExhausted);
    }
    else
    {
        rc = KExtFileFormatMake (&self->ext, exttable, sizeof (exttable) - 1,
                                    formattable, sizeof (formattable) - 1);
        if (rc == 0)
        {
            rc = KMagicFileFormatMake (&self->magic, magictable,
                                        sizeof (magictable) - 1,
                                        formattable, sizeof (formattable) - 1);
            if (rc == 0)
            {
                atomic32_set (&self->refcount , 1);
                *p = self;
                return 0;
            }
        }
        free (self);
    }
    *p = NULL;
    return rc;
}

rc_t CCFileFormatGetType (const CCFileFormat *self, const KFile *file,
     const char *path, CCFileNode *node, uint32_t *ptype, uint32_t *pclass)
{
    static const char u_u[] = "Unknown/Unknown";
    rc_t rc, orc;

    int ret;
    size_t mtz;
    size_t etz;
    size_t num_read;
    KFileFormatType mtype;
    KFileFormatType etype;
    KFileFormatClass mclass;
    KFileFormatClass eclass;
    char    mclassbuf   [256] = {0};
    char    mtypebuf    [256] = {0};
    char    eclassbuf   [256] = {0};
    char    etypebuf    [256] = {0};
    /* Read buffer for initial inspection.
       BGZF blocks may compress up to ~64K, so allocate enough
       space to hold an entire block. */
    uint8_t	preread	[64 * 1024];
    char * buffer;
    size_t buffsize;

    DEBUG_ENTRY();
    DEBUG_STATUS(("%s: getting type for (%s)\n",__func__,path));

    /* initially assume that we don't know the type or class
     * these are just treated as files with no special processing
     * more than we that we don't know the type or class */
    *pclass = *ptype = 0;
    buffer = node -> ftype;
    buffsize = sizeof node -> ftype;

    strncpy (buffer, u_u, buffsize);
    buffer[buffsize-1] = '\0'; /* in case we got truncated in the copy above */

    orc = KFileRead (file, 0, preread, sizeof (preread), &num_read);
    if (orc == 0)
    {
        if (num_read > 7 && CCFileFormatIsKar (preread))
        {
            *pclass = ccffcArchive;
            *ptype = ccfftaSra;
            strncpy (buffer, "Archive/SequenceReadArchive", buffsize);
            buffer[buffsize-1] = '\0';
            DEBUG_STATUS(("%s: SRA file identified by content\n", __func__));
            return 0;
        }
        if (num_read > 7 && CCFileFormatIsNCBIEncrypted (preread))
        {
            *pclass = ccffcEncoded;
            *ptype = ccffteNCBI;
            strncpy (buffer, "Encoded/NCBI", buffsize);
            buffer[buffsize-1] = '\0';
            DEBUG_STATUS(("%s: NCBI encrypted format identified by content\n", __func__));
            return 0;
        }
        /* Sorta kinda hack to see if the file is WGA encrypted
         * We short cut the other stuff if it is WGA encoded
         */
        if (KFileIsWGAEnc (preread, num_read) == 0)
        {
            *pclass = ccffcEncoded;
            *ptype = ccffteWGA;
            strncpy (buffer, "Encoded/WGA", buffsize);
            buffer[buffsize-1] = '\0';
            DEBUG_STATUS(("%s: WGA format identified by content\n", __func__));
            return 0;
        }
        if (num_read > 3 && CCFileFormatIsSff (preread))
        {
            strncpy (buffer, "Read/StandardFlowgramFormat", buffsize);
            buffer[buffsize-1] = '\0';
            DEBUG_STATUS(("%s: SFF format identified by content\n", __func__));
            return 0;
        }
        /* Identify FASTQ file by its content
         */
        if (num_read > 6 && CCFileFormatIsFastq (preread, num_read, node))
        {
            strncpy (buffer, "Read/FASTQ", buffsize);
            buffer[buffsize-1] = '\0';
            DEBUG_STATUS(("%s: FASTQ format identified by content\n", __func__));
            return 0;
        }
        else
        {
            node->defline_name[0] = '\0';
            node->defline_pair = 0;
        }

        /* Identify BGZF-based formats (BAM, CSI) — decompress first block once */
        if (num_read >= 512)
        {
            uint8_t decompressed[512];
            size_t n = CCFileFormatDecompressFirstBGZFBlock(preread, num_read, decompressed, sizeof decompressed);
            if (n >= 4)
            {
                if (decompressed[0]=='B' && decompressed[1]=='A' &&
                    decompressed[2]=='M' && decompressed[3]==0x01)
                {
                    strncpy(buffer, "Read/BinaryAlignmentMap", buffsize);
                    buffer[buffsize-1] = '\0';
                    DEBUG_STATUS(("%s: BAM format identified by content\n", __func__));
                    return 0;
                }
                if (decompressed[0]=='C' && decompressed[1]=='S' &&
                    decompressed[2]=='I' && decompressed[3]==0x01)
                {
                    strncpy(buffer, "Read/CoordinateSortedIndex", buffsize);
                    buffer[buffsize-1] = '\0';
                    DEBUG_STATUS(("%s: CSI format identified by content\n", __func__));
                    return 0;
                }
            }
        }

        /* Identify FASTA file by its content (min 3 bytes for the header plus min MIN_FASTA_SEQUENCE_LENGTH bytes for sequence) */
        if (num_read >= (3 + MIN_FASTA_SEQUENCE_LENGTH) && CCFileFormatIsFasta (preread, num_read))
        {
            strncpy (buffer, "Read/FASTA", buffsize);
            buffer[buffsize-1] = '\0';
            DEBUG_STATUS(("%s: FASTA format identified by content\n", __func__));
            return 0;
        }

        rc = KFileFormatGetTypePath (self->ext, NULL, path, &etype, &eclass,
                                 etypebuf, sizeof (etypebuf), &etz);
        if (rc == 0)
        {
            rc = KFileFormatGetTypeBuff (self->magic, preread, num_read, &mtype,
                                         &mclass, mtypebuf, sizeof (mtypebuf), &mtz);
            if (rc == 0)
            {
                if (mclass == kffcNotFound) {
                    // Magic didn't recognize the content; treat as Unknown
                    strcpy(mtypebuf,  "Unknown");
                    strcpy(mclassbuf, "Unknown");
                }
                rc = KFileFormatGetClassDescr (self->ext, eclass, eclassbuf, sizeof (eclassbuf));
                if (rc == 0)
                {
                    rc = KFileFormatGetClassDescr (self->magic, mclass, mclassbuf, sizeof (mclassbuf));
                    if (rc == 0)
                    {
                        DEBUG_STATUS(("%s: (%s) %s/%s<=%s/%s\n", __func__,
                                      path, mclassbuf, mtypebuf, eclassbuf, etypebuf));

                        /* first handle known special cases */
                        if ((strcmp("WinZip", mtypebuf) == 0) &&
                            (strcmp("GnuZip", etypebuf) == 0))
                        {
                            /* we've gotten in too many Zip files with extension gz */
                            PLOGMSG (klogWarn,
                                     (klogWarn, "File '$(path)' is in unsupported winzip/pkzip format",
                                      "path=%s", path));
                        }
                        else if (strcmp ("GnuZip", mtypebuf) == 0 && (
                                    strcmp("BinaryAlignmentMap", etypebuf) == 0 ||
                                    strcmp("PacBioBAMIndex", etypebuf) == 0 ||
                                    strcmp("CRAMIndex", etypebuf) == 0 ||
                                    strcmp("CoordinateSortedIndex", etypebuf) == 0
                                    )
                                )
                        {
                            /* bam, pbi, crai, csi files have gnuzip magic, we need to treat them as data files ***/
                            strcpy (mclassbuf, eclassbuf);
                            strcpy (mtypebuf, etypebuf);
                        }
                        else if ((strcmp("SequenceReadArchive", etypebuf) == 0) &&
                                 (strcmp("Unknown", mtypebuf) == 0))
                        {
                            /* magic might not detect SRA/KAR files yet */
                            strcpy (mclassbuf, eclassbuf);
                            strcpy (mtypebuf, etypebuf);
                        }
                        else if ((strcmp("HD5", etypebuf) == 0) &&
                                 (strcmp("Unknown", mtypebuf) == 0))
                        {
                            strcpy (mclassbuf, eclassbuf);
                            strcpy (mtypebuf, etypebuf);
                        }
                        else if (strcmp("FASTQ", etypebuf) == 0)
                        {
                            PLOGMSG (klogWarn,
                                    (klogWarn, "File '$(path)' has a FASTQ extension but failed built-in FASTQ format validation",
                                    "path=%s", path));
                        }
                        else if (strcmp("FASTA", etypebuf) == 0)
                        {
                            PLOGMSG (klogWarn,
                                    (klogWarn, "File '$(path)' has a FASTA extension but failed built-in FASTA format validation",
                                    "path=%s", path));
                        }
                        else if ((strcmp("FASTAIndex", etypebuf) == 0) && (strcmp("Unknown", mtypebuf) == 0))
                        {
                            strcpy (mclassbuf, eclassbuf);
                            strcpy (mtypebuf, etypebuf);
                        }

                        /* now that we've fixed a few cases use the magic derived
                         * class and type as the extensions could be wrong and can
                         * cause failures */
                        if (strcmp ("Archive", mclassbuf) == 0)
                        {
                            *pclass = ccffcArchive;
                            if (strcmp ("TapeArchive", mtypebuf) == 0)
                                *ptype = ccfftaTar;
                            else if (strcmp ("SequenceReadArchive", mtypebuf) == 0)
                                *ptype = ccfftaSra;
                        }
                        else if (strcmp("Compressed", mclassbuf) == 0)
                        {
                            *pclass = ccffcCompressed;
                            if (strcmp ("Bzip", mtypebuf) == 0)
                            {
                                *ptype = ccfftcBzip2;
                                if ( no_bzip2 )
                                    * pclass = *ptype = 0;
                            }
                            else if (strcmp ("GnuZip", mtypebuf) == 0)
                                *ptype = ccfftcGzip;
                        }

                        /* Hmmm... we are using extension to determine XML though
                         * Probably okay */
                        else if (strcmp ("Cached", eclassbuf) == 0)
                        {
                            *pclass = ccffcCached;
                            if (strcmp ("ExtensibleMarkupLanguage", etypebuf) == 0)
                                *ptype = ccfftxXML;
                            strcpy (mclassbuf, eclassbuf);
                            strcpy (mtypebuf, etypebuf);
                        }

                        /* build the eventual filetype string - vaguely mime type like */
                        ret = snprintf (buffer, buffsize, "%s/%s", mclassbuf, mtypebuf);
                        if (ret >= buffsize)
                        {
                            ret = buffsize-1;
                            buffer[buffsize-1] = '\0';
                        }

                    }
                }
            }
        }
        if (rc)
        {
            *pclass = *ptype = 0;
            strncpy (buffer, u_u, buffsize);
            buffer[buffsize-1] = '\0'; /* in case we got truncated in the copy above */
        }
    }
    return orc;
}
