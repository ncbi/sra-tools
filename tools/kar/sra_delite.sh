#!/bin/bash

###
##  Start.
#
TVAR=`dirname $0`

SCRIPT_DIR=`cd $TVAR; pwd`

SCRIPT_NAME=`basename $0`
SCRIPT_NAME_SHORT=`basename $0 .sh`

###############################################################################################
###############################################################################################
###<<>>### Initial command line arguments parsing, usage, and help
##############################################################################################

###
##  Used words and other
#
IMPORT_TAG="import"
DELITE_TAG="delite"
EXPORT_TAG="export"
STATUS_TAG="status"

SOURCE_TAG="--source"
TARGET_TAG="--target"
CONFIG_TAG="--config"
SCHEMA_TAG="--schema"
FORCE_TAG="--force"

###
##  Usage
#
usage () 
{
    TMSG="$@"
    if [ -n "$TMSG" ]
    then
        cat << EOF >&2

ERROR: $TMSG

EOF
    fi

    cat << EOF >&2

Syntax:
    $SCRIPT_NAME action [ options ]

Where :

    action - is a word which defines which procedure script will follow.
             Action values are:

             $IMPORT_TAG - script will download and/or unpack archive to
                      working directory
             $DELITE_TAG - script will perform DELITE on database content
             $EXPORT_TAG - script will create 'delited' KAR archive
             $STATUS_TAG - script will report some status, or whatever.

Options:

    -h|--help - script will show that message

    $SOURCE_TAG <name> - path to KAR archive, which could be as accesssion
                      as local path.
                      String, mandatory for 'import' action only.
    $TARGET_TAG <path> - path to directory, where script will put it's output.
                      String, mandatory.
    $CONFIG_TAG <path> - path to existing configuration file.
                      String, optional.
    $SCHEMA_TAG <paht> - path to directory with schemas to use
                      String, mandatory for 'delite' action only.
    $FORCE_TAG         - flag to force process does not matter what

EOF

    exit 1
}

###
##  Inigial arguments processing
#
ARGS=( $@ )
ARG_QTY=${#ARGS[*]}

if [ $ARG_QTY -eq 0 ]
then
    usage missing arguments
fi

ACTION=${ARGS[0]}
case $ACTION in
    $IMPORT_TAG)
        ;;
    $DELITE_TAG)
        ;;
    $EXPORT_TAG)
        ;;
    $STATUS_TAG)
        ;;
    *)
        usage invalid action \'$ACTION\'
        ;;
esac

ACTION_PROC="${ACTION}_proc"

TCNT=1
while [ $TCNT -lt $ARG_QTY ]
do
    TARG=${ARGS[$TCNT]}

    case $TARG in
        -h)
            usage
            ;;
        --help)
            usage
            ;;
        $SOURCE_TAG)
            TCNT=$(( $TCNT + 1 ))
            SOURCE_VAL=${ARGS[$TCNT]}
            ;;
        $TARGET_TAG)
            TCNT=$(( $TCNT + 1 ))
            TARGET_VAL=${ARGS[$TCNT]}
            ;;
        $CONFIG_TAG)
            TCNT=$(( $TCNT + 1 ))
            CONFIG_VAL=${ARGS[$TCNT]}
            ;;
        $SCHEMA_TAG)
            TCNT=$(( $TCNT + 1 ))
            SCHEMA_VAL=${ARGS[$TCNT]}
            ;;
        $FORCE_TAG)
            FORCE_VAL=1
            ;;
        *)
            usage invalid argument \'$TARG\'
            ;;
    esac

    TCNT=$(( $TCNT + 1 ))
done

###############################################################################################
###############################################################################################
###<<>>### Location and environment.
##############################################################################################

###
##  Loading config file, if such exists
#

if [ -n "$CONFIG_VAL" ]
then
    if [ ! -f "$CONFIG_VAL" ]
    then
        echo ERROR: can not stat config file \'$CONFIG_VAL\' >&2
        exit 1
    fi
    CONFIG_FILE=$CONFIG_VAL
    echo WARNING: loading user defined config file \'$CONFIG_FILE\'
else
    TVAL=$SCRIPT_DIR/${SCRIPT_NAME_SHORT}.kfg
    if [ -f "$TVAL" ]
    then
        CONFIG_FILE=$SCRIPT_DIR/${SCRIPT_NAME_SHORT}.kfg
        echo WARNING: loading default config file \'$CONFIG_FILE\'
    fi
fi

if [ -n "$CONFIG_FILE" ]
then
    . $CONFIG_FILE
    if [ $? -ne 0 ]
    then
        echo ERROR: invalid config file \'$CONFIG_FILE\'
        exit 1
    fi
else

### Schema traslations
#original by Kenneth
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:consensus_nanopore        1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence  1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence_log_odds 1.0     2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence_nanopore 1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence_no_name  1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Helicos:tbl:v2 1.0.4   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Illumina:qual4 2.1.0   3"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Illumina:tbl:phred:v2  1.0.4   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Illumina:tbl:q1:v2     1.1     2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Illumina:tbl:q4:v2     1.1.0   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Illumina:tbl:v2        1.0.4   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:IonTorrent:tbl:v2      1.0.3   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Nanopore:consensus     1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Nanopore:sequence      1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:PacBio:smrt:basecalls  1.0.2   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:PacBio:smrt:cons       1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:PacBio:smrt:fastq      1.0.3   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:PacBio:smrt:sequence   1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:_454_:tbl:v2   1.0.7   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:tbl:spotdesc   1.0.2   1.1"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:tbl:spotdesc_nocol     1.0.2   1.1"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:tbl:spotdesc_nophys    1.0.2   1.1"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:WGS:tbl:nucleotide 1.1     2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:align:tbl:reference        2       3"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:align:tbl:seq      1.1     2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:align:tbl:seq      1       2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:refseq:tbl:reference       1.0.2   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:tbl:base_space     2.0.3   3"

#added first pass
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence  1       2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence_nanopore 1       2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence_no_name  1       2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Illumina:tbl:phred:v2  1.0.3   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Nanopore:sequence      1       2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:PacBio:smrt:cons       1.0.2   2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:PacBio:smrt:sequence   1.0.2   2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:_454_:tbl:v2   1.0.6   2"

#added second pass
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:consensus_nanopore        1       2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:GenericFastq:sequence_log_odds 1       2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Helicos:tbl:v2 1.0.3   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NNCBI:SRA:Nanopore:consensus     1       2"

### Columns to drop
DROPCOLUMNS[${#DROPCOLUMNS[*]}]=QUALITY
DROPCOLUMNS[${#DROPCOLUMNS[*]}]=QUALITY2
DROPCOLUMNS[${#DROPCOLUMNS[*]}]=CMP_QUALITY
DROPCOLUMNS[${#DROPCOLUMNS[*]}]=POSITION
DROPCOLUMNS[${#DROPCOLUMNS[*]}]=SIGNAL

### Common section
# DELITE_BIN_DIR="/panfs/pan1/trace_work/iskhakov/Tundra/KAR+TST/bin"

fi

TRANSLATION_QTY=${#TRANSLATIONS[*]}
DROPCOLUMN_QTY=${#DROPCOLUMNS[*]}

###
##  Binaries
#
if [ -n "$DELITE_BIN_DIR" ]
then
    echo WARNING: using alternative bin directory \'$DELITE_BIN_DIR\' >&2
    KAR_BIN=$DELITE_BIN_DIR/kar+
    KARMETA_BIN=$DELITE_BIN_DIR/kar+meta
    VDBLOCK_BIN=$DELITE_BIN_DIR/vdb-lock
    VDBUNLOCK_BIN=$DELITE_BIN_DIR/vdb-unlock
    VDBVALIDATE_BIN=$DELITE_BIN_DIR/vdb-validate
else
    KAR_BIN=$SCRIPT_DIR/kar+
    KARMETA_BIN=$SCRIPT_DIR/kar+meta
    VDBLOCK_BIN=$SCRIPT_DIR/vdb-lock
    VDBUNLOCK_BIN=$SCRIPT_DIR/vdb-unlock
    VDBVALIDATE_BIN=$SCRIPT_DIR/vdb-validate
fi

for i in KAR_BIN KARMETA_BIN VDBLOCK_BIN VDBUNLOCK_BIN VDBVALIDATE_BIN
do
    if [ ! -e ${!i} ]; then echo ERROR: can not stat executable \'${!i}\' >&2; exit 1; fi
    if [ ! -x ${!i} ]; then echo ERROR: has no permission to execute for \'${!i}\' >&2; exit 1; fi
done

###
##  Useful reuseful code
#
info_msg ()
{
    TMSG="$@"

    if [ -n "$TMSG" ]
    then
        echo `date +%Y-%m-%d_%H:%M:%S` INFO: $TMSG
    fi
}

warn_msg ()
{
    TMSG="$@"

    if [ -n "$TMSG" ]
    then
        echo `date +%Y-%m-%d_%H:%M:%S` WARNING: $TMSG >&2
    fi
}

err_msg ()
{
    TMSG="$@"

    echo >&2
    if [ -n "$TMSG" ]
    then
        echo `date +%Y-%m-%d_%H:%M:%S` ERROR: $TMSG >&2
    else
        echo `date +%Y-%m-%d_%H:%M:%S` ERROR: unknown error >&2
    fi
}

err_exit ()
{
    err_msg $@
    echo Exiting ... >&2
    exit 1
}

exec_cmd_exit ()
{
    TCMD="$@"

    if [ -z "$TCMD" ]
    then
        return
    fi

    echo "`date +%Y-%m-%d_%H:%M:%S` ## $TCMD"
    eval $TCMD
    if [ $? -ne 0 ]
    then
        err_exit command failed \'$TCMD\'
    fi
}

###
##  Directories
#

##
## Since target is mandatory, and actually it is place where we do play
if [ -z "$TARGET_VAL" ]
then
    err_exit missed mandatory parameter \'$TARGET_TAG\'
fi

TARGET_DIR=$TARGET_VAL
DATABASE_DIR=$TARGET_DIR/orig
NEWKAR_FILE=$TARGET_DIR/new.kar
PRESERVED_KAR_FILE=$TARGET_DIR/preserved.kar
STATUS_FILE=$TARGET_DIR/.status.txt

###############################################################################################
###############################################################################################
###<<>>### Unpacking original KAR archive
##############################################################################################

import_proc ()
{
    ##
    ## Checking args
    if [ -z "$SOURCE_VAL" ]
    then
        err_exit missed mandatory parameter \'SOURCE_VAL\'
    fi

    if [ -d "$TARGET_DIR" ]
    then
        if [ -n "$FORCE_VAL" ]
        then
            info_msg forcing to remove old data for \'$TARGET_DIR\'
            exec_cmd_exit rm -r $TARGET_DIR
        else
            err_exit target directory \'$TARGET_DIR\' exist.
        fi
    fi

    exec_cmd_exit mkdir $TARGET_DIR

}

eval $ACTION_PROC

