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

####
##  Some interesting stuff
#

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

F_TEST_DIR=$TEST_DIR/remote_fuser
if [ ! -d "$F_TEST_DIR" ]
then
    _msg "Creating directory '$F_TEST_DIR'"
    _exec mkdir $F_TEST_DIR
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
_check_assign_dir cache F_CACHE_DIR
_check_assign_dir mount F_MOUNT_DIR
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

##  Here we are logging and execing
##
F_TIME_STAMP=`date +%Y-%m-%d_%H-%M-%S`
F_LOG_FILE=$F_LOG_DIR/${SCR_SNAME}.log.${F_TIME_STAMP}
echo Log file: $F_LOG_FILE
## exec >$F_LOG_FILE 2>&1

## Here we are copying binaries
##
_copy_assign_app ()
{
    if [ $# -ne 2 ]
    then
        _err_exit "_copy_assign_app(): requires two arguments"
    fi

    SRC=$BIN_DIR/$1
    if [ ! -x "$SRC" ]
    then
        _err_exit "_copy_assign_app(): can not stat file '$SRC'"
    fi

    LNK=$F_BIN_DIR/$1

    eval cmp $SRC $LNK
    if [ $? -ne 0 ]
    then
        DST=$F_DEPOT_DIR/${1}.${F_TIME_STAMP}
        _exec cp -p $SRC $DST
        _exec rm -f $LNK
        _exec ln -s $DST $LNK
    fi

    eval "$2=$LNK"
}

_copy_assign_app remote-fuser REMOTE_FUSER_APP

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

F_FUSE_XML=$F_TEMP_DIR/FUSE.xml.$F_TIME_STAMP
eval GET "$XML_URL" >$F_FUSE_XML
if [ $? -ne 0 ]
then
    _err_exit "Can not download XML file from '$XML_URL'"
fi

###
##  Here we are starting FUSER
#
_msg Starting Fuse

FUSER_LOG=$F_LOG_DIR/remote-fuser.log.$F_TIME_STAMP
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
            return 0
        fi
    fi

    _check_mount $F_MOUNT_DIR
    if [ $? -eq 0 ]
    then
echo MOUNT_EXISTS
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

    sleep 1

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

    sleep 10

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
TEMP_FILE=$F_TEMP_DIR/${LOCAL_FILE}.${F_TIME_STAMP}

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
_msg TEST 2: Multithread acces to single file
_msg TEST 2: Passed

#####################################################################
#####################################################################
#####
### Second multithread access to set of files
#
_msg TEST 3: Multithread acces to set of files
_msg TEST 2: Passed

#####
### Here we are stopping fuser
#
_stop_fuser
