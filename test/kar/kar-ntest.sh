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

echo "## Das Buch"

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

Syntax: `basename $0` path_to_kar_utility

Where:  path_to_kar_utility is a path to kar utility, and there suppose
                            to be srapath somewhere around

Enjoy

EOF
}

if [ $# -ne 1 ]
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
    usage Error: can not stat executable '$KAR_B'

    exit 1
fi

##
## Looking for srapath utility
##
SPT_B=`which srapath 2>/dev/null`
if [ $? -ne 0 ]
then
    ##
    ## No srapath in a PATH, it could be at the same location as a kar
    ##
    SPT_B=`dirname $KAR_B`/srapath
fi

if [ ! -x "$SPT_B" ]
then
    usage Error: can not stat executable 'srapath'

    exit 1
fi

##
## Known accessions of small size
## They are sorted by increase of size, we need choose good one
##
KNOWN_ACC=( SRR1025702 86869    \
            SRR1022482 91711    \
            SRR3029805 93038    \
            SRR2064214 93502    \
            SRR1537524 141289   \
            SRR258852 178966    \
            SRR053769 193481    \
            SRR611340 211261    \
            SRR572719 230945    \
            DRR121070 234427    \
            )

check_accn()
{
    AN=$1
    AS=$2

    TPT=`$SPT_B $AN 2>/dev/null`
    if [ $? -ne 0 ]
    then
        return 1
    fi

    NS=`HEAD $TPT | grep "Content-Length:" | awk ' { print $2 } '`
    if [ $NS -ne $AS ]
    then
        return 1
    fi

    WORK_URL=$TPT

    return 0
}

QQ=${#KNOWN_ACC[*]}
RQ=$(( $QQ / 2 ))
RC=0
while [ $RC -lt $RQ ]
do
    ACC_N=${KNOWN_ACC[$(($RC*2))]}
    ACC_S=${KNOWN_ACC[$(($RC*2+1))]}

    check_accn $ACC_N $ACC_S
    if [ $? -eq 0 ]
    then
        WORK_ACCN=$ACC_N
        break;
    fi

    RC=$(( $RC + 1 ))
done

if [ -z "$WORK_ACCN" ]
then
    echo "Error: can not find appropriate accession" >&2
    exit 1
fi

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
## Here we are cleaning and starting tests
##
VOTCHINA="votchina"

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
## Second we are using kar utility to extract data from far away
##
OUT1=$VOTCHINA/d1
multi_bark $KAR_B --extract $WORK_ACCN --directory $OUT1

##
## Third we download data and un-kar it locally
##
SRC_F=$VOTCHINA/f2
multi_bark "GET $WORK_URL >$SRC_F"
OUT2=$VOTCHINA/d2
multi_bark $KAR_B --extract $SRC_F --directory $OUT2

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
echo TEST PASSED

##
## Finishing touch
##
clean_up
