#!/bin/bash

###
##  Start.
#
TVAR=`dirname $0`

SCRIPT_DIR=`cd $TVAR; pwd`

SCRIPT_NAME=`basename $0`
SCRIPT_NAME_SHORT=`basename $0 .sh`

ORIGINAL_CMD="$0 $@"

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
TEST_TAG="test"
STATUS_TAG="status"

ACCESSION_TAG="--accession"
ARCHIVE_TAG="--archive"
TARGET_TAG="--target"
CONFIG_TAG="--config"
SCHEMA_TAG="--schema"
FORCE_TAG="--force"
SKIPTEST_TAG="--skiptest"

IMPORTED_TAG="IMPORTED:"
INITIALIZED_TAG="INITIALIZED:"
DELITED_TAG="DELITED:"
DOWNLOADED_TAG="DOWNLOADED:"
REJECTED_TAG="REJECTED:"
NODELITE_TAG="NODELITE:"

SEQUENCE_TBL_NAME="SEQUENCE"
QUALITY_COL_NAME="QUALITY"

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
               $TEST_TAG - script will test 'exported' KAR archive
             $STATUS_TAG - script will report some status, or whatever.

Options:

    -h|--help - script will show that message

    $ACCESSION_TAG <name> - accession
                         String, for '$IMPORT_TAG' action only, there should
                         be defined one of '$ACCESSION_TAG' or '$ARCHIVE_TAG'
    $ARCHIVE_TAG <path>   - path to SRA archive
                         String, for '$IMPORT_TAG' action only, there should
                         be defined one of '$ACCESSION_TAG' or '$ARCHIVE_TAG'
    $TARGET_TAG <path>    - path to directory, where script will put it's output.
                         String, mandatory.
    $CONFIG_TAG <path>    - path to existing configuration file.
                         String, optional.
    $SCHEMA_TAG <paht>    - path to directory with schemas to use
                         String, mandatory for 'delite' action only.
    $FORCE_TAG            - flag to force process does not matter what
    $SKIPTEST_TAG         - flag to skip testing

EOF

    exit 100
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
    $TEST_TAG)
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
        $ACCESSION_TAG)
            TCNT=$(( $TCNT + 1 ))
            ACCESSION_VAL=${ARGS[$TCNT]}
            ;;
        $ARCHIVE_TAG)
            TCNT=$(( $TCNT + 1 ))
            ARCHIVE_VAL=${ARGS[$TCNT]}
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
        $SKIPTEST_TAG)
            SKIPTEST_VAL=1
            ;;
        *)
            usage invalid argument \'$TARG\'
            ;;
    esac

    TCNT=$(( $TCNT + 1 ))
done

if [ -n "$ACCESSION_VAL" -a -n "$ARCHIVE_VAL" ]
then
    echo "ERROR: there could be defined only '$ARCHIVE_TAG' or '$ACCESSION_TAG'" >&2
    exit 100
fi

if [ -n "$ACCESSION_VAL" ]
then
    if [[ ! $ACCESSION_VAL =~ [S,E,D]RR[0-9][0-9]*$ ]]
    then
        echo "ERROR: invalid accession format '$ACCESSION_VAL'. Should match '[S,E,D]RR[0-9][0-9]*$'">&2
        exit 101
    fi
fi

if [ -n "$ARCHIVE_VAL" ]
then
    ARCHIVE_FILE=$( readlink -e $ARCHIVE_VAL )
    if [ $? -ne 0 ]
    then
        echo "ERROR: can not stat file '$ARCHIVE_VAL'" >&2
        exit 105
    fi
fi

TRANSLATION_QTY=0
DROPCOLUMN_QTY=0

###############################################################################################
###############################################################################################
###<<>>### Location and environment.
##############################################################################################

###
##  Loading config file, if such exists, overwise will load standard config
#

if [ -n "$CONFIG_VAL" ]
then
    if [ ! -r "$CONFIG_VAL" ]
    then
        echo ERROR: can not stat config file \'$CONFIG_VAL\' >&2
        exit 102
    fi
    CONFIG_FILE=$CONFIG_VAL
    echo WARNING: loading user defined config file \'$CONFIG_FILE\' >&2
else
    TVAL=$SCRIPT_DIR/${SCRIPT_NAME_SHORT}.kfg
    if [ -r "$TVAL" ]
    then
        CONFIG_FILE=$SCRIPT_DIR/${SCRIPT_NAME_SHORT}.kfg
        echo WARNING: loading default config file \'$CONFIG_FILE\' >&2
    fi
fi

print_config_to_stdout ()
{
    if [ -z "$CONFIG_FILE" ]
    then
        echo INFO: using internal configuration settings >&2
        cat <<EOF
## Standard configuration file.
### '#'# character in beginning of line is treated as a commentary

### Schema traslations
### Syntax: translate SCHEMA_NAME OLD_VER NEW_VER
#original by Kenneth
translate NCBI:SRA:GenericFastq:consensus_nanopore        1.0     2.0
translate NCBI:SRA:GenericFastq:sequence  1.0     2.0
translate NCBI:SRA:GenericFastq:sequence_log_odds 1.0     2
translate NCBI:SRA:GenericFastq:sequence_nanopore 1.0     2.0
translate NCBI:SRA:GenericFastq:sequence_no_name  1.0     2.0
translate NCBI:SRA:Helicos:tbl:v2 1.0.4   2
translate NCBI:SRA:Illumina:qual4 2.1.0   3
translate NCBI:SRA:Illumina:tbl:phred:v2  1.0.4   2
translate NCBI:SRA:Illumina:tbl:q1:v2     1.1     2
translate NCBI:SRA:Illumina:tbl:q4:v2     1.1.0   2
translate NCBI:SRA:Illumina:tbl:v2        1.0.4   2
translate NCBI:SRA:IonTorrent:tbl:v2      1.0.3   2
translate NCBI:SRA:Nanopore:consensus     1.0     2.0
translate NCBI:SRA:Nanopore:sequence      1.0     2.0
translate NCBI:SRA:PacBio:smrt:basecalls  1.0.2   2
translate NCBI:SRA:PacBio:smrt:cons       1.0     2.0
translate NCBI:SRA:PacBio:smrt:fastq      1.0.3   2
translate NCBI:SRA:PacBio:smrt:sequence   1.0     2.0
translate NCBI:SRA:_454_:tbl:v2   1.0.7   2
translate NCBI:SRA:tbl:spotdesc   1.0.2   1.1
translate NCBI:SRA:tbl:spotdesc_nocol     1.0.2   1.1
translate NCBI:SRA:tbl:spotdesc_nophys    1.0.2   1.1
translate NCBI:WGS:tbl:nucleotide 1.1     2
translate NCBI:align:tbl:reference        2       3
translate NCBI:align:tbl:seq      1.1     2
translate NCBI:align:tbl:seq      1       2
translate NCBI:refseq:tbl:reference       1.0.2   2
translate NCBI:tbl:base_space     2.0.3   3

#added first pass
translate NCBI:SRA:GenericFastq:sequence  1       2.0
translate NCBI:SRA:Illumina:tbl:phred:v2  1.0.3   2
translate NCBI:SRA:_454_:tbl:v2   1.0.6   2

#added second pass
translate NCBI:SRA:GenericFastq:consensus_nanopore        1       2
translate NCBI:SRA:Helicos:tbl:v2 1.0.3   2

#added by Zalunin
translate NCBI:SRA:GenericFastq:db  1   2

#added during new test from script
translate NCBI:SRA:Illumina:tbl:q4:v2     1.1   2
translate NCBI:align:db:alignment_sorted    1.2.1   2
translate NCBI:align:db:alignment_sorted    1.3   2
translate NCBI:SRA:IonTorrent:tbl:v2    1.0.2   2

translate NCBI:SRA:Nanopore:db  1   2
translate NCBI:SRA:Nanopore:consensus  1   2
translate NCBI:SRA:Nanopore:sequence  1   2
translate NCBI:SRA:PacBio:smrt:db   1.0.1   2
translate NCBI:SRA:PacBio:smrt:cons 1.0.2   2
translate NCBI:SRA:PacBio:smrt:sequence 1.0.2   2

translate NCBI:align:db:alignment_sorted    1.2   2
translate NCBI:SRA:GenericFastqNoNames:db   1   2
translate NCBI:SRA:GenericFastq:sequence_no_name    1   2

translate NCBI:align:db:alignment_sorted    1.1   2
translate NCBI:align:tbl:align_sorted   1.0.1   1.2
translate NCBI:align:tbl:align_sorted   1.1   1.2
translate NCBI:align:tbl:align_unsorted 1.1 1.2
translate NCBI:SRA:Illumina:db  1   2

### Columns to drop
### Syntax: exclude COLUMN_NAME
exclude QUALITY
exclude QUALITY2
exclude CMP_QUALITY
exclude POSITION
exclude SIGNAL

### Rejected platforms
### Syntax: reject PLATFORM_NAME ["optional platform description"]
# reject SRA_PLATFORM_454 "454 architecture run"
reject SRA_PLATFORM_ABSOLID "colorspace run"

### Columns to skip during vdb-diff testing
### Syntax: diff-exclude COLUMN_NAME
diff-exclude CLIPPED_QUALITY
diff-exclude SAM_QUALITY
diff-exclude QUALITY_VALUE
diff-exclude READ_FILTER
diff-exclude RD_FILTER
diff-exclude SAM_FLAGS
diff-exclude READ_SEG
diff-exclude B_INFO
diff-exclude CSREAD
diff-exclude CLIPPED_HAS_MISMATCH
diff-exclude CLIPPED_HAS_REF_OFFSET
diff-exclude CLIPPED_MISMATCH
diff-exclude CLIPPED_READ
diff-exclude EDIT_DISTANCE
diff-exclude RIGHT_SOFT_CLIP
diff-exclude TEMPLATE_LEN

### Environment definition section.
### Syntax: NAME=VALUE
### Please, do not allow spaces between parameters
# DELITE_BIN_DIR=/panfs/pan1/trace_work/iskhakov/Tundra/KAR+TST/bin
# USE_OWN_TEMPDIR=1
### That is for docker, and please do not modify it by yourself
# DELITE_GUID=

EOF
    else
        cat "$CONFIG_FILE"
    fi
}

LINE_NUM=1
while read -r INPUT_LINE
do
    LINE_NUM=$(( $LINE_NUM + 1 ))
    case $INPUT_LINE in 
        \#*)
            :
            ;;
        *=*)
            eval $INPUT_LINE 2>/dev/null
            if [ $? -ne 0 ]
            then
                echo ERROR: invalid definition in configuration file at line $LINE_NUM >&2
                echo ERROR: invalid line \"$INPUT_LINE\" >&2
                exit 103
            fi
            ;;
        translate*)
            eval "FARG=($INPUT_LINE)"
            if [ ${#FARG[*]} -ne 4 ]
            then
                echo ERROR: invalid amount of tokens in configuration file at line $LINE_NUM >&2
                echo ERROR: invalid line \"$INPUT_LINE\" >&2
                exit 103
            fi
            TRANSLATIONS[${#TRANSLATIONS[*]}]="${FARG[1]} ${FARG[2]} ${FARG[3]}"
            ;;
        exclude*)
            eval "FARG=($INPUT_LINE)"
            if [ ${#FARG[*]} -ne 2 ]
            then
                echo ERROR: invalid amount of tokens in configuration file at line $LINE_NUM >&2
                echo ERROR: invalid line \"$INPUT_LINE\" >&2
                exit 103
            fi
            DROPCOLUMNS[${#DROPCOLUMNS[*]}]=${FARG[1]}
            ;;
        diff-exclude*)
            eval "FARG=($INPUT_LINE)"
            if [ ${#FARG[*]} -ne 2 ]
            then
                echo ERROR: invalid amount of tokens in configuration file at line $LINE_NUM >&2
                echo ERROR: invalid line \"$INPUT_LINE\" >&2
                exit 103
            fi
            if [ -z "$DIFFEXCLUDE" ]
            then
                DIFFEXCLUDE="${FARG[1]}"
            else
                DIFFEXCLUDE="${DIFFEXCLUDE},${FARG[1]}"
            fi
            ;;
        reject*)
            eval "FARG=($INPUT_LINE)"
            if [ ${#FARG[*]} -ne 2 -a ${#FARG[*]} -ne 3 ]
            then
                echo ERROR: invalid amount of tokens in configuration file at line $LINE_NUM >&2
                echo ERROR: invalid line \"$INPUT_LINE\" >&2
                exit 103
            fi
            REJECTED_ARC[${#REJECTED_ARC[*]}]=${FARG[1]}
            if [ ${#FARG[*]} -eq 3 ]
            then
                REJECTED_ARC_COMM[${#REJECTED_ARC_COMM[*]}]=${FARG[2]}
            else
                REJECTED_ARC_COMM[${#REJECTED_ARC_COMM[*]}]="unsupported"
            fi
            ;;
        *)
            if [ -n "$INPUT_LINE" ]
            then
                echo ERROR: invalid statement in configuration file at line $LINE_NUM >&2
                echo ERROR: invalid line \"$INPUT_LINE\" >&2
                exit 103
            fi
            ;;
    esac
done < <( print_config_to_stdout )

TRANSLATION_QTY=${#TRANSLATIONS[*]}

##
## Here we manually adding DROPCOLUMN_QTY to list
ORIGINAL_QUALITY="ORIGINAL_QUALITY"
DROPCOLUMNS[${#DROPCOLUMN_QTY[*]}]=$ORIGINAL_QUALITY
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
    VDBDIFF_BIN=$DELITE_BIN_DIR/vdb-diff
    VDBDUMP_BIN=$DELITE_BIN_DIR/vdb-dump
    PREFETCH_BIN=$DELITE_BIN_DIR/prefetch
    MAKEREADFILTER_BIN=$DELITE_BIN_DIR/make-read-filter
else
    KAR_BIN=$SCRIPT_DIR/kar+
    KARMETA_BIN=$SCRIPT_DIR/kar+meta
    VDBLOCK_BIN=$SCRIPT_DIR/vdb-lock
    VDBUNLOCK_BIN=$SCRIPT_DIR/vdb-unlock
    VDBVALIDATE_BIN=$SCRIPT_DIR/vdb-validate
    VDBDIFF_BIN=$SCRIPT_DIR/vdb-diff
    VDBDUMP_BIN=$SCRIPT_DIR/vdb-dump
    PREFETCH_BIN=$SCRIPT_DIR/prefetch
    MAKEREADFILTER_BIN=$SCRIPT_DIR/make-read-filter
fi

for i in KAR_BIN KARMETA_BIN VDBLOCK_BIN VDBUNLOCK_BIN VDBVALIDATE_BIN PREFETCH_BIN VDBDIFF_BIN VDBDUMP_BIN MAKEREADFILTER_BIN
do
    if [ ! -e ${!i} ]; then echo ERROR: can not stat executable \'${!i}\' >&2;          exit 104; fi
    if [ ! -x ${!i} ]; then echo ERROR: has no permission to execute for \'${!i}\' >&2; exit 104; fi
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

## There is special vairable DPEC__, which by default is integer and contains exit code of program;
## It's default value is 1, but it could be alterated and set different value, but it will be reset to 1
## once execution passed
## dpec__ function if it called with param, it will set DPEC__ value, if it called without param, it
##        will return value of DPEC__ and will reset it to 1
dpec__ ()
{
    if [ $# -ne 0 ]
    then
        DPEC__=`echo $1 | awk ' { if ( int ( $1 ) == 0 ) { print "1"; } else { print int ($1); } } ' `
    else
        RV=1
        if [ -n "$DPEC__" ]
        then
            RV=`echo $DPEC__ | awk ' { if ( int ( $1 ) == 0 ) { print "1"; } else { print int ($1); } } ' `
        fi
        DPEC__=1
        echo $RV
    fi
}

err_exit ()
{
    err_msg $@
    TEXCOD=`dpec__`
    echo "Exiting ($TEXCOD)..." >&2
    exit $TEXCOD
}

exec_cmd ()
{
    TCMD="$@"

    if [ -z "$TCMD" ]
    then
        return 0
    fi

    echo "`date +%Y-%m-%d_%H:%M:%S` #### $TCMD"
    eval $TCMD
    return $?
}

exec_cmd_exit ()
{
    TCMD="$@"

    if [ -z "$TCMD" ]
    then
        return
    fi

    echo "`date +%Y-%m-%d_%H:%M:%S` #### $TCMD"
    eval $TCMD
    if [ $? -ne 0 ]
    then
        err_exit command failed \'$TCMD\'
    fi

    ## reset return code to default
    dpec__ 1
}

###
##  Directories
#

##
## Since target is mandatory, and actually it is place where we do play
if [ -z "$TARGET_VAL" ]
then
    dpec__ 100; err_exit missed mandatory parameter \'$TARGET_TAG\'
fi

    ## Readlink -f can return non-zero result if entry
    ## parent doesn't exist
TDVAL=`readlink -f $TARGET_VAL`
if [ $? -ne 0 ]
then
    dpec__ 105; err_exit can not resolve parameter \'$TARGET_TAG\' in \'$TARGET_VAL\'
fi

TARGET_DIR=$TDVAL
DATABASE_DIR=$TARGET_DIR/work
DATABASE_CACHE_DIR=$TARGET_DIR/work.vch
NEW_KAR_FILE=$TARGET_DIR/out.sra
NEW_CACHE_FILE=$TARGET_DIR/out.sra.vdbcache
ORIG_KAR_FILE=$TARGET_DIR/in.sra
ORIG_CACHE_FILE=$TARGET_DIR/in.sra.vdbcache
STATUS_FILE=$TARGET_DIR/.status.txt
VDBCFG_NAME=vdbconfig.kfg
VDBCFG_FILE=$TARGET_DIR/$VDBCFG_NAME

## IMPORTANT NOTE:
## Prefetch will not work correctly without that
export NCBI_SETTINGS=/
export VDB_CONFIG=$VDBCFG_NAME

if [ -z "$DELITE_GUID" ]
then
    UUIDGEN=$( which uuidgen )
    if [ $? -eq 0 ]
    then
        DELITE_GUID=$( uuidgen )
    fi
fi

###############################################################################################
##  There will be description of status file, which is quite secret file
##  ...
##
###############################################################################################
log_status ()
{
    TMSG=$@

    cat <<EOF >>$STATUS_FILE
############
`date +%Y-%m-%d_%H:%M:%S` usr:[$USER] log:[$LOGNAME] pid:[$$] hst:[`hostname`]
$TMSG

EOF
}

###############################################################################################
###############################################################################################
###<<>>### Misc checks
##############################################################################################
check_rejected_run_exit ()
{
    ## Checking architecture type
    TCS=`$VDBDUMP_BIN -R 1 -f tab -C PLATFORM $ORIG_KAR_FILE 2>/dev/null`
    if [ -n "$TCS" ]
    then
        TCNT=0
        while [ $TCNT -lt ${#REJECTED_ARC[*]} ]
        do
            if [ "$TCS" = "${REJECTED_ARC[$TCNT]}" ]
            then
                log_status "$REJECTED_TAG unsupported architecture run '$TCS' detected: ${REJECTED_ARC_COMM[$TCNT]}"
                dpec__ 80; err_exit "$REJECTED_TAG unsupported architecture run '$TCS' detected: ${REJECTED_ARC_COMM[$TCNT]}"
            fi

            TCNT=$(( $TCNT + 1 ));
        done
    fi

    ## Checking colorspace by CS_NATIVE column
    TCS=`$VDBDUMP_BIN -R 1 -f tab -C CS_NATIVE $ORIG_KAR_FILE 2>/dev/null`
    if [ "$TCS" = "true" ]
    then
        log_status "$REJECTED_TAG can not process colorspace rund, and here 'CS_NATIVE' column detected"
        dpec__ 80; err_exit "$REJECTED_TAG can not process colorspace rund, and here 'CS_NATIVE' column detected"
    fi

    ## Last, my paranoidal check
    TCS=`find $DATABASE_DIR -name CSREAD -type d`
    if [ -n "$TCS" ]
    then
        log_status "$REJECTED_TAG can not process colorspace rund, and here 'CSREAD' physical column detected"
        dpec__ 80; err_exit "$REJECTED_TAG can not process colorspace rund, and here 'CSREAD' physical column detected"
    fi
}

###############################################################################################
###############################################################################################
###<<>>### Unpacking original KAR archive
##############################################################################################

check_remove_target_dir ()
{
    if [ ! -e "$TARGET_DIR" ]
    then
        return
    fi

    if [ -z "$FORCE_VAL" ]
    then
        dpec__ 106; err_exit target directory \'$TARGET_DIR\' exist.
    fi

    info_msg forcing to remove old data for \'$TARGET_DIR\'

    ## Checking that there exists status file, which should be created first.
    ## if that file does not exist, it could be some mistake
    if [ ! -e "$STATUS_FILE" ]
    then
        dpec__ 106; err_exit target directory \'$TARGET_DIR\' does not looks as valid. Please, remove it manually.
    fi

    ## Checking if there are remnaints of database, trying to unlock it
    ##
    if [ -d "$DATABASE_DIR" ]
    then
        warn_msg trying to unlock database \'$DATABASE_DIR\'
        $VDBUNLOCK_BIN $DATABASE_DIR >/dev/null 2>&1
    fi

    ## Checking if content of directory is writtable and belong to effective user
    ##
    for i in `find $TARGET_DIR`
    do
        if [ ! -w $i -o ! -O $i ]
        then
            dpec__ 106; err_exit target directory contains files which could not be delited or does not belong to current user. Please remove it manually
        fi
    done

    ## Removing contents
    ##
    dpec__ 107; exec_cmd_exit rm -r $TARGET_DIR
}

import_proc ()
{
    ##
    ## Checking args
    if [ -z "$ACCESSION_VAL" -a -z "$ARCHIVE_VAL" ]
    then
        dpec__ 100; err_exit missed mandatory parameter \'$ACCESSION_TAG\' or \'$ARCHIVE_TAG\'
    fi

    check_remove_target_dir

    info_msg "IMPORT: $ACCESSION_VAL to $TARGET_DIR"

    dpec__ 109; exec_cmd_exit mkdir $TARGET_DIR

    ##
    ## Here we are checking if we should or should not call prefetch
    if [ -n "$ACCESSION_VAL" ]
    then
        log_status $INITIALIZED_TAG $ACCESSION_VAL
    else
        log_status $INITIALIZED_TAG $ARCHIVE_FILE
    fi

    cat <<EOF >$VDBCFG_FILE
/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"
/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"
/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"
/repository/user/main/public/root = "$TARGET_DIR"
/repository/site/main/tracearc/apps/refseq/volumes/refseq = "refseq"
/repository/site/main/tracearc/apps/wgs/volumes/wgs2 = "wgs03:wgs01"
/repository/site/main/tracearc/root = "/netmnt/traces04"
/libs/cloud/report_instance_identity = "true"
# /libs/cloud/accept_aws_charges = "true"
# /libs/cloud/accept_gcp_charges = "true"
/sra/quality_type = "raw_scores"
EOF

    ###
    ##  In the case of AWS, we needed GUID for correct work
    #
    if [ -n "$DELITE_GUID" ]
    then
        echo /LIBS/GUID = \"$DELITE_GUID\" >>$VDBCFG_FILE
    fi

    info_msg Changing directory to \'$TARGET_DIR\'
    cd $TARGET_DIR

    if [ -n "$ACCESSION_VAL" ]
    then
        dpec__ 60; exec_cmd_exit $PREFETCH_BIN --max-size 1000000000 $ACCESSION_VAL

        TOUTD=$ACCESSION_VAL
        if [ ! -d "$TOUTD" ]
        then
            dpec__ 105; err_exit can not stat directory \'$TOUTD\'
        fi

        TOUTF=$TOUTD/${ACCESSION_VAL}.sra
        if [ ! -f "$TOUTF" ]
        then
            dpec__ 105; err_exit can not stat file \'$TOUTF\'
        fi

        info_msg Read `stat --format="%s" $TOUTF` bytes to \'$TARGET_DIR/$TOUTF\'

        dpec__ 61; exec_cmd_exit ln -s $TOUTF $ORIG_KAR_FILE

        TOUTC=${TOUTF}.vdbcache
        if [ -f "$TOUTC" ]
        then
            info_msg "Found .VDBCACHE file"
            dpec__ 61; exec_cmd_exit ln -s $TOUTC $ORIG_CACHE_FILE
        fi
    else
        dpec__ 61; exec_cmd_exit ln -s $ARCHIVE_FILE $ORIG_KAR_FILE

        TOUTC=${ARCHIVE_FILE}.vdbcache
        if [ -f "$TOUTC" ]
        then
            info_msg "Found .VDBCACHE file"
            dpec__ 61; exec_cmd_exit ln -s $TOUTC $ORIG_CACHE_FILE
        fi
    fi

    ICMD="$KAR_BIN "
    if [ -n "$FORCE_VAL" ]
    then
        ICMD="$ICMD --force"
    fi

    dpec__ 62; exec_cmd_exit $ICMD --extract $ORIG_KAR_FILE --directory $DATABASE_DIR

    if [ -e "$ORIG_CACHE_FILE" ]
    then
        dpec__ 62; exec_cmd_exit $ICMD --extract $ORIG_CACHE_FILE --directory $DATABASE_CACHE_DIR
    fi

    ## Checking if it is colorspace run
    check_rejected_run_exit

    log_status "$IMPORTED_TAG $ORIGINAL_CMD"

    info_msg "DONE"
}

###############################################################################################
###############################################################################################
###<<>>### Delite 
##############################################################################################
check_ready_for_delite ()
{
    ## Checking ARGS
    if [ -z "$SCHEMA_VAL" ]
    then
        dpec__ 100; err_exit missed mandatory parameter \'$SCHEMA_TAG\'
    fi

    if [ ! -e "$SCHEMA_VAL" ]
    then
        dpec__ 105; err_exit can not stat directory \'$SCHEMA_VAL\'
    fi

    SCHEMA_DIR=`cd $SCHEMA_VAL; pwd`
    if [ -z "$SCHEMA_DIR" ]
    then
        dpec__ 105; err_exit can not resolve directory \'$SCHEMA_VAL\'
    fi

    if [ ! -d "$DATABASE_DIR" ]
    then
        dpec__ 105; err_exit can not stat database \'$DATABASE_DIR\'
    fi

    if [ -e "$ORIG_CACHE_FILE" ]
    then
        if [ ! -d "$DATABASE_CACHE_DIR" ]
        then
            dpec__ 105; err_exit can not stat database VDBCACHE \'$DATABASE_CACHE_DIR\'
        fi
    fi

    if [ ! -f "$STATUS_FILE" ]
    then
        dpec__ 105; err_exit can not stat status file
    fi

    TVAR=`grep "$REJECTED_TAG" $STATUS_FILE 2>/dev/null`
    if [ -n "$TVAR" ]
    then
        dpec__ 81; err_exit status shows that object was recected for delite process \"$TVAR\"
    fi

    ## Checking if it is colorspace run
    check_rejected_run_exit
}

check_delited ()
{
    TVAR=`grep "$DELITED_TAG" $STATUS_FILE 2>/dev/null`
    if [ -n "$TVAR" ]
    then
        dpec__ 82; err_exit status shows that object was delited already: \'$TVAR\'
    fi

    TVAR=`$KARMETA_BIN --info SOFTWARE/delite $DATABASE_DIR 2>/dev/null`
    if [ -n "$TVAR" ]
    then
        dpec__ 82; err_exit object was delited already: \'$TVAR\'
    fi
}

## QUALITY column could exist at SEQUENCE table, and if there is not
## tables, the run itself is a table. If that column does not exists,
## we do reject deliting that run
##
## We ought to replace schemas for all tables, and for DB object,
## if run contains DB object ( case when there are several tables )
##
## There could be a case when DB could contain two QUALITY columns
## ( SEQUENCE and CONSENSUS ), and we will try to drop both, it
## work sometimes.
## 
## Variables we are using for modification of schemas
##     QUALITY_TABLES - tables with QUALITY column
##     OBJECTS - list of tables to modify schemas
##     ORIGINAL_QUALITY_COLUMNS - list of quality columns we need to
##                                drop at the end of delite process
rename_quality_column ()
{
    ## checking if there is 'regular' QUALITY column
    TDIR=$DATABASE_DIR/col/$QUALITY_COL_NAME
    if [ ! -d "$TDIR" ]
    then
        TDIR=$DATABASE_DIR/tbl/$SEQUENCE_TBL_NAME/col/$QUALITY_COL_NAME
        if [ ! -d "$TDIR" ]
        then
            dpec__ 83; err_exit can not stat table with $QUALITY_COL_NAME column
        fi
    fi

    for i in `find $DATABASE_DIR -type d -name $QUALITY_COL_NAME`
    do
        TDIR=`dirname $i`

        QUALITY_TABLES[${#QUALITY_TABLES[*]}]=`dirname $TDIR`

        ORIGINAL_QUALITY_COL=$TDIR/${ORIGINAL_QUALITY}
        if [ -e "$ORIGINAL_QUALITY_COL" ]
        then
            dpec__ 106; err_exit can not rename \'$i\'. Directory \'$ORIGINAL_QUALITY_COL\' already exists
        fi

        dpec__ 110; exec_cmd_exit mv $i $ORIGINAL_QUALITY_COL
        ORIGINAL_QUALITY_COLUMNS[${#ORIGINAL_QUALITY_COLUMNS[*]}]=$ORIGINAL_QUALITY_COL
    done

    if [ ${#QUALITY_TABLES[*]} -eq 0 ]
    then
        dpec__ 83; err_exit can not stat table with $QUALITY_COL_NAME column
    fi

    if [ ${#QUALITY_TABLES[*]} -gt 1 ]
    then
        warn_msg RUN HAS MORE THAN ONE $QUALITY_COL_NAME COLUMNS \[${#QUALITY_TABLES[*]}\]
    fi
}

add_object ()
{
    ON=`cd $1; pwd`

    Q=${#OBJECTS[*]}
    C=0
    while [ $C -lt $Q ]
    do
        if [ "${OBJECTS[$C]}" = "$ON" ]
        then
            return
        fi

        C=$(( $C + 1 ))
    done

    OBJECTS[${#OBJECTS[*]}]="$ON"
    OBJECTS_QTY=${#OBJECTS[*]}
}

find_objects_to_modify ()
{
    add_object $DATABASE_DIR

    TDIR="$DATABASE_DIR/tbl"
    if [ -d "$TDIR" ]
    then
        for i in `ls $TDIR`
        do
            add_object $TDIR/$i
        done
    fi
}

check_schema_valid ()
{
    S2CH=$1

    case "$S2CH" in
        NCBI:SRA:Illumina:tbl:q4*)
            log_status "$REJECTED_TAG can not process schema ${SATR[0]} yet"
            dpec__ 80; err_exit rejected \'$S2CH\' type run detected
            ;;
        NCBI:SRA:Illumina:tbl:q1:v2)
            log_status "$REJECTED_TAG can not process schema ${SATR[0]} yet"
            dpec__ 80; err_exit rejected \'$S2CH\' type run detected
            ;;
        NCBI:sra:db:trace)
            log_status "$REJECTED_TAG can not process TRACE object"
            dpec__ 80; err_exit rejected \'$S2CH\' TRACE object detected
            ;;
        NCBI:sra:tbl:trace)
            log_status "$REJECTED_TAG can not process TRACE object"
            dpec__ 80; err_exit rejected \'$S2CH\' TRACE object detected
            ;;
    esac
}

get_schema_for_object ()
{
    OPTH=$1

    NEW_SCHEMA=""
    FARR=(`$KARMETA_BIN --info schema@name $OPTH 2>/dev/null`)
    OLD_SCHEMA=${FARR[1]}
    if [ -z "$OLD_SCHEMA" ]
    then
        dpec__ 84; err_exit can not retrieve schema name for \'$OPTH\'
    fi

    if [ $TRANSLATION_QTY -eq 0 ]
    then
        return
    fi

    SATR=( `echo $OLD_SCHEMA | sed "s#\## #1"` )

    check_schema_valid ${SATR[0]}

    CNT=0
    while [ $CNT -lt $TRANSLATION_QTY ]
    do
        NATR=( ${TRANSLATIONS[$CNT]} )

        if [ "${NATR[0]}" = "${SATR[0]}" -a "${NATR[1]}" = "${SATR[1]}" ]
        then
            NEW_SCHEMA=${NATR[0]}\#${NATR[2]}
            break
        fi

        CNT=$(( $CNT + 1 ))
    done
}

modify_object ()
{
    M2D=$1

    info_msg modifying object \'$M2D\'

    get_schema_for_object $M2D

    if [ -n "$NEW_SCHEMA" ]
    then
        info_msg subst $OLD_SCHEMA to $NEW_SCHEMA
        dpec__ 63; exec_cmd_exit $KARMETA_BIN --spath $SCHEMA_DIR --updschema schema=\'$NEW_SCHEMA\' $M2D
    else
        warn_msg no subst found for $OLD_SCHEMA
    fi

    info_msg mark object DELITED

    dpec__ 63; exec_cmd_exit $KARMETA_BIN --setvalue SOFTWARE/delite@date=\"`date`\" $M2D
    dpec__ 63; exec_cmd_exit $KARMETA_BIN --setvalue SOFTWARE/delite@name=delite $M2D
    dpec__ 63; exec_cmd_exit $KARMETA_BIN --setvalue SOFTWARE/delite@vers=1.1.1 $M2D
}

modify_objects ()
{
    find_objects_to_modify

    OBJECTS_QTY=${#OBJECTS[*]}

    if [ $OBJECTS_QTY -ne 0 ]
    then
        info_msg found $OBJECTS_QTY objects to modify
        for i in ${OBJECTS[*]}
        do
            modify_object $i
        done
    fi
}

check_read_quality_exit_with_message ()
{
    TCHK_CMD="$VDBDUMP_BIN -f tab -C $QUALITY_COL_NAME -R 1 $DATABASE_DIR"
    info_msg "Checking run ## $TCHK_CMD"
    TMSG=`$TCHK_CMD 2>/dev/null | wc -l`

    if [ "$TMSG" -ne 1 ]
    then
        err_msg "Check failed ## $@ ## $TCHK_CMD"
        if [ $# -ne 0 ]
        then
            log_status "$NODELITE_TAG $@"
        else
            log_status "$NODELITE_TAG no qualities found for $DATABASE_DIR"
        fi

        dpec__ 85; err_exit $@
    else
        info_msg "Check passed ## $TCHK_CMD"
    fi
}

## Delite process is process of substituting schemas and removing QUALITY column
## with making read filter
## If delite process exited by any means, archive should remain exactly as it was
## before
delite_proc ()
{
    check_ready_for_delite

    info_msg Changing directory to \'$TARGET_DIR\'
    cd $TARGET_DIR

    if [ ! -f "$VDBCFG_NAME" ]
    then
        dpec__ 105; err_exit can not stat file \'$VDBCFG_FILE\'
    fi

    ## Checking that it was already delited
    check_delited

## KENNETH: In pseudo code, it should be something like:
## if not can_read(QUALITY):
##     exit "Can not process this run, there is no QUALITY"
##
## if not has_column(ORIGINAL_QUALITY):
##     rename_column(QUALITY, ORIGINAL_QUALITY)
##     new_schema = get_schema_substitution()
##     if new_schema:
##         update_schema(new_schema)
##         if not can_read(QUALITY):
##             exit "Can not read QUALITY, the substitute schema did not work"
##     else if not can_read(QUALITY):
##         exit "Can not read QUALITY, and there is no substitute schema"
##
## run_make_read_filter()
## drop_column(ORIGINAL_QUALITY)
## if not can_read(QUALITY):
##     exit "Can not read QUALITY, delite failed"

    check_read_quality_exit_with_message "can not process this run, there is no QUALITY"

    ## Unlocking db
    dpec__ 65; exec_cmd_exit $VDBUNLOCK_BIN $DATABASE_DIR

    rename_quality_column

    HAS_SUBST=1
    for i in ${QUALITY_TABLES[*]}
    do
        get_schema_for_object $i
        if [ -n "$NEW_SCHEMA" ]
        then
            info_msg found substitute schema $i : $OLD_SCHEMA -\> $NEW_SCHEMA
        else
            info_msg no substitute schema $i : $OLD_SCHEMA -\> NONE
            HAS_SUBST=0
            break
        fi
    done

    if [ $HAS_SUBST -eq 1 ]
    then
        modify_objects
        check_read_quality_exit_with_message "Can not read $QUALITY_COL_NAME, the substitute schema did not work"
    else
        check_read_quality_exit_with_message "Can not read $QUALITY_COL_NAME, and there is no substitute schema"
    fi

    ## Make ReadFilter
    if [ -d "$DATABASE_CACHE_DIR" ]
    then
        VCH_PARAM="--vdbcache $DATABASE_CACHE_DIR"
    fi

    if [ -n "$USE_OWN_TEMPDIR" ]
    then
        TTMP_DIR=$TARGET_DIR/temp
        dpec__ 109; exec_cmd_exit mkdir $TTMP_DIR
        dpec__ 64; exec_cmd_exit $MAKEREADFILTER_BIN -t $TTMP_DIR $VCH_PARAM $DATABASE_DIR
        dpec__ 107; exec_cmd_exit rm -rf $TTMP_DIR
    else
        dpec__ 64; exec_cmd_exit $MAKEREADFILTER_BIN $VCH_PARAM $DATABASE_DIR
    fi

    ## Dropping original quality columns
    for i in ${ORIGINAL_QUALITY_COLUMNS[*]}
    do
        dpec__ 107; exec_cmd_exit rm -rf $i
    done

    check_read_quality_exit_with_message "Can not read $QUALITY_COL_NAME, delite failed"

    ## Locking db
    dpec__ 66; exec_cmd_exit $VDBLOCK_BIN $DATABASE_DIR

    log_status "$DELITED_TAG $ORIGINAL_CMD" 

    info_msg "DONE"
}

###############################################################################################
###############################################################################################
###<<>>### Exporting
##############################################################################################
check_ready_for_export ()
{
    if [ ! -d "$DATABASE_DIR" ]
    then
        dpec__ 105; err_exit can not stat database \'$DATABASE_DIR\'
    fi

    if [ ! -f "$STATUS_FILE" ]
    then
        dpec__ 105; err_exit can not stat status file
    fi

    ## Checking if it is colorspace run
    check_rejected_run_exit

    TVAR=`grep "$DELITED_TAG" $STATUS_FILE 2>/dev/null`
    if [ -z "$TVAR" ]
    then
        dpec__ 86; err_exit status shows that object was not delited yet
    fi

    TVAR=`$KARMETA_BIN --info SOFTWARE/delite $DATABASE_DIR 2>/dev/null`
    if [ -z "$TVAR" ]
    then
        dpec__ 86; err_exit object was not delited yet
    fi
}

find_columns_to_drop ()
{
    if [ $DROPCOLUMN_QTY -eq 0 ]
    then
        warn_msg there are no column names to drop defined
        return
    fi

    cd $DATABASE_DIR >/dev/null

    for i in ${DROPCOLUMNS[*]}
    do
        for u in `find . -type d -name $i`
        do
            TD=$u
            info_msg found: $TD
            TO_DROP[${#TO_DROP[*]}]=$TD
        done
    done

    DROP_QTY=${#TO_DROP[*]}

    cd - >/dev/null
}

check_read_and_quality_len ()
{
    K2T=$1

    if [ ! -r "$K2T" ]
    then
        dpec__ 105; err_exit can not stat KAR file \'$K2T\'
    fi

    echo "`date +%Y-%m-%d_%H:%M:%S` #### Checking READ and QUALITY array lengths"
    $VDBDUMP_BIN $K2T -f tab '-C READ,(INSDC:quality:text:phred_33)QUALITY' -u | awk '$1 != $2 { exit (1); } ' 
    if [ $? -ne 0 ]
    then
        dpec__ 88; err_exit READ and QUALITY length are different for $K2T
    fi

    echo "`date +%Y-%m-%d_%H:%M:%S` #### DONE: array lengths are GOOD"

    info_msg PASSED
}

test_kar ()
{
    check_read_and_quality_len $NEW_KAR_FILE

    if [ ! -f $ORIG_KAR_FILE ]
    then
        dpec__ 105; err_exit SKIPPING DIFF TESTS for \'$NEW_KAR_FILE\', can not stat original KAR file \'$ORIG_KAR_FILE\'
    fi

    exec_cmd $VDBVALIDATE_BIN -x $NEW_KAR_FILE
    if [ $? -ne 0 ]
    then
        warn_msg vdb-validate step failed, checking original KAR file
        exec_cmd $VDBVALIDATE_BIN -x $ORIG_KAR_FILE
        if [ $? -ne 0 ]
        then
            dpec__ 87; err_exit corrupted original KAR file
        else
            dpec__ 67; err_exit vdb-validate failed or original file, that means DELITE process failed
        fi
    fi

    TCMD="$VDBDIFF_BIN $ORIG_KAR_FILE $NEW_KAR_FILE -c -i"

    TDC="$DIFFEXCLUDE"

    if [ $DROPCOLUMN_QTY -ne 0 ]
    then
        TCNT=0
        while [ $TCNT -lt $DROPCOLUMN_QTY ]
        do
            TCN=${DROPCOLUMNS[$TCNT]}

            if [ -n "$TDC" ]
            then
                TDC="${TDC},$TCN"
            else
                TDC="$TCN"
            fi

            if [ "$TCN" = "SIGNAL" ]
            then
                TDC="${TDC},SIGNAL_LEN,SPOT_DESC"
            fi

            TCNT=$(( $TCNT + 1 ))
        done
    fi

    if [ -n "$TDC" ]
    then
        TCMD="$TCMD -x $TDC"
    fi

    dpec__ 68; exec_cmd_exit $TCMD
}

check_force_remove_old_kar ()
{
    F2R=$1
    MSS=$2

    if [ -z "$MSS" ]
    then
        MSS="KAR"
    fi

    if [ -f "$F2R" ]
    then
        if [ -n "$FORCE_VAL" ]
        then
            info_msg forcing to remove old $MSS file \'$F2R\'
            dpec__ 107; exec_cmd_exit rm "$F2R"
        else
            dpec__ 106; err_exit old $MSS file found \'$F2R\'
        fi
    fi
}

kar_new ()
{
    check_force_remove_old_kar $NEW_KAR_FILE KAR
    check_force_remove_old_kar $NEW_CACHE_FILE .VDBCACHE

    TCMD="$KAR_BIN"
    if [ -n "$FORCE_VAL" ]
    then
        TCMD="$TCMD -f"
    fi

    TCNT=0
    while [ $TCNT -lt $DROP_QTY ]
    do
        TCMD="$TCMD --drop ${TO_DROP[$TCNT]}"

        TCNT=$(( $TCNT + 1 ))
    done

    TCMD="$TCMD --create $NEW_KAR_FILE --directory $DATABASE_DIR"

    dpec__ 62; exec_cmd_exit $TCMD

    if [ -d "$DATABASE_CACHE_DIR" ]
    then
        info_msg "Creating .VDBCACHE file"

        TCMD="$KAR_BIN"
        if [ -n "$FORCE_VAL" ]
        then
            TCMD="$TCMD -f"
        fi

        TCMD="$TCMD --create $NEW_CACHE_FILE --directory $DATABASE_CACHE_DIR"

        dpec__ 62; exec_cmd_exit $TCMD
    fi
}

print_stats ()
{
    NEW_SIZE=`stat --format="%s" $NEW_KAR_FILE`

    ORIG_FILE=`readlink -f $ORIG_KAR_FILE 2>/dev/null`

    if [ -f "$ORIG_FILE" ]
    then
        OLD_SIZE=`stat --format="%s" $ORIG_FILE`
    else
        OLD_SIZE=$NEW_SIZE
    fi

    info_msg New KAR size $NEW_SIZE
    info_msg Old KAR size $OLD_SIZE
    if [ $OLD_SIZE -ne 0 ]
    then
        info_msg Diff $(( $OLD_SIZE - $NEW_SIZE )) \($(( $NEW_SIZE * 100 / $OLD_SIZE ))%\)
    else
        info_msg Diff $(( $OLD_SIZE - $NEW_SIZE ))
    fi
}

export_proc ()
{
    ## checking if it is was delited
    check_ready_for_export

    info_msg Changing directory to \'$TARGET_DIR\'
    cd $TARGET_DIR

    if [ ! -f "$VDBCFG_NAME" ]
    then
        dpec__ 105; err_exit can not stat file \'$VDBCFG_FILE\'
    fi

    ## looking up for all columns to drop
    find_columns_to_drop

    ## writing delited kar archive
    kar_new


    if [ -n "$SKIPTEST_VAL" ]
    then
        warn_msg skipping tests for \'$NEW_KAR_FILE\' ...
        return
    else
        test_kar
    fi

    ## just printing stats
    print_stats

    info_msg "DONE"
}

###############################################################################################
###############################################################################################
###<<>>### Test
##############################################################################################
check_ready_for_test ()
{
    if [ ! -f "$STATUS_FILE" ]
    then
        dpec__ 105; err_exit can not stat status file
    fi

    TVAR=`grep "$DELITED_TAG" $STATUS_FILE 2>/dev/null`
    if [ -z "$TVAR" ]
    then
        dpec__ 86; err_exit status shows that object was not delited yet
    fi

    if [ ! -e "$ORIG_KAR_FILE" ]
    then
        dpec__ 105; err_exit can not stat original KAR file \'$ORIG_KAR_FILE\'
    fi

    if [ ! -f "$NEW_KAR_FILE" ]
    then
        dpec__ 105; err_exit can not stat delited KAR file \'$NEW_KAR_FILE\'
    fi

    TVAR=`$KARMETA_BIN --info SOFTWARE/delite $NEW_KAR_FILE 2>/dev/null`
    if [ -z "$TVAR" ]
    then
        dpec__ 86; err_exit object was not delited yet
    fi

    if [ -h "$ORIG_CACHE_FILE" ]
    then
        if [ ! -e "$ORIG_CACHE_FILE" ]
        then
            dpec__ 105; err_exit can not stat .VDBCACHE for delited KAR file \'$ORIG_CACHE_FILE\'
        fi

        if [ ! -f "$NEW_CACHE_FILE" ]
        then
            dpec__ 105; err_exit can not stat .VDBCACHE for delited KAR file \'$NEW_CACHE_FILE\'
        fi
    fi
}

test_proc ()
{
    ## checking if it is was delited
    check_ready_for_test

    info_msg Changing directory to \'$TARGET_DIR\'
    cd $TARGET_DIR

    if [ ! -f "$VDBCFG_NAME" ]
    then
        dpec__ 105; err_exit can not stat file \'$VDBCFG_FILE\'
    fi

    test_kar

    info_msg "DONE"
}

###############################################################################################
###############################################################################################
###<<>>### Status
##############################################################################################

status_proc ()
{
    if [ ! -d "$TARGET_DIR" ]
    then
        dpec__ 105; err_exit can not stat directory \'$TARGET_DIR\'
    fi

    if [ ! -f "$STATUS_FILE" ]
    then
        dpec__ 105; err_exit can not stat status file
    fi

    cat "$STATUS_FILE"

    if [ -f "$NEW_KAR_FILE" ]
    then
        info_msg found delited KAR archive \'$NEW_KAR_FILE\'
    fi

    info_msg DONE
}

###############################################################################################
###############################################################################################
###<<>>### That is main line of script
##############################################################################################

trap "dpec__ 69; err_exit received SIGINT signal, exiting" SIGINT
trap "dpec__ 69; err_exit received SIGTERM signal, exiting" SIGTERM

eval $ACTION_PROC

