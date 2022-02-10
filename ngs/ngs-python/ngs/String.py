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

from ctypes import c_void_p, c_size_t, c_char_p, string_at, byref, cast
from . import NGS

class NGS_String:
    """Refcounted object to work with StringItf objects imported from ngs-sdk
    
    Provides read-only operaton getPyString: returns python str object constructed from StringItf
    """
    
    def __init__(self):
        self.init_members_with_null()
    
    def __del__(self):
        self.close()
            
    def __enter__(self):
        return self
    
    def __exit__(self, t, value, traceback):
        self.close()

    def close(self):
        if self.ref:
            from .Refcount import RefcountRelease
            RefcountRelease(self.ref)
            self.init_members_with_null()
            
    def init_members_with_null(self):
        self.ref = c_void_p()
        self.size = c_size_t()
        self.data = c_char_p()        

    def getData(self):
        if not self.ref:
            pass # TODO: process error?
        res = NGS.lib_manager.PY_NGS_StringGetData(self.ref, byref(self.data))
        # TODO: process error?
        return self.data.value
        
    def getSize(self):
        if not self.ref:
            pass # TODO: process error?
        res = NGS.lib_manager.PY_NGS_StringGetSize(self.ref, byref(self.size))
        # TODO: process error?
        return self.size.value
        
    def getPyString(self):
        ret = string_at(self.getData(), self.getSize()).decode()
        if isinstance(ret, bytes):
            ret = ret.decode(encoding='UTF-8')        
        return ret


class NGS_RawString:
    """object to work with raw string (char*) objects imported from ngs-sdk
    
    Provides read-only operaton getPyString: returns python str object constructed from raw ngs-sdk string
    This class is used for getting error string from ngs-sdk sicne its StringItf class doesn't provide
    interface for NGS string duplication (creating a copy)
    """
    
    def __init__(self):
        self.init_members_with_null()
    
    def __del__(self):
        self.close()
            
    def __enter__(self):
        return self
    
    def __exit__(self, t, value, traceback):
        self.close()

    def close(self):
        if self.ref:
            from .Refcount import RefcountRawStringRelease
            RefcountRawStringRelease(self.ref)
            self.init_members_with_null()
            
    def init_members_with_null(self):
        self.ref = c_void_p()
        
    def ref_to_char_p(self):
        return cast(self.ref, c_char_p)
    
    def getData(self):
        if not self.ref:
            pass # TODO: process error?
        return self.ref_to_char_p().value
        
    def getSize(self):
        if not self.ref:
            pass # TODO: process error?
        return len(self.ref_to_char_p().value)
        
    def getPyString(self):
        ret = string_at(self.getData(), self.getSize()).decode()
        if isinstance(ret, bytes):
            ret = ret.decode(encoding='UTF-8')
        return ret


def getNGSString(self, py_func):
    """Getter that returns a string-attribute for a given NGS-object (Read, Fragment, Alignment etc.)
    
    :param self: python class representing NGS-object (like Read or Fragment)
    :param py_func: PY-function returning NGS_String for a given NGS-object
    :returns: python str object
    :throws: ErrorMsg
    
    :remarks: NGS_String object is automatically released after this function returns
    """
    ngs_str_err = NGS_RawString()
    try:
        ngs_str_seq = NGS_String()
        try:
            res = py_func(self.ref, byref(ngs_str_seq.ref), byref(ngs_str_err.ref))
            #check_res(res, ngs_str_err)
            return ngs_str_seq.getPyString()
        finally:
            ngs_str_seq.close()
    finally:
        ngs_str_err.close()

        
def getNGSValue(self, py_func, value_type):
    """Getter that returns a typed attribute for a given NGS-object (Read, Fragment, Alignment etc.)
    
    :param self: python class representing NGS-object (like Read or Fragment)
    :param py_func: PY-function returning a typed object for a given NGS-object
    :param value_type: c_type of the object to query from 'self'
    :returns: python str object
    :throws: ErrorMsg
    """
    ret = value_type()
    ngs_str_err = NGS_RawString()
    try:
        res = py_func(self.ref, byref(ret), byref(ngs_str_err.ref))
        #check_res(res, ngs_str_err)
    finally:
        ngs_str_err.close()
    
    return ret.value

