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

#import os

from ctypes import c_char_p, c_int, byref, create_string_buffer, string_at

from .LibManager import LibManager # TODO probably, LibManager should not be the part of ngs package and be specific to engine

PY_RES_OK    = 0

class NGS:
    lib_manager = LibManager()

    @staticmethod
    def setAppVersionString(app_version):
        NGS.lib_manager.initialize_ngs_bindings()
        
        ERROR_BUFFER_SIZE = 4096
        str_err = create_string_buffer(ERROR_BUFFER_SIZE)
        res = NGS.lib_manager.PY_NGS_Engine_SetAppVersionString(app_version.encode(), str_err, len(str_err))
        if res != PY_RES_OK:
            raise ErrorMsg(str_err.value)

            
    @staticmethod
    def getVersion_impl():
        if NGS.lib_manager.PY_NGS_Engine_GetVersion is None:
            return "0"

        from .String import NGS_RawString
        
        ret = c_char_p()
        ERROR_BUFFER_SIZE = 4096
        str_err = create_string_buffer(ERROR_BUFFER_SIZE)
        res = NGS.lib_manager.PY_NGS_Engine_GetVersion(byref(ret), str_err, len(str_err))
        if res != PY_RES_OK:
            raise ErrorMsg(str_err.value)
        
        s = string_at(ret.value).decode()
        if isinstance(s, bytes):
            s = s.decode(encoding='UTF-8')
        return s
        
        

    @staticmethod
    def getVersion():
        NGS.lib_manager.initialize_ngs_bindings()
        return NGS.getVersion_impl()


    @staticmethod
    def getPackageVersion_impl():
        from .Package import Package
        return Package.getPackageVersion()

    @staticmethod
    def getPackageVersion():
        NGS.lib_manager.initialize_ngs_bindings()
        from .Package import Package
        return Package.getPackageVersion()

    @staticmethod
    def isValid(spec):
        from .String import NGS_RawString
        NGS.lib_manager.initialize_ngs_bindings()
        
        ret = c_int()
        ERROR_BUFFER_SIZE = 4096
        str_err = create_string_buffer(ERROR_BUFFER_SIZE)
        res = NGS.lib_manager.PY_NGS_Engine_IsValid(spec.encode(), byref(ret), str_err, len(str_err))
        if res != PY_RES_OK:
            raise ErrorMsg(str_err.value)
        
        return bool(ret.value)

            
    @staticmethod
    def openReadCollection(spec):
        NGS.lib_manager.initialize_ngs_bindings()
    
        from .ReadCollection import openReadCollection  # entry point - adding name to ngs package global namespace
        return openReadCollection(spec)

    @staticmethod
    def openReferenceSequence(spec):
        NGS.lib_manager.initialize_ngs_bindings()
    
        from .ReferenceSequence import openReferenceSequence  # entry point - adding name to ngs package global namespace
        return openReferenceSequence(spec)
        
    @staticmethod
    def checkLibVersions():
        from . LibChecker import check_versions
        return check_versions()
