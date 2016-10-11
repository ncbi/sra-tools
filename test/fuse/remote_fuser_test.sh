#!/bin/bash

ulimit -c unlimited

#####################################################################
## Lyrics:
##
## That script has three parameters:
## 
##      config_name : it will try to find configuration file
##                    cfg/config_name.cfg ( optional )
##                    default config name is 'standard'
##     time_minutes : run time of script in minutes ( optional )
##                    default time is 180 minutes
##    bin_directory : place where binary files like remote-fuser
##                    etc are located ( mandatory )
##
#####################################################################

##  Where we are, who we are?
##
SCR_DIR=`dirname $0`
SCR_DIR=`cd $SCR_DIR; pwd`

SCR_NAME=`basename $0`
SCR_SNAME=`basename $0 .sh`

DAS_PID=$$
DAS_CTX=${HOSTNAME}.${DAS_PID}

####
##  Environment: Jy's a mal naaier maar ek hou van jou baie
#

_print_env ()
{
    echo "[ARGS] [$@]"
    for i in @ USER REMOTE_USER HOME HOST HOSTNAME HOSTTYPE
    do
        echo "[$i] [${!i}]"
    done
    echo "[groups] [`groups`]"
}

_print_env

####
##  Some interesting stuff
#

_bump ()
{
    echo ""
    echo "####################################################################"
    echo "####################################################################"
    echo "##"
    echo "## `date`"
    echo "##"
    echo "####################################################################"
}

_msg ( )
{
    echo "INF[$SCR_SNAME][`date +%Y-%m-%d_%H:%M:%S`] $@"
}

_wrn ( )
{
    echo "WRN[$SCR_SNAME][`date +%Y-%m-%d_%H:%M:%S`] $@" >&2
}

_err ( )
{
    echo "ERR[$SCR_SNAME][`date +%Y-%m-%d_%H:%M:%S`] $@" >&2
}

_err_exit ( )
{
    _err $@
    _err Exiting ...
    exit 1
}

_exec ()
{
    CDM="$@"
    _msg "## $CDM"
    eval "$CDM"
    if [ $? -ne 0 ]
    then
        _err_exit "FAILED: $CDM"
    fi
}

check_dir ( )
{
    if [ -n "$1" ]
    then
        if [ -d "$1" ]
        then
            return 0
        else
            _err_exit "check_dir(): can not stat directory '$1'"
        fi
    else
        _err_exit "cneck_dir(): missed parameters"
    fi
}

##  Some usefuls
##
CFG_DIR=$SCR_DIR/cfg
check_dir $CFG_DIR

####
##  Here the usage and Arguments processing
#
CFG_NAME_DEF="standard"
CFG_NAME=$CFG_NAME_DEF

RUN_TIME_DEF=180
RUN_TIME=$RUN_TIME_DEF

BIN_DIR=""

usage ( )
{
    cat >&2 <<EOF

This script will test remote_fuser utility. 
Syntax is :

    $SCR_NAME [ config_name ] [ run_time ] binary_directory

Where :

         config_name - name of config to use. 
                       String. Optional. Default "$CFG_NAME_DEF"
            run_time - time to run script.
                       Integer. Optional. Default "$RUN_TIME_DEF"
    binary_directory - directory which contains executables, like
                       remote-fuser, remote-fuser-reader, etc

EOF
}

_qualify ()
{
    VAL=$1

    if [ -z "$VAL" ]
    then
        _err_exit "qualify(): missed parameter"
    fi

##  Simple, if it is number, it is time, overwise it is config name
##
    if [[ $VAL != *[!0-9]* ]]
    then
        RUN_TIME=$VAL
    else
        CFG_NAME=$VAL
    fi
}

case $# in
    3)
        _qualify $1
        _qualify $2
        BIN_DIR=$3
        ;;
    2)
        _qualify $1
        BIN_DIR=$2
        ;;
    1)
        BIN_DIR=$1
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

##  Checking RUN_TIME ... it should not be less than 3 minutes
##
if [ $RUN_TIME -le 3 ]
then
    _err_exit "Run time parameter should be greater than 3 minutes"
fi

##  Checking BIN_DIR and binaries
##
if [ ! -d "$BIN_DIR" ]
then
    _err_exit "Can not stat BIN_DIR directory '$BIN_DIR'"
fi

BIN_DIR=$BIN_DIR/../bin
if [ ! -d "$BIN_DIR" ]
then
    _err_exit "Can not stat BIN_DIR directory '$BIN_DIR'"
fi
BIN_DIR=`cd $BIN_DIR; pwd`

TESTBIN_DIR=$BIN_DIR/../test-bin
if [ ! -d "$TESTBIN_DIR" ]
then
    _err_exit "Can not stat TESTBIN_DIR directory '$TESTBIN_DIR'"
fi
TESTBIN_DIR=`cd $TESTBIN_DIR; pwd`

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
EFF_USER=$USER
if [ -z "$EFF_USER" ]
then
    EFF_USER=`id -n -u`
    if [ -z "$EFF_USER" ]
    then
        EFF_USER="undefined-user"
        # _err_exit "Environment variable \$USER is not set"
    fi
fi

if [ -z "$TEST_DIR" ]
then
    _err_exit "Config does not have definition for variable TEST_DIR"
fi

if [ ! -d "$TEST_DIR" ]
then
    _err_exit "Can not stat directory '$TEST_DIR'"
fi

F_TEST_DIR=$TEST_DIR/$EFF_USER
if [ ! -d "$F_TEST_DIR" ]
then
    _msg "Creating directory '$F_TEST_DIR'"
    _exec mkdir $F_TEST_DIR
    _exec chmod ugoa+rwx $F_TEST_DIR
fi
F_TEST_DIR=`cd $F_TEST_DIR; pwd`

_check_assign_dir ()
{
    if [ $# -ne 2 ]
    then
        _err_exit "_check_assign_dir(): requires two arguments"
    fi

    VAL=$F_TEST_DIR/$1
    if [ ! -d "$VAL" ]
    then
        _msg "_check_assign_dir(): creating directory '$VAL'"

        _exec mkdir $VAL
    fi

    eval "$2=$VAL"
}

_check_assign_dir bin F_BIN_DIR
_check_assign_dir cache.${DAS_CTX} F_CACHE_DIR
_check_assign_dir mount.${DAS_CTX} F_MOUNT_DIR
_check_assign_dir temp F_TEMP_DIR
_check_assign_dir log F_LOG_DIR
_check_assign_dir depot F_DEPOT_DIR

## Here we are checking that there is no remote-fuser mount
##
_same_dir ()
{
    if [ $# -ne 2 ]
    then
        _err_exit "_same_dir(): requires two arguments"
    fi

    N1=`basename $1`
    N2=`basename $2`
    if [ "$N1" == "$N2" ]
    then
        N1=`dirname $1`
        N2=`dirname $2`
        if [ "$N1" -ef "$N2" ]
        then
            return 0
        fi
    fi

    return 1
}

_check_mount ()
{
        ## We are simple not check an mount directory.
    for i in `mount | awk ' { print $3 } ' `
    do
        _same_dir $1 $i
        if [ $? -eq 0 ]
        then
            return 0
        fi
    done

    return 1
}

_check_mount $F_MOUNT_DIR
if [ $? -eq 0 ]
then
    _err_exit "Mount point is in use '$F_MOUNT_DIR'"
fi


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
            echo rm -f $F2R
            if [ $? -ne 0 ]
            then
                _wrn Can not remove file \'$F2R\'
            fi
        fi
    fi
}

clear_old_logs ()
{
    _msg Clearing old logs ...

    for i in `ls $F_LOG_DIR`
    do
        check_remove $F_LOG_DIR/$i
    done
}

clear_old_logs

##  Here we are logging and execing
##
F_TIME_STAMP=`date +%Y-%m-%d_%H-%M-%S`
F_LOG_FILE=$F_LOG_DIR/${SCR_SNAME}.log.${DAS_CTX}.${F_TIME_STAMP}
echo Log file: $F_LOG_FILE
exec >$F_LOG_FILE 2>&1

## One more time for log file
##
_print_env

## Here we are copying binaries
##
_copy_assign_app ()
{
    if [ $# -ne 3 ]
    then
        _err_exit "_copy_assign_app(): requires two arguments"
    fi

    SRC=$1/$2
    if [ ! -x "$SRC" ]
    then
        _err_exit "_copy_assign_app(): can not stat file '$SRC'"
    fi

    LNK=$F_BIN_DIR/$2

    eval cmp $SRC $LNK
    if [ $? -ne 0 ]
    then
        DST=$F_DEPOT_DIR/${2}.${F_TIME_STAMP}
        _exec cp -p $SRC $DST
        _exec rm -f $LNK
        _exec ln -s $DST $LNK
    fi

    eval "$3=$LNK"
}

_copy_assign_app $BIN_DIR remote-fuser REMOTE_FUSER_APP
_copy_assign_app $TESTBIN_DIR remote-fuser-test REMOTE_FUSER_TEST_APP

###
##  Here we are checking and pre-processing XML file
#
if [ -z "$XML_URL" ]
then
    _err_exit "Config does not have definition for variable XML_URL"
fi

eval HEAD "$XML_URL" >/dev/null
if [ $? -ne 0 ]
then
    _err_exit "Can not stat XML file from '$XML_URL'"
fi

F_FUSE_XML=$F_TEMP_DIR/FUSE.xml.${DAS_CTX}.${F_TIME_STAMP}
eval GET "$XML_URL" >$F_FUSE_XML
if [ $? -ne 0 ]
then
    _err_exit "Can not download XML file from '$XML_URL'"
fi

###
##  Here we are starting FUSER
#
_msg Starting Fuse

FUSER_LOG=$F_LOG_DIR/remote-fuser.log.${DAS_CTX}.${F_TIME_STAMP}
FUSER_CMD="$REMOTE_FUSER_APP -d -x $XML_URL -m $F_MOUNT_DIR -e $F_CACHE_DIR -L 5 -B 4 -o kernel_cache"

_msg "## $FUSER_CMD"
$FUSER_CMD >$FUSER_LOG 2>&1 &
FUSER_PID=$!

_is_fuser_run ()
{
    if [ -n "$FUSER_PID" ]
    then
        ps -p $FUSER_PID >/dev/null 2>&1
        if [ $? -eq 0 ]
        then
            _wrn PROCESS WITH PID $FUSER_PID EXISTS
            return 0
        fi
    fi

    _check_mount $F_MOUNT_DIR
    if [ $? -eq 0 ]
    then
        _wrn MOUNT STILL EXISTS
        return 0
    fi

    return 1
}

_shutdown_exit ()
{
    _err Shutdown FUSER and exit
        ##  Check if there is still mount
        ##
    _check_mount $F_MOUNT_DIR
    if [ $? -eq 0 ]
    then
        /bin/fusermount -u $F_MOUNT_DIR
    fi

    sleep 1

        ##  Check that process is still in the memory
        ##
    if [ -n "$FUSER_PID" ]
    then
        ps -p $FUSER_PID >/dev/null 2>&1
        if [ $? -eq 0 ]
        then
            _err Killing FUSER process $FUSER_PID
            kill -9 $FUSER_PID
        fi
    fi

    sleep 3

    if [ -n "$FUSER_PID" ]
    then
        ps -p $FUSER_PID >/dev/null 2>&1
        if [ $? -eq 0 ]
        then
            _err_exit Failed to shutdown FUSER
        fi
    fi

    _check_mount $F_MOUNT_DIR
    if [ $? -eq 0 ]
    then
        _err_exit Failed to shutdown FUSER
    fi

    _err_exit FUSER was shut down
}

_stop_fuser ()
{
    _msg "Stopping Fule"
    MCD="$REMOTE_FUSER_APP -u -m $F_MOUNT_DIR"
    _msg "## $MCD"
    $MCD

    sleep 50

    _is_fuser_run
    if [ $? -eq 0 ]
    then
        _shutdown_exit
    fi
}

_test_failed ()
{
    _err "TEST FAILED: $@"

    _stop_fuser

    _err_exit 
}

##  Here we should wait a some time and check that mount is success
##  The mount is success if there is record in mtab and some files
##  appeared in mount directory
##
for i in All the way home from Baltimore "(C)" Talking Heads THE_END
do
    case $i in
        THE_END)
            _err Can not start fuser
            _shutdown_exit
            ;;
        *)
            echo -n "$i "
            sleep 1
            _check_mount $F_MOUNT_DIR
            if [ $? -eq 0 ]
            then
                echo ...
                break
            fi
            ;;
    esac
done

#####################################################################
#####################################################################
#####
### First test is checking that file downloads full and correctly
#

_bump
_msg TEST 1: Downloading single file

## First we are trying to choose not big file to download
##
LINF=` grep "<File" $F_FUSE_XML | sed "s#name=##1" | sed "s#size=##1" | sed "s#path=##1" | sed "s#\"##g" | awk ' { print $2 " " $3 " " $4 } ' | sort -n -k 2 | awk ' BEGIN { SZ=0; NM=""; UR="" } { if ( SZ < 100000 ) { if ( SZ < int ( $2 ) ) { SZ = int ( $2 ); NM = $1; UR = $3 } } } END { print NM " " SZ " "  UR } ' `
if [ -z "$LINF" ]
then
    _test_failed "Invalid XML file '$F_FUSE_XML'"
fi

REMOTE_URL=`echo $LINF | awk ' { print $3 } ' `
LOCAL_FILE=`echo $LINF | awk ' { print $1 } ' `
TEMP_FILE=$F_TEMP_DIR/${LOCAL_FILE}.${DAS_CTX}.${F_TIME_STAMP}

## Downloading copy of proxied file
##
_msg "Downloading file '$REMOTE_URL'"
eval GET "$REMOTE_URL" >$TEMP_FILE
if [ $? -ne 0 ]
then
    _test_failed "Can not load remote file '$REMOTE_URL'"
fi

## Compareing downloaded file with file proxied by FUSE
##
_msg "Compareing downloaded file '$REMOTE_URL' with $F_MOUNT_DIR/$LOCAL_FILE"
eval cmp $TEMP_FILE $F_MOUNT_DIR/$LOCAL_FILE
if [ $? -ne 0 ]
then
    _test_failed "Invalid file content '$F_MOUNT_DIR/$LOCAL_FILE'"
fi

## Checking CACHE directory, cuz here should be only one file,
## without ".cache" extention
##
RCA_DIR=$F_CACHE_DIR/.cache
FILES=(`ls $RCA_DIR`)
if [ ${#FILES[@]} -ne 1 ]
then
    ls $RCA_DIR >&2
    _test_failed "Invalid content of cache directory '$RCA_DIR'"
fi

if [[ "${FILES[0]}" == *.cache ]]
then
    ls $RCA_DIR >&2
    _test_failed "Partially downloaded file at '$RCA_DIR'"
fi

_msg TEST 1: Passed

#####################################################################
#####################################################################
#####
### Second multithread access to singe file
#

_bump
_msg TEST 2: Multithread acces to single file

R_TM=2
N_THR=30

## First we should choose file big enough for a test ...
## couple hundred megs is OK
##

for i in `ls $F_MOUNT_DIR`
do
    if [ `stat --print="%s" $F_MOUNT_DIR/$i` -gt 200000000 ]
    then
        FILE_NAME=$i
        break
    fi
done

if [ -z "$FILE_NAME" ]
then
    _test_failed "Can not find file with suitable size"
fi

## TEST_LOG="$F_LOG_DIR/remote-fuser-test-single.log.$F_TIME_STAMP"
## TEST_CMD="$REMOTE_FUSER_TEST_APP -t $N_THR -r $R_TM $F_MOUNT_DIR/$FILE_NAME >$TEST_LOG 2>&1"
TEST_CMD="$REMOTE_FUSER_TEST_APP -t $N_THR -r $R_TM $F_MOUNT_DIR/$FILE_NAME"
_msg "## $TEST_CMD"
eval "$TEST_CMD"
if [ $? -ne 0 ]
then
    _test_failed "Single file test failed"
fi

_msg TEST 2: Passed

#####################################################################
#####################################################################
#####
### Second multithread access to set of files
#

_bump
_msg TEST 3: Multithread acces to set of files

## First we should create list of files with limitation
##

LIST_FILE=$F_TEMP_DIR/listfile.${DAS_CTX}.$F_TIME_STAMP
for i in `ls $F_MOUNT_DIR`
do
    echo $F_MOUNT_DIR/$i >>$LIST_FILE
done

TEST_CMD="$REMOTE_FUSER_TEST_APP -t $N_THR -r $RUN_TIME -l $LIST_FILE"
_msg "## $TEST_CMD"
eval "$TEST_CMD"
if [ $? -ne 0 ]
then
    _test_failed "Multiply files test failed"
fi


_msg TEST 3: Passed

#####
### Here we are stopping fuser
#
_stop_fuser

_msg TEST PASSED!
