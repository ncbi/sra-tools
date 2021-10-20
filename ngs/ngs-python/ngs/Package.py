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

from ctypes import byref
from . import NGS

from .String import NGS_RawString

class Package:
    @staticmethod
    def getPackageVersion():
        if NGS.lib_manager.PY_NGS_PackageGetPackageVersion is None:
            return "0"
        ngs_str_err = NGS_RawString()
        try:
            ngs_str_ver = NGS_RawString()
            try:
                res = NGS.lib_manager.PY_NGS_PackageGetPackageVersion(byref(ngs_str_ver.ref), byref(ngs_str_err.ref))
                #check_res(res, ngs_str_err)
                return ngs_str_ver.getPyString()
            finally:
                ngs_str_ver.close()
        finally:
            ngs_str_err.close()

