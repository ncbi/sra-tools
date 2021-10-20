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

from ctypes import c_void_p, c_char, c_uint32, c_int64
from . import NGS

from .String import getNGSString, getNGSValue

from .PileupEventIterator import PileupEventIterator

# Represents a slice through a stack of Alignments at a given position on the Reference

class Pileup(PileupEventIterator):

    # ----------------------------------------------------------------------
    # Reference

    def getReferenceSpec(self):
        return getNGSString(self, NGS.lib_manager.PY_NGS_PileupGetReferenceSpec)
        
    def getReferencePosition(self):
        return getNGSValue(self, NGS.lib_manager.PY_NGS_PileupGetReferencePosition, c_int64)

    def getReferenceBase(self):
        """
        :return: base at current Reference position
        """
        return getNGSValue(self, NGS.lib_manager.PY_NGS_PileupGetReferenceBase, c_char).decode("utf-8")

    # ----------------------------------------------------------------------
    # details of this pileup row
        
    def getPileupDepth(self):
        """
        :returns: the coverage depth at the current reference position
        """
        return getNGSValue(self, NGS.lib_manager.PY_NGS_PileupGetPileupDepth, c_uint32)

