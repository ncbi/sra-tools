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

_assign_create_dir $TEST_DIR/$DAS_USER U_TEST_DIR
_assign_create_dir $U_TEST_DIR/$DAS_HOST H_TEST_DIR
_assign_create_dir $H_TEST_DIR/$DAS_PID P_TEST_DIR

###
## There are logs, bin and depot on user level of test direcotry
#
_assign_create_dir $U_TEST_DIR/bin   F_BIN_DIR
_assign_create_dir $U_TEST_DIR/depot F_DEPOT_DIR
_assign_create_dir $U_TEST_DIR/log   F_LOG_DIR

###
## There are cache, mount and temp on process level of test direcotry
#
_assign_create_dir $P_TEST_DIR/cache F_CACHE_DIR
_assign_create_dir $P_TEST_DIR/mount F_MOUNT_DIR
_assign_create_dir $P_TEST_DIR/temp  F_TEMP_DIR

F_STATUS_FILE=$P_TEST_DIR/status.file

##  Checking mounts and old logs are moved to outside script
##

##  Here we are logging and execing
##
F_LOG_FILE=$F_LOG_DIR/${SCR_SNAME}.log.${DAS_CTX}.${DAS_TSTAMP}
echo Log file: $F_LOG_FILE

exec >$F_LOG_FILE 2>&1

## One more time for log file
##
_print_env $@

## Cleaning old data
##
_bump
_msg Cleaning old data

CLN_CMD="$SCR_DIR/remote_fuser_clean.sh"
if [ -f "$CLN_CMD" ]
then
    nohup $CLN_CMD $CFG_NAME &
fi

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
        DST=$F_DEPOT_DIR/${2}.${DAS_TSTAMP}
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

F_FUSE_XML=$F_TEMP_DIR/FUSE.xml.${DAS_CTX}.${DAS_TSTAMP}
eval GET "$XML_URL" >$F_FUSE_XML
if [ $? -ne 0 ]
then
    _err_exit "Can not download XML file from '$XML_URL'"
fi

###
##  Here we are starting FUSER
#
_msg Starting Fuse

FUSER_LOG=$F_LOG_DIR/remote-fuser.log.${DAS_CTX}.${DAS_TSTAMP}
FUSER_CMD="$REMOTE_FUSER_APP -d -x $XML_URL -m $F_MOUNT_DIR -e $F_CACHE_DIR -L 5 -B 4 -o kernel_cache"

_msg "## $FUSER_CMD"
$FUSER_CMD >$FUSER_LOG 2>&1 &
FUSER_PID=$!

## Because!

_store_fuser_params $F_STATUS_FILE $FUSER_PID $F_MOUNT_DIR

_clear_data ()
{
    _msg "Removing test data"

    _exec_plain rm -rf $P_TEST_DIR
    if [ $? -ne 0 ]
    then
        _err "Can not remove test data"

        sleep 2

        _msg "Removing test data attempt 2"

        _exec_plain rm -rf $P_TEST_DIR
        if [ $? -ne 0 ]
        then
            _err "Can not remove test data"
            return 1
        fi
    fi

    _msg "Test data removed"
}

_stop_fuser ()
{
    _msg "Stopping Fuse"
    MCD="$REMOTE_FUSER_APP -u -m $F_MOUNT_DIR"
    _msg "## $MCD"
    $MCD

    sleep 100

    _is_fuser_run $FUSER_PID $F_MOUNT_DIR
    if [ $? -eq 0 ]
    then
        _shutdown_fuser $FUSER_PID $F_MOUNT_DIR
    fi

    _clear_data
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
            _shutdown_fuser $FUSER_PID $F_MOUNT_DIR
            ;;
        *)
            echo -n "$i "
            sleep 1
            _check_dir_mounted $F_MOUNT_DIR
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
TEMP_FILE=$F_TEMP_DIR/${LOCAL_FILE}.${DAS_CTX}.${DAS_TSTAMP}

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

LIST_FILE=$F_TEMP_DIR/listfile.${DAS_CTX}.$DAS_TSTAMP
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
