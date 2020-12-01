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
* ==============================================================================
*
* CSVM Orders
*/

#include "orders.h" /* KDirectory_ReadFile */

#include <kfs/file.h> /* KFileSize */

#include <klib/printf.h> /* string_printf */
#include <klib/rc.h> /* RC */

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

typedef struct CSVMOrder {
    SLList dad;
    int number; /* sequential order number */
    CloudProviderId provider;
    char * email;
    char * workorder;
    EStatus status;
} CSVMOrder;

/* read content of 'path' file into allocated 'aBuffer' of 'aSize' */
rc_t KDirectory_ReadFile(const struct KDirectory * self,
    const char * path, char ** aBuffer, uint64_t * aSize)
{
    rc_t rc = 0;
    const KFile * file = NULL;
    uint64_t pos = 0;
    uint64_t size = 0;
    char * buffer = NULL;

    uint64_t dummy = 0;
    if (aSize == NULL)
        aSize = &dummy;
    *aSize = 0;

    assert(aBuffer);
    *aBuffer = NULL;

    if (rc == 0)
        rc = KDirectoryOpenFileRead(self, &file, "%s", path);

    if (rc == 0)
        rc = KFileSize(file, &size);

    if (rc == 0) {
        buffer = calloc(1, size + 1);
        if (buffer == NULL)
            rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    }

    while (rc == 0 && pos < size) {
        size_t num_read = 0;
        rc = KFileRead(file, pos, buffer + pos, size - pos, &num_read);
        if (num_read == 0)
            break;
        if (rc == 0)
            pos += num_read;
    }

    if (rc == 0) {
        assert(pos == size);
        buffer[pos] = '\0';
    }

    if (rc != 0)
        free(buffer);
    else {
        *aBuffer = buffer;
        *aSize = size;
    }

    RELEASE(KFile, file);

    return rc;
}

void CSVMOrderWhack(CSVMOrder * self) {
    assert(self);

    free(self->email);
    free(self->workorder);

    memset(self, 0, sizeof * self);

    free(self);
}

rc_t CSVMOrderInit(CSVMOrder ** self, const char * email,
    const char * workorder, CloudProviderId provider, EStatus status)
{
    static int number = 0;

    rc_t rc = 0;
    CSVMOrder * p = NULL;

    assert(self && email && workorder);
    *self = NULL;

    p = calloc(1, sizeof *p);
    if (p == NULL)
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);

    p->email = string_dup_measure(email, NULL);
    if (p->email == NULL)
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);

    p->workorder = string_dup_measure(workorder, NULL);
    if (p->workorder == NULL)
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);

    p->provider = provider;
    p->status = status;
    if (status != eRemoved)
        p->number = ++number;
    else
        p->number = 0;

    if (rc == 0)
        *self = p;
    else
        CSVMOrderWhack(p);

    return rc;
}

void CSVMOrderData(const struct CSVMOrder * self,
    EStatus * status, CloudProviderId * provider,
    const char ** email, const char ** workorder, int * number)
{
    assert(self && provider && email && workorder);

    *provider = self->provider;
    *email = self->email;
    *workorder = self->workorder;

    if (status != NULL)
        *status = self->status;

    if (number != NULL)
        *number = self->number;
}

/* update order status */
bool CSVMOrderUpdate(struct CSVMOrder * self, const char * status,
    int * number)
{
    uint32_t m = 0;

    String completed;
    CONST_STRING(&completed, "completed");

    assert(self && number);

    *number = self->number;

    m = string_measure(status, NULL);
    if (m != completed.len)
        return false;
    else if (string_cmp(status, m, completed.addr, m, m) == 0
        && self->status == eSubmitted)
    {
        self->status = eCompleted;
        return true;
    }
    else
        return false;
}

static void CC whackOrder(SLNode * n, void * data)
{   CSVMOrderWhack((CSVMOrder*)n); }

/* Structure to keep orders to be serialized */
typedef struct {
    char * buffer;
    size_t size;
    size_t num_writ;
    char * p;
    rc_t rc;
} CSVMOrdersData;

/* print an Order to be saved to disk */
static rc_t printOrder(char *dst, size_t bsize, size_t *num_writ,
    const CSVMOrder * o)
{
    char c = ' ';
    const char * status = "";

    assert(o);

    switch (o->status) {
    case eSubmitted: status = ""; break;
    case eCompleted: status = "+"; break;
    case eRemoved: status = "*"; break;
    default: assert(0); break;
    }

    if (o->provider == cloud_provider_aws)
        c = 'a';
    else if (o->provider == cloud_provider_gcp)
        c = 'g';

    return string_printf(dst, bsize, num_writ, "%s%c %s %s\n",
        status, c, o->workorder, o->email);
}

static void CC CSVMOrdersToSaveAdd(SLNode * n, void * data) {
    CSVMOrdersData * d = data;
    CSVMOrder * o = (CSVMOrder*)n;
    size_t remaining = 0;
    size_t num_writ = 0;
    assert(d && n);

    if (d->rc != 0)
        return;

    if (d->size == 0) {
        d->size = 4096;
        d->buffer = malloc(d->size);
        if (d->buffer == NULL)
            d->rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    }
    if (d->buffer == NULL)
        return;

    printOrder(NULL, 0, &num_writ, o);
    ++num_writ;
    remaining = d->size - d->num_writ;
    if (remaining <= num_writ) {
        if (num_writ > d->size)
            d->size += num_writ;
        else
            d->size *= 2;
        assert(d->size - d->num_writ > num_writ);
        d->buffer = realloc(d->buffer, d->size);
    }

    if (d->buffer == NULL)
        d->rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    else {
        d->rc = printOrder(d->buffer + d->num_writ,
            d->size - d->num_writ, &num_writ, o);
        if (d->rc == 0)
            d->num_writ += num_writ;
    }
}

/* Serialize CSVMOrders */
static rc_t CSVMOrdersSave(CSVMOrders * self,
    KDirectory * dir, const char * path)
{
    rc_t rc = 0;
    KFile * f = NULL;
    CSVMOrdersData o2s;

    assert(self);

    if (!self->dirty)
        return 0;

    memset(&o2s, 0, sizeof o2s);

    SLListForEach(&self->list, CSVMOrdersToSaveAdd, &o2s);
    rc = o2s.rc;

    if (rc == 0)
        rc = KDirectoryCreateFile(dir, &f, false, 0664, kcmInit | kcmParents,
            path);
    if (rc == 0)
        rc = KFileWriteExactly(f, 0, o2s.buffer, o2s.num_writ);
    if (rc == 0)
        self->dirty = false;

    free(o2s.buffer);
    RELEASE(KFile, f);

    return rc;
}

rc_t CSVMOrdersFini(CSVMOrders * self,
    KDirectory * dir, const char * path)
{
    rc_t rc = 0;

    assert(self);

    rc = CSVMOrdersSave(self, dir, path);
    SLListWhack(&self->list, whackOrder, NULL);

    memset(self, 0, sizeof * self);

    return rc;
}

/* Load 1 row from orders file */
static rc_t CSVMOrdersLoad1(CSVMOrders * self,
    char * buffer, uint64_t size)
{
    rc_t rc = 0;
    CSVMOrder * o = NULL;
    CloudProviderId provider = cloud_provider_none;
    EStatus status = eSubmitted;
    char * email = NULL;
    char * workorder = NULL;
    char * w1 = NULL;

    if (*buffer == '+')
        status = eCompleted;
    else if (*buffer == '*')
        status = eRemoved;
    if (status != eSubmitted) {
        ++buffer;
        --size;
    }

    if (*buffer == 'a')
        provider = cloud_provider_aws;
    else if (*buffer == 'g')
        provider = cloud_provider_gcp;
    else
        return 0;
    ++buffer;
    --size;

    if (*buffer != ' ')
        return 0;
    ++buffer;
    --size;

    w1 = string_chr(buffer, size, ' ');
    if (w1 == NULL)
        return 0;
    workorder = buffer;
    *(w1++) = '\0';
    --size;

    assert(buffer[size] == '\n');
    buffer[size] = '\0';
    email = w1;

    rc = CSVMOrderInit(&o, email, workorder, provider, status);

    if (rc == 0)
        SLListPushTail(&self->list, (SLNode*)o);

    return rc;
}

/* Load orders file buffer */
static rc_t CSVMOrdersLoadAll(CSVMOrders * self,
    char * buffer, int64_t size)
{
    rc_t rc = 0;

    while (rc == 0 && size > 0) {
        uint64_t lsize = 0;
        char * s1 = string_chr(buffer, size, '\n');
        if (s1 == NULL)
            break;

        lsize = s1 - buffer + 1;
        rc = CSVMOrdersLoad1(self, buffer, lsize);

        buffer += lsize;
        size -= lsize;
    }

    return rc;
}

/* Load orders file */
rc_t CSVMOrdersLoad(CSVMOrders * self,
    const KDirectory * dir, const char * path)
{
    rc_t rc = 0;
    char * buffer = 0;
    uint64_t size = 0;

    assert(self);
    memset(self, 0, sizeof * self);

    if (rc == 0)
        rc = KDirectory_ReadFile(dir, path, &buffer, &size);
    if (rc != 0 && GetRCState(rc) == rcNotFound)
        return 0;

    if (rc == 0)
        rc = CSVMOrdersLoadAll(self, buffer, size);

    free(buffer);

    return rc;
}

/* add a new order to orders list */
rc_t CSVMOrdersAdd(CSVMOrders * self, CloudProviderId cld,
    const char * email, const char * workorder, int * number)
{
    CSVMOrder * o = NULL;
    rc_t rc = CSVMOrderInit(&o, email, workorder, cld, eSubmitted);

    assert(number);
    *number = 0;

    if (rc == 0) {
        SLListPushTail(&self->list, (SLNode*)o);
        self->dirty = true;

        assert(o);

        *number = o->number;
    }

    return rc;
}
