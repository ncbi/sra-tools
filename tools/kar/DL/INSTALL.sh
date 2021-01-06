#!/bin/bash

#THIS is not a script ... lol

usage ()
{
    AGA="$@"

    if [ -n "$AGA" ]
    then
        echo
        echo $AGA >&2
    fi

    cat <<EOF >&2

This script will install or update delite binaries in somewhere

Syntax: `basename $0` [-n] directory_where_to_install

Where:
    -n - file will be updated only if there is newer version

EOF

    exit 1
}


NEW_ONLY=0

case $# in
    1)
        TARGET_DIR=$1
        ;;
    2)
        if [ $1 = "-n" ]
        then
            NEW_ONLY=1
        else
            usage ERROR: invalid argument \'$1\'
        fi

        TARGET_DIR=$2
        ;;
    *)
        usage ERROR: invalid arguments
        ;;
esac


if [ -z "$TARGET_DIR" ]
then
    echo ERROR: target directory is not defined >&2
    exit 1
fi

##
## Looking to place where is everything
INSDIR=$( cd $( dirname $0 ); pwd )
VER_DIR=$INSDIR/VER

if [ ! -d "$VER_DIR" ]
then
    echo ERROR: can not stat version directory \'$VER_DIR\' >&2
    exit 1
fi

##
## Some usefuls
run_cmd ()
{
    CMD="$@"
    if [ -z "$CMD" ]
    then
        echo WARNING: run_cmd called with empty arguments >&2
        return
    fi

    echo "## $CMD"
    eval "$CMD"
    if [ $? -ne 0 ]
    then
        echo ERROR: command failed >&2
        exit 1
    fi
}

##
## Looking for target directory

check_make_dir ()
{
    if [ ! -d "$1" ]
    then
        echo "## Creating directory '$1'"
        mkdir $1
        if [ $? -ne 0 ]
        then
            echo ERROR: failed to create directory \'$1\'
            exit 1
        fi
    fi
}

check_make_dir $TARGET_DIR

##
## Copy binaries
BIN_SRC=$VER_DIR/BIN
BIN_DST=$TARGET_DIR/bin

check_make_dir $BIN_DST

if [ "$NEW_ONLY" = "1" -a -d "$BIN_DST" ]
then
    echo "## Updating newer binaries"
    for i in `ls $BIN_SRC`
    do
        SF=$BIN_SRC/$i
        DF=$BIN_DST/$i
        if [ "$SF" -nt "$DF" ]
        then
            run_cmd cp -pP "$SF" "$DF"
        else
            echo "## SKIP: $i"
        fi
    done
else
    echo "## Replacing binaries"

    run_cmd rm -r $BIN_DST
    run_cmd cp -rpP $BIN_SRC $BIN_DST
fi

##
## Copy schemas
SCM_SRC=$VER_DIR/SCM
SCM_DST=$TARGET_DIR/schema

check_make_dir $SCM_DST

if [ "$NEW_ONLY" = "1" ]
then
    echo "## Updating newer schemas"
    for SF in `find $SCM_SRC -type f -name "*.vschema"`
    do
        DF=$SCM_DST/`echo $SF | sed "s#$SCM_SRC##1" `
        if [ "$SF" -nt "$DF" ]
        then
            FDO=$( dirname $DF )
            if [ ! -d $FDO ]
            then
                run_cmd mkdir $FDO
            fi
            run_cmd cp -pP "$SF" "$DF"
        else
            echo "## SKIP: $SF"
        fi
    done
else
    echo "## Replacing schemas"

    run_cmd rm -r $SCM_DST
    run_cmd cp -rpP $SCM_SRC $SCM_DST
fi

for i in `find $SCM_DST -type f -name trace.vschema`
do
    run_cmd rm -f $i
done


UPDD=$( cd $TARGET_DIR; pwd )
UPDS=$UPDD/UPDATE.sh

echo Generating update script : $UPDS

cat <<EOF > $UPDS
#!/bin/bash

echo Update: $UPDD
$INSDIR/INSTALL.sh $UPDD
if [ $? -ne 0 ]
then
    echo UPDATE FAILED>&2
    exit 1
fi

echo ENJOY
EOF

chmod 774 $UPDS

echo For future update binaries, please run script $UPDS

echo DONE

