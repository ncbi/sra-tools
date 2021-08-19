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

from ctypes import cdll, c_char, c_int, c_char_p, c_int32, c_int64, c_double, POINTER, c_size_t, c_void_p, c_uint64, c_uint32
import os, sys, platform, tempfile, subprocess

if sys.version_info[0] > 2:
    from urllib.parse import urlencode
    from urllib.request import urlopen
else:
    from urllib import urlencode
    from urllib2 import urlopen

from .ErrorMsg import check_res_embedded
from .ErrorMsg import ErrorMsg

def process_bits():
    """Return bitness of the running python process, or None if unknown."""
    architecture2bits = {'64bit': 64, '32bit': 32}
    return architecture2bits.get(platform.architecture()[0], None)

def lib_filename(lib_name):
    if platform.system() == "Windows":
        return lib_name+LibManager.get_lib_extension()
    else:
        return "lib"+lib_name+LibManager.get_lib_extension()

def load_saved_library(lib_name):
    """search library in different possible locations
    and load it if found
    """
    lib = None

    for dir in LibManager.get_directories_to_find_dll():
        try:
            lib = cdll.LoadLibrary(os.path.join(dir, lib_filename(lib_name) ))
            if lib:
                break
        except OSError:
            pass

    return lib

def load_updated_library(lib_name):
    """download library from ncbi and load it
    """
    directory_created = (False, None)
    file_created      = (False, None)
    file_saved        = (False, None)
    for dir in LibManager.get_directories_to_find_dll():
        try:
            if not dir:
                continue
            if not os.path.exists(dir):
                os.makedirs(dir)
            directory_created = (True, dir)
        except:
            directory_created = (False, dir)
            continue

        lib_path = os.path.join(dir, lib_filename(lib_name))

        try:
            f = open(lib_path, "wb")
            file_created = (True, lib_path)
        except:
            file_created = (False, lib_path)
            continue

        params = urlencode({
            'cmd':     'lib',
            'libname': lib_name,
            'os_name': LibManager.get_post_os_name_param(),
            'bits':    str(process_bits())
        })
        resp = urlopen(url=LibManager.URL_NCBI_SRATOOLKIT, data=params.encode())
        if resp.code != 200:
            raise ErrorMsg("Failed to download dll: url=" + LibManager.URL_NCBI_SRATOOLKIT + "; params=" + params + "; response code=" + str(resp.code))

        try:
            f.write(resp.read())
            f.close()
            file_saved = (True, lib_path)
        except:
            file_saved = (False, lib_path)
            continue
        return cdll.LoadLibrary(lib_path)

    if not directory_created[0]:
        raise ErrorMsg("Failed to create directory '" + directory_created[1] + "' for " + lib_filename(lib_name))
    elif not file_created[0]:
        raise ErrorMsg("Failed to create file " + file_created[1])
    elif not file_saved[0]:
        raise ErrorMsg("Failed to save file " + file_saved[1])

def version_tuple(version_str):
    assert isinstance(version_str, str) or isinstance(version_str, unicode)
    import re

    arr = re.split('\.\s*|\-\s*|\s+', version_str)
    arr_typed = []

    for x in arr:
        try:
            arr_typed.append(int(x))
        except ValueError:
            arr_typed.append(x) # TODO: apply a smarter parsing here if needed

    return tuple ( arr_typed )

def get_library_version_tuple_remote(lib_name):
    params = urlencode({
        'cmd':     'vers',
        'libname': lib_name,
        'os_name': LibManager.get_post_os_name_param(),
        'bits':    str(process_bits())
    })
    resp = urlopen(url=LibManager.URL_NCBI_SRATOOLKIT, data=params.encode())
    if resp.code != 200:
        raise ErrorMsg("Failed to query dll version: url=" + LibManager.URL_NCBI_SRATOOLKIT + "; params=" + params + "; response code=" + str(resp.code))

    version_str = resp.read().strip()
    if isinstance(version_str, bytes):
        version_str = version_str.decode(encoding='utf8')

    return version_tuple (version_str)

def should_download_library():
    do_download = os.environ.get("NGS_PY_DOWNLOAD_LIBRARY", "1")
    return do_download.lower() in ("1", "yes", "true", "on")


def load_library(lib_name, do_download, silent):
    if do_download:
        library = load_updated_library(lib_name)
    else:
        library = load_saved_library(lib_name)

    if not library and not silent:
        raise ErrorMsg("Failed to load library " +
            lib_name +
            " (NGS_PY_DOWNLOAD_LIBRARY=" + os.environ.get("NGS_PY_DOWNLOAD_LIBRARY", "<not set>") + ", "
            + "NGS_PY_LIBRARY_PATH=" + os.environ.get("NGS_PY_LIBRARY_PATH", "<not set>") + ", "
            + "do_download=" + str(do_download) + ")")
    else:
        return library

class LibManager:
    c_lib_engine = None
    c_lib_sdk = None

    URL_NCBI_SRATOOLKIT = 'https://trace.ncbi.nlm.nih.gov/Traces/sratoolkit/sratoolkit.cgi'

    def _bind(self, c_lib, c_func_name_str, param_types_list, errorcheck):
        setattr(self, c_func_name_str, getattr(c_lib, c_func_name_str))
        func = getattr(self, c_func_name_str)
        func.argtypes = param_types_list
        func.restype = c_int
        if errorcheck:
            func.errcheck = errorcheck

    def bind_sdk(self, c_func_name_str, param_types_list):
        return self._bind(self.c_lib_sdk, c_func_name_str, param_types_list, check_res_embedded)

    @staticmethod
    def get_directories_to_find_dll():
        env_path = os.environ.get("NGS_PY_LIBRARY_PATH", None)
        if env_path is None:
            return (
                os.path.join(os.path.expanduser('~'), ".ncbi", "lib"+str(process_bits())),
                "",
                ".",
                tempfile.gettempdir(),
            )
        else:
            return env_path.split(';')

    @staticmethod
    def get_post_os_name_param():
        if (platform.system() == "Darwin"):
            return "Mac"
        else:
            return platform.system()

    @staticmethod
    def get_lib_extension():
        if platform.system() == "Windows":
            return ".dll"
        elif platform.system() == "Darwin":
            return ".dylib"
        elif platform.system() == "Linux":
            return ".so"
        else:
            return ""

    def initialize_ngs_bindings(self):
        if self.c_lib_engine and self.c_lib_sdk: # already initialized
            return

        # check versions - must be run in a separate script to free library before overwriting it
        # check_vers_res = os.system('python -c "from ngs import NGS; exit(NGS.checkLibVersions())"')
        # if platform.system() != "Windows":
            # check_vers_res = check_vers_res >> 8 # python is a cross-platform language

        # os.system is not that reliable and cross-platform as subprocess. So using subprocess
        check_vers_res = subprocess.call([sys.executable, "-c", "from ngs import NGS; exit(NGS.checkLibVersions())"])

        do_update_engine = check_vers_res & 1
        do_update_sdk    = check_vers_res & 2

        libname_engine = "ncbi-vdb"
        libname_sdk = "ngs-sdk"

        self.c_lib_engine = load_library(libname_engine, do_update_engine, silent=False)
        self.c_lib_sdk = load_library(libname_sdk, do_update_sdk, silent=False)

        ##############  ngs-engine imports below  ####################

        self._bind(self.c_lib_engine, "PY_NGS_Engine_ReadCollectionMake",    [c_char_p, POINTER(c_void_p), POINTER(c_char), c_size_t], None)
        self._bind(self.c_lib_engine, "PY_NGS_Engine_ReferenceSequenceMake", [c_char_p, POINTER(c_void_p), POINTER(c_char), c_size_t], None)

        self._bind(self.c_lib_engine, "PY_NGS_Engine_SetAppVersionString",   [c_char_p, POINTER(c_char), c_size_t], None)
        self._bind(self.c_lib_engine, "PY_NGS_Engine_GetVersion",            [POINTER(c_char_p), POINTER(c_char), c_size_t], None)
        self._bind(self.c_lib_engine, "PY_NGS_Engine_IsValid",               [c_char_p, POINTER(c_int), POINTER(c_char), c_size_t], None)

        # self._bind(self.c_lib_engine, "PY_NGS_Engine_RefcountRelease",       [c_void_p, POINTER(c_char), c_size_t], None)
        # self._bind(self.c_lib_engine, "PY_NGS_Engine_StringData", [c_void_p, POINTER(c_char_p)], None)
        # self._bind(self.c_lib_engine, "PY_NGS_Engine_StringSize", [c_void_p, POINTER(c_size_t)], None)

        ##############  ngs-sdk imports below  ####################

        # Common

        self._bind(self.c_lib_sdk, "PY_NGS_StringGetData",    [c_void_p, POINTER(c_char_p)], None)
        self._bind(self.c_lib_sdk, "PY_NGS_StringGetSize",    [c_void_p, POINTER(c_size_t)], None)
        self._bind(self.c_lib_sdk, "PY_NGS_RawStringRelease", [c_void_p, POINTER(c_void_p)], None)
        self._bind(self.c_lib_sdk, "PY_NGS_RefcountRelease",  [c_void_p, POINTER(c_void_p)], None)

        # ReadCollection

        self.bind_sdk("PY_NGS_ReadCollectionGetName",           [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetReadGroups",     [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionHasReadGroup",      [c_void_p, c_char_p, POINTER(c_int), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetReadGroup",      [c_void_p, c_char_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetReferences",     [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionHasReference",      [c_void_p, c_char_p, POINTER(c_int), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetReference",      [c_void_p, c_char_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetAlignment",      [c_void_p, c_char_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetAlignments",     [c_void_p, c_uint32, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetAlignmentCount", [c_void_p, c_uint32, POINTER(c_uint64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetAlignmentRange", [c_void_p, c_uint64, c_uint64, c_uint32, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetRead",           [c_void_p, c_char_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetReads",          [c_void_p, c_uint32, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetReadCount",      [c_void_p, c_uint32, POINTER(c_uint64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadCollectionGetReadRange",      [c_void_p, c_uint64, c_uint64, c_uint32, POINTER(c_void_p), POINTER(c_void_p)])

        # Alignment

        self.bind_sdk("PY_NGS_AlignmentGetAlignmentId",               [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetReferenceSpec",             [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetMappingQuality",            [c_void_p, POINTER(c_int32), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetReferenceBases",            [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetReadGroup",                 [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetReadId",                    [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetClippedFragmentBases",      [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetClippedFragmentQualities",  [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetAlignedFragmentBases",      [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetAlignmentCategory",         [c_void_p, POINTER(c_uint32), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetAlignmentPosition",         [c_void_p, POINTER(c_int64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetAlignmentLength",           [c_void_p, POINTER(c_uint64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetIsReversedOrientation",     [c_void_p, POINTER(c_int), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetSoftClip",                  [c_void_p, c_uint32, POINTER(c_int32), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetTemplateLength",            [c_void_p, POINTER(c_uint64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetShortCigar",                [c_void_p, c_int, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetLongCigar",                 [c_void_p, c_int, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetRNAOrientation",            [c_void_p, POINTER(c_char), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentHasMate",                      [c_void_p, POINTER(c_int), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetMateAlignmentId",           [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetMateAlignment",             [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetMateReferenceSpec",         [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_AlignmentGetMateIsReversedOrientation", [c_void_p, POINTER(c_int), POINTER(c_void_p)])

        self.bind_sdk("PY_NGS_AlignmentIteratorNext",                 [c_void_p, POINTER(c_int), POINTER(c_void_p)])

        # Fragment

        self.bind_sdk("PY_NGS_FragmentGetFragmentId",        [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_FragmentGetFragmentBases",     [c_void_p, c_uint64, c_uint64, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_FragmentGetFragmentQualities", [c_void_p, c_uint64, c_uint64, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_FragmentIsPaired",             [c_void_p, POINTER(c_int), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_FragmentIsAligned",            [c_void_p, POINTER(c_int), POINTER(c_void_p)])

        self.bind_sdk("PY_NGS_FragmentIteratorNext",         [c_void_p, POINTER(c_int), POINTER(c_void_p)])

        # Package

        self.bind_sdk("PY_NGS_PackageGetPackageVersion", [POINTER(c_void_p), POINTER(c_void_p)])

        # PileupEvent

        self.bind_sdk("PY_NGS_PileupEventGetMappingQuality",         [c_void_p, POINTER(c_int32), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetAlignmentId",            [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetAlignmentPosition",      [c_void_p, POINTER(c_int64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetFirstAlignmentPosition", [c_void_p, POINTER(c_int64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetLastAlignmentPosition",  [c_void_p, POINTER(c_int64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetEventType",              [c_void_p, POINTER(c_uint32), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetAlignmentBase",          [c_void_p, POINTER(c_char), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetAlignmentQuality",       [c_void_p, POINTER(c_char), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetInsertionBases",         [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetInsertionQualities",     [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetEventRepeatCount",       [c_void_p, POINTER(c_uint32), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventGetEventIndelType",         [c_void_p, POINTER(c_uint32), POINTER(c_void_p)])

        self.bind_sdk("PY_NGS_PileupEventIteratorNext",              [c_void_p, POINTER(c_int), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupEventIteratorReset",             [c_void_p, POINTER(c_int), POINTER(c_void_p)])

        # Pileup

        self.bind_sdk("PY_NGS_PileupGetReferenceSpec",     [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupGetReferencePosition", [c_void_p, POINTER(c_int64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupGetReferenceBase",     [c_void_p, POINTER(c_char), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_PileupGetPileupDepth",       [c_void_p, POINTER(c_uint32), POINTER(c_void_p)])

        self.bind_sdk("PY_NGS_PileupIteratorNext",         [c_void_p, POINTER(c_int), POINTER(c_void_p)])

        # ReadGroup

        self.bind_sdk("PY_NGS_ReadGroupGetName",       [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadGroupGetStatistics", [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])

        self.bind_sdk("PY_NGS_ReadGroupIteratorNext",  [c_void_p, POINTER(c_int), POINTER(c_void_p)])

        # Read

        self.bind_sdk("PY_NGS_ReadGetReadId",        [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadGetNumFragments",  [c_void_p, POINTER(c_uint32), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadFragmentIsAligned",[c_void_p, c_uint32, POINTER(c_int32), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadGetReadCategory",  [c_void_p, POINTER(c_uint32), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadGetReadGroup",     [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadGetReadName",      [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadGetReadBases",     [c_void_p, c_uint64, c_uint64, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReadGetReadQualities", [c_void_p, c_uint64, c_uint64, POINTER(c_void_p), POINTER(c_void_p)])

        self.bind_sdk("PY_NGS_ReadIteratorNext",     [c_void_p, POINTER(c_int), POINTER(c_void_p)])

        # Reference

        self.bind_sdk("PY_NGS_ReferenceGetCommonName",             [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetCanonicalName",          [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetIsCircular",             [c_void_p, POINTER(c_int), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetIsLocal",                [c_void_p, POINTER(c_int), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetLength",                 [c_void_p, POINTER(c_uint64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetReferenceBases",         [c_void_p, c_uint64, c_uint64, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetReferenceChunk",         [c_void_p, c_uint64, c_uint64, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetAlignment",              [c_void_p, c_char_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetAlignments",             [c_void_p, c_uint32, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetAlignmentSlice",         [c_void_p, c_int64, c_uint64, c_uint32, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetFilteredAlignmentSlice", [c_void_p, c_int64, c_uint64, c_uint32, c_uint32, c_int32, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetPileups",                [c_void_p, c_uint32, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetFilteredPileups",        [c_void_p, c_uint32, c_uint32, c_int32, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetPileupSlice",            [c_void_p, c_int64, c_uint64, c_uint32, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceGetFilteredPileupSlice",    [c_void_p, c_int64, c_uint64, c_uint32, c_uint32, c_int32, POINTER(c_void_p), POINTER(c_void_p)])

        self.bind_sdk("PY_NGS_ReferenceIteratorNext",              [c_void_p, POINTER(c_int), POINTER(c_void_p)])

        # ReferenceSequence

        self.bind_sdk("PY_NGS_ReferenceSequenceGetCanonicalName",  [c_void_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceSequenceGetIsCircular",     [c_void_p, POINTER(c_int), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceSequenceGetLength",         [c_void_p, POINTER(c_uint64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceSequenceGetReferenceBases", [c_void_p, c_uint64, c_uint64, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_ReferenceSequenceGetReferenceChunk", [c_void_p, c_uint64, c_uint64, POINTER(c_void_p), POINTER(c_void_p)])

        # Statistics

        self.bind_sdk("PY_NGS_StatisticsGetValueType", [c_void_p, c_char_p, POINTER(c_uint32), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_StatisticsGetAsString",  [c_void_p, c_char_p, POINTER(c_void_p), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_StatisticsGetAsI64",     [c_void_p, c_char_p, POINTER(c_int64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_StatisticsGetAsU64",     [c_void_p, c_char_p, POINTER(c_uint64), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_StatisticsGetAsDouble",  [c_void_p, c_char_p, POINTER(c_double), POINTER(c_void_p)])
        self.bind_sdk("PY_NGS_StatisticsGetNextPath",  [c_void_p, c_char_p, POINTER(c_void_p), POINTER(c_void_p)])
