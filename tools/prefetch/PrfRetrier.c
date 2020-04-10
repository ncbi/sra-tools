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
* =========================================================================== */

#include <kfs/file.h> /* KFileRelease */

#include <klib/time.h> /* KSleep */
#include <klib/status.h> /* KStsLevelGet */

#include <strtol.h> /* strtou64 */

#include "PrfMain.h"
#include "PrfRetrier.h"

void PrfRetrierReset(PrfRetrier * self, uint64_t pos) {
    self->_pos = pos;

    if (self->_failed) {
        self->_failed = false;

        self->_state = eRSJustRetry;
        self->curSize = self->_bsize;
        self->_sleepTO = 0;

        if (KStsLevelGet() >= 1)
            PLOGERR(klogErr, (klogErr, 0,
                "KFileRead success: '$(name)':$(pos)/$(sz)",
                "name=%S,pos=%\'lu,sz=%\'lu",
                self->_src, self->_pos, self->_size));
    }
}

void PrfRetrierInit(PrfRetrier * self, const PrfMain * mane,
    const struct VPath * path, const struct String * src, bool isUri,
    const struct KFile ** f, size_t size, uint64_t pos)
{
    assert(self && f && *f && src);

    memset(self, 0, sizeof *self);

    self->_bsize = mane->bsize;
    self->_mgr = mane->kns;
    self->_path = path;
    self->_src = src;
    self->_size = size;
    self->_isUri = isUri;

    self->curSize = self->_bsize;
    self->_f = f;

    PrfRetrierReset(self, pos);
}

static rc_t PrfRetrierReopenRemote(PrfRetrier * self) {
    rc_t rc = 0;

    uint32_t timeout = 1;

    KFileRelease(*self->_f);
    *self->_f = NULL;

    while (timeout < 10 * 60 /* 10 minutes */) {
        rc = _KFileOpenRemote(self->_f, self->_mgr, self->_path,
            self->_src, !self->_isUri);
        if (rc == 0)
            break;

        KSleep(timeout);
        timeout *= 2;
    }

    return rc;
}

static bool PrfRetrierDecBufSz(PrfRetrier * self, rc_t rc) {
    const size_t MIN = 3768;

    assert(self);

    /* 3768 is usually returned by KStreamRead (size of TLS buffer?) */
    if (self->curSize == MIN)
        return false;

    self->curSize /= 2;
    if (self->curSize < MIN)
        self->curSize = MIN;

    return true;
}

static bool PrfRetrierIncSleepTO(PrfRetrier * self) {
    if (self->_sleepTO == 0)
        self->_sleepTO = 1;
    else
        self->_sleepTO *= 2;

    if (self->_sleepTO == 16)
        --self->_sleepTO;

    if (self->_sleepTO > 20 * 60 /* 20 minutes */)
        return false;
    else
        return true;
}

rc_t PrfRetrierAgain(PrfRetrier * self, rc_t rc, uint64_t pos) {
    bool retry = true;

    static bool INITED = false;
    static KTime_t D_T = ~0;
    if (!INITED) {
        const char * str = getenv("NCBI_VDB_PREFETCH_RETRY");
        if (str != NULL) {
            char *end = NULL;
            D_T = strtou64(str, &end, 0);
            if (end[0] != 0)
                D_T = ~0;
        }
        INITED = true;
    }

    assert(self);

    if (D_T == 0)
        retry = false;
    else if (pos <= self->_pos)
        switch (self->_state) {
        case eRSJustRetry:
            break;
        case eRSReopen:
            if (PrfRetrierReopenRemote(self) != 0)
                retry = false;
            break;
        case eRSDecBuf:
            if (PrfRetrierDecBufSz(self, rc))
                break;
            else
                self->_state = eRSIncTO;
        case eRSIncTO:
            if (!PrfRetrierIncSleepTO(self))
                retry = false;
            else if (D_T != ~0 && KTimeStamp() - self->_tFailed > D_T)
                retry = false;
            break;
        default: assert(0); break;
        }

    if (!self->_failed) {
        self->_failed = true;
        self->_tFailed = KTimeStamp();
    }

    if (retry) {
        KStsLevel lvl = KStsLevelGet();
        if (lvl > 0) {
            if (lvl == 1)
                PLOGERR(klogErr, (klogErr, rc,
                    "Cannot KFileRead: retrying '$(name)'...",
                    "name=%S", self->_src));
            else
                switch (self->_state) {
                case eRSJustRetry:
                    PLOGERR(klogErr, (klogErr, rc,
                        "Cannot KFileRead: retrying '$(name)':$(pos)...",
                        "name=%S,pos=%lu", self->_src, self->_pos));
                    break;
                case eRSReopen:
                    PLOGERR(klogErr, (klogErr, rc,
                        "Cannot KFileRead: reopened, retrying '$(name)'...",
                        "name=%S", self->_src));
                    break;
                case eRSDecBuf:
                    PLOGERR(klogErr, (klogErr, rc, "Cannot KFileRead: "
                        "buffer size = $(sz), retrying '$(name)'...",
                        "sz=%zu,name=%S", self->curSize, self->_src));
                    break;
                case eRSIncTO:
                    PLOGERR(klogErr, (klogErr, rc, "Cannot KFileRead: "
                        "sleep TO = $(to)s, retrying '$(name)'...",
                        "to=%u,name=%S", self->_sleepTO, self->_src));
                    break;
                default: assert(0); break;
                }
        }

        rc = 0;
    }
    else
        PLOGERR(klogErr, (klogErr, rc,
            "Cannot KFileRead '$(name)': sz=$(sz), to=$(to)",
            "name=%S,sz=%zu,to=%u",
            self->_src, self->curSize, self->_sleepTO));

    if (rc == 0) {
        if (self->_sleepTO > 0) {
            STSMSG(2, ("Sleeping %us...", self->_sleepTO));
#ifndef TESTING_FAILURES
            KSleep(self->_sleepTO);
#endif
        }

        if (++self->_state == eRSMax)
            self->_state = eRSReopen;
    }

    return rc;
}

