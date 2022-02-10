#ifndef _Included_py_ngs_ncbi_Manager
#define _Included_py_ngs_ncbi_Manager
#ifdef __cplusplus
extern "C" {
#endif

#include "py_ngs_defs.h"
#include <stddef.h>

LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_SetAppVersionString(char const* app_version, char* pStrError, size_t nStrErrorBufferSize);
LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_GetVersion(char const** pRet, char* pStrError, size_t nStrErrorBufferSize);
LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_IsValid(char const* spec, int* pRet, char* pStrError, size_t nStrErrorBufferSize);
LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_ReadCollectionMake(char const* spec, void** ppReadCollection, char* pStrError, size_t nStrErrorBufferSize);
LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_ReferenceSequenceMake(char const* spec, void** ppReadCollection, char* pStrError, size_t nStrErrorBufferSize);
/*
These functions are not needed:
*We don't export yet another string from engine

LIB_EXPORT PY_RES_TYPE PY_NGS_Engine_RefcountRelease(void* pRefcount, char* pStrError, size_t nStrErrorBufferSize);
PY_RES_TYPE PY_NGS_Engine_StringData(void const* pNGSString, char const** pRetBufPtr);
PY_RES_TYPE PY_NGS_Engine_StringSize(void const* pNGSString, size_t* pRetSize);
*/

#ifdef __cplusplus
}
#endif
#endif
