#!/bin/bash

ulimit -c unlimited

#####################################################################
## Lyrics:
##
## The goal of that script is to clean data leftovers, run away fuser
## processes, and report if here is any problem
##
## That script has one parameter :
## 
##      config_name : it will try to find configuration file
##                    cfg/config_name.cfg ( optional )
##                    default config name is 'standard'
##
#####################################################################

##  Where we are, who we are?
##
SCR_DIR=`dirname $0`
SCR_DIR=`cd $SCR_DIR; pwd`

SCR_NAME=`basename $0`
SCR_SNAME=`basename $0 .sh`

##  Loading common things to live
##
CMN_SCR="$SCR_DIR/remote_fuser_common.sh"
if [ ! -f "$CMN_SCR" ]
then
    echo "ERROR: Can not stat script '$CMN_SCR' ... exiting"
    exit 1
fi

. $CMN_SCR

#####################################################################
##
## Simple setup : directory
##
## TEST_DIR ---/ USER ---/ HOST ---/ PID ---/ cache_dir /
##                 |-/ logs           |-/ mount_point /
##                 |-/ bin            |-/ tmp_dir /
##                 |-/ depot          |-/ status_file
##
## depot - directory with test binaries, just a history
## bin - test binaries in work
## status_file - says a lot ... but heart-bot
##
## It is checking USER/HOST directory for PID and clean unused.
## If there is a process which is unable to kill, it will be reported
## to MAIL_LIST from config file
##
#####################################################################

##  Config file
##
CFG_DIR=$SCR_DIR/cfg
if [ ! -d "$CFG_DIR" ]
then
    _err_exit "Can not stat directory '$CFG_DIR'"
fi

####
##  Here the usage and Arguments processing
#
CFG_NAME_DEF="standard"
CFG_NAME=$CFG_NAME_DEF

usage ( )
{
    cat >&2 <<EOF

This script will remove unused test data for remote_fuser utility. 

Syntax is :

    $SCR_NAME [ config_name ]

Where :

         config_name - name of config to use. 
                       String. Optional. Default "$CFG_NAME_DEF"

EOF
}

case $# in
    0)
        ;;
    1)
        CFG_NAME=$1
        ;;
    *)
        usage
        _err_exit Invalid parameters
        ;;
esac

##  Checking arguments
##
CFG_FILE=$CFG_DIR/${CFG_NAME}.cfg
if [ ! -f "$CFG_FILE" ]
then
    _err_exit "Can not stat config file with name '$CFG_NAME'"
fi

####
##  Loading fonfig file
#
. $CFG_FILE
if [ $? -ne 0 ]
then
    _err_exit "Can not load config file '$CFG_NAME'"
fi

####
##  Checking config file data and prepareing environment
#
if [ -z "$TEST_DIR" ]
then
    _err_exit "Config does not have definition for variable TEST_DIR"
fi

if [ ! -d "$TEST_DIR" ]
then
    _err_exit "Can not stat directory '$TEST_DIR'"
fi

###
### Prefixes U_(ser), H_(ost), P_(rocess), F_(inal) - is what we use
###

_assign_create_dir ()
{
    if [ $# -ne 2 ]
    then
        _err_exit "_assign_create_dir(): requires two arguments"
    fi

    DVAL=$1
    if [ ! -d "$DVAL" ]
    then
        _msg "Creating directory '$DVAL'"
        _exec mkdir $DVAL
        _exec chmod ugoa+rwx $DVAL
    fi

    DVAL=`cd $DVAL; pwd`

    eval "$2=$DVAL"
}

###
## There are logs, bin and depot on user level of test direcotry
#
_assign_create_dir $TEST_DIR/$DAS_USER U_TEST_DIR
_assign_create_dir $U_TEST_DIR/$DAS_HOST H_TEST_DIR

_assign_create_dir $U_TEST_DIR/log   F_LOG_DIR

F_VALID="cache mount status.file temp"
U_VALID="bin depot log"

##  Here we are logging and execing
##
F_TIME_STAMP=`date +%Y-%m-%d_%H-%M-%S`
F_LOG_FILE=$F_LOG_DIR/${SCR_SNAME}.log.${DAS_CTX}.${F_TIME_STAMP}
echo Log file: $F_LOG_FILE

exec >$F_LOG_FILE 2>&1

## One more time for log file
##
_print_env $@

####
### OLD LOGS
##
#
_bump
_msg OLD LOGS

##  Here we are clearing old logs
##
DAYS_KEEP_LOG=10

SEC_IN_DAY=$(( 60 * 60 * 24 ))
NOW_DAY=$(( `date +%s` / $SEC_IN_DAY ))

check_remove ()
{
    F2R=$1

    if [ -n "$F2R" ]
    then
        FILE_DAY=$(( `stat --print="%X" $F2R` / $SEC_IN_DAY ))
        DALT=$(( $NOW_DAY - $FILE_DAY ))
        if [ $DAYS_KEEP_LOG -le $DALT ]
        then
            _msg Log file is $DALT days old, removing \'$F2R\'
            rm -f $F2R
            if [ $? -ne 0 ]
            then
                _wrn Can not remove file \'$F2R\'
            fi
        fi
    fi
}

clear_old_logs ()
{
    _msg Cleaning old logs ...

    for i in `ls $F_LOG_DIR 2>/dev/null`
    do
        check_remove $F_LOG_DIR/$i
    done
}

clear_old_logs

_msg OLD LOGS DONE

####
### OLD FUSES
##
#
_bump
_msg OLD FUSES

_remove_dir ()
{
    D2R=$1

    if [ -d "$D2R" ]
    then
        _exec_plain rm -rf $D2R
        if [ $? -ne 0 ]
        then
            _msg Failed to remove directory $D2R
            sleep 3
            _exec_plain rm -rf $D2R
            if [ $? -ne 0 ]
            then
                INVALID_FUSES[${#INVALID_FUSES[*]}]="CAN NOT REMOVE: $D2R"
            fi
        fi
    fi
}

_clean_data_for_pid ()
{
    DPD=$1

    _msg "<=================================>"
    _msg Cleaning data for PID = $DPD

    F_TEST_DIR=$H_TEST_DIR/$DPD

    ## First we are trying to read data from status file
    ##

    unset F_PID
    unset F_MNT
    ST_FL=$F_TEST_DIR/status.file
    if [ -f $ST_FL ]
    then
        _load_fuser_params $ST_FL F_PID F_MNT
    else
        F_MNT=$F_TEST_DIR/mount
    fi

    if [ -n "$F_PID" ]
    then
        _shutdown_fuser $F_PID $F_MNT
        if [ $? -ne 0 ]
        then
            ## Failed to shutdown
            INVALID_FUSES[${#INVALID_FUSES[*]}]="STIL RUN: PID=$F_PID MPOINT=$F_MNT"
        else
            _remove_dir $F_TEST_DIR
        fi
    else
        _check_dir_mounted $F_MNT
        if [ $? -eq 0 ]
        then
            /bin/fusermount -u $F_MNT

            _check_dir_mounted $F_MNT
            if [ $? -eq 0 ]
            then
                INVALID_FUSES[${#INVALID_FUSES[*]}]="STIL RUN: MPOINT=$F_MNT"
            else
                _remove_dir $F_TEST_DIR
            fi
        else
            _remove_dir $F_TEST_DIR
        fi
    fi
}

_clean_test_data ()
{
    DPD=$1

    ps -p $DPD > /dev/null 2>&1
    if [ $? -eq 0 ]
    then
        _msg Process with PID = $DPD is still running
        return
    fi

    _clean_data_for_pid $DPD
}

for i in ` ls $H_TEST_DIR 2>/dev/null `
do
    _clean_test_data $i
done

####
### SANITY CHECK
##
#
_bump
_msg SANITY CHECK

_check_sanity ()
{
    N2C=$1

    for i in $U_VALID
    do
        if [ "$i" == "$N2C" ]
        then
            return 0
        fi
    done

    /usr/bin/nslookup $N2C >/dev/null 2>&1
    if [ $? -eq 0 ]
    then
        return 0
    fi

    return 1
}

_check_sanity_remove ()
{
    SDR=$1

    ENT=$U_TEST_DIR/$SDR

    _check_sanity $SDR
    if [ $? -eq 0 ]
    then
        _msg LEGIT: $ENT
    else
        _msg REMOVING: $ENT
        _exec_plain rm -rf $ENT
        if [ $? -ne 0 ]
        then
            _msg Failed to remove $ENT
            sleep 3
            _msg Another attempt to remove $ENT
            _exec_plain rm -rf $ENT
            if [ $? -ne 0 ]
            then
                INVALID_FUSES[${#INVALID_FUSES[*]}]="CAN NOT REMOVE: $ENT"
            fi
        fi
    fi
}

for i in ` ls $U_TEST_DIR 2>/dev/null `
do
    _check_sanity_remove $i
done

####
### REPORTING EXITING
##
#
_bump
_msg CLEANING COMPLETED

PQT=${#INVALID_FUSES[*]}

_msg FOUND $PQT PROBLEMS

if [ $PQT -ne 0 ]
then
    CNT=0
    while [ $CNT -lt $PQT ]
    do
        CNT=$(( $CNT + 1 ))
        _msg "  $CNT: ${INVALID_FUSES[$CNT]}"
    done
fi 

_msg BYE ...
