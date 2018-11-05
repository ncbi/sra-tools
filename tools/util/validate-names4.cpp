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
* =========================================================================== */

#include <kapp/main.h> /* Args */
#include <kfs/directory.h> /* KDirectoryNativeDir */
#include <kfs/file.h> /* KFileRead */
#include "../../../ncbi-vdb/libs/vfs/remote-services.c" /* KSrvResponseSetR4  */

const char UsageDefaultName[] = "validate-names4";
rc_t CC UsageSummary ( const char * progname ) { return 0; }
rc_t CC Usage ( const Args * args ) { return 0; }

#define T_ALIAS  "t"
#define T_OPTION "type"
static const char * T_USAGE [] = { "Type: file, args", NULL };

static OptDef OPTIONS [] = {
/*        name    alias   fkt  usage-txt cnt needs value required */
    { T_OPTION, T_ALIAS, NULL, T_USAGE ,  1, true      , false },
};

static rc_t _Response4Print(const Response4 * self) {
    rc_t rc = 0;
    KService * service = NULL;
    uint32_t l = 0, i = 0;

    rc = KServiceMake(&service);

    if (rc == 0)
        rc = KSrvResponseSetR4(service->resp.list, self);

    if (rc == 0) {
        l = KSrvResponseLength(service->resp.list);
        OUTMSG(("Response length: %u\n", l));
    }

    for (i = 0; i < l && rc == 0; ++ i) {
        const KSrvRespObj * obj = NULL;
        KSrvRespObjIterator * it = NULL;
        KSrvRespFile * file = NULL;
        KSrvRespFileIterator * fi = NULL;
        const VPath * path = NULL;
        const char * acc = NULL;
        uint32_t id = 0;
        const char * itemClass = NULL;

        rc = KSrvResponseGetObjByIdx(service->resp.list, i, &obj);
        if (rc == 0)
            rc = KSrvRespObjGetAccOrId(obj, &acc, &id);
        if (rc == 0)
            if ( acc != NULL )
                OUTMSG(("Object %u/%u: '%s':\n", i + 1, l, acc));
        if (rc == 0)
            rc = KSrvRespObjMakeIterator(obj, &it);
        if (rc == 0)
            rc = KSrvRespObjIteratorNextFile(it, &file);
        if (rc == 0)
            rc = KSrvRespFileGetClass(file, & itemClass);
        if (rc == 0) {
            rc = KSrvRespFileGetAccOrId(file, &acc, &id);
            if (rc == 0)
                if (acc != NULL)
                    OUTMSG(("  '%s' '%s'\n", itemClass, acc));
        }
        if (rc == 0)
            rc = KSrvRespFileMakeIterator(file, &fi);
        if (rc == 0)
            rc = KSrvRespFileIteratorNextPath(fi, & path);
        OUTMSG(("\n"));

        RELEASE(VPath, path);
        RELEASE(KSrvRespFileIterator, fi);
        RELEASE(KSrvRespFile, file);
        RELEASE(KSrvRespObjIterator, it);
        RELEASE(KSrvRespObj, obj);
    }

    RELEASE(KService, service );

    return rc;
}

rc_t CC KMain ( int argc, char * argv [] ) {
    rc_t rc = 0; Args * args = NULL; KDirectory * dir = NULL;
    uint32_t argCount = 0; uint32_t i = 0;
    rc = ArgsMakeAndHandle ( & args, argc, argv, 1,
                             OPTIONS, sizeof OPTIONS / sizeof OPTIONS [ 0 ] );
    if ( rc == 0 )
        rc = ArgsParamCount ( args, & argCount );
    for ( i = 0; i < argCount; ++ i ) {
        const KFile * file = NULL;
        const char * value = NULL;
        rc = ArgsParamValue ( args, i, ( const void ** ) & value );
        if ( rc == 0 && dir == NULL )
            rc = KDirectoryNativeDir ( & dir );
        if ( rc == 0 ) {
            Response4 * response = NULL;
            char buffer [ 60000 ] = ""; size_t num_read = 0;
            rc = KDirectoryOpenFileRead ( dir, & file, value );
            if ( rc != 0 )
                PLOGERR ( klogErr,
                    ( klogErr, rc, "Cannot open '$(fil)'", "fil=%s", value ) );
            else {
                rc = KFileRead ( file, 0, buffer, sizeof buffer, & num_read );
                if ( rc == 0 && num_read == sizeof buffer )
                    rc = RC
                        ( rcExe, rcFile, rcReading, rcBuffer, rcInsufficient );
            }
            if ( rc == 0 )
                rc = Response4Make ( & response, buffer );
            if (rc == 0)
                OUTMSG(("File '%s' conforms "
                    "to \"Name Resolver Protocol 4.0\"\n", value));
            else
                OUTMSG(("File '%s' fails to conform "
                    "to \"Name Resolver Protocol 4.0\"\n", value));
            if (rc == 0)
                rc = _Response4Print(response);
            RELEASE ( Response4, response );
        }
        RELEASE ( KFile, file ); 
    }
    RELEASE ( KDirectory, dir ); RELEASE ( Args, args ); return rc;
}
