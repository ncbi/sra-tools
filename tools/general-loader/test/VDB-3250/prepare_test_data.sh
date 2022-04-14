#!/bin/bash

########################################################
## That script will run tests for error VDB-3250
#  which related to uint16_t and size of reading data 
## bu GeneralLoader
########################################################

####
### Usefuls
#

_S_NM=`basename $0`

_msg ()
{
    echo "[$_S_NM]: $@"
}

_err ()
{
    echo "[$_S_NM] ERROR: $@"
}

_err_exit ()
{
    echo "[$_S_NM] ERROR: $@" >&2
    exit 1
}

_exec ()
{
    CMD="$@"
    echo "## $CMD"
    eval $CMD
    if [ $? -ne 0 ]
    then
        _err_exit FAILED: $CMD
    fi
}

_exec_plain ()
{
    CMD="$@"
    echo "## $CMD"
    eval $CMD
    if [ $? -ne 0 ]
    then
        _err FAILED: $CMD
    fi
}

####
### Args
#

syntax ()
{
    cat <<EOF >&2

Syntax : $_S_NM top_dir bin_dir

EOF
}

if [ $# -ne 2 ]
then
    echo "[$_S_NM] ERROR: invalid arguments" >&2

    syntax

    exit 1
fi

T_VAR=$1
if [ ! -d "$T_VAR" ]
then
    _err_exit can not stat directory \'$T_VAR\'
fi

test_check_dir ()
{
    A=$2
    V=$1

    if [ ! -d "$A" ]
    then
        _err_exit directory \'$A\' does not exists
    fi

    eval $V=$A
}

test_check_dir TOP_DIR `cd $T_VAR; pwd`
test_check_dir TEST_DIR $TOP_DIR/test/general-loader
test_check_dir TEST_DATA_DIR $TEST_DIR/VDB-3250


T_VAR=$2
if [ ! -d "$T_VAR" ]
then
    _err_exit can not stat directory \'$T_VAR\'
fi

test_check_dir TBD `cd $T_VAR/..;pwd`
test_check_dir BIN_DIR $TBD/bin
test_check_dir TEST_BIN_DIR $TBD/test-bin

####
### Checking data to work on
##
#
INPUT_TEST_DATA=$TEST_DATA_DIR/test_data.bin
if [ ! -f "$INPUT_TEST_DATA" ]
then
    _err_exit can not stat file \'$INPUT_TEST_DATA\'
fi

TEST_SCHEMA=$TEST_DATA_DIR/test_data.vschema
if [ ! -f "$TEST_SCHEMA" ]
then
    _err_exit can not stat file \'$TEST_SCHEMA\'
fi

OUTPUT_TEST_DATA=$TEST_DATA_DIR/test_data.gw
if [ -f "$OUTPUT_TEST_DATA" ]
then
    _exec rm -f $OUTPUT_TEST_DATA
fi

TEST_DATABASE=$TEST_DATA_DIR/test
if [ -d "$TEST_DATABASE" ]
then
    _exec rm -rf $TEST_DATABASE
fi

####
### Checing if there are necessary binaries: general-writer and
##  general-loader
#

test_check_exe ()
{
    A=$2
    V=$1

    if [ ! -x "$A" ]
    then
        _err_exit executable \'$A\' does not exists
    fi

    eval $V=$A
}

test_check_exe LOADER_EXE $BIN_DIR/general-loader

####
### Making test binaries
##
#
cd $TEST_DATA_DIR

_exec make

test_check_exe PREPARER_EXE $TEST_BIN_DIR/prepare_test_data
test_check_exe CHECKER_EXE $TEST_BIN_DIR/check_test_result

####
### Here test starts
##
#
_exec $PREPARER_EXE -d $INPUT_TEST_DATA -s $TEST_SCHEMA -o $OUTPUT_TEST_DATA
_exec "export VDB_CONFIG=.. ; $LOADER_EXE <$OUTPUT_TEST_DATA -T $TEST_DATABASE"
_exec $CHECKER_EXE -d  $INPUT_TEST_DATA -s $TEST_SCHEMA -T $TEST_DATABASE

####
### Clearing unnecessary data
##
#
if [ -f "$OUTPUT_TEST_DATA" ]
then
    _exec_plain rm -f $OUTPUT_TEST_DATA
fi

if [ -d "$TEST_DATABASE" ]
then
    _exec_plain rm -rf $TEST_DATABASE
fi

echo TEST PASSED
