#!/bin/bash

ulimit -c unlimited

#####################################################################
## That file contains common methods and variables for remote-fuser
## test scripts
#####################################################################

###
## Some impoertants
#
DAS_PID=$$

DAS_HOST=$HOSTNAME
if [ -z "$DAS_HOST" ]
then
    DAS_HOST=`hostname 2>/dev/null`
    if [ -z "$DAS_HOST" ]
    then
        DAS_HOST="unknown-host"
    fi
fi

DAS_USER=$USER
if [ -z "$DAS_USER" ]
then
    DAS_USER=`id -n -u`
    if [ -z "$DAS_USER" ]
    then
        DAS_USER="unknown-user"
    fi
fi

DAS_CTX=${DAS_HOST}.${DAS_PID}

DAS_TSTAMP=`date +%Y-%m-%d_%H-%M-%S`

####
##  Environment: Jy's a mal naaier maar ek hou van jou baie
#

_print_env ()
{
    for i in @ DAS_PID USER REMOTE_USER DAS_USER HOME HOST HOSTNAME DAS_HOST HOSTTYPE
    do
        echo "[$i] [${!i}]"
    done
    echo "[groups] [`groups`]"
}

_print_env $@

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

_exec_plain ()
{
    CDM="$@"
    _msg "## $CDM"
    eval "$CDM"

    return $?
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

## Syntax : _assign_create_dir path_to_dir VARIABLE_NAME_TO_ASSIGN
##
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

## Here we are checking that two pathes points to the same directory
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

## Checking that path is in mtab, returns 0 if dir mounted and used
## Syntax: _check_dir_mounted path_to_dir
##
_check_dir_mounted ()
{
        ## We are simple not check an mount directory.
    for i in `mount | awk ' { print $3 } ' `
    do
        _same_dir $1 $i
        if [ $? -eq 0 ]
        then
            _msg "Mount point is in use '$1'"
            return 0
        fi
    done

    _msg "Mount point is not in use '$1'"
    return 1
}

###
##  Various FUSE checks
#

##  Stores fuser PID and MountPoint to file
##  Syntax: _store_fuser_params path PID MOUNT_POINT
_store_fuser_params ()
{
    cat>$1 <<EOF
pid = $2
mount_point = $3
EOF
}

##  Read fuser PID and MountPoint from file
##  Syntax: _load_fuser_params path PID MOUNT_POINT
_load_fuser_params ()
{
    RPD=`grep pid $1 | tail -n 1 | sed "s#=# #1" | awk ' { print int ( $2 ); } ' `
    RMP=`grep mount_point $1 | tail -n 1 | sed "s#=# #1" | awk ' { print $2; } ' `
    eval "$2=$RPD"
    eval "$3=$RMP"
}

##  Check that there is a process with such a PID or mount in use
##  Syntax: _is_fuser_run FUSE_PID MOUNT_POINT
##
_is_fuser_run ()
{
    FUP=$1
    if [ -n "$FUP" ]
    then
        ps -p $FUP >/dev/null 2>&1
        if [ $? -eq 0 ]
        then
            _wrn PROCESS WITH PID = $FUP EXISTS
            return 0
        fi
    fi

    MOP=$2
    _check_dir_mounted $MOP
    if [ $? -eq 0 ]
    then
        _wrn MOUNT to \'$MOP\' STILL EXISTS
        return 0
    fi

    return 1
}

##  Shutdown fuser. Returns 0 if it is success
##  Syntax: _shutdown_fuser PID MOUNT_POINT
##  
_shutdown_fuser ()
{
    _err Shutdown FUSER and exit
        ##  Check if there is still mount
        ##

    MPO=$2
    _check_dir_mounted $MPO
    if [ $? -eq 0 ]
    then
        /bin/fusermount -u $MPO
    fi

    sleep 1

        ##  Check that process is still in the memory
        ##
    FPI=$1
    if [ -n "$FPI" ]
    then
        ps -p $FPI >/dev/null 2>&1
        if [ $? -eq 0 ]
        then
            _err Killing FUSER process $FPI mounted on $MPO
            kill -9 $FPI
        fi
    fi

    sleep 3

    if [ -n "$FPI" ]
    then
        ps -p $FPI >/dev/null 2>&1
        if [ $? -eq 0 ]
        then
            _err Failed to shutdown FUSER process $FPI mounted on $MPO
            return 1
        fi
    fi

    _check_dir_mounted $MPO
    if [ $? -eq 0 ]
    then
        _err Failed to shutdown FUSER process $FPI mounted on $MPO
        return 1
    fi

    _err FUSER process $FPI was shut down

    return 0
}

