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

#include "EBI_ReferenceSequence.h"

typedef struct EBI_ReferenceSequence EBI_ReferenceSequence;
#define NGS_REFERENCESEQUENCE EBI_ReferenceSequence
#include "NGS_ReferenceSequence.h"

#include "NGS_String.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/refcount.h>
#include <klib/rc.h>
#include <klib/data-buffer.h>
#include <kns/http.h>
#include <kns/stream.h>
#include <kns/manager.h>

#include <stddef.h>
#include <assert.h>

#include <strtol.h>
#include <string.h>

#include <sysalloc.h>

/*--------------------------------------------------------------------------
 * EBI_ReferenceSequence
 */

static void                EBI_ReferenceSequenceWhack ( EBI_ReferenceSequence * self, ctx_t ctx );
static NGS_String *        EBI_ReferenceSequenceGetCanonicalName ( EBI_ReferenceSequence * self, ctx_t ctx );
static bool                EBI_ReferenceSequenceGetIsCircular ( EBI_ReferenceSequence const* self, ctx_t ctx );
static uint64_t            EBI_ReferenceSequenceGetLength ( EBI_ReferenceSequence * self, ctx_t ctx );
static struct NGS_String * EBI_ReferenceSequenceGetBases ( EBI_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size );
static struct NGS_String * EBI_ReferenceSequenceGetChunk ( EBI_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size );

static NGS_ReferenceSequence_vt EBI_ReferenceSequence_vt_inst =
{
    /* NGS_Refcount */
    { EBI_ReferenceSequenceWhack }

    /* NGS_ReferenceSequence */
    ,EBI_ReferenceSequenceGetCanonicalName
    ,EBI_ReferenceSequenceGetIsCircular
    ,EBI_ReferenceSequenceGetLength
    ,EBI_ReferenceSequenceGetBases
    ,EBI_ReferenceSequenceGetChunk
};


struct EBI_ReferenceSequence
{
    NGS_ReferenceSequence dad;

    uint64_t cur_length; /* size of current reference in bases (0 = not yet counted) */

    char* buf_ref_data;     /* contains reference data
                            */
    NGS_String* ebi_ref_spec;
};

static
void EBI_ReferenceSequenceWhack ( EBI_ReferenceSequence * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcNS, rcTable, rcClosing );

    if ( self->buf_ref_data != NULL )
    {
        free ( self->buf_ref_data );
        self->buf_ref_data = NULL;
        self->cur_length = 0;
    }

    NGS_StringRelease ( self -> ebi_ref_spec, ctx );
}

/* Init
 */
static
void EBI_ReferenceSequenceInit ( ctx_t ctx,
                           EBI_ReferenceSequence * ref,
                           const char *clsname,
                           const char *instname )
{
    FUNC_ENTRY ( ctx, rcNS, rcTable, rcOpening );

    if ( ref == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    else
    {
        TRY ( NGS_ReferenceSequenceInit ( ctx, & ref -> dad, & EBI_ReferenceSequence_vt_inst, clsname, instname ) )
        {
            /* TODO: maybe initialize more*/
        }
    }
}

static bool is_md5 ( const char * spec )
{
    size_t char_count = 32;
    const char allowed_chars[] = "0123456789abcdefABCDEF";

    size_t i;
    for ( i = 0; spec [i] != '\0' && i < char_count; ++i )
    {
        if ( strchr ( allowed_chars, spec[i] ) == NULL )
        {
            return false;
        }
    }

    return i == char_count;
}

/* TBD - WHAT IS THIS? IT TAKES A CONTEXT AND RETURNS AN RC!!
   THIS CODE CAN'T WORK AS INTENDED.
*/
static rc_t NGS_ReferenceSequenceComposeEBIUrl ( ctx_t ctx, const char * spec, bool ismd5, char* url, size_t url_size )
{
    /* TBD - obtain these from configuration */
    char const url_templ_md5[] = "http://www.ebi.ac.uk/ena/cram/md5/%s";
    char const url_templ_acc[] = "https://eutils.ncbi.nlm.nih.gov/entrez/eutils/efetch.fcgi?db=nucleotide&rettype=fasta&id=%s";

    size_t num_written = 0;
    rc_t rc = string_printf ( url, url_size, & num_written, ismd5 ? url_templ_md5 : url_templ_acc, spec );

    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcStorageExhausted, "insufficient url buffer for NGS_ReferenceSequenceComposeEBIUrl" );
    }

    return rc;
}

static rc_t NGS_ReferenceSequenceEBIInitReference (
    ctx_t ctx, bool ismd5, EBI_ReferenceSequence * ref,
    const char* ebi_data, size_t ebi_data_size)
{
    rc_t rc = 0;
    ref -> buf_ref_data = malloc ( ebi_data_size );
    if ( ref -> buf_ref_data == NULL )
        return RC ( rcRuntime, rcBuffer, rcAllocating, rcMemory, rcExhausted );

    if ( ismd5 )
    {
        memmove ( ref->buf_ref_data, ebi_data, ebi_data_size );
        ref -> cur_length = ebi_data_size;
    }
    else
    {
        size_t i, i_dst;

        /* this is FASTA file - skip first line and parse out all other '\n' */

        /* 1. skip 1st line */
        for ( i = 0; i < ebi_data_size && ebi_data [i] != '\0'; ++i )
        {
            if ( ebi_data [i] == '\n' )
            {
                ++i;
                break;
            }
        }

        if ( i == ebi_data_size || ebi_data [i] == '\0' )
            return RC ( rcText, rcDoc, rcParsing, rcFormat, rcInvalid );

        /* copy everything except '\n' to the reference buffer */

        i_dst = 0;
        for (; i < ebi_data_size && ebi_data [i] != '\0'; ++i)
        {
            if ( ebi_data [i] != '\n' )
                ref->buf_ref_data [i_dst++] = ebi_data [i];
        }
        ref -> cur_length = i_dst;
    }

    return rc;
}


#define URL_SIZE 512

static rc_t NGS_ReferenceSequenceOpenEBI ( ctx_t ctx, const char * spec, EBI_ReferenceSequence * ref )
{
    rc_t rc = 0;
    KDataBuffer result;
    KHttpRequest *req = NULL;
    KHttpResult *rslt = NULL;
    bool ismd5 = is_md5 ( spec );
    KNSManager * mgr;

    size_t const url_size = URL_SIZE;
    char url_request [ URL_SIZE ];

    rc = KNSManagerMake ( & mgr );
    if ( rc != 0 )
        return rc;

    memset(&result, 0, sizeof result);
    rc = NGS_ReferenceSequenceComposeEBIUrl ( ctx, spec, ismd5, url_request, url_size );

    if ( rc == 0 )
        rc = KNSManagerMakeRequest (mgr, &req, 0x01010000, NULL,url_request);

    if ( rc == 0 )
        rc = KHttpRequestGET(req, &rslt);

    if ( rc == 0 )
    {
        uint32_t code = 0;
        rc = KHttpResultStatus(rslt, &code, NULL, 0, NULL);
        if (rc == 0 && code != 200)
            rc = RC(rcNS, rcFile, rcReading, rcFile, rcInvalid);
    }

    if ( rc == 0 )
    {
        size_t total = 0;
        KStream *response = NULL;
        rc = KHttpResultGetInputStream(rslt, &response);
        if (rc == 0)
            rc = KDataBufferMakeBytes(&result, 1024);

        while (rc == 0)
        {
            size_t num_read = 0;
            uint8_t *base = NULL;
            uint64_t avail = result.elem_count - total;
            if (avail < 256)
            {
                rc = KDataBufferResize(&result, result.elem_count + 1024);
                if (rc != 0)
                    break;
            }
            base = result.base;
            rc = KStreamRead(response, &base[total], result.elem_count - total, &num_read);
            if (rc != 0)
            {
                /* TBD - look more closely at rc */
                if (num_read > 0)
                    rc = 0;
                else
                    break;
            }
            if (num_read == 0)
                break;

            total += num_read;
        }
        KStreamRelease ( response );
        if (rc == 0)
        {
            result.elem_count = total;
        }
    }

    if ( rc == 0 )
    {
        const char* start = (const char*) result.base;
        size_t size = KDataBufferBytes ( & result );

        rc = NGS_ReferenceSequenceEBIInitReference ( ctx, ismd5, ref, start, size );
        if (rc == 0)
            ref->ebi_ref_spec = NGS_StringMakeCopy ( ctx, spec, strlen(spec) );
    }

    /* TODO: release only if they were allocated */
    KDataBufferWhack ( &result );
    KHttpResultRelease ( rslt );
    KHttpRequestRelease ( req );

    KNSManagerRelease ( mgr );

    return rc;
}

NGS_ReferenceSequence * NGS_ReferenceSequenceMakeEBI ( ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcNS, rcTable, rcOpening );

    EBI_ReferenceSequence * ref;

    assert ( spec != NULL );
    assert ( spec [0] != '\0' );

    ref = calloc ( 1, sizeof *ref );
    if ( ref == NULL )
    {
        SYSTEM_ERROR ( xcNoMemory, "allocating EBI_ReferenceSequence ( '%s' )", spec );
    }
    else
    {
        TRY ( EBI_ReferenceSequenceInit ( ctx, ref, "NGS_ReferenceSequence", spec ) )
        {
            rc_t rc = NGS_ReferenceSequenceOpenEBI ( ctx, spec, ref );
            if ( rc != 0 )
            {
                INTERNAL_ERROR ( xcUnexpected, "failed to open table '%s': rc = %R", spec, rc );
            }
            else
            {
                return (NGS_ReferenceSequence*) ref;
            }
            EBI_ReferenceSequenceWhack ( ref , ctx );
        }
        free ( ref );
    }
    return NULL;
}


NGS_String * EBI_ReferenceSequenceGetCanonicalName ( EBI_ReferenceSequence * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcNS, rcDoc, rcReading );

    assert ( self != NULL );

    return NGS_StringDuplicate ( self -> ebi_ref_spec, ctx );
}

bool EBI_ReferenceSequenceGetIsCircular ( const EBI_ReferenceSequence * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcNS, rcDoc, rcReading );

    assert ( self );

    return false;
}

uint64_t EBI_ReferenceSequenceGetLength ( EBI_ReferenceSequence * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcNS, rcDoc, rcReading );

    assert ( self );

    return self -> cur_length;
}

struct NGS_String * EBI_ReferenceSequenceGetBases ( EBI_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcNS, rcDoc, rcReading );

    assert ( self );

    {
        uint64_t totalBases = EBI_ReferenceSequenceGetLength ( self, ctx );
        if ( offset >= totalBases )
        {
            return NGS_StringMake ( ctx, "", 0 );
        }
        else
        {
            uint64_t basesToReturn = totalBases - offset;

            if (size != (size_t)-1 && basesToReturn > size)
                basesToReturn = size;

            return NGS_StringMakeCopy ( ctx, (const char*) self -> buf_ref_data + offset, basesToReturn );
        }
    }
}

struct NGS_String * EBI_ReferenceSequenceGetChunk ( EBI_ReferenceSequence * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    FUNC_ENTRY ( ctx, rcNS, rcDoc, rcReading );

    assert ( self );

    return EBI_ReferenceSequenceGetBases ( self, ctx, offset, size );
}
