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

from ctypes import c_int

from . import NGS
from .String import getNGSValue
from .Reference import Reference

# Iterates across a list of References

class ReferenceIterator(Reference):
    def nextReference(self):
        """Advance to first Reference on initial invocation
        Advance to next Reference subsequently
        :returns: false if no more References are available.
        :throws: ErrorMsg if there is an error
        """
        return bool(getNGSValue(self, NGS.lib_manager.PY_NGS_ReferenceIteratorNext, c_int))