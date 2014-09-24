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

# define linker params
LD_EXPORT_GLOBAL="-Wl,--export-dynamic"
LD_MULTIPLE_DEFS="-Wl,-zmuldefs"
LD_STATIC="-Wl,-Bstatic"
LD_DYNAMIC="-Wl,-Bdynamic"
LD_ALL_SYMBOLS="-Wl,-whole-archive"
LD_REF_SYMBOLS="-Wl,-no-whole-archive"

# build command
DLIB_CMD="$LD -shared"
EXE_CMD="$LD"

# versioned output
if [ "$VERS" = "" ]
then
    DLIB_CMD="$DLIB_CMD -o $TARG"
    EXE_CMD="$EXE_CMD -o $TARG"
else
    set-vers $(echo $VERS | tr '.' ' ')
    DLIB_CMD="$DLIB_CMD -o $OUTDIR/$NAME$DBGAP.so.$VERS -Wl,-soname,$NAME.so.$MAJ"
    EXE_CMD="$EXE_CMD -o $OUTDIR/$NAME$DBGAP.$VERS"
fi
