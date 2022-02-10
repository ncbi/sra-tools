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

from ctypes import byref, c_int
from . import NGS

from .Refcount import Refcount
from .String import NGS_String, NGS_RawString, getNGSString, getNGSValue


# Represents an NGS biological fragment

class Fragment(Refcount):
    def getFragmentId(self):
        return getNGSString(self, NGS.lib_manager.PY_NGS_FragmentGetFragmentId)
            
    def getFragmentBases(self, offset=0, length=-1):
        """
        :param: offset is zero-based and non-negative
        :param: length must be >= 0
        :returns: sequence bases
        """
        ngs_str_err = NGS_RawString()
        try:
            ngs_str_seq = NGS_String()
            try:
                res = NGS.lib_manager.PY_NGS_FragmentGetFragmentBases(self.ref, offset, length, byref(ngs_str_seq.ref), byref(ngs_str_err.ref))
                return ngs_str_seq.getPyString()
            finally:
                ngs_str_seq.close()
        finally:
            ngs_str_err.close()

    def getFragmentQualities(self, offset=0, length=-1):
        """getFragmentQualities using ASCII offset of 33
        :param: offset is zero-based and non-negative
        :param: length must be >= 0
        :returns: phred quality values
        """
        ngs_str_err = NGS_RawString()
        try:
            ngs_str_seq = NGS_String()
            try:
                res = NGS.lib_manager.PY_NGS_FragmentGetFragmentQualities(self.ref, offset, length, byref(ngs_str_seq.ref), byref(ngs_str_err.ref))
                return ngs_str_seq.getPyString()
            finally:
                ngs_str_seq.close()
        finally:
            ngs_str_err.close()

    def isPaired(self):
        return bool(getNGSValue(self, NGS.lib_manager.PY_NGS_FragmentIsPaired, c_int))

    def isAligned(self):
        return bool(getNGSValue(self, NGS.lib_manager.PY_NGS_FragmentIsAligned, c_int))
