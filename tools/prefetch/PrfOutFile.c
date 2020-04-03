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
* =========================================================================== */

#include <kfs/directory.h> /* KDirectoryNativeDir */

#include <klib/out.h> /* OUTMSG */
#include <klib/rc.h> /* RC */
#include <klib/status.h> /* STSMSG */
#include <klib/printf.h> /* string_printf */
#include <klib/time.h> /* KTimeStamp */

#include <strtol.h> /* strtou64 */

#include "PrfMain.h" /* RELEASE */
#include "PrfOutFile.h"

//#define DEBUGGING

static rc_t StringRelease(const String *self) {
    free((String*)self);

    return 0;
}

static rc_t KDirectory_MkTmpName(const KDirectory *self,
    const String *prefix, char *out, size_t sz)
{
    rc_t rc = string_printf(out, sz, NULL, "%S.tmp", prefix);
    if (rc != 0) {
        PLOGERR(
            klogInt, (klogInt, rc, "string_printf($(arg))", "arg=%S", prefix));
        return rc;
    }
    else
        return rc;
}

static bool KDirectory_Exist(const KDirectory * self,
    const String * name, const char * sfx)
{
    assert(name && sfx);

    KPathType type = KDirectoryPathType(
        self, "%.*s%s", name->size, name->addr, sfx);
    if (type == kptNotFound)
        return false;
    else
        return true;
}

static const char * TFExt(const PrfOutFile * self) {
    switch (self->_tfType) {
    case eTextual:
        return ".prt";
    default:
        assert(0);
        return "";
    }
}

static bool TFExist(PrfOutFile * self) {
    assert(self);

    if (KDirectory_Exist(self->_dir, self->cache, TFExt(self)))
        return true;
    else
        return false;
}

static rc_t TFRm(PrfOutFile * self) {
    assert(self);

    if (TFExist(self)) {
        assert(self->cache);
        STSMSG(STS_DBG, ("removing %S%s", self->cache, TFExt(self)));
        return KDirectoryRemove(self->_dir, false,
            "%.*s%s", self->cache->size, self->cache->addr, TFExt(self));
    }
    else
        return 0;
}

static rc_t TFKill(PrfOutFile * self, rc_t rc, const char * msg) {
    assert(self);

    if (msg != NULL)
        PLOGERR(klogInt, (klogInt, rc,
            "Cannot keep transaction file: $(msg)", "msg=%s", msg));
    else
        LOGERR(klogInt, rc, "Cannot keep transaction file");

    self->_resume = false;

    return TFRm(self);
}

static uint64_t GetEnv(const char *name, uint64_t aDefault) {
    uint64_t val = 0;

    const char * str = getenv(name);
    if (str != NULL) {
        char *end = NULL;
        uint64_t n = strtou64(str, &end, 0);
        if (end[0] != 0)
            n = 0;
        else
            val = n;
    }
    else
        val = 0;

    if (val == 0)
        return aDefault;
    else
        return val;
}

static bool FTTimeToCommit(PrfOutFile * self) {
    static uint64_t D_PS = 0;
    static KTime_t D_TM = 0;

    if (D_PS == 0)
        D_PS = GetEnv("NCBI_VDB_PREFETCH_COMMIT_SZ", 16 * 1024 * 1024);
    if (D_TM == 0)
        D_TM = GetEnv("NCBI_VDB_PREFETCH_COMMIT_TM", 10 * 60);

    assert(self);

    if (self->_committed == 0) {
        self->_committed = KTimeStamp();
        return false;
    }
    else if (self->pos - self->_lastPos >= D_PS)
        return true;
    else if (KTimeStamp() - self->_committed >= D_TM)
        return true;
    else
        return false;
}

static void FTToCommit(PrfOutFile * self) {
    assert(self);

    self->_committed = KTimeStamp();
    self->_lastPos = self->pos;
}

static rc_t TFPutPosAsText(PrfOutFile * self,
    char * b, size_t sz, size_t * num)
{
    rc_t rc = string_printf(b, sz, num, "%lu\n", self->pos);
    if (rc != 0) {
        PLOGERR(klogInt, (klogInt, rc, "string_printf($(arg).$(ext))",
            "arg=%S,ext=%s", self->cache, TFExt(self)));
        return rc;
    }
    else
        return rc;
}

static
rc_t TFPutPos(PrfOutFile * self, char * b, size_t sz, size_t * num)
{
    switch (self->_tfType) {
    case eTextual:
        return TFPutPosAsText(self, b, sz, num);
    default:
        assert(0);
        return RC(rcExe, rcString, rcCreating, rcType, rcUnexpected);
    }
}

static rc_t TFWritePos(PrfOutFile * self) {
    rc_t rc = 0;
    char b[99] = "";
    size_t num = 0;

    assert(self && self->cache);

    if (self->pos == 0)
        return 0;

    STSMSG(STS_DBG, ("writing %S%s", self->cache, TFExt(self)));
    rc = TFPutPos(self, b, sizeof b, &num);
    if (rc != 0) {
        TFKill(self, rc, "Cannot write position");

        return rc;
    }
    else {
        size_t num_writ = 0;
        rc = KFileWrite(self->_tf, self->_tfPos, b, num, &num_writ);
        if (rc != 0)
            TFKill(self, rc, "Cannot Write(prf)");
        else
        {
            if (num_writ != num) {
                rc = RC(rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete);
                TFKill(self, rc, "Cannot Write(prf)");
            }
            else
                self->_tfPos += num_writ;
        }

        return rc;
    }
}

static rc_t TFSetPos(PrfOutFile * self, uint64_t pos, uint64_t tfPos) {
    rc_t rc = 0;

    assert(self);

    self->pos = pos;
    self->_tfPos = tfPos;

    rc = KFileSetSize(self->_tf, self->_tfPos);
    if (rc != 0) {
        TFKill(self, rc, "Cannot SetSize(prf)");
        return rc;
    }
    else
        return rc;
}

static void TFGetPosAsText(PrfOutFile * self, uint64_t posSize, uint64_t fSize,
    uint64_t *pPos, uint64_t *pTfPos)
{
    const char * buf = NULL;
    uint64_t first = 0, last = 0, prevLast = 0, prevPos = 0;

    assert(self && pPos && pTfPos);

    for (buf = self->_buf.base;;) {
        uint64_t pos = 0;
        size_t i = 0;
        const char * p = NULL;

        if (posSize < first) {
            assert(posSize >= first);
            first = posSize;
        }

        *pTfPos = first;

        p = string_chr(buf + first, posSize - first, '\n');
        if (p == NULL) {
            *pPos = prevPos;
            *pTfPos = prevLast + 1;
            return;
        }
        else if (p == buf + first) {
            *pPos = *pTfPos = 0;
            return;
        }

        for (i = first, last = p - buf, pos = 0; i < last; ++i) {
            if (buf[i] < '0' || buf[i] > '9') { /* non-digit: ignore bad file */
                *pPos = *pTfPos = 0;
                return;
            }
            else
                pos = pos * 10 + buf[i] - '0';
        }

        if (pos < fSize) {
            prevPos = pos;
            prevLast = last;
            first = last + 1;
        }
        else if (pos == fSize) {
            *pPos = pos;
            *pTfPos = last + 1;
            return;
        }
        else {
            *pPos = prevPos;
            if (prevPos == 0)
                *pTfPos = 0;
            else
                *pTfPos = prevLast + 1;
            return;
        }
    }
}

static void TFGetPos(PrfOutFile * self, uint64_t posSize, uint64_t fSize,
    uint64_t *pPos, uint64_t *pTfPos)
{
    switch (self->_tfType) {
    case eTextual:
        TFGetPosAsText(self, posSize, fSize, pPos, pTfPos);
        return;
    default:
        assert(0);
    }
}

static rc_t TFReadPos(PrfOutFile * self, uint64_t origSize) {
    rc_t rc = 0;
    uint64_t fsize = 0;
    uint64_t pos = 0, tfPos = 0;

    assert(self);

    STSMSG(STS_DBG, ("reading %S%s", self->cache, TFExt(self)));

    if (origSize == 0)
        return TFSetPos(self, 0, 0);

    rc = KFileSize(self->_tf, &fsize);
    if (rc != 0) {
        TFKill(self, rc, "Cannot Size(prf)");
        return rc;
    }
    if (rc == 0 && fsize == 0) {
        self->pos = self->_tfPos = 0;
        return rc;
    }

    if (rc == 0) {
        rc = KDataBufferResize(&self->_buf, fsize);
        if (rc != 0)
            LOGERR(klogInt, rc, "KDataBufferResize");
    }

    if (rc == 0) {
        rc = KFileReadExactly(self->_tf, 0, self->_buf.base, fsize);
        if (rc != 0) {
            TFKill(self, rc, "Cannot Read(prf)");
            return rc;
        }
    }

    TFGetPos(self, fsize, origSize, &pos, &tfPos);
    return TFSetPos(self, pos, tfPos);
}

static rc_t TFNegotiatePos(PrfOutFile * self) {
    rc_t rc = 0;
    uint64_t fsize = 0;

    assert(self);

#ifdef DEBUGGING
    OUTMSG(("%s: %S.prt found: loading...\n", __FUNCTION__, self->cache));
#endif
    STSMSG(STS_DBG, ("loading %S.prf", self->cache));

    rc = KFileSize(self->file, &fsize);
    if (rc != 0) {
        self->_fatal = true;
        PLOGERR(klogInt,
            (klogInt, rc, "Cannot Size($(arg).prf)", "arg=%S", self->cache));
    }
    else {
        rc = TFReadPos(self, fsize);
        if (rc != 0) {
            self->pos = 0;
            KFileSetSize(self->file, self->pos);
        }
        if (!self->_resume)
            return rc;
    }

    if (rc == 0 && self->pos > 0)
        self->_loaded = true;

    assert(self->pos <= fsize);
    if (self->pos > fsize)
        self->pos = fsize; /* should never happen */
    else if (self->pos < fsize) {
        rc = KFileSetSize(self->file, self->pos);
        if (rc != 0) {
            self->_fatal = true;
            PLOGERR(klogInt,
                (klogInt, rc, "Cannot SetSize($(arg))", "arg=%s", self->name));
        }
    }

#ifdef DEBUGGING
    OUTMSG(("%s: fsize=%lu, starting from %lu\n", __FUNCTION__,
        fsize, self->pos));
#endif

    return rc;
}

static rc_t TFOpen(PrfOutFile * self, bool rm) {
    rc_t rc = 0;
    bool exists = false;

    assert(self && self->cache);

    if (KDirectory_Exist(self->_dir, self->cache, "")) {
        STSMSG(STS_DBG, ("removing %S", self->cache));
        KDirectoryRemove(self->_dir, false,
            "%.*s", self->cache->size, self->cache->addr);
    }

    if (rm)
        rc = TFRm(self);
    else if (!self->_resume)
        rc = TFRm(self);
    else if (TFExist(self))
        exists = true;

    if (exists) {
        STSMSG(STS_DBG, ("opening %S%s", self->cache, TFExt(self)));
        rc = KDirectoryOpenFileWrite(self->_dir, &self->_tf, true, "%.*s%s",
            self->cache->size, self->cache->addr, TFExt(self));
        if (rc != 0)
            PLOGERR(klogInt, (klogInt, rc, "Cannot OpenFileWrite(($(arg).prf)",
                "arg=%S", self->cache));
    }
    else if (self->_resume) {
#ifdef DEBUGGING
        OUTMSG(("%s: %S%s not found: creating...\n",
            __FUNCTION__, self->cache, TFExt(self)));
#endif
        STSMSG(STS_DBG, ("creating %S%s", self->cache, TFExt(self)));

        rc = KDirectoryCreateFile(self->_dir, &self->_tf,
            false, 0664, kcmInit | kcmParents, "%.*s%s",
            self->cache->size, self->cache->addr, TFExt(self));
        if (rc != 0)
            PLOGERR(klogInt, (klogInt, rc, "Cannot CreateFile(($(arg).prf)",
                "arg=%S", self->cache));

        self->pos = self->_tfPos = 0;

        if (rc == 0)
            rc = TFWritePos(self);
    }

    if (rc != 0)
        TFKill(self, rc, NULL);

    return rc;
}

static rc_t PrfOutFileOpenWrite(PrfOutFile * self) {
    rc_t rc = 0;

    assert(self);

    STSMSG(STS_DBG, ("opening %s", self->name));

    rc = KDirectoryOpenFileWrite(self->_dir,
        &self->file, true, "%s", self->name);
    if (rc != 0) {
        self->_fatal = true;
        PLOGERR(klogInt, (
            klogInt, rc, "Cannot OpenFileWrite($(arg))", "arg=%s", self->name));
        return rc;
    }
    else
        return rc;
}

static rc_t PrfOutFileCommit(PrfOutFile * self, bool force) {
    rc_t rc = 0;

    if (!self->_resume)
        return 0;

    if (force || FTTimeToCommit(self)) {
        uint64_t size = 0;
        rc = KFileRelease(self->file);
        if (rc != 0) {
            self->_fatal = true;
            PLOGERR(klogInt, (
                klogInt, rc, "Cannot Release($(arg))", "arg=%s", self->name));
        }
        else
            rc = PrfOutFileOpenWrite(self);

        if (rc == 0) {
            rc = KFileSize(self->file, &size);
            if (rc != 0) {
                self->_fatal = true;
                PLOGERR(klogInt, (
                    klogInt, rc, "Cannot Size($(arg))", "arg=%s", self->name));
            }
        }

        if (rc == 0 && size < self->pos)
            self->pos = size;

        if (rc == 0)
            rc = TFWritePos(self);

        if (rc == 0) {
            rc = KFileRelease(self->_tf);
            if (rc != 0) {
                TFKill(self, rc, "Cannot Release(prf)");
                return rc;
            }
        }

        if (rc == 0)
            rc = TFOpen(self, false);

        if (rc == 0)
            FTToCommit(self);
    }

    return rc;
}

rc_t PrfOutFileInit(PrfOutFile * self, bool resume) {
    rc_t rc = 0;

    assert(self);

    memset(self, 0, sizeof *self);

    self->_buf.elem_bits = 8;
    self->_tfType = eTextual;
    self->_resume = resume;

    rc = KDirectoryNativeDir(&self->_dir);
    if (rc != 0) {
        LOGERR(klogInt, rc, "KDirectoryNativeDir");
        return rc;
    }
    else
        return rc;
}

rc_t PrfOutFileMkName(PrfOutFile * self, const String * cache) {
    rc_t rc = 0;

    assert(self);

    rc = KDirectory_MkTmpName(self->_dir, cache, self->name, sizeof self->name);
    if (rc == 0) {
        rc = StringCopy(&self->cache, cache);
        return rc;
    }
    else
        return rc;
}

rc_t PrfOutFileOpen(PrfOutFile * self, bool force, const char * name) {
    rc_t rc = 0;
    bool negotiated = false;

    rc_t ro = TFOpen(self, force);
    if (ro != 0)
        TFKill(self, ro, "Cannot open TF");

    assert(self && self->cache);

    if (KDirectoryPathType(self->_dir, "%s", self->name)
        == kptNotFound)
    {
#ifdef DEBUGGING
        OUTMSG(("%s: %s not found: creating...\n", __FUNCTION__, self->name));
#endif
        STSMSG(STS_DBG, ("creating %s", self->name));

        rc = KDirectoryCreateFile(self->_dir, &self->file,
            false, 0664, kcmInit | kcmParents, "%s", self->name);
        if (rc != 0)
            PLOGERR(klogInt, (klogInt, ro, "Cannot CreateFile($(arg))",
                "arg=%s", self->name));
    }
    else {
        rc = PrfOutFileOpenWrite(self);
        if (rc == 0) {
            if (force || !self->_resume)
                self->pos = 0;
            else if (ro == 0) {
                ro = TFNegotiatePos(self);
                if (ro == 0)
                    negotiated = true;
                else if (self->_fatal && rc == 0)
                    rc = ro;
            }
        }
    }

    if (ro == 0 && !negotiated) {
        self->pos = self->_tfPos = 0;

        if (self->_resume) {
            ro = KFileSetSize(self->_tf, 0);
            if (ro != 0)
                TFKill(self, ro, "Cannot SetSize(prf)");
        }
    }
    
    if (rc == 0) {
        uint64_t fsize = 0;
        rc = KFileSize(self->file, &fsize);
        DISP_RC2(rc, "Cannot Size", self->name);
        if (rc == 0) {
            if (self->pos < fsize) {
                rc = KFileSetSize(self->file, self->pos);
                DISP_RC2(rc, "Cannot SetSize", self->name);
            }
            else if (self->pos > fsize)
                self->pos = fsize;
        }
    }

    if (rc == 0 && self->pos > 0)
        STSMSG(STS_TOP, (
            "   Continue download of '%s' from %lu", name, self->pos));

#ifdef DEBUGGING
    OUTMSG(("%s: start from %lu\n", __FUNCTION__, self->pos));
#endif

    return rc;
}

bool PrfOutFileIsLoaded(const PrfOutFile * self) {
    assert(self);

    if (self->_loaded)
        return true;
    else
        return false;
}

rc_t PrfOutFileCommitTry(PrfOutFile * self) {
    return PrfOutFileCommit(self, false);
}

rc_t PrfOutFileCommitDo(PrfOutFile * self) {
    return PrfOutFileCommit(self, true);
}

rc_t PrfOutFileClose(PrfOutFile * self, bool success) {
    rc_t rc = 0, r2 = 0;

    assert(self);

    KFileRelease(self->_tf);
    self->_tf = NULL;

#ifndef DEBUGGINGG
    if (success)
        rc = TFRm(self);
#endif

    RELEASE(String, self->cache);
    RELEASE(KFile, self->file);
    RELEASE(KDirectory, self->_dir);

    r2 = KDataBufferWhack(&self->_buf);
    if (rc == 0 && r2 != 0)
        rc = r2;

    return rc;
}

/******************************************************************************/
/* add
prn
prb
md5sum check*/