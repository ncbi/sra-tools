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
from ctypes import cast, c_char_p, c_void_p, POINTER


class ErrorMsg(RuntimeError):
    pass


# def check_res(res, ngs_str_error):
    # if res != PY_RES_OK:
        # raise ErrorMsg(ngs_str_error.getPyString())

def _dereference_byref(byref_arg):
    # this helper function exctracts a value of the argument passed to the C-function with byref
    # it uses internal variable so it may change in the future version of python
    return byref_arg._obj
       
def check_res_embedded(result, func, args):
    from . import PY_RES_OK
    if result != PY_RES_OK:
        # We rely on the fact that we always have
        # POINTER(c_void_p) parameter as the last one
        # in args (check_res_embedded is intended to be
        # used with ngs-sdk C-functions)
        ref_err = _dereference_byref(args[-1]) # void* ref_err = *void_pp; (void** void_pp = args[-1])
        assert isinstance(ref_err, c_void_p)
        char_p = cast(ref_err, c_char_p) # char* char_p = (char*)ref_err;
        raise ErrorMsg(str(char_p.value))
