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
*=============================================================================*/

#include <kapp/main.h>

#include <vfs/path-priv.h> /* VPathRelease */
#include <vfs/services.h> /* KServiceRelease */

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)

int main(int argc, char * argv[]) {
    Args * args = NULL;
    rc_t rc = ArgsMakeAndHandle(&args, argc, argv, 0);

    uint32_t count = 0;
    if (rc == 0)
        rc = ArgsParamCount(args, &count);

    const char * t = NULL;
    const char * acc = NULL;

    if (rc == 0 && count > 0) {
        rc = ArgsParamValue(args, 0, (const void**)&t);
        if (rc == 0) {
            assert(t);
            if (t[0] == 'l') {
                VPath * path = NULL;
                rc = VFSManagerMakePath(0, &path, "%s/%s.sra", acc, acc);
                if (rc == 0)
                    rc = VPathSaveQuality(path);
                RELEASE(VPath, path);
            }
        }
    }

    if (rc == 0 && count > 1) {
        rc = ArgsParamValue(args, 1, (const void**)&acc);
        if (rc == 0) {
            assert(acc);
        }
    }

    if (rc == 0 && count > 1) {
        KService * service = NULL;
        if (rc == 0)
            rc = KServiceMake(&service);
        if (rc == 0)
            rc = KServiceAddId(service, acc);

        const KSrvResponse * response = NULL;
        if (rc == 0)
            rc = KServiceNamesQuery(service, 0, &response);
        if (rc == 0 && KSrvResponseLength(response) != 1)
            rc = 1;

        const KSrvRespObj * obj = NULL;
        if (rc == 0)
            rc = KSrvResponseGetObjByIdx(response, 0, &obj);

        KSrvRunIterator * ri = NULL;
        if (rc == 0)
            rc = KSrvResponseMakeRunIterator(response, &ri);

        const KSrvRun * run = NULL;
        if (rc == 0)
            rc = KSrvRunIteratorNextRun(ri, &run);

        assert(t);
        const VPath * path = NULL;
        if (rc == 0 && t[0] == 'r')
            rc = KSrvRunQuery(run, NULL, &path, NULL, NULL);
        else if (rc == 0 && t[0] == 'l')
            rc = KSrvRunQuery(run, &path, NULL, NULL, NULL);

        VQuality q = eQualLast;
        if (rc == 0)
            q = VPathGetQuality(path);

        if (rc == 0 && count > 2) {
            const char * e = NULL;
            rc = ArgsParamValue(args, 2, (const void**)&e);

            if (rc == 0) switch (e[0]) {
            case 'd':if (q != eQualDefault) rc = 2; break;
            case 'n':if (q != eQualNo) rc = 3; break;
            case 'f':if (q != eQualFull) rc = 4; break;
            }
        }

        RELEASE(VPath, path);
        RELEASE(KSrvRun, run);
        RELEASE(KSrvRunIterator, ri);
        RELEASE(KSrvRespObj, obj);
        RELEASE(KSrvResponse, response);
        RELEASE(KService, service);
    }

    rc_t r2 = ArgsWhack(args);
    if (r2 != 0 && rc == 0)
        rc = r2;

    return (int)rc;
}
