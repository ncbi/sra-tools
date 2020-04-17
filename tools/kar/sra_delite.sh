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
STATUS_TAG="status"

ACCESSION_TAG="--accession"
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

SEQUENCE_COLUMN_NAME="SEQUENCE"

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

    $ACCESSION_TAG <name> - accession
                         String, mandatory for 'import' action only.
    $TARGET_TAG <path>    - path to directory, where script will put it's output.
                         String, mandatory.
    $CONFIG_TAG <path>    - path to existing configuration file.
                         String, optional.
    $SCHEMA_TAG <paht>    - path to directory with schemas to use
                         String, mandatory for 'delite' action only.
    $FORCE_TAG            - flag to force process does not matter what
    $SKIPTEST_TAG         - flag to skip testing

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
        $ACCESSION_TAG)
            TCNT=$(( $TCNT + 1 ))
            ACCESSION_VAL=${ARGS[$TCNT]}
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

if [ -n "$ACCESSION_VAL" ]
then
    if [[ ! $ACCESSION_VAL =~ [S,E,D]RR[0-9][0-9]*$ ]]
    then
        echo "ERROR: invalid accession format '$ACCESSION_VAL'. Should match '[S,E,D]RR[0-9][0-9]*$'">&2
        exit 1
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
        exit 1
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
exclude QUALITY
exclude QUALITY2
exclude CMP_QUALITY
exclude POSITION
exclude SIGNAL

### Environment definition section.
### Please, do not allow spaces between parameters
# DELITE_BIN_DIR=/panfs/pan1/trace_work/iskhakov/Tundra/KAR+TST/bin

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
                echo ERROR: invalid definition in configuration file at line $LINE_NUM
                echo ERROR: invalid line \[$INPUT_LINE\]
                exit 1
            fi
            ;;
        translate*)
            WNUM=`echo $INPUT_LINE | wc -w` 2>/dev/null
            if [ $WNUM -ne 4 ]
            then
                echo ERROR: invalid amount of tokens in configuration file at line $LINE_NUM
                echo ERROR: invalid line \[$INPUT_LINE\]
                exit 1
            fi
            TRANSLATIONS[${#TRANSLATIONS[*]}]=`echo $INPUT_LINE | awk ' { print $2 " " $3 " " $4 } '`
            ;;
        exclude*)
            WNUM=`echo $INPUT_LINE | wc -w` 2>/dev/null
            if [ $WNUM -ne 2 ]
            then
                echo ERROR: invalid amount of tokens in configuration file at line $LINE_NUM
                echo ERROR: invalid line \[$INPUT_LINE\]
                exit 1
            fi
            DROPCOLUMNS[${#DROPCOLUMNS[*]}]=`echo $INPUT_LINE | awk ' { print $2 } '`
            ;;
        *)
            if [ -n "$INPUT_LINE" ]
            then
                echo ERROR: invalid statement in configuration file at line $LINE_NUM
                echo ERROR: invalid line \[$INPUT_LINE\]
                exit 1
            fi
            ;;
    esac
done < <( print_config_to_stdout )

TRANSLATION_QTY=${#TRANSLATIONS[*]}

##
## Here we manually adding DROPCOLUMN_QTY to list
ORIGINAL_QUALITY=ORIGINAL_QUALITY
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
NEW_KAR_FILE=$TARGET_DIR/new.kar
ORIG_KAR_FILE=$TARGET_DIR/orig.kar
STATUS_FILE=$TARGET_DIR/.status.txt
VDBCFG_NAME=vdbconfig.kfg
VDBCFG_FILE=$TARGET_DIR/$VDBCFG_NAME

## IMPORTANT NOTE:
## Prefetch will not work correctly without that
export NCBI_SETTINGS=/
export VDB_CONFIG=$VDBCFG_NAME

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
check_colorspace_exit ()
{
    TCS=`find $DATABASE_DIR -name CSREAD -type d`
    if [ -n "$TCS" ]
    then
        log_status "$REJECTED_TAG can not procees colorspace type runs."
        err_exit colorspace type run detected
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
        err_exit target directory \'$TARGET_DIR\' exist.
    fi

    info_msg forcing to remove old data for \'$TARGET_DIR\'

    ## Checking that there exists status file, which should be created first.
    ## if that file does not exist, it could be some mistake
    if [ ! -e "$STATUS_FILE" ]
    then
        err_exit target directory \'$TARGET_DIR\' does not looks as valid. Please, remove it manually.
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
            err_exit target directory contains files which could not be delited or does not belong to current user. Please remove it manually
        fi
    done

    ## Removing contents
    ##
    exec_cmd_exit rm -r $TARGET_DIR
}

import_proc ()
{
    ##
    ## Checking args
    if [ -z "$ACCESSION_VAL" ]
    then
        err_exit missed mandatory parameter \'$ACCESSION_TAG\'
    fi

    check_remove_target_dir

    info_msg "IMPORT: $ACCESSION_VAL to $TARGET_DIR"

    exec_cmd_exit mkdir $TARGET_DIR
    log_status $INITIALIZED_TAG $ACCESSION_VAL

    cat <<EOF >$VDBCFG_FILE
/repository/remote/main/SDL.2/resolver-cgi = "https://locate.ncbi.nlm.nih.gov/sdl/2/retrieve"
/repository/user/main/public/apps/refseq/volumes/refseq = "refseq"
/repository/user/main/public/apps/wgs/volumes/wgsFlat = "wgs"
/repository/user/main/public/root = "$TARGET_DIR"
/repository/site/main/tracearc/apps/refseq/volumes/refseq = "refseq"
/repository/site/main/tracearc/apps/wgs/volumes/wgs2 = "wgs03:wgs01"
/repository/site/main/tracearc/root = "/netmnt/traces04"
/sra/quality_type = "raw_scores"
EOF

    info_msg Changing directory to \'$TARGET_DIR\'
    cd $TARGET_DIR

    exec_cmd_exit $PREFETCH_BIN --max-size 1000000000 $ACCESSION_VAL

    TOUTD=$TARGET_DIR/$ACCESSION_VAL
    if [ ! -d "$TOUTD" ]
    then
        err_exit can not stat directory \'$TOUTD\'
    fi

    TOUTF=$TOUTD/${ACCESSION_VAL}.sra
    if [ ! -f "$TOUTF" ]
    then
        err_exit can not stat file \'$TOUTF\'
    fi

    info_msg Read `stat --format="%s" $TOUTF` bytes to \'$TOUTF\'

    exec_cmd_exit ln -s $TOUTF $ORIG_KAR_FILE

    ICMD="$KAR_BIN "
    if [ -n "$FORCE_VAL" ]
    then
        ICMD="$ICMD --force"
    fi

    exec_cmd_exit $ICMD --extract $ORIG_KAR_FILE --directory $DATABASE_DIR

    ## Checking if it is colorspace run
    check_colorspace_exit

    log_status "$IMPORTED_TAG $ORIGINAL_CMD"

    info_msg "DONE"

exit 1
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
        err_exit missed mandatory parameter \'$SCHEMA_TAG\'
    fi

    if [ ! -e "$SCHEMA_VAL" ]
    then
        err_exit can not stat directory \'$SCHEMA_VAL\'
    fi

    if [ ! -d "$DATABASE_DIR" ]
    then
        err_exit can not stat database \'$DATABASE_DIR\'
    fi

    if [ ! -f "$STATUS_FILE" ]
    then
        err_exit can not stat status file
    fi

    TVAR=`grep "$REJECTED_TAG" $STATUS_FILE 2>/dev/null`
    if [ -n "$TVAR" ]
    then
        err_exit status shows that object was recected for delite process \"$TVAR\"
    fi

    ## Checking if it is colorspace run
    check_colorspace_exit
}

check_delited ()
{
    TVAR=`grep "$DELITED_TAG" $STATUS_FILE 2>/dev/null`
    if [ -n "$TVAR" ]
    then
        err_exit status shows that object was delited already: \'$TVAR\'
    fi

    TVAR=`$KARMETA_BIN --info SOFTWARE/delite $DATABASE_DIR 2>/dev/null`
    if [ -n "$TVAR" ]
    then
        err_exit object was delited already: \'$TVAR\'
    fi
}

## QUALITY column exists only at SEQUENCE table, and if there is not
## tables, the run itself is a table
## We should replace schemas for all tables, and for DB object, if run
## contains DB object ( case when there are several tables )
## There are two variables we are using for modification of schemas
## QUALITY_TABLES - tables containing QUALITY column, and OBJECTS -
## list of tables to modify schemas
## Variable ORIGINAL_QUALITY_COLUMNS is need to drop it
rename_quality_column ()
{
    for i in `find $DATABASE_DIR -type d -name QUALITY`
    do
        # if [ -n "$SEQUENCE_TABLE" ]
        # then
            # err_exit invalid run object, has more than one QUALITY columns
        # fi

        TDIR=`dirname $i`

        QUALITY_TABLES[${#QUALITY_TABLES[*]}]=`dirname $TDIR`

        ORIGINAL_QUALITY_COL=$TDIR/${ORIGINAL_QUALITY}
        if [ -e "$ORIGINAL_QUALITY_COL" ]
        then
            err_exit can not rename \'$i\'. Directory \'$ORIGINAL_QUALITY_COL\' already exists
        fi
        exec_cmd_exit mv $i $ORIGINAL_QUALITY_COL
        ORIGINAL_QUALITY_COLUMNS[${#ORIGINAL_QUALITY_COLUMNS[*]}]=$ORIGINAL_QUALITY_COL
    done

    # if [ -z "$SEQUENCE_TABLE" ]
    if [ ${#QUALITY_TABLES[*]} -eq 0 ]
    then
        # err_exit invalid run object, can not stat SEQUENCE table
        err_exit invalid run object, can not stat table with QUALITY
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
            err_exit rejected \'$S2CH\' type run detected
            ;;
        NCBI:SRA:Illumina:tbl:q1:v2)
            log_status "$REJECTED_TAG can not process schema ${SATR[0]} yet"
            err_exit rejected \'$S2CH\' type run detected
            ;;
        NCBI:sra:db:trace)
            log_status "$REJECTED_TAG can not process TRACE object"
            err_exit rejected \'$S2CH\' TRACE object detected
            ;;
        NCBI:sra:tbl:trace)
            log_status "$REJECTED_TAG can not process TRACE object"
            err_exit rejected \'$S2CH\' TRACE object detected
            ;;
    esac
}

get_schema_for_object ()
{
    OPTH=$1

    NEW_SCHEMA=""
    OLD_SCHEMA=`$KARMETA_BIN --info schema@name $OPTH 2>/dev/null | awk ' { print $2 } '`
    if [ -z "$OLD_SCHEMA" ]
    then
        err_exit can not retrieve schema name for \'$OPTH\'
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
        exec_cmd_exit $KARMETA_BIN --spath $SCHEMA_VAL --updschema schema=\'$NEW_SCHEMA\' $M2D
    else
        warn_msg no subst found for $OLD_SCHEMA
    fi

    info_msg mark object DELITED

    exec_cmd_exit $KARMETA_BIN --setvalue SOFTWARE/delite@date=\"`date`\" $M2D
    exec_cmd_exit $KARMETA_BIN --setvalue SOFTWARE/delite@name=delite $M2D
    exec_cmd_exit $KARMETA_BIN --setvalue SOFTWARE/delite@vers=1.1.1 $M2D
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

is_make_read_filter_applicable ()
{
    ## first we check if it is DB or TABLE. 
    ## if it TABLE, by default it is SEQUENCE table
    TFF=`find $DATABASE_DIR -type d -name tbl`
    if [ -z "$TFF" ]
    then
        return 0;
    fi

    ## second if it is db, it should contain "tbl/SEQUENCE" subdirectory
    ## if not, it is make_read_filter is not applicable
    for i in $TFF
    do
        if [ -d "$i/$SEQUENCE_COLUMN_NAME" ]
        then
            return 0;
        fi
    done

    return 1
}

do_make_read_filter ()
{
    ## First we should be sure that we need to call make_read_filter utility
    is_make_read_filter_applicable
    if [ $? -ne 0 ]
    then
        info_msg $SEQUENCE_COLUMN_NAME column does not present. Skipping make_read_filter step.
        return
    fi

    TTMP_DIR=$TARGET_DIR/temp
    exec_cmd_exit mkdir $TTMP_DIR

    exec_cmd_exit $MAKEREADFILTER_BIN -t $TTMP_DIR $DATABASE_DIR

    exec_cmd_exit rm -rf $TTMP_DIR
}

check_read_quality_exit_with_message ()
{
    TCHK_CMD="$VDBDUMP_BIN -f tab -C QUALITY -R 1 $DATABASE_DIR"
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

        err_exit $@
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
        err_exit can not stat file \'$VDBCFG_FILE\'
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
    exec_cmd_exit $VDBUNLOCK_BIN $DATABASE_DIR

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
        fi
    done
    # get_schema_for_object $SEQUENCE_TABLE

    # if [ -n "$NEW_SCHEMA" ]
    if [ $HAS_SUBST -eq 1 ]
    then
        # info_msg found substitute schema $SEQUENCE_TABLE \($OLD_SCHEMA\ -\> $NEW_SCHEMA\)
        modify_objects
        check_read_quality_exit_with_message "Can not read QUALITY, the substitute schema did not work"
    else
        # warn_msg no substitute schema for $SEQUENCE_TABLE \($OLD_SCHEMA\)
        check_read_quality_exit_with_message "Can not read QUALITY, and there is no substitute schema"
    fi

    ## Make ReadFilter
    do_make_read_filter

    for i in ${ORIGINAL_QUALITY_COLUMNS[*]}
    do
        exec_cmd_exit rm -rf $i
    done

    # exec_cmd_exit rm -rf $ORIGINAL_QUALITY_COL
    check_read_quality_exit_with_message "Can not read QUALITY, delite failed"

    ## Locking db
    exec_cmd_exit $VDBLOCK_BIN $DATABASE_DIR

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
        err_exit can not stat database \'$DATABASE_DIR\'
    fi

    if [ ! -f "$STATUS_FILE" ]
    then
        err_exit can not stat status file
    fi

    ## Checking if it is colorspace run
    check_colorspace_exit

    TVAR=`grep "$DELITED_TAG" $STATUS_FILE 2>/dev/null`
    if [ -z "$TVAR" ]
    then
        err_exit status shows that object was not delited yet
    fi

    TVAR=`$KARMETA_BIN --info SOFTWARE/delite $DATABASE_DIR 2>/dev/null`
    if [ -z "$TVAR" ]
    then
        err_exit object was not delited yet
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

test_kar ()
{
    F2T=$1

    if [ -n "$SKIPTEST_VAL" ]
    then
        warn_msg skipping tests for \'$F2T\' ...
        return
    fi

    if [ ! -f $ORIG_KAR_FILE ]
    then
        err_exit SKIPPING DIFF TESTS for \'$F2T\', can not stat original KAR file \'$ORIG_KAR_FILE\'
    fi

    exec_cmd $VDBVALIDATE_BIN -x $F2T
    if [ $? -ne 0 ]
    then
        warn_msg vdb-validate step failed, checking original KAR file
        exec_cmd $VDBVALIDATE_BIN -x $ORIG_KAR_FILE
        if [ $? -ne 0 ]
        then
            err_exit corrupted original KAR file
        else
            err_exit vdb-validate failed or original file, that means DELITE process failed
        fi
    fi

    TCMD="$VDBDIFF_BIN $ORIG_KAR_FILE $F2T -i"

    TDC="-x CLIPPED_QUALITY,SAM_QUALITY,QUALITY_VALUE"

    if [ $DROPCOLUMN_QTY -ne 0 ]
    then
        TCNT=0
        while [ $TCNT -lt $DROPCOLUMN_QTY ]
        do
            TCN=${DROPCOLUMNS[$TCNT]}

            TDC="${TDC},$TCN"

            if [ "$TCN" = "SIGNAL" ]
            then
                TDC="${TDC},SIGNAL_LEN,SPOT_DESC"
            fi

            TCNT=$(( $TCNT + 1 ))
        done
    fi

    is_make_read_filter_applicable
    if [ $? -eq 0 ]
    then
        TDC="${TDC},READ_FILTER,RD_FILTER,SAM_FLAGS"
    fi

    TCMD="$TCMD $TDC"

    exec_cmd_exit $TCMD
}

kar_new ()
{
    if [ -f "$NEW_KAR_FILE" ]
    then
        if [ -n "$FORCE_VAL" ]
        then
            info_msg forcing to remove odl KAR file \'$NEW_KAR_FILE\'
            exec_cmd_exit rm -rf $NEW_KAR_FILE
        else
            err_exit old KAR file found \'$NEW_KAR_FILE\'
        fi
    fi

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

    exec_cmd_exit $TCMD

    test_kar $NEW_KAR_FILE
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
        err_exit can not stat file \'$VDBCFG_FILE\'
    fi

    ## looking up for all columns to drop
    find_columns_to_drop

    ## writing delited kar archive
    kar_new

    ## just printing stats
    print_stats

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
        err_exit can not stat directory \'$TARGET_DIR\'
    fi

    if [ ! -f "$STATUS_FILE" ]
    then
        err_exit can not stat status file
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

trap "err_exit received SIGINT signal, exiting" SIGINT
trap "err_exit received SIGTERM signal, exiting" SIGTERM

eval $ACTION_PROC

