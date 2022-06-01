/*==============================================================================
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
#include <kapp/extern.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <klib/debug.h>
#include <klib/container.h>
#include <kfs/directory.h>
#include <kfs/file.h>

#include <loader/progressbar.h>

#include <stdlib.h>
#include <string.h>

#define DBG(msg) DBGMSG(DBG_LOADLIB,DBG_FLAG(DBG_LOADLIB_PBAR), msg)

struct KLoadProgressbar
{
    SLNode node;
    bool active;
    uint64_t used;
    uint64_t total;
};

struct KJobs {
    const char* severity;
    SLList jobs;
    uint64_t percent;
} g_jobs =
{
    "status",
    {NULL, NULL},
    0
};

LIB_EXPORT rc_t CC KLoadProgressbar_Make(const KLoadProgressbar** cself, uint64_t size)
{
    rc_t rc = 0;
    KLoadProgressbar* obj = NULL;

    if( cself == NULL ) {
        rc = RC(rcApp, rcFunction, rcConstructing, rcSelf, rcNull);
    } else if( (obj = calloc(1, sizeof(*obj))) == NULL ) {
        *cself = NULL;
        rc = RC(rcApp, rcFunction, rcConstructing, rcMemory, rcExhausted);
    } else {
        DBG(("%s: %p %lu\n", __func__, obj, size));
        SLListPushTail(&g_jobs.jobs, &obj->node);
        *cself = obj;
        obj->active = true;
        obj->total = size;
    }
    return rc;
}

static
bool CC job_active( SLNode *n, void *data )
{
    return ((KLoadProgressbar*)n)->active;
}

static
void CC job_whack( SLNode *n, void *data )
{
    free(n);
}

struct job_data {
    float percent;
    uint64_t qty;
};

static
void CC job_percent( SLNode *node, void *data )
{
    const KLoadProgressbar* n = (const KLoadProgressbar*)node;
    struct job_data* d = (struct job_data*)data;
    if (n->used && n->total)
        d->percent += n->used * 100 / n->total;
    d->qty++;
}

static
void CC job_report(bool force_report, bool final)
{
    struct job_data d;

    memset(&d, 0, sizeof(d));
    SLListForEach(&g_jobs.jobs, job_percent, &d);
    d.qty = (uint64_t) (d.qty ? d.percent / d.qty : (final ? 100 : 0) );
    if( force_report || d.qty != g_jobs.percent ) {
        g_jobs.percent = d.qty;
        PLOGMSG(klogInfo, (klogInfo, "processed $(percent)%",
            "severity=%s,percent=%lu", g_jobs.severity, g_jobs.percent));
    }
}

LIB_EXPORT void CC KLoadProgressbar_Release(const KLoadProgressbar* cself, bool exclude)
{
    if( cself ) {
        KLoadProgressbar* self = (KLoadProgressbar*)cself;
        if( exclude ) {
            SLListUnlink(&g_jobs.jobs, &self->node);
            free(self);
        } else {
            self->active = false;
        }
        if( !SLListDoUntil(&g_jobs.jobs, job_active, NULL) ) {
            /* no more active jobs: make last report and drop whole list */
            job_report(false, true);
            SLListWhack(&g_jobs.jobs, job_whack, NULL);
        }
    }
}

LIB_EXPORT rc_t CC KLoadProgressbar_Append(const KLoadProgressbar* cself, uint64_t chunk)
{
    rc_t rc = 0;

    if( cself == NULL ) {
        rc = RC(rcApp, rcFunction, rcWriting, rcSelf, rcNull);
    } else {
        uint64_t c = cself->total + chunk;
        DBG(("%s: +%lu\n", __func__, chunk));
        if( c >= cself->total ) {
            ((KLoadProgressbar*)cself)->total += chunk;
        } else {
            rc = RC(rcApp, rcFunction, rcResizing, rcData, rcOutofrange);
        }
    }
    return rc;
}

LIB_EXPORT rc_t CC KLoadProgressbar_File(const KLoadProgressbar** cself, const char* filename, const KDirectory* dir)
{
    rc_t rc = 0;
    uint64_t sz;
    KDirectory* tmp = (KDirectory*)dir;

    if( cself == NULL || filename == NULL ) {
        rc = RC(rcApp, rcFunction, rcWriting, rcParam, rcNull);
    } else if( tmp == NULL && (rc = KDirectoryNativeDir(&tmp)) != 0 ) {
    } else if( (rc = KDirectoryFileSize(tmp, &sz, "%s", filename)) == 0 ) {
        rc = KLoadProgressbar_Make(cself, sz);
    }
    if( tmp != dir ) {
        KDirectoryRelease(tmp);
    }
    return rc;
}

LIB_EXPORT rc_t CC KLoadProgressbar_KFile(const KLoadProgressbar** cself, const KFile* file)
{
    rc_t rc = 0;
    uint64_t sz;

    if( cself == NULL || file == NULL ) {
        rc = RC(rcApp, rcFunction, rcWriting, rcParam, rcNull);
    } else if( (rc = KFileSize(file, &sz)) == 0 ) {
        rc = KLoadProgressbar_Make(cself, sz);
    }
    return rc;
}

LIB_EXPORT rc_t CC KLoadProgressbar_Process(const KLoadProgressbar* cself, uint64_t chunk, bool force_report)
{
    rc_t rc = 0;

    if( cself == NULL ) {
        rc = RC(rcApp, rcFunction, rcLogging, rcSelf, rcNull);
    } else {
        ((KLoadProgressbar*)cself)->used += chunk;
        DBG(("%s: %p +%lu %lu of %lu\n", __func__, cself, chunk, cself->used, cself->total));
        job_report(force_report, false);
    }
    return rc;
}
