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

from ctypes import byref, c_int, c_uint64
from . import NGS
from .Refcount import Refcount
from .String import NGS_RawString, NGS_String, getNGSString, getNGSValue

from .Alignment import Alignment
from .AlignmentIterator import AlignmentIterator
from .PileupIterator import PileupIterator

# Represents a reference sequence

class Reference(Refcount):

    def getCommonName(self):
        """
        :returns: the common name of reference, e.g. "chr1"
        """
        return getNGSString(self, NGS.lib_manager.PY_NGS_ReferenceGetCommonName)

    def getCanonicalName(self):
        '''
        :returns: the accessioned name of reference, e.g. "NC_000001.11"
        '''
        return getNGSString(self, NGS.lib_manager.PY_NGS_ReferenceGetCanonicalName)

    def getIsCircular(self):
        return bool(getNGSValue(self, NGS.lib_manager.PY_NGS_ReferenceGetIsCircular, c_int))

    def getIsLocal(self):
        return bool(getNGSValue(self, NGS.lib_manager.PY_NGS_ReferenceGetIsLocal, c_int))

    def getLength(self):
        return getNGSValue(self, NGS.lib_manager.PY_NGS_ReferenceGetLength, c_uint64)

    def getReferenceBases(self, offset, length=-1):
        """
        :param: offset is zero-based and non-negative
        :param: length must be >= 0
        :returns: sub-sequence bases for Reference
        """
        ngs_str_err = NGS_RawString()
        try:
            ngs_str_ret = NGS_String()
            try:
                res = NGS.lib_manager.PY_NGS_ReferenceGetReferenceBases(self.ref, offset, length, byref(ngs_str_ret.ref), byref(ngs_str_err.ref))
                return ngs_str_ret.getPyString()
            finally:
                ngs_str_ret.close()
        finally:
            ngs_str_err.close()

    def getReferenceChunk(self, offset, length=-1):
        """
        :param: offset is zero-based and non-negative
        :param: length must be >= 0
        :returns: largest contiguous chunk available of sub-sequence bases for Reference
        NB - actual returned sequence may be shorter
        than requested. to obtain all bases available
        in chunk, use a negative "size" value
        """
        ngs_str_err = NGS_RawString()
        try:
            ngs_str_ret = NGS_String()
            try:
                res = NGS.lib_manager.PY_NGS_ReferenceGetReferenceChunk(self.ref, offset, length, byref(ngs_str_ret.ref), byref(ngs_str_err.ref))
                return ngs_str_ret.getPyString()
            finally:
                ngs_str_ret.close()
        finally:
            ngs_str_err.close()

    # ----------------------------------------------------------------------
    # ALIGNMENTS

    def getAlignment(self, alignmentId):
        """
        :returns: an individual Alignment
        :throws: ErrorMsg if Alignment does not exist or is not part of this Reference
        """
        ret = Alignment()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReferenceGetAlignment(self.ref, alignmentId.encode("UTF-8"), byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret

    def getAlignments(self, categories):
        """
        :returns: an iterator of contained alignments
        """
        ret = AlignmentIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReferenceGetAlignments(self.ref, categories, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret

    def getAlignmentSlice(self, start, length, categories=Alignment.all):
        """
        :param: start is a signed 0-based offset from the start of the Reference
        :param: length is the length of the slice.
        :param: categories provides a means of filtering by AlignmentCategory
        :returns: an iterator across a range of Alignments
        """
        ret = AlignmentIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReferenceGetAlignmentSlice(self.ref, start, length, categories, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret

    def getFilteredAlignmentSlice(self, start, length, categories, filters, mappingQuality):
        """Behaves like "getAlignmentSlice" except that supported filters are applied to selection
        :param: start is a signed 0-based offset from the start of the Reference
        :param: length is the length of the slice.
        :param: categories provides a means of filtering by AlignmentCategory
        :param: filters is a set of filter bits defined in Alignment
        :param: mappingQuality is a cutoff to be used according to bits in "filter"
        :returns: an iterator across a range of Alignments
        """
        ret = AlignmentIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReferenceGetFilteredAlignmentSlice(self.ref, start, length, categories, filters, mappingQuality, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret

    # ----------------------------------------------------------------------
    # PILEUP

    def getPileups(self, categories):
        """
        :returns: an iterator of contained Pileups
        """
        ret = PileupIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReferenceGetPileups(self.ref, categories, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret

    def getFilteredPileups(self, categories, filters, mappingQuality):
        """
        :returns: an iterator of contained Pileups
        """
        ret = PileupIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReferenceGetFilteredPileups(self.ref, categories, filters, mappingQuality, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret

    def getPileupSlice(self, start, length, categories=Alignment.all):
        """Creates a PileupIterator on a slice (window) of reference
        :param: start is the signed starting position on reference
        :param: length is the unsigned number of bases in the window
        :param: categories provides a means of filtering by AlignmentCategory
        :returns: an iterator of contained Pileups
        """
        ret = PileupIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReferenceGetPileupSlice(self.ref, start, length, categories, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret

    def getFilteredPileupSlice(self, start, length, categories, filters, mappingQuality):
        """Creates a PileupIterator on a slice (window) of reference
        :param: start is the signed starting position on reference
        :param: length is the unsigned number of bases in the window
        :param: categories provides a means of filtering by AlignmentCategory
        :param: filters is a set of filter bits defined in Alignment
        :param: mappingQuality is a cutoff to be used according to bits in "filter"
        :returns: an iterator of contained Pileups
        """
        ret = PileupIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReferenceGetFilteredPileupSlice(self.ref, start, length, categories, filters, mappingQuality, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret





