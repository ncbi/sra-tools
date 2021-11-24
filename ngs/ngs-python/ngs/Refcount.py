from ctypes import byref, c_void_p

from . import NGS
from .String import NGS_RawString

def RefcountRelease(ref):
    """Releases NGS-object imported from ngs-sdk
    
    :param ref: reference to refcounted NGS-object to be released. It's expected to be of type c_void_p
    :returns: None
    :throws: ErrorMsg
    """
    ngs_str_err = NGS_RawString()
    try:
        res = NGS.lib_manager.PY_NGS_RefcountRelease(ref, byref(ngs_str_err.ref))
    finally:
        ngs_str_err.close()

# def RefcountEngineRelease(ref):
    # """Releases NGS-object imported from ngs engine
    
    # :param ref: reference to refcounted NGS-object to be released. It's expected to be of type c_void_p
    # :returns: None
    # :throws: ErrorMsg
    # """

    # ERROR_BUFFER_SIZE = 4096
    # str_err = create_string_buffer(ERROR_BUFFER_SIZE)
    # from . import PY_RES_OK
    # res = NGS.lib_manager.PY_NGS_Engine_RefcountRelease(ref, str_err, len(str_err))
    # if res != PY_RES_OK:
        # raise ErrorMsg(str_err.value)


def RefcountRawStringRelease(ref):
    """Releases raw string imported from ngs-sdk
    
    :param ref: reference to raw char string. It's expected to be of type c_char_p
    :returns: None
    :throws: ErrorMsg
    """
    ngs_str_err = NGS_RawString()
    try:
        res = NGS.lib_manager.PY_NGS_RawStringRelease(ref, byref(ngs_str_err.ref))
    finally:
        ngs_str_err.close()


class Refcount:
    """ Base class for all refcounted objects imported from ngs-sdk
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
            RefcountRelease(self.ref)
            self.init_members_with_null()
            
    def init_members_with_null(self):
        self.ref = c_void_p()
