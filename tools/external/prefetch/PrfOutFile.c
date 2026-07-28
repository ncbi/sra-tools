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

#define EXT_1   ".pr"
#define EXT_BIN ".prf"
#define EXT_TXT ".prt"

static const char * TFExt(const PrfOutFile * self) {
    switch (self->_tfType) {
    case eTextual:
        return EXT_TXT;
    case eBinEol:
        return ".prb";
    case eBin8:
        return EXT_BIN;
    default:
        assert(0);
        return "";
    }
}

static bool TFExist(PrfOutFile * self) {
    assert(self);

    if (self->cache == NULL)
        return false;

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

static rc_t TFRmEmpty(PrfOutFile * self) {
    if (TFExist(self)) {
        const KFile * f = NULL;
        uint64_t size = 0;
        rc_t rc = KDirectoryOpenFileRead(self->_dir, &f,
            "%.*s%s", self->cache->size, self->cache->addr, TFExt(self));
        if (rc == 0)
            rc = KFileSize(f, &size);
        KFileRelease(f);
        if (rc == 0 && size == 0)
            return TFRm(self);
    }

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
        D_PS = GetEnv("NCBI_VDB_PREFETCH_COMMIT_SZ", ~0);
    if (D_TM == 0)
        D_TM = GetEnv("NCBI_VDB_PREFETCH_COMMIT_TM", 5 * 60);

    assert(self);

    if (self->_committed == 0) {
        self->_committed = KTimeStamp();
        return false;
    }
    else if (self->pos - self->_lastPos >= D_PS) {
        STSMSG(STS_DBG, ("committing by PZ: pos=%ld", self->pos));
        return true;
    }
    else if (D_PS != ~0) {
        if (KTimeStamp() - self->_committed >= D_TM) {
            STSMSG(STS_DBG, ("committing by TM: pos=%ld", self->pos));
            return true;
        }
        else
            return false;
    }
    else {
        if (self->pos - self->_lastPos < 16 * 1024 * 1024)
            return false;
        else {
            self->_lastPos = self->pos;
            if (KTimeStamp() - self->_committed >= D_TM) {
                STSMSG(STS_DBG, ("committing by TM: pos=%ld", self->pos));
                return true;
            }
            else
                return false;
        }
    }
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

static rc_t TFPutPosAsBinEol(PrfOutFile * self,
    char * b, size_t sz, size_t * num)
{
    assert(num);

    assert(sz >= sizeof self->pos + 1);
    memmove(b, &self->pos, sizeof self->pos);
    *num = sizeof self->pos;

    b[(*num)++] = '\n';

    return 0;
}

#define MAGIC "NCBIprTr"

static rc_t TFPutPosAsBin8(PrfOutFile * self, uint64_t tfPos,
    char * b, size_t sz, size_t * num)
{
    int i = 0;

    assert(num);

    assert(sz >= sizeof self->pos + 8);

    if (tfPos == 0) {
        i = sizeof MAGIC - 1;
        memmove(b, MAGIC, i);
    }

    memmove(b + i, &self->pos, sizeof self->pos);
    *num = i + sizeof self->pos;

    return 0;
}

static
rc_t TFPutPos(PrfOutFile * self, char * b, size_t sz, size_t * num)
{
    switch (self->_tfType) {
    case eTextual:
        return TFPutPosAsText(self, b, sz, num);
    case eBinEol:
        return TFPutPosAsBinEol(self, b, sz, num);
    case eBin8:
        return TFPutPosAsBin8(self, self->_tfPos, b, sz, num);
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

static void TFGetPosAsBinEol(PrfOutFile * self,
    uint64_t posSize, uint64_t fSize, uint64_t *pPos, uint64_t *pTfPos)
{
    uint64_t pos = 0, prevPos = 0;
    uint64_t first = 0;
    const char * buf = NULL;
    assert(self && pPos && pTfPos);
    for (buf = self->_buf.base, first = 0; first < posSize; ) {
        if (first + sizeof pos + 1 > posSize) {
            *pPos = *pTfPos = 0;
            break;
        }
        memmove(&pos, buf + first, sizeof pos);
        first += sizeof pos;
        if (buf[first] != '\n') {
            *pPos = *pTfPos = 0;
            break;
        }
        ++first;
        if (pos == fSize) {
            *pPos = pos;
            *pTfPos = first;
            break;
        }
        else if (pos > fSize) {
            *pPos = prevPos;
            if (first < sizeof pos + 1)
                *pTfPos = 0;
            else
                *pTfPos = first - sizeof pos - 1;
            break;
        }
        else
            prevPos = pos;
    }
}

static rc_t TFGetPosAsBin8(PrfOutFile * self,
    uint64_t posSize, uint64_t fSize, uint64_t *pPos, uint64_t *pTfPos)
{
    uint64_t pos = 0, prevPos = 0;
    uint64_t first = 0;
    const char * buf = NULL;
    bool found = false;

    assert(self && pPos && pTfPos);

    buf = self->_buf.base;
    if (posSize < sizeof MAGIC - 1) {
        *pPos = *pTfPos = 0;
        return RC(rcExe, rcFile, rcReading, rcFile, rcInsufficient);
    }

    if (string_cmp(buf, sizeof MAGIC - 1, MAGIC, sizeof MAGIC - 1,
        sizeof MAGIC - 1) != 0)
    {
        *pPos = *pTfPos = 0;
        return RC(rcExe, rcFile, rcReading, rcData, rcInvalid);
    }

    buf += sizeof MAGIC - 1;
    posSize -= sizeof MAGIC - 1;

    for (first = 0; first < posSize; ) {
        if (first + sizeof pos > posSize) {
            *pPos = prevPos;
            if (first < sizeof pos)
                *pTfPos = 0;
            else
                *pTfPos = first - sizeof pos;
            found = true;
            break;
        }

        memmove(&pos, buf + first, sizeof pos);
        first += sizeof pos;

        if (pos == fSize) {
            *pPos = pos;
            *pTfPos = first;
            found = true;
            break;
        }
        else if (pos > fSize) {
            *pPos = prevPos;
            if (first < sizeof pos)
                *pTfPos = 0;
            else
                *pTfPos = first - sizeof pos;
            found = true;
            break;
        }
        else
            prevPos = pos;
    }

    if (!found) {
        *pPos = prevPos;
        *pTfPos = first;
    }

    *pTfPos += sizeof MAGIC - 1;

    return 0;
}

static rc_t TFGetPos(PrfOutFile * self, uint64_t posSize, uint64_t fSize,
    uint64_t *pPos, uint64_t *pTfPos)
{
    switch (self->_tfType) {
    case eTextual:
        TFGetPosAsText(self, posSize, fSize, pPos, pTfPos);
        break;
    case eBinEol:
        TFGetPosAsBinEol(self, posSize, fSize, pPos, pTfPos); 
        break;
    case eBin8:
        return TFGetPosAsBin8(self, posSize, fSize, pPos, pTfPos);
    default:
        assert(0);
        break;
    }

    return 0;
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

    rc = TFGetPos(self, fsize, origSize, &pos, &tfPos);
    if (rc != 0)
        return rc;
    else
        return TFSetPos(self, pos, tfPos);
}

static rc_t TFNegotiatePos(PrfOutFile * self) {
    rc_t rc = 0;
    uint64_t fsize = 0;

    assert(self);

#ifdef DEBUGGING
    OUTMSG(("%s: %S.prt found: loading...\n", __FUNCTION__, self->cache));
#endif
    STSMSG(STS_DBG, ("loading %S%s", self->cache, TFExt(self)));

    rc = KFileSize(self->file, &fsize);
    if (rc != 0) {
        self->_fatal = true;
        PLOGERR(klogInt,
            (klogInt, rc, "Cannot Size($(arg))", "arg=%s", self->tmpName));
    }
    else {
        rc = TFReadPos(self, fsize);
        if (rc != 0) {
            self->pos = 0;
            KFileSetSize(self->file, 0);
        }
        if (!self->_resume)
            return rc;
    }

    if (!self->_fatal) {
        if (rc == 0 && self->pos > 0)
            self->_loaded = true;

        assert(self->pos <= fsize);
        if (self->pos > fsize)
            self->pos = fsize; /* should never happen */
        else if (self->pos < fsize) {
            rc = KFileSetSize(self->file, self->pos);
            if (rc != 0) {
                self->_fatal = true;
                PLOGERR(klogInt, (klogInt, rc,
                    "Cannot SetSize($(arg))", "arg=%s", self->tmpName));
            }
        }
    }

    if(self->_fatal)
        STSMSG(STS_DBG, ("failed to load %S%s",self->cache, TFExt(self)));
    else
        STSMSG(STS_DBG, ("loaded %S%s: fsize=%lu, starting from %lu",
            self->cache, TFExt(self), fsize, self->pos));
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

    STSMSG(STS_DBG, ("opening %s", self->tmpName));

    rc = KDirectoryOpenFileWrite(self->_dir,
        &self->file, true, "%s", self->tmpName);
    if (rc != 0) {
        self->_fatal = true;
        PLOGERR(klogInt, (klogInt, rc,
            "Cannot OpenFileWrite($(arg))", "arg=%s", self->tmpName));
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
            PLOGERR(klogInt, (klogInt, rc,
                "Cannot Release($(arg))", "arg=%s", self->tmpName));
        }
        else
            rc = PrfOutFileOpenWrite(self);

        if (rc == 0) {
            rc = KFileSize(self->file, &size);
            if (rc != 0) {
                self->_fatal = true;
                PLOGERR(klogInt, (klogInt, rc,
                    "Cannot Size($(arg))", "arg=%s", self->tmpName));
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

static
rc_t ConvertBinToTxt(const uint8_t * in, uint64_t sz, KFile * out)
{
    rc_t rc = 0;
    uint64_t from = 0, to = 0;
    size_t num_writ = 0;

    if (sz < 8)
        return KFileWrite(out, to, in, sz, &num_writ);

    rc = KFileWrite(out, to, in, 8, &num_writ);
    to += num_writ;

    if (rc == 0) {
        rc = KFileWrite(out, to, "\n", 1, &num_writ);
        to += num_writ;
    }

    for (from = 8; rc == 0 && from < sz;) {
        union {
            uint64_t u;
            uint8_t b[8];
        } n;
        char buf[128] = "";
        int i = 0;

        for (i = 0, n.u = 0; rc == 0 && i < 8 && from < sz; ++i, ++from)
            n.b[i] = in[from];

        rc = string_printf(buf, sizeof buf, &num_writ, "%lu\n", n.u);

        if (rc == 0) {
            rc = KFileWrite(out, to, buf, num_writ, &num_writ);
            to += num_writ;
        }
    }

    return rc;
}
static rc_t ConvertTxtToBin(const char * in, uint64_t sz, KFile * out) {
    rc_t rc = 0;
    uint64_t from = 0, to = 0;
    size_t num_writ = 0;

    if (sz < 9)
        return KFileWrite(out, to, in, sz, &num_writ);

    rc = KFileWrite(out, to, in + from, 8, &num_writ);
    to += num_writ;

    for (from = 9; rc == 0 && from < sz;) {
        bool truncate = false;
        int len = 0;
        union {
            uint64_t u;
            uint8_t b[8];
        } n;

        if (in[from] == '0')
            truncate = true;
        for (n.u = 0, len = 0; rc == 0 && from < sz && in[from] != '\n';
            ++from, ++len)
        {
            if (in[from] < '0' || in[from] > '9')
                return RC(rcExe, rcFile, rcReading, rcData, rcInvalid);
            n.u = n.u * 10 + in[from] - '0';
        }
        ++from;

        if (rc == 0) {
            if (truncate) {
                int i = 0;
                for (i = 0; i < len; ++i, ++to)
                    rc = KFileWrite(out, to, &n.b[i], 1, NULL);
            }
            else {
                rc = KFileWrite(out, to, &n.u, sizeof n.u, &num_writ);
                to += num_writ;
            }
        }
    }

    return rc;
}

static
rc_t Convert(KDirectory * dir, const String * from, bool fromBin)
{
    rc_t rc = 0, r2 = 0;
    const KFile * in = NULL;
    KFile * out = NULL;
    uint64_t fsize = 0;

    KDataBuffer buf;
    const String * to = NULL;

    memset(&buf, 0, sizeof buf);
    buf.elem_bits = 8;
    rc = StringCopy(&to, from);

    if (rc == 0) {
        assert(to);
        if (fromBin)
            ((char*)to->addr)[to->size - 1] = 't';
        else
            ((char*)to->addr)[to->size - 1] = 'f';
        OUTMSG(("%S -> %S\n", from, to));
    }

    if (rc == 0)
        rc = KDirectoryOpenFileRead(dir, &in, "%s", from->addr);

    if (rc == 0 && KDirectory_Exist(dir, to, ""))
        rc = KDirectoryRemove(dir, false, "%s", to->addr);

    if (rc == 0)
        rc = KDirectoryCreateFile(dir, &out, false,
            0664, kcmInit | kcmParents, "%s", to->addr);

    if (rc == 0)
        rc = KFileSize(in, &fsize);

    if (rc == 0)
        rc = KDataBufferResize(&buf, fsize);

    if (rc == 0)
        rc = KFileReadExactly(in, 0, buf.base, fsize);

    RELEASE(KFile, in);

    if (rc == 0) {
        if (fromBin)
            rc = ConvertBinToTxt(buf.base, fsize, out);
        else
            rc = ConvertTxtToBin(buf.base, fsize, out);
    }
    
    RELEASE(KFile, out);
    RELEASE(String, to);

    r2 = KDataBufferWhack(&buf);
    if (rc == 0 && r2 != 0)
        rc = r2;

    return rc;
}

rc_t PrfOutFileInit(PrfOutFile * self, bool resume,
    const char * name, bool vdbcache)
{
    rc_t rc = 0;

    assert(self);

    memset(self, 0, sizeof *self);

    self->_buf.elem_bits = 8;
    self->_tfType = eBin8;
    self->_resume = resume;
    self->_name = name; /* don't free ! */
    self->_vdbcache = vdbcache;

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

    rc = KDirectory_MkTmpName(self->_dir, cache,
        self->tmpName, sizeof self->tmpName);
    if (rc == 0) {
        rc = StringCopy(&self->cache, cache);
        return rc;
    }
    else
        return rc;
}

rc_t PrfOutFileOpen(PrfOutFile * self, bool force) {
    rc_t rc = 0;
    bool negotiated = false;

    rc_t ro = TFOpen(self, force);
    if (ro != 0)
        TFKill(self, ro, "Cannot open TF");

    assert(self && self->cache);

    if (KDirectoryPathType(self->_dir, "%s", self->tmpName)
        == kptNotFound)
    {
#ifdef DEBUGGING
        OUTMSG(("%s: %s not found: creating...\n",
            __FUNCTION__, self->tmpName));
#endif
        STSMSG(STS_DBG, ("%s not found: creating...", self->tmpName));

        rc = KDirectoryCreateFile(self->_dir, &self->file,
            false, 0664, kcmInit | kcmParents, "%s", self->tmpName);
        if (rc != 0)
            PLOGERR(klogInt, (klogInt, ro, "Cannot CreateFile($(arg))",
                "arg=%s", self->tmpName));
    }
    else {
        rc = PrfOutFileOpenWrite(self);
        if (rc == 0) {
            if (force || !self->_resume) {
                self->pos = 0;
                if (force)
                    STSMSG(STS_DBG, ("forced to ignore transaction file"));
                else
                    STSMSG(STS_DBG, (
                        "ignoring transaction file by command line option"));
            }
            else if (ro == 0) {
                ro = TFNegotiatePos(self);
                if (ro == 0)
                    negotiated = true;
                else if (self->_fatal && rc == 0)
                    rc = ro;
            }
        }
    }

    if (rc == 0 && ro == 0 && !negotiated) {
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
        DISP_RC2(rc, "Cannot Size", self->tmpName);
        if (rc == 0) {
            if (self->pos < fsize) {
                rc = KFileSetSize(self->file, self->pos);
                DISP_RC2(rc, "Cannot SetSize", self->tmpName);
            }
            else if (self->pos > fsize)
                self->pos = fsize;
        }
    }

    if (rc == 0 && self->pos > 0) {
        STSMSG(STS_TOP, ("   Continue download of '%s%s' from %lu",
            self->_name, self->_vdbcache ? ".vdbcache" : "", self->pos));
        self->info.info = ePIResumed;
        self->info.pos = self->pos;
    }

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
    if (self->_resume) {
        STSMSG(STS_DBG, ("committing on exit: pos=%ld", self->pos));
        return PrfOutFileCommit(self, true);
    }
    else
        return 0;
}

rc_t PrfOutFileClose(PrfOutFile * self) {
    rc_t rc = 0, r2 = 0;

    assert(self);

    KFileRelease(self->_tf);
    self->_tf = NULL;

    RELEASE(KFile, self->file);

    r2 = KDataBufferWhack(&self->_buf);
    if (rc == 0 && r2 != 0)
        rc = r2;

    return rc;
}

rc_t PrfOutFileWhack(PrfOutFile * self, bool success) {
    rc_t rc = 0;

#ifndef DEBUGGINGG
    if (success && !self->invalid)
        rc = TFRm(self);
    else if (!success && !self->invalid)
        rc = TFRmEmpty(self);
#endif

    RELEASE(String, self->cache);
    RELEASE(KDirectory, self->_dir);

    return rc;
}

rc_t PrfOutFileConvert(KDirectory * dir, const char * path,
    bool * recognized)
{
    assert(recognized);
    *recognized = false;

    if (path == NULL)
        return 0;

    else {
        String s;
        StringInitCString(&s, path);
        if (s.size < sizeof EXT_BIN)
            return 0;

        if (string_cmp(s.addr + s.size - sizeof EXT_1, sizeof EXT_1 - 1,
            EXT_1, sizeof EXT_1 - 1, sizeof EXT_1 - 1) != 0)
        {
            return 0;
        }

        switch (s.addr[s.size - 1]) {
        case 'f':
            *recognized = true;
            return Convert(dir, &s, true);
        case 't':
            *recognized = true;
            return Convert(dir, &s, false);
        default:
            return 0;
        }
    }
}

const char * PrfOutFileMkLog(const PrfOutFile * self) {
    rc_t rc = 0;
    static char c[99] = "";
    char * what = "";
    assert(self);
    switch (self->info.info) {
    case ePIStreamed: what = "streamed"; break;
    case ePIFiled   : what = "loaded"  ; break;
    case ePIResumed : what = "resumed" ; break;
    default         : assert(0);
    }
    rc = string_printf(c, sizeof c, NULL, "%lu bytes were %s from %lu",
        self->pos, what, self->info.pos);
    if (rc == 0)
        return c;
    LOGERR(klogInt, rc, "Cannot PrfInfoMkLog");
    return "";
}

/******************************************************************************/
