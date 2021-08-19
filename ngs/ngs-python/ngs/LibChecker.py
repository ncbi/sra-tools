# ===========================================================================
# 
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
# 
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
# 
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
# 
#  Please cite the author in any work or product based on this material.
# 
# ===========================================================================
# 
# 

def check_versions():
    from ctypes import POINTER, c_char_p, c_void_p, c_size_t, c_char

    from . LibManager import LibManager, load_library, should_download_library, version_tuple
    from . LibManager import get_library_version_tuple_remote, load_updated_library
    from . import NGS

    if not should_download_library():
        return 0
    
    libname_engine = "ncbi-vdb"
    libname_sdk = "ngs-sdk"
    
    NGS.lib_manager.c_lib_engine = load_library(libname_engine, do_download=False, silent=True)
    NGS.lib_manager.c_lib_sdk = load_library(libname_sdk, do_download=False, silent=True)
    
    ret = 0

    if NGS.lib_manager.c_lib_engine is None:
        ret = ret | 1
    else:
        try:
            NGS.lib_manager._bind(NGS.lib_manager.c_lib_engine, "PY_NGS_Engine_GetVersion", [POINTER(c_char_p), POINTER(c_char), c_size_t], None)
        except AttributeError:
            NGS.lib_manager.setattr("PY_NGS_Engine_GetVersion", None)

        if version_tuple( NGS.getVersion_impl() ) < get_library_version_tuple_remote(libname_engine):
            ret = ret | 1
        
    if NGS.lib_manager.c_lib_sdk is None:
        ret = ret | 2
    else:
        try:
            NGS.lib_manager.bind_sdk("PY_NGS_PackageGetPackageVersion", [POINTER(c_void_p), POINTER(c_void_p)])
            # Functions needed to make a call to NGS version functions
            NGS.lib_manager._bind(NGS.lib_manager.c_lib_sdk, "PY_NGS_RawStringRelease", [c_void_p, POINTER(c_void_p)], None)
        except AttributeError:
            NGS.lib_manager.setattr("PY_NGS_PackageGetPackageVersion", None)

        if version_tuple( NGS.getPackageVersion_impl() ) < get_library_version_tuple_remote(libname_sdk):
            ret = ret | 2

    return ret

