#===========================================================================
#
#                           PUBLIC DOMAIN NOTICE
#              National Center for Biotechnology Information
#
# This software/database is a "United States Government Work" under the
# terms of the United States Copyright Act.  It was written as part of
# the author's official duties as a United States Government employee and
# thus cannot be copyrighted.  This software/database is freely available
# to the public for use. The National Library of Medicine and the U.S.
# Government have not placed any restriction on its use or reproduction.
#
# Although all reasonable efforts have been taken to ensure the accuracy
# and reliability of the software and data, the NLM and the U.S.
# Government do not and cannot warrant the performance or results that
# may be obtained by using this software or data. The NLM and the U.S.
# Government disclaim all warranties, express or implied, including
# warranties of performance, merchantability or fitness for any particular
# purpose.
#
# Please cite the author in any work or product based on this material.
#
#===========================================================================
#
import sys
import traceback

from ngs import NGS
from ngs.ErrorMsg import ErrorMsg

def process(ref):
    length = ref.getLength()
    line = 0

    print( ">" + ref.getCanonicalName() )
    try:
        offset = 0
        while offset < length:
            chunk = ref.getReferenceChunk ( offset, 5000 )
            chunk_len = len (chunk)
            
            chunk_idx = 0
            while chunk_idx < chunk_len:
                endIndex = chunk_idx + 70 - line
                if endIndex > chunk_len:
                    endIndex = chunk_len
                chunk_line = chunk [ chunk_idx : endIndex ]
                line = line + len (chunk_line)
                chunk_idx = chunk_idx + len (chunk_line)

                sys.stdout.write( chunk_line )
                if line >= 70:
                    print("")
                    line = 0
            offset = offset + 5000
    except ErrorMsg as x:
        pass

def run(acc, refName=None):
    # open requested accession using SRA implementation of the API
    with NGS.openReadCollection(acc) as run:
        if refName:
            with run.getReference(refName) as ref:
                process(ref)
        else:
            with run.getReferences() as refs:
                while refs.nextReference():
                    process(refs)
                    print("")

if len(sys.argv) < 2 or len(sys.argv) > 3:
    print ("Usage: DumpReferenceFASTA accession [ reference ]\n")
    exit(1)
else:
    try:
        acc = sys.argv[1]
        refName = sys.argv[2] if len(sys.argv) == 3 else None
        run ( acc, refName )
    except ErrorMsg as x:
        print (x)
        traceback.print_exc()
        # x.printStackTrace - not implemented
        exit(1)
    except BaseException as x:
        traceback.print_exc()
        exit(1)
