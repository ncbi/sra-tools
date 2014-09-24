#!/bin/bash
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
#echo "$0 $*"

# ===========================================================================
# input library types, and their handling
#
#  normal linkage
#   -l : find shared or static
#   -s : require static
#   -d : require shared
#
#  static linkage
#   -l : require static
#   -s : require static
#   -d : ignore
# ===========================================================================

# script name
SELF_NAME="$(basename $0)"

# parameters and common functions
source "${0%rwin.stub.sh}win.cmn.sh"

# discover tool chain
case "$LD" in
link)
    source "${0%stub.sh}vc++.sh"
    ;;
 *)
    echo "$SELF_NAME: unrecognized ld tool - '$LD'"
    exit 5
esac

# produce stub library and exp file
echo $STUB_CMD
$STUB_CMD
