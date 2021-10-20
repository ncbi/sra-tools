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

from ctypes import byref, c_uint32, c_int64, c_uint64, c_double
from . import NGS

from .String import NGS_String, NGS_RawString, getNGSString, getNGSValue
from .Refcount import Refcount

# Statistical data container

class Statistics(Refcount):
    none   = 0
    string = 1
    int64  = 2
    uint64 = 3
    real   = 4

    def getValueType(self, path):
        ret = c_uint32()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_StatisticsGetValueType(self.ref, path, byref(ret), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()
        
        return ret.value

    def getAsString(self, path):
        """
        :param: path is hierarchical path to value node
        :returns: textual representation of value
        :throws: ErrorMsg if path not found or value cannot be converted
        """
        ngs_str_err = NGS_RawString()
        try:
            ngs_str_ret = NGS_String()
            try:
                res = NGS.lib_manager.PY_NGS_StatisticsGetAsString(self.ref, path, byref(ngs_str_ret.ref), byref(ngs_str_err.ref))
                return ngs_str_ret.getPyString()
            finally:
                ngs_str_ret.close()
        finally:
            ngs_str_err.close()

    def getAsI64(self, path):
        """
        :param: path is hierarchical path to value node
        :returns: a signed 64-bit integer
        :throws: ErrorMsg if path not found or value cannot be converted
        """
        ret = c_int64()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_StatisticsGetAsI64(self.ref, path, byref(ret), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()
        
        return ret.value

    def getAsU64(self, path):
        """
        :param: path is hierarchical path to value node
        :returns: a non-negative 64-bit integer
        :throws: ErrorMsg if path not found or value cannot be converted
        """
        ret = c_uint64()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_StatisticsGetAsU64(self.ref, path.encode("UTF-8"), byref(ret), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()
        
        return ret.value

    def getAsDouble(self, path):
        """
        :param: path is hierarchical path to value node
        :returns: a 64-bit floating point
        :throws: ErrorMsg if path not found or value cannot be converted
        """
        ret = c_double()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_StatisticsGetAsDouble(self.ref, path, byref(ret), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()
        
        return ret.value

    def nextPath(self, path):
        """advance to next path in container
        :param: path is null or empty to request first path, or a valid path string
        :returns: null if no more paths, or a valid path string
        """
        ngs_str_err = NGS_RawString()
        try:
            ngs_str_ret = NGS_String()
            try:
                res = NGS.lib_manager.PY_NGS_StatisticsGetNextPath(self.ref, path, byref(ngs_str_ret.ref), byref(ngs_str_err.ref))
                return ngs_str_ret.getPyString()
            finally:
                ngs_str_ret.close()
        finally:
            ngs_str_err.close()
