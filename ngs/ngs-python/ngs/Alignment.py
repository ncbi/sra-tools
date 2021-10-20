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

from ctypes import byref, c_char, c_int, c_int32, c_uint32, c_int64, c_uint64, c_void_p
from . import NGS

from .String import NGS_String, NGS_RawString, getNGSString, getNGSValue

from .Fragment import Fragment

# Represents an alignment between a Fragment and Reference sub-sequence
# provides a path to Read and mate Alignment

class Alignment(Fragment):

    # AlignmentFilter constants
    passFailed          = 1
    passDuplicates      = 2
    minMapQuality       = 4
    maxMapQuality       = 8
    noWraparound        = 16
    startWithinSlice    = 32

    # AlignmentCategory constants
    primaryAlignment    = 1
    secondaryAlignment  = 2
    all                 = primaryAlignment | secondaryAlignment
    
    # ClipEdge constants
    clipLeft            = 0
    clipRight           = 1


    def getAlignmentId(self):
        """Retrieve an identifying String that can be used for later access.
        The id will be unique within ReadCollection.
        :returns: alignment id string
        :throws: ErrorMsg if the property cannot be retrieved
        """
        return getNGSString(self, NGS.lib_manager.PY_NGS_AlignmentGetAlignmentId)

    # ------------------------------------------------------------------
    # Reference        
        
    def getReferenceSpec(self):
        return getNGSString(self, NGS.lib_manager.PY_NGS_AlignmentGetReferenceSpec)
        
    def getMappingQuality(self):
        return getNGSValue(self, NGS.lib_manager.PY_NGS_AlignmentGetMappingQuality, c_int32)

    def getReferenceBases(self):
        return getNGSString(self, NGS.lib_manager.PY_NGS_AlignmentGetReferenceBases)

    # ------------------------------------------------------------------        
    # Fragment        
 
    def getReadGroup(self):
        return getNGSString(self, NGS.lib_manager.PY_NGS_AlignmentGetReadGroup)

    def getReadId(self):
        return getNGSString(self, NGS.lib_manager.PY_NGS_AlignmentGetReadId)

    def getClippedFragmentBases(self):
        return getNGSString(self, NGS.lib_manager.PY_NGS_AlignmentGetClippedFragmentBases)

    def getClippedFragmentQualities(self):
        """
        :returns: clipped fragment phred quality values using ASCII offset of 33
        """
        return getNGSString(self, NGS.lib_manager.PY_NGS_AlignmentGetClippedFragmentQualities)

    def getAlignedFragmentBases(self):
        """
        :returns: fragment bases in their aligned orientation
        """
        return getNGSString(self, NGS.lib_manager.PY_NGS_AlignmentGetAlignedFragmentBases)

    # ------------------------------------------------------------------        
    # details of this alignment
    
    def getAlignmentCategory(self):
        """Alignments are categorized as primary or secondary (alternate).
        :returns: either Alignment.primaryAlignment or Alignment.secondaryAlignment
        :throws: ErrorMsg if the property cannot be retrieved
        """
        return getNGSValue(self, NGS.lib_manager.PY_NGS_AlignmentGetAlignmentCategory, c_uint32)
    
    def getAlignmentPosition(self):
        """Retrieve the Alignment's starting position on the Reference
        :returns: unsigned 0-based offset from start of Reference
        :throws: ErrorMsg if the property cannot be retrieved
        """
        return getNGSValue(self, NGS.lib_manager.PY_NGS_AlignmentGetAlignmentPosition, c_int64)
    
    def getAlignmentLength(self):
        """Retrieve the projected length of an Alignment projected upon Reference.
        :returns: unsigned length of projection
        :throws: ErrorMsg if the property cannot be retrieved
        """
        return getNGSValue(self, NGS.lib_manager.PY_NGS_AlignmentGetAlignmentLength, c_uint64)

    def getIsReversedOrientation(self):
        """Test if orientation is reversed with respect to the Reference sequence.
        :returns: true if reversed
        :throws: ErrorMsg if the property cannot be retrieved
        """
        return bool(getNGSValue(self, NGS.lib_manager.PY_NGS_AlignmentGetIsReversedOrientation, c_int))

    def getSoftClip(self, edge):
        ret = c_int32()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_AlignmentGetSoftClip(self.ref, edge, byref(ret), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret.value

    def getTemplateLength(self):
        return getNGSValue(self, NGS.lib_manager.PY_NGS_AlignmentGetTemplateLength, c_uint64)

    def getShortCigar(self, clipped):
        """
        :returns: a text string describing alignment details
        """
        ngs_str_err = NGS_RawString()
        try:
            ngs_str_ret = NGS_String()
            try:
                res = NGS.lib_manager.PY_NGS_AlignmentGetShortCigar(self.ref, clipped, byref(ngs_str_ret.ref), byref(ngs_str_err.ref))
                return ngs_str_ret.getPyString()
            finally:
                ngs_str_ret.close()
        finally:
            ngs_str_err.close()

    def getLongCigar(self, clipped):
        """
        :returns: a text string describing alignment details
        """
        ngs_str_err = NGS_RawString()
        try:
            ngs_str_ret = NGS_String()
            try:
                res = NGS.lib_manager.PY_NGS_AlignmentGetLongCigar(self.ref, clipped, byref(ngs_str_ret.ref), byref(ngs_str_err.ref))
                return ngs_str_ret.getPyString()
            finally:
                ngs_str_ret.close()
        finally:
            ngs_str_err.close()

    def getRNAOrientation(self):
        """
        :returns: '+' if positive strand is transcribed
        :returns: '-' if negative strand is transcribed
        :returns: '?' if unknown
        """
        return getNGSValue(self, NGS.lib_manager.PY_NGS_AlignmentGetRNAOrientation, c_char).decode("utf-8")

    # ------------------------------------------------------------------
    # details of mate alignment            
            
    def hasMate(self):
        return bool(getNGSValue(self, NGS.lib_manager.PY_NGS_AlignmentHasMate, c_int))
        
    def getMateAlignmentId(self):
        return getNGSString(self, NGS.lib_manager.PY_NGS_AlignmentGetMateAlignmentId)

    def getMateAlignment(self):
        ret = Alignment()
        ret.ref = getNGSValue(self, NGS.lib_manager.PY_NGS_AlignmentGetMateAlignment, c_void_p) # TODO: check if it works
        return ret

    def getMateReferenceSpec(self):
        return getNGSString(self, NGS.lib_manager.PY_NGS_AlignmentGetMateReferenceSpec)

    def getMateIsReversedOrientation(self):
        return bool(getNGSValue(self, NGS.lib_manager.PY_NGS_AlignmentGetMateIsReversedOrientation, c_int))
