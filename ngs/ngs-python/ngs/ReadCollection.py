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


from ctypes import c_void_p, c_uint64, byref, create_string_buffer, c_char_p, c_int
from . import NGS
    
from .Refcount import Refcount
from .ErrorMsg import ErrorMsg
from .String import NGS_RawString, getNGSString, getNGSValue
from .Read import Read
from .ReadIterator import ReadIterator
from .ReadGroup import ReadGroup
from .ReadGroupIterator import ReadGroupIterator
from .Reference import Reference
from .ReferenceIterator import ReferenceIterator
from .Alignment import Alignment
from .AlignmentIterator import AlignmentIterator

class ReadCollection(Refcount):
    """Represents an NGS-capable object with a collection of
    *Reads*, *References* and *Alignments*.
    
    Each of the basic content types may be accessed by *id*
    as either a standalone object, or more commonly through
    an *Iterator* over a selected collection of objects.

    Reads are grouped by *ReadGroup*. When
    not specifically assigned, Reads will be placed into the
    *default* ReadGroup.
    """
    
    def getName(self):
        """Access the simple name of the ReadCollection.
        This name is generally extracted from the "spec"
        used to create the object, but may also be mapped
        to a canonical name if one may be determined and
        differs from that given in the spec.
        
        if the name is extracted from "spec" and contains
        well-known file extensions that do not form part of
        a canonical name (e.g. ".sra"), they will be removed.
        
        :returns: the simple name of the ReadCollection
        :throws: ErrorMsg if the name cannot be retrieved
        """
        return getNGSString(self, NGS.lib_manager.PY_NGS_ReadCollectionGetName)
    
    #----------------------------------------------------------------------
    # READ GROUPS
    
    def getReadGroups(self):
        """Access all non-empty ReadGroups.
        Iterator will contain at least one ReadGroup
        unless the ReadCollection itself is empty.
        
        :returns: an unordered Iterator of ReadGroups
        :throws: ErrorMsg only upon an error accessing data
        """
        ret = ReadGroupIterator()
        ret.ref = getNGSValue(self, NGS.lib_manager.PY_NGS_ReadCollectionGetReadGroups, c_void_p) # TODO: check if it works
        return ret

    def hasReadGroup(self, spec):
        """check existence of a ReadGroup by name.

        :param spec: the name of a possibly contained read group
        :returns: true if the read group exists
        """
        ret = c_int()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionHasReadGroup(self.ref, spec.encode("UTF-8"), byref(ret), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()
    
        return bool(ret.value)

    def getReadGroup(self, spec):
        """Access a single ReadGroup by name.
        
        :param spec: the name of a contained read group
        :returns: an instance of the designated ReadGroup
        :throws: ErrorMsg if specified ReadGroup is not found
        :throws: ErrorMsg upon an error accessing data
        """
        ret = ReadGroup()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionGetReadGroup(self.ref, spec.encode("UTF-8"), byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()
        
        return ret
    
    #----------------------------------------------------------------------
    # REFERENCES
    
    def getReferences(self):
        """Access all References having aligned Reads.
        Iterator will contain at least one ReadGroup
        unless no Reads are aligned.
        
        :returns: an unordered Iterator of References
        :throws: ErrorMsg upon an error accessing data
        """
        ret = ReferenceIterator()
        ret.ref = getNGSValue(self, NGS.lib_manager.PY_NGS_ReadCollectionGetReferences, c_void_p) # TODO: check if it works
        return ret

    def hasReference(self, spec):
        """check existence of a Reference by name.

        :param spec: the name of a possibly contained reference sequence
        :returns: true if the reference exists
        """
        ret = c_int()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionHasReference(self.ref, spec.encode("UTF-8"), byref(ret), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()
    
        return bool(ret.value)

    def getReference(self, spec):
        ret = Reference()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionGetReference(self.ref, spec.encode("UTF-8"), byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()
        
        return ret
        
    #----------------------------------------------------------------------
    # ALIGNMENTS

    def getAlignment(self, alignmentId):
        """:returns: an individual Alignment
        :throws: ErrorMsg if Alignment does not exist
        """
        ret = Alignment()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionGetAlignment(self.ref, alignmentId.encode("UTF-8"), byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret
    
    # AlignmentCategory
    #  see Alignment for categories
    
    def getAlignments(self, categories):
        """
        :returns: an iterator of all Alignments from specified categories
        """
        ret = AlignmentIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionGetAlignments(self.ref, categories, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret
        
    def getAlignmentCount(self, categories=Alignment.all):
        '''"categories" provides a means of filtering by AlignmentCategory
        :returns: count of all alignments
        '''
        ret = c_uint64()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionGetAlignmentCount(self.ref, categories, byref(ret), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret.value
        
    def getAlignmentRange(self, first, count, categories=Alignment.all): # TODO: parameters order!
        '''"first" is an unsigned ordinal into set
        "categories" provides a means of filtering by AlignmentCategory
        :returns: an iterator across a range of Alignments
        '''
        ret = AlignmentIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionGetAlignmentRange(self.ref, first, count, categories, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret

    #----------------------------------------------------------------------
    # READ

    def getRead(self, readId):
        """
        :returns: an individual Read
        :throws: ErrorMsg if Read does not exist
        """
        ret = Read()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionGetRead(self.ref, readId.encode("UTF-8"), byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret
    
    # ReadCategory
    #  see Read for categories
    
    def getReads(self, categories):
        """
        :returns: an iterator of all contained machine Reads
        """
        ret = ReadIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionGetReads(self.ref, categories, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret

    def getReadCount(self, categories=Read.all):
        """of all combined categories
        :returns: the number of reads in the collection
        """
        ret = c_uint64()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionGetReadCount(self.ref, categories, byref(ret), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret.value

    def getReadRange(self, first, count, categories=Read.all):
        ret = ReadIterator()
        ngs_str_err = NGS_RawString()
        try:
            res = NGS.lib_manager.PY_NGS_ReadCollectionGetReadRange(self.ref, first, count, categories, byref(ret.ref), byref(ngs_str_err.ref))
        finally:
            ngs_str_err.close()

        return ret


def openReadCollection(spec):
    """Create an object representing a named collection of reads
     
     :param: spec may be a path to an object or may be an id, accession, or URL
     
     :throws: ErrorMsg if object cannot be located
     :throws: ErrorMsg if object cannot be converted to a ReadCollection
     :throws: ErrorMsg if an error occurs during construction
    """

    ret = ReadCollection()
    ERROR_BUFFER_SIZE = 4096
    str_err = create_string_buffer(ERROR_BUFFER_SIZE)
    from . import PY_RES_OK
    res = NGS.lib_manager.PY_NGS_Engine_ReadCollectionMake(spec.encode("UTF-8"), byref(ret.ref), str_err, len(str_err))
    if res != PY_RES_OK:
        raise ErrorMsg(str_err.value)
        
    return ret
