#!python

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

import sys
import os
sys.path.append(os.path.abspath("../../shared/python"))
import GeneralWriter

tbl = {
    'SEQUENCE': {
        'READ': {
            'expression': 'READ',
            'elem_bits': 8
        }
    }
}

def main():
    gw = GeneralWriter.GeneralWriter(
        './actual/test.gw'
        , 'sra/nanopore.vschema'
        , 'NCBI:SRA:Nanopore:db'
        , 'GeneralWriterTest.py'
        , '1.0.0'
        , tbl)

    gw._writeColumnData(tbl["SEQUENCE"]["READ"]["_columnId"], 1, "A".encode('ascii'))
    gw._writeNextRow(tbl["SEQUENCE"]["_tableId"])
    gw._writeColumnData(tbl["SEQUENCE"]["READ"]["_columnId"], 1, "C".encode('ascii'))
    gw._writeNextRow(tbl["SEQUENCE"]["_tableId"])

    gw.writeDbMetadata("dbpath", "dbvalue")
    gw.writeTableMetadata(tbl["SEQUENCE"], "tblpath", "tblvalue")
    gw.writeColumnMetadata(tbl["SEQUENCE"]["READ"], "colpath", "colvalue")

    gw.writeDbMetadataNodeAttr("dbpath", "dbattr", "dbattrvalue")
    gw.writeTableMetadataNodeAttr(tbl["SEQUENCE"], "tblpath", "tblattr", "tblattrvalue")
    gw.writeColumnMetadataNodeAttr(tbl["SEQUENCE"]["READ"], "colpath", "colattr", "colattrvalue")

    gw = None # close stream and flush

main()
