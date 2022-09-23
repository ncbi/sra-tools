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

#include <kfc/defs.h> /* rc_t */

/* #define TESTING_FAILURES */

struct PrfMain;

typedef enum {
    eRSJustRetry,
    eRSReopen,
    eRSDecBuf,
    eRSIncTO,
    eRSMax,
} ERetryState;

typedef struct {
    size_t _bsize;
    struct KNSManager * _mgr;
    const struct VPath * _path;
    const struct String * _src;
    bool _isUri;
    const struct KFile ** _f;
    uint64_t _size;

    bool _failed;
    KTime_t _tFailed;
    uint64_t _pos;
    ERetryState _state;
    size_t curSize;
    uint32_t _sleepTO;
} PrfRetrier;

void PrfRetrierInit(PrfRetrier * self, const struct PrfMain * mane,
    const struct VPath * path, const struct String * src, bool isUri,
    const struct KFile ** f, size_t size, uint64_t pos);

void PrfRetrierReset(PrfRetrier * self, uint64_t pos);

rc_t PrfRetrierAgain(PrfRetrier * self, rc_t rc, uint64_t pos);
