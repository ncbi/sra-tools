#include "py_Manager.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <kfc/rc.h>

#include <kfc/rsrc-global.h>

#include <klib/text.h>

#include "NGS_String.h"
#include "NGS_ReadCollection.h"
#include "NGS_ReferenceSequence.h"
#include "NGS_Refcount.h"

#include <kdb/manager.h> /* KDBManager */
#include <kns/manager.h>
#include <klib/ncbi-vdb-version.h> /* GetPackageVersion */
#include "../klib/release-vers.h"

#include <vfs/manager.h> /* VFSManager */
#include <vfs/path.h> /* VPath */

#include <assert.h>
#include <string.h>

static PY_RES_TYPE NGSErrorHandler(ctx_t ctx, char* pStrError, size_t nStrErrorBufferSize)
{
    char const* pszErrorDesc = WHAT();
    size_t source_size = string_size ( pszErrorDesc );
    size_t copied;
    assert(pStrError);
    copied = string_copy( pStrError, nStrErrorBufferSize, pszErrorDesc, source_size );
    if ( copied == nStrErrorBufferSize ) // no space for 0-terminator
    {
        pStrError [ copied - 1 ] = 0;
    }
    CLEAR();
    return PY_RES_ERROR; /* TODO: return error (exception) type */
}


static bool have_user_version_string;

static void set_app_version_string ( const char * app_version )
{
    // get a KNSManager
    KNSManager * kns;
    rc_t rc = KNSManagerMake ( & kns );
    if ( rc == 0 )
    {
        have_user_version_string = true;
        KNSManagerSetUserAgent ( kns, "ncbi-ngs.%V %s", RELEASE_VERS, app_version );
        KNSManagerRelease ( kns );
    }
}

LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_SetAppVersionString(char const* app_version, char* pStrError, size_t nStrErrorBufferSize)
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcMgr, rcUpdating );

    set_app_version_string ( app_version );

    if (FAILED())
    {
        return NGSErrorHandler(ctx, pStrError, nStrErrorBufferSize);
    }

    CLEAR();
    return PY_RES_OK;
}

LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_GetVersion(char const** pRet, char* pStrError, size_t nStrErrorBufferSize)
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcMgr, rcUpdating );

    char const* ret = GetPackageVersion();

    if (FAILED())
    {
        return NGSErrorHandler(ctx, pStrError, nStrErrorBufferSize);
    }

    assert ( pRet != NULL );
    *pRet = ret;

    CLEAR();
    return PY_RES_OK;
}

LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_IsValid(char const* spec, int* pRet, char* pStrError, size_t nStrErrorBufferSize)
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcMgr, rcAccessing );

    int ret = false;

    VFSManager * vfs = NULL;
    rc_t rc = VFSManagerMake ( & vfs );

    if ( rc == 0 )
    {
        VPath * path = NULL;
        rc = VFSManagerMakePath ( vfs, & path, spec );

        if ( rc == 0 )
        {
            const KDBManager * kdb = NULL;
            rc = KDBManagerMakeRead ( & kdb, NULL );

            if ( rc == 0 )
            {
                KPathType t = KDBManagerPathTypeVP ( kdb, path );
                if (t == kptDatabase || t == kptTable)
                {
                    ret = true;
                }

                KDBManagerRelease ( kdb );
                kdb = NULL;
            }

            VPathRelease ( path );
            path = NULL;
        }

        VFSManagerRelease ( vfs );
        vfs = NULL;
    }

    assert ( pRet != NULL );

    *pRet = ret;

    CLEAR();
    return PY_RES_OK;
}


LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_ReadCollectionMake(char const* spec, void** ppReadCollection, char* pStrError, size_t nStrErrorBufferSize)
{
    HYBRID_FUNC_ENTRY(rcSRA, rcMgr, rcConstructing);

    void* pRet = NULL;

    if ( ! have_user_version_string )
        set_app_version_string ( "ncbi-ngs: unknown-application" );

    pRet = (void*)NGS_ReadCollectionMake(ctx, spec);

    if (FAILED())
    {
        return NGSErrorHandler(ctx, pStrError, nStrErrorBufferSize);
    }

    assert(pRet != NULL);
    assert(ppReadCollection != NULL);

    *ppReadCollection = pRet;

    CLEAR();
    return PY_RES_OK;
}

LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_ReferenceSequenceMake(char const* spec, void** ppReadCollection, char* pStrError, size_t nStrErrorBufferSize)
{
    HYBRID_FUNC_ENTRY(rcSRA, rcMgr, rcConstructing);

    void* pRet = NULL;

    if ( ! have_user_version_string )
        set_app_version_string ( "ncbi-ngs: unknown-application" );

    pRet = (void*)NGS_ReferenceSequenceMake(ctx, spec);

    if (FAILED())
    {
        return NGSErrorHandler(ctx, pStrError, nStrErrorBufferSize);
    }

    assert(pRet != NULL);
    assert(ppReadCollection != NULL);

    *ppReadCollection = pRet;

    CLEAR();
    return PY_RES_OK;
}

#if 0
PY_RES_TYPE PY_NGS_Engine_RefcountRelease(void* pRefcount, char* pStrError, size_t nStrErrorBufferSize)
{
    HYBRID_FUNC_ENTRY(rcSRA, rcRefcount, rcReleasing);

    NGS_RefcountRelease((NGS_Refcount*)pRefcount, ctx);

    if (FAILED())
    {
        return NGSErrorHandler(ctx, pStrError, nStrErrorBufferSize);
    }

    CLEAR();
    return PY_RES_OK;
}

PY_RES_TYPE PY_NGS_Engine_StringData(void const* pNGSString, char const** pRetBufPtr/* TODO: add new error return? */)
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcString, rcAccessing );
    assert(pRetBufPtr);
    *pRetBufPtr = NGS_StringData(pNGSString, ctx);
    return PY_RES_OK;
}

PY_RES_TYPE PY_NGS_Engine_StringSize(void const* pNGSString, size_t* pRetSize/*TODO: add new error return?*/)
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcString, rcAccessing );
    assert(pRetSize);
    *pRetSize = NGS_StringSize(pNGSString, ctx);
    return PY_RES_OK;
}
#endif



