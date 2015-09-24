#!python
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

"""Options:
    output:     destination VDB database
    name:       name of build, this must be followed by a list of AGP files
    help:       displays this message and exits
Note: There must be two builds specified
"""
import sys
import traceback
from AGP import AGP

usage = "Usage: {} -help | [ -output=<name of output object> -name=<build name> <AGP files> -name=<build name> <AGP files> ]\n".format(sys.argv[0])
build = []
output = None

for arg in sys.argv[1:]:
    if arg[0] == '-':
        if arg == "-help":
            sys.stderr.write(usage)
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


def main():
    build[0]["object"] = loadFiles(build[0]["files"])
    build[1]["object"] = loadFiles(build[1]["files"])
    remap = build[0]["object"].remap(build[1]["object"])

def cleanup():
    pass

main()
#print(build)
cleanup()
