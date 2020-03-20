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

SOURCE_TAG="--source"
TARGET_TAG="--target"
CONFIG_TAG="--config"
SCHEMA_TAG="--schema"
FORCE_TAG="--force"
PRESERVE_TAG="--preserve"
WRITEALL_TAG="--writeall"

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
    $PRESERVE_TAG      - flag to preserve dropped columns in separate KAR file
    $WRITEALL_TAG      - flag to write KAR file including all columns

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
        $PRESERVE_TAG)
            PRESERVE_VAL=1
            ;;
        $WRITEALL_TAG)
            WRITEALL_VAL=1
            ;;
        *)
            usage invalid argument \'$TARG\'
            ;;
    esac

    TCNT=$(( $TCNT + 1 ))
done

TRANSLATION_QTY=0
DROPCOLUMN_QTY=0

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
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:sequence  1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:sequence_log_odds 1.0     2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:sequence_nanopore 1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:sequence_no_name  1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Helicos:tbl:v2 1.0.4   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Illumina:qual4 2.1.0   3"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Illumina:tbl:phred:v2  1.0.4   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Illumina:tbl:q1:v2     1.1     2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Illumina:tbl:q4:v2     1.1.0   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Illumina:tbl:v2        1.0.4   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:IonTorrent:tbl:v2      1.0.3   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Nanopore:consensus     1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Nanopore:sequence      1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:PacBio:smrt:basecalls  1.0.2   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:PacBio:smrt:cons       1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:PacBio:smrt:fastq      1.0.3   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:PacBio:smrt:sequence   1.0     2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:_454_:tbl:v2   1.0.7   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:tbl:spotdesc   1.0.2   1.1"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:tbl:spotdesc_nocol     1.0.2   1.1"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:tbl:spotdesc_nophys    1.0.2   1.1"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:WGS:tbl:nucleotide 1.1     2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:align:tbl:reference        2       3"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:align:tbl:seq      1.1     2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:align:tbl:seq      1       2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:refseq:tbl:reference       1.0.2   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:tbl:base_space     2.0.3   3"

#added first pass
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:sequence  1       2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:sequence_nanopore 1       2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:sequence_no_name  1       2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Illumina:tbl:phred:v2  1.0.3   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Nanopore:sequence      1       2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:PacBio:smrt:cons       1.0.2   2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:PacBio:smrt:sequence   1.0.2   2.0"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:_454_:tbl:v2   1.0.6   2"

#added second pass
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:consensus_nanopore        1       2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:GenericFastq:sequence_log_odds 1       2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Helicos:tbl:v2 1.0.3   2"
TRANSLATIONS[${#TRANSLATIONS[*]}]="NCBI:SRA:Nanopore:consensus     1       2"

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

ORIGINAL_QUALITY=ORIGINAL_QUALITY


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
PRESERVED_KAR_FILE=$TARGET_DIR/preserved.kar
ALLCOLUMNS_KAR_FILE=$TARGET_DIR/all.kar
STATUS_FILE=$TARGET_DIR/.status.txt

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
###<<>>### Unpacking original KAR archive
##############################################################################################

import_proc ()
{
    ##
    ## Checking args
    if [ -z "$SOURCE_VAL" ]
    then
        err_exit missed mandatory parameter \'$SOURCE_TAG\'
    fi

    info_msg "IMPORT: $SOURCE_VAL to $TARGET_DIR"

    if [ -d "$TARGET_DIR" ]
    then
        if [ -n "$FORCE_VAL" ]
        then
            info_msg forcing to remove old data for \'$TARGET_DIR\'
            exec_cmd_exit chmod -R +w $TARGET_DIR
            exec_cmd_exit rm -rf $TARGET_DIR
        else
            err_exit target directory \'$TARGET_DIR\' exist.
        fi
    fi

    exec_cmd_exit mkdir $TARGET_DIR
    log_status initialized: $SOURCE_VAL

    ICMD="$KAR_BIN "
    if [ -n "$FORCE_VAL" ]
    then
        ICMD="$ICMD --force"
    fi

    exec_cmd_exit $ICMD --extract $SOURCE_VAL --directory $DATABASE_DIR

    log_status "IMPORTED: $ORIGINAL_CMD"

    info_msg "DONE"
}

###############################################################################################
###############################################################################################
###<<>>### Delite 
##############################################################################################
check_delited ()
{
    if [ ! -d "$DATABASE_DIR" ]
    then
        err_exit can not stat database \'$DATABASE_DIR\'
    fi

    if [ ! -f "$STATUS_FILE" ]
    then
        err_exit can not stat status file
    fi

    TVAR=`grep DELITED: $STATUS_FILE 2>/dev/null`
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

make_original_qualities ()
{
    for i in `find $DATABASE_DIR -type d -name QUALITY`
    do
        TDIR=`dirname $i`
        exec_cmd_exit mv $i $TDIR/${ORIGINAL_QUALITY}
    done
}

add_object ()
{
    ON=$1

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
}

find_objects_to_modify ()
{
    if [ $DROPCOLUMN_QTY -eq 0 ]
    then
        warn_msg there are no column names to drop defined
        return
    fi

    cd $DATABASE_DIR >/dev/null 2>&1

    add_object "."
    for i in `find . -type d -name $ORIGINAL_QUALITY`
    do
        info_msg found column: $i

        TVAR=`dirname $i`
        TVAR=`dirname $TVAR`
        add_object $TVAR

        TVAR=`dirname $TVAR`
        TVAR=`dirname $TVAR`
        add_object $TVAR
    done

    for i in ${DROPCOLUMNS[*]}
    do
        for u in `find . -type d -name $i`
        do
            info_msg found: $u

            TVAR=`dirname $u`
            TVAR=`dirname $TVAR`
            add_object $TVAR

            TVAR=`dirname $TVAR`
            TVAR=`dirname $TVAR`
            add_object $TVAR
        done
    done

    OBJECTS_QTY=${#OBJECTS[*]}

    cd - >/dev/null 2>&1
}

find_new_schema ()
{
    S2R=$1

    NEW_SCHEMA=""

    if [ $TRANSLATION_QTY -eq 0 ]
    then
        return
    fi

    SATR=( `echo $S2R | sed "s#\## #1"` )

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
    O2M=$1
    M2D=$DATABASE_DIR/$O2M

    info_msg mdifying object \'$M2D\'

    OLD_SCHEMA=`$KARMETA_BIN --info schema@name $M2D 2>/dev/null | awk ' { print $2 } '`
    if [ -z "$OLD_SCHEMA" ]
    then
        err_exit can not retrieve schema name for \'$O2M\'
    fi

    find_new_schema $OLD_SCHEMA

    if [ -n "$NEW_SCHEMA" ]
    then
        info_msg subst $OLD_SCHEMA to $NEW_SCHEMA
        exec_cmd_exit $KARMETA_BIN --spath $SCHEMA_VAL --updschema schema=\'$NEW_SCHEMA\' $M2D
    fi

    info_msg mark object DELITED

    exec_cmd_exit $KARMETA_BIN --setvalue SOFTWARE/delite@date=\"`date`\" $M2D
    exec_cmd_exit $KARMETA_BIN --setvalue SOFTWARE/delite@name=delite $M2D
    exec_cmd_exit $KARMETA_BIN --setvalue SOFTWARE/delite@vers=1.1.1 $M2D
}

modify_objects ()
{
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

delite_proc ()
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

    ## Unlocking db
    exec_cmd_exit $VDBUNLOCK_BIN $DATABASE_DIR

    ## Checking that it was already delited
    check_delited

    ## Rename original qualities
    make_original_qualities

    ## Searching for drop columns
    find_objects_to_modify

    ## Modifying all objects
    modify_objects

    ## Locking db
    exec_cmd_exit $VDBLOCK_BIN $DATABASE_DIR

    log_status "DELITED: $ORIGINAL_CMD" 

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

    TVAR=`grep DELITED: $STATUS_FILE 2>/dev/null`
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

    for i in `find . -type d -name $ORIGINAL_QUALITY`
    do
        TD=$i
        info_msg found column: $TD
        TO_DROP[${#TO_DROP[*]}]=$TD
    done

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
    exec_cmd_exit $VDBVALIDATE_BIN $NEW_KAR_FILE
}

kar_all ()
{
    if [ -z "$WRITEALL_VAL" ]
    then
        return
    fi

    if [ -f "$ALLCOLUMNS_KAR_FILE" ]
    then
        if [ -n "$FORCE_VAL" ]
        then
            info_msg forcing to remove odl KAR file \'$ALLCOLUMNS_KAR_FILE\'
            exec_cmd_exit rm -rf $ALLCOLUMNS_KAR_FILE
        else
            err_exit old KAR file found \'$ALLCOLUMNS_KAR_FILE\'
        fi
    fi

    TCMD="$KAR_BIN"
    if [ -n "$FORCE_VAL" ]
    then
        TCMD="$TCMD -f"
    fi

    TCMD="$TCMD --create $ALLCOLUMNS_KAR_FILE --directory $DATABASE_DIR"

    exec_cmd_exit $TCMD
    exec_cmd_exit $VDBVALIDATE_BIN $ALLCOLUMNS_KAR_FILE
}

kar_preserved ()
{
    if [ -z "$PRESERVE_VAL" ]
    then
        return
    fi

    if [ -f "$PRESERVED_KAR_FILE" ]
    then
        if [ -n "$FORCE_VAL" ]
        then
            info_msg forcing to remove odl KAR file \'$PRESERVED_KAR_FILE\'
            exec_cmd_exit rm -rf $PRESERVED_KAR_FILE
        else
            err_exit old KAR file found \'$PRESERVED_KAR_FILE\'
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
        TCMD="$TCMD --keep ${TO_DROP[$TCNT]}"

        TCNT=$(( $TCNT + 1 ))
    done

    TCMD="$TCMD --create $PRESERVED_KAR_FILE --directory $DATABASE_DIR"

    exec_cmd_exit $TCMD
}

export_proc ()
{
    ## checking if it is was delited
    check_ready_for_export

    ## looking up for all columns to drop
    find_columns_to_drop

    ## writing delited kar archive
    kar_new

    ## writing kar archive with all columns
    kar_all

    ## writing preserved kar data
    kar_preserved

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

    if [ -f "$ALLCOLUMNS_KAR_FILE" ]
    then
        info_msg found all columns KAR archive \'$ALLCOLUMNS_KAR_FILE\'
    fi

    if [ -f "$PRESERVED_KAR_FILE" ]
    then
        info_msg found preserved dropped columns KAR archive \'$PRESERVED_KAR_FILE\'
    fi

    info_msg DONE
}

###############################################################################################
###############################################################################################
###<<>>### That is main line of script
##############################################################################################

eval $ACTION_PROC

