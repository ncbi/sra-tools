#/bin/bash
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

#echo $*

# install.sh
#   installs required .kfg files from $1 (default.kfg and certs.kfg) and $2 (vdb-copy.kfg)
#   to $3 (non-root) or $4 (root)
#   if root, create initialization files /etc/profile.d.sra-tools.[c]sh with
#            $5 added to PATH and $6 added to LD_LIBRARY_PATH
#   $7 is the file with MD5 sums of the vdb-copy.kfg ($1 has the kfgsums for the 2 others)

SRC_DIR=$1
SRC_DIR_COPY=$2
KONFIG_DIR=$3
KONFIG_DIR_ROOT=$4
BINDIR=$5
LIBDIR=$6
KFGSUMS_FILE=$7

SCRIPT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

if [ "$EUID" -eq 0 ]; then
    $SCRIPT_DIR/install-kfg.sh default.kfg  $SRC_DIR      $KONFIG_DIR_ROOT $SRC_DIR/kfgsums
    $SCRIPT_DIR/install-kfg.sh certs.kfg    $SRC_DIR      $KONFIG_DIR_ROOT $SRC_DIR/kfgsums
    if [ "$SRC_DIR_COPY" != "" ]; then
        $SCRIPT_DIR/install-kfg.sh vdb-copy.kfg $SRC_DIR_COPY $KONFIG_DIR_ROOT $KFGSUMS_FILE
    fi

PROFILE_FILE=/etc/profile.d/sra-tools

    echo "Updating ${PROFILE_FILE}.sh"
    printf \
"#version ${VERSION}\n"\
"if ! echo \$PATH | /bin/grep -q ${BINDIR}\n"\
"then export PATH=${BINDIR}:\$PATH\n"\
"fi\n"\
"if ! echo \$LD_LIBRARY_PATH | /bin/grep -q ${LIBDIR}\n"\
"then export LD_LIBRARY_PATH=${LIBDIR}:\$LD_LIBRARY_PATH\n"\
"fi\n"\
"export NGS_LIBDIR=${LIBDIR}\n" \
        >${PROFILE_FILE}.sh && chmod 644 ${PROFILE_FILE}.sh

    echo "Updating ${PROFILE_FILE}.csh"
    printf \
"#version ${VERSION}\n"\
"echo \$LD_LIBRARY_PATH | /bin/grep -q ${LIBDIR}\n"\
"if ( \$status ) setenv LD_LIBRARY_PATH ${LIBDIR}:\$LD_LIBRARY_PATH\n"\
"setenv NGS_LIBDIR ${LIBDIR}\n" \
        >${PROFILE_FILE}.csh && chmod 644 ${PROFILE_FILE}.csh

else

    $SCRIPT_DIR/install-kfg.sh default.kfg  $SRC_DIR      $KONFIG_DIR $SRC_DIR/kfgsums
    $SCRIPT_DIR/install-kfg.sh certs.kfg    $SRC_DIR      $KONFIG_DIR $SRC_DIR/kfgsums
    $SCRIPT_DIR/install-kfg.sh vdb-copy.kfg $SRC_DIR_COPY $KONFIG_DIR $KFGSUMS_FILE

fi
