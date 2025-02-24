#!/usr/bin/env bash

# ==============================================================================
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
# =============================================================================$

bin_dir=$1
sra_stat=$2

OS=$(uname -o 2>/dev/null) || OS=$(uname -s)

if [ "${OS}" = 'GNU/Linux' ]; then
    LL='ls -l'
    MD5SUM='md5sum -b'
    RSLV=realpath
    TIME='ls -l --time-style=+%Y-%m-%dT%H:%M:%S'
    DTIME="$TIME -d"
elif [ "${OS}" = 'Darwin' ]; then
    LL='stat -F'
    MD5SUM='/sbin/md5 -q'
    RSLV='readlink -f'
    TIME='stat -f %Sm -t %Y-%m-%dT%H:%M:%S'
    DTIME="$TIME"
elif [ "${OS}" = 'FreeBSD' ]; then
    LL='stat -F'
    MD5SUM='/sbin/md5 -q'
    RSLV='realpath'
    TIME='stat -f %Sm -t %Y-%m-%dT%H:%M:%S'
    DTIME="$TIME"
else
    echo "Skipped; not GNU/Linux"
    exit 2
fi

echo Testing $sra_stat from $bin_dir

rm -rf actual
mkdir -p actual

# REMOTE RUN ###################################################################

run=SRR053325
xml=actual/$run.xml

NCBI_SETTINGS=/ $bin_dir/$sra_stat -lx $run > $xml || exit 2

# the following 4 arguments are not printed for a remote run

grep size $xml && echo "size found" && exit 1
grep date $xml && echo "date found" && exit 1
grep path $xml && echo "path found" && exit 1
grep md5  $xml && echo "md5  found" && exit 1

# LOCAL RUN FILE: LOCAL INFO NOT REQUESTED  ####################################

acc=SRR22714250
run=db/$acc.lite.1
xml=actual/$acc-no.xml

NCBI_SETTINGS=/ $bin_dir/$sra_stat -x $run > $xml || exit 3

# the following arguments are not printed for a local run file if not requested

size=`sed -n 's/.*size="\([^"]*\).*/\1/p' $xml`
date=`sed -n 's/.*date="\([^"]*\).*/\1/p' $xml`
path=`sed -n 's/.*path="\([^"]*\).*/\1/p' $xml`
md5=`sed -n 's/.*md5="\([^"]*\).*/\1/p'   $xml`

# LOCAL RUN FILE ###############################################################

acc=SRR22714250
run=db/$acc.lite.1
xml=actual/$acc.xml

NCBI_SETTINGS=/ $bin_dir/$sra_stat -lx $run > $xml || exit 4

# the following 4 arguments are printed for a local run file and match expected

size=`sed -n 's/.*size="\([^"]*\).*/\1/p' $xml`
date=`sed -n 's/.*date="\([^"]*\).*/\1/p' $xml`
path=`sed -n 's/.*path="\([^"]*\).*/\1/p' $xml`
md5=`sed -n 's/.*md5="\([^"]*\).*/\1/p'   $xml`

a_size=`$LL    $run | cut -d' ' -f5`
a_date=`$TIME  $run | cut -d' ' -f6`
a_md5=`$MD5SUM $run | cut -d' ' -f1`
a_path=`$RSLV  $run`

if [ "$a_size" != "$size" ]; then
    echo "size no match"
    exit 1
fi

if [ "$a_date" != "$date" ]; then
    echo "date no match"
    exit 1
fi

if [ "$a_md5" != "$md5" ]; then
    echo "md5 no match"
    exit 1
fi

if [ "$a_path" != "$path" ]; then
    echo "path no match"
    exit 1
fi

# LOCAL UNKARED RUN ############################################################

acc=SRR053325
run=db/$acc
xml=actual/$acc-dir.xml

NCBI_SETTINGS=/ $bin_dir/$sra_stat -lx $run > $xml || exit 5

# 2 arguments are printed and 2 not for a local unkared locked run

grep size $xml && echo "size found" && exit 1
grep md5  $xml && echo "md5  found" && exit 1
date=`sed -n 's/.*date="\([^"]*\).*/\1/p' $xml`
path=`sed -n 's/.*path="\([^"]*\).*/\1/p' $xml`

a_date=`$TIME $run/lock | cut -d' ' -f6`
a_path=`$RSLV $run`

if [ "$a_date" != "$date" ]; then
    echo "date no match"
    exit 1
fi

if [ "$a_path" != "$path" ]; then
    echo "path no match"
    exit 1
fi

# LOCAL UNKARED UNLOCKED RUN ###################################################

acc=SRR053325.unlocked
run=db/$acc
xml=actual/$acc-dir.xml

NCBI_SETTINGS=/ $bin_dir/$sra_stat -lx $run > $xml || exit 6

# 2 arguments are printed and 2 not for a local unkared locked run

grep size $xml && echo "size found" && exit 1
grep md5  $xml && echo "md5  found" && exit 1
date=`sed -n 's/.*date="\([^"]*\).*/\1/p' $xml`
path=`sed -n 's/.*path="\([^"]*\).*/\1/p' $xml`

a_date=`$DTIME $run | cut -d' ' -f6`
a_path=`$RSLV  $run`

if [ "$a_date" != "$date" ]; then
    echo "date no match"
    exit 1
fi

if [ "$a_path" != "$path" ]; then
    echo "path no match"
    exit 1
fi

################################################################################

rm -rf actual

################################################################################
