#!/bin/sh
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

# set -x

#####
#### This script will test ability of kar utility to work with remote
### archives. It will download remote file during executing extract
## procedure. It will compare data and exit scientifically ( no bye )
# 

#######################################################################
# Actially many car archives are huge, and that is a list of small ones
# which I believe are good for comparision
#
#   - accn ----- size
#   ERR451271   26957
#   ERR2684777  27268
#   SRR4382039  33051
#   ERR318535   36751
#   SRR2581216  36867
#   SRR1836013  38561
#   ERR204727   43785
#   SRR768459   49392
#   SRR449080   51369
#   SRR1138217  74893
#
#   I think that we may implement the scan these names to find 
#   right one. ( actually I did, with another set )
#######################################################################

echo "## TEST INIT"

##
## Usual stuff
multi_bark ()
{
    CMD="$@"
    if [ -z "$CMD" ]
    then
        echo Error: no command defined >&2
        exit 1
    fi

    echo "## $CMD"
    # eval time "$CMD"
    eval "$CMD"
    if [ $? -ne 0 ]
    then
        echo Error: command failed \"$CMD\" >&2
        exit 1
    fi
}

##
## ### ARGUMENTS
##
usage ()
{
    A="$@"
    if [ -n "$A" ]
    then
        echo >&2
        echo $A>&2
    fi

    cat <<EOF >&2

Syntax: `basename $0` path_to_kar_utility path_to_prefetch_utility

Where: 

             path_to_kar_utility - a path to kar utility
        path_to_prefetch_utility - a path to prefetch utility. Script
                                   will try to find srapath near that
                                   utility

Enjoy

EOF
}

if [ $# -ne 2 ]
then
    usage

    exit 1
fi

##
## Looking for kar utility
##
KAR_B=$1
if [ ! -x "$KAR_B" ]
then
    usage Error: can not stat executable \'$KAR_B\'
    exit 1
fi

PRE_B=$2
if [ ! -x "$PRE_B" ]
then
    usage Error: can not stat executable \'$PRE_B\'
    exit 1
fi

SPT_B=$( dirname $PRE_B )/srapath
if [ ! -x "$SPT_B" ]
then
    usage Error: can not stat executable \'$SPT_B\'
    exit 1
fi

echo "## TEST START"

##
## Here we are cleaning and starting tests
##
BASEDIR=$( pwd )
VOTCHINA=$BASEDIR/votchina

clean_up ()
{
    if [ -d "$VOTCHINA" ]
    then
        multi_bark chmod -R u+w $VOTCHINA
        multi_bark rm -rf $VOTCHINA
    fi
}

clean_up

##
## First we are creating environment
##
multi_bark mkdir $VOTCHINA

##
## Second, we do set environment
##
cd $VOTCHINA

export VDB_CONFIG=config.kfg
cat <<EOF >$VDB_CONFIG
/LIBS/GUID = "8test002-6ab7-41b2-bfd0-karkarkarkar"
/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"
/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"
/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"
/repository/user/main/public/root = "$TARGET_DIR"
/sra/quality_type = "raw_scores"
EOF

##
## Known accessions of small size
## They are sorted by increase of size, we need choose good one
##
for i in  SRR1025702 SRR1022482 SRR3029805 SRR2064214 SRR1537524 SRR258852 SRR053769 SRR611340 SRR572719 DRR121070
do
    $SPT_B $i >/dev/null 2>&1
    if [ $? -eq 0 ]
    then
        WORK_ACCN=$i
        break;
    fi
done

if [ -z "$WORK_ACCN" ]
then
    echo "Error: can not find appropriate accession" >&2
    exit 1
fi

echo "## TEST ACCESSION $WORK_ACCN"

##
## Third we are using kar utility to extract data from far away
##
OUT1=$VOTCHINA/d1
multi_bark $KAR_B --extract $WORK_ACCN --directory $OUT1

##
## Forth we download data and un-kar it locally
##
multi_bark $PRE_B $WORK_ACCN  > /dev/null
POUT=$WORK_ACCN/${WORK_ACCN}.sra
if [ ! -f "$POUT" ]
then
    echo can not stat result of prefetch command $POUT, checking lite >&2
    POUT=$WORK_ACCN/${WORK_ACCN}.sralite
    if [ ! -f "$POUT" ]; then
        echo Error: can not stat result of prefetch command $POUT >&2
        exit 1
    fi
fi
OUT2=$VOTCHINA/d2
multi_bark $KAR_B --extract $POUT --directory $OUT2

##
## Now we are comparing trees
##
echo "## Comparing trez"
for i in `cd $OUT1; find . -type f`
do
    cmp $OUT1/$i $OUT2/$i >/dev/null 2>&1
    if [ $? -ne 0 ]
    then
        echo Error: remotely and locally un-karred datasets are different >&2
        exit 1
    fi
done

##
## Everything is OK
##
echo "## TEST PASSED"

##
## Finishing touch
##
clean_up
