#!python
"""The loader requires the following arguments:
* for source: -name=<build name> <AGP file> [<AGP file>...]
* for result: -name=<build name> <AGP file> [<AGP file>...]
* -output=<output directory>

<AGP file> can be an AGP file or a tar containing, possibly gzip'ed, AGP files.
<build name> is stored in the output database (see below)

Example:
  python agp-load.py -name=GRCh37 GRCh37.tar -name=GRCh38.p2 GRCh38.tar -output=37_to_38.remap

The loader generates three tables containing AGP data.
# SOURCE: the build being remapped from
# RESULT: the build being remapped to
# REMAP: the remapping

SOURCE and RESULT will have a metadata node BuildName containing the given build
names. This AGP data is only for components, i.e. it doesn't specify gaps, as
they can be implied.

To produce approximately valid AGP from a loaded database:
  vdb-dump 37_to_38.remap -T REMAP -f tab -b1 -C "SEQID,START,LENGTH,COMPONENT,OFFSET,REVERSED" | awk 'BEGIN{OFS="\\t"; P[""]=0}{print $1, $2, $2+$3-1, ++P[$1], "O", $4, $5, $5+$3-1, 1-($6*2)}'
"""
# =============================================================================
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
# =============================================================================

import sys
import traceback
from AGP import AGP
from GeneralWriter import GeneralWriter
from array import array

usage = "Usage: {} -help | [ -output=<name of output object> -name=<build name> <AGP files> -name=<build name> <AGP files> ]\n".format(sys.argv[0])
build = []
output = None

for arg in sys.argv[1:]:
    if arg[0] == '-':
        if arg == "-help":
            sys.stderr.write(usage)
            sys.stderr.write(__doc__)
            exit(0)
        if arg[0:8] == "-output=":
            output = arg[8:]
        elif arg[0:6] == "-name=":
            build.append({"name": arg[6:], "files": []})
        else:
            sys.stderr.write(usage)
            exit(3)
    else:
        build[-1]["files"].append(arg)

if output == None:
    sys.stderr.write("output must be specified\n")
    sys.stderr.write(usage);
    exit(3)

if len(build) != 2:
    sys.stderr.write("Two builds must be specified\n")
    sys.stderr.write(usage);
    exit(3)

if len(build[0]["files"]) == 0 or len(build[1]["files"]) == 0:
    sys.stderr.write("AGP file lists must be specified\n")
    sys.stderr.write(usage);
    exit(3)

if build[0]["name"] == build[1]["name"]:
    sys.stderr.write("Build names must be unique\n")
    exit(3)


def loadFiles(list):
    agp = AGP()
    for f in list:
        agp.loadFile(f)
    return agp

def assemblyTblDef():
    return {
        'SEQID': {
            'elem_bits': 8,
        },
        'START': {
            'elem_bits': 32,
        },
        'LENGTH': {
            'elem_bits': 32,
        },
        'COMPONENT': {
            'elem_bits': 8,
        },
        'OFFSET': {
            'elem_bits': 32,
        },
        'REVERSED': {
            'elem_bits': 8,
        },
    }

tbl = {
    'SOURCE': assemblyTblDef(),
    'RESULT': assemblyTblDef(),
    'REMAP': assemblyTblDef(),
}

gw = GeneralWriter(  output
                   , 'agp.vschema'
                   , 'NCBI:refseq:db:remap'
                   , 'agp-load.py'
                   , '1.0.0'
                   , tbl)

def logMessage(msg):
    sys.stderr.write("{}\n".format(msg))

def writeAGP(listOfAGP, tbl):
    for r in listOfAGP:
        tbl['SEQID'    ]['data'] = str(r[0]).encode('ascii')
        tbl['START'    ]['data'] = array('I', [int(r[1])])
        tbl['LENGTH'   ]['data'] = array('I', [int(r[2] - r[1])])
        tbl['COMPONENT']['data'] = str(r[5]).encode('ascii')
        tbl['OFFSET'   ]['data'] = array('I', [int(r[6])])
        tbl['REVERSED' ]['data'] = array('b', [1 if r[8] < 0 else 0])
        gw.write(tbl)

def main():
    gw.writeTableMetadata(tbl['SOURCE'], "BuildName", build[0]["name"])
    gw.writeTableMetadata(tbl['RESULT'], "BuildName", build[1]["name"])

    logMessage("loading {} ...".format(build[0]["name"]))
    build[0]["object"] = loadFiles(build[0]["files"])
    writeAGP(build[0]["object"].flattened(), tbl["SOURCE"])
    logMessage("loaded {}".format(build[0]["name"]))

    logMessage("loading {} ...".format(build[1]["name"]))
    build[1]["object"] = loadFiles(build[1]["files"])
    writeAGP(build[1]["object"].flattened(), tbl["RESULT"])
    logMessage("loaded {}".format(build[1]["name"]))

    logMessage("generating remapping data ...")
    writeAGP(build[0]["object"].remap(build[1]["object"]), tbl["REMAP"])
    logMessage("loaded remapping data")

def cleanup():
    logMessage("done")
    pass

main()
gw = None
cleanup()
