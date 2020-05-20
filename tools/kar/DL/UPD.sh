#!/bin/bash

## That script will update binaries and schemas for delite to current directory
##
## Syntax: UPD.sh [--help|-h] [--debug] [--schemas] [--binaries]
##
## Directories with sources :
## /panfs/pan1/sra-test/vdb3/git/inst
## ~/work/Kar+/sra-tools/tools/kar
##
## Structure:
## .../BIN - THAT BINARY is in here
## .../REP - repository
##      |-- VER - version of data
##           |-- BIN - binaries
##           |-- SCM - schemas
## .../VER - link to current version
## .../TMP - temparary directory
##



## Directories with data sources
##
DBIN_DIR=/panfs/pan1/sra-test/vdb3/git/inst
DSCR_DIR=/panfs/pan1/sra-test/vdb3/git/sra-tools/tools/kar
DSCM_DIR=/panfs/pan1/sra-test/vdb3/git/ncbi-vdb/interfaces

## Other directories and links
##
BIN_D=$( cd $( dirname "$0" ); pwd )
BASE_D=$( cd $( dirname "$BIN_D" ); pwd )
REP_D=$BASE_D/REP
VER_D=$BASE_D/VER
TMP_D=$BASE_D/TMP

for i in $REP_D $VER_D $TMP_D
do
    if [ ! -d "$i" ]
    then
        echo WARNING: directory missed \"$i\" >&2
        mkdir $i >/dev/null 2>&1
        if [ $? -ne 0 ]
        then
            echo ERROR: can not create directory \"$i\" >&2
            exit 1
        fi
    fi
done

##
## Arguments and envir
##
usage ()
{
    cat << EOF

That script will update binares and schemas for delite project. By default, it will 
put binaries to the same location where script is, and it will put schemas to directory
'schemas', which has the same parent directory as script location.

Syntax: `basename $0` [--help|-h] [--debug]

Where:
    --help|-h - will show that message
      --debug - will update binaries with debug information

EOF

}

for i in $@
do
    case $i in
        --help)
            usage
            exit 0
            ;;
        -h)
            usage
            exit 0
            ;;
        --debug)
            DEBUG_FL=1
            ;;
        *)
            echo ERROR: invalid argument \"$i\" >&2
            usage
            exit 1
            ;;
    esac
done

##
## Something we should put here
##
run_cmd ()
{
    CMD="$@"

    if [ -z "$CMD" ]
    then
        echo ERROR: invalid usage \"run_cmd\". >&2
        exit 1
    fi

    echo "## $CMD"
    eval "$CMD"
    if [ $? -ne 0 ]
    then
        echo ERROR: command failed >&2
        exit 1
    fi
}

## removing data from previous builds
##
for i in `ls $TMP_D`
do
    run_cmd rm -rf $TMP_D/$( basename $i )
done

##
## First we should make a new version
##
TMP_VER=$TMP_D/VER
if [ -d "$TMP_VER" ]
then
    echo WARNING: removing old bundle directory \"$TMP_VER\". >&2
    run_cmd rm -r $TMP_VER
    if [ $? -ne 0 ]
    then
        echo ERROR: can not remove old bundle directory \"$TMP_VER\". >&2
        exit 1
    fi
fi
TMP_BIN=$TMP_VER/BIN
TMP_SCM=$TMP_VER/SCM

for i in $TMP_VER $TMP_BIN $TMP_SCM
do
    run_cmd mkdir $i
done

##
## Copying binaries
##

copy_f ()
{
    for i in $@
    do
        FILES=`ls $SRC_D/${i}*`
        if [ $? -ne 0 ]
        then
            echo ERROR: can nog stat appropriate files for \'${i}*\' >&2
            exit 1
        fi

        for u in `ls $SRC_D/${i}*`
        do
            FOH=`basename $u`
            run_cmd cp -pP $SRC_D/$FOH $DST_D/$FOH
        done
    done
}

if [ -n "$DEBUG_FL" ]
then
    DSRC_D=$DBIN_DIR/linux-dbg
else
    DSRC_D=$DBIN_DIR/linux-rel
fi

## Here we are copying binaries
##
copy_bins ()
{
    echo "###################################################"
    echo "## CP BIN: $TMP_BIN"

    BIN2CP="    \
            fastq-dump  \
            kar+  \
            kar+meta  \
            make-read-filter  \
            prefetch  \
            srapath  \
            sratools    \
            vdb-diff  \
            vdb-dump  \
            vdb-lock  \
            vdb-unlock \
            vdb-validate  \
            vdb-config  \
            "
    SRC_D=$DSRC_D/bin
    DST_D=$TMP_BIN

    copy_f $BIN2CP
}

## copying scripts
##

## copyinng schemas
##
copy_schemas ()
{
    echo CP SCM: $TMP_SCM

    for i in `find $DSCM_DIR -type f -name "*.vschema"`
    do
        FF=$( echo $i | sed "s#$DSCM_DIR##1" )
        DNM=$( dirname $FF )
        DSTD=$TMP_SCM/$DNM
        if [ ! -d "$DSTD" ]
        then
            run_cmd mkdir $DSTD
        fi
        run_cmd cp -p $i $DSTD
    done

    FF="$TMP_SCM/ncbi/trace.vschema"
    if [ -f "$FF" ]
    then
        run_cmd rm $FF
    fi
}

copy_scripts ()
{
    ## Here we are copying scripts
    ##
    echo CP SCR: $TMP_BIN

    BREP="https://github.com/ncbi/sra-tools.git"

    run_cmd git clone -b VDB-4252-script https://github.com/ncbi/sra-tools.git $TMP_D/sra-tools

    SCR2CP="    \
            sra_delite.sh   \
            sra_delite.kfg   \
            README.txt   \
            DL/TEST.sh  \
            "
    SRC_D=$TMP_D/sra-tools/tools/kar
    DST_D=$TMP_BIN

    copy_f $SCR2CP

    run_cmd rm -rf $TMP_D/sra-tools
}

make_prep ()
{
    FUFIL=$( date +%y-%m-%d_%H:%M )

    echo $FUFIL >$TMP_VER/version

    if [ -d "$REP_D/$FUFIL" ]
    then
        run_cmd rm -rf $REP_D/$FUFIL
    fi
    run_cmd mv $TMP_VER $REP_D/$FUFIL

    run_cmd cp -rp $REP_D/$FUFIL $BASE_D/$FUFIL
    run_cmd mv $VER_D ${VER_D}.obsolete
    run_cmd mv $BASE_D/$FUFIL $VER_D
    run_cmd rm -rf  ${VER_D}.obsolete
}

copy_bins

copy_scripts

copy_schemas

make_prep
