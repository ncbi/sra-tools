#!/bin/bash

##
## NOTE: That script does not have permissions for execution, because it should
##       run only from docker container. It is umbrella for sra_delite.sh script
##

SKIPTEST_TAG="--skiptest"

usage ()
{
    cat <<EOF >&2

That script will run delite process on SRA fun by it's accession

Syntax:

    `basename $0` < -h | --help | [$SKIPTEST_TAG] PATH TARGET_DIR >

Where:

            PATH - path to valid KAR file
      TARGET_DIR - directory for delited run, it should not exist,
                   script will create one
     -h | --help - print that help message

EOF

}

case $# in
    1)
        case $1 in
            -h)
                usage
                exit 0
                ;;
            --help)
                usage
                exit 0
                ;;
            *)
                echo ERROR: invalid argument \'$1\' >&2
                exit 1
                ;;
        esac
        ;;
    2)
        SRC_KAR=$1
        DST_DIR=$2
        ;;
    3)
        if [ $1 != "$SKIPTEST_TAG" ]
        then
            echo ERROR: invalid argument \'$1\' >&2
            exit 1
        fi
        SKIPTEST=$1
        SRC_KAR=$2
        DST_DIR=$3
        ;;
    *)
        echo ERROR: invalid arguments \'$1\' >&2
        exit 1
        ;;
esac

run_cmd ()
{
    CMD="$@"

    if [ -z "$CMD" ]
    then
        echo ERROR: invalid usage or run_cmd command >&2
        exit 1
    fi

    echo
    echo "## $CMD"
    eval $CMD
    RV=$?
    if [ $RV -ne 0 ]
    then
        echo ERROR: command failed with exit code $RV >&2
        exit $RV
    fi
}

delite_work_dir ()
{
    D2R=$1
    if [ -d "$D2R" ]
    then
            ## We do not care about if that command will fail
        echo Removing directory $D2R
        vdb-unlock $D2R
        rm -r $D2R
        if [ $? -ne 0 ]
        then
            echo WARNING: can not remove directory $D2R
        fi
    fi

}

cat <<EOF 

#######################################################################################
## Running delite process for $SRC_KAR to $DST_DIR
## `date`
EOF

run_cmd sra_delite.sh import --archive $SRC_KAR --target $DST_DIR

run_cmd sra_delite.sh delite --target $DST_DIR  --schema /etc/ncbi/schema

run_cmd sra_delite.sh export --target $DST_DIR $SKIPTEST

##
## Removing work directory

delite_work_dir $DST_DIR/work
delite_work_dir $DST_DIR/work.vch

cat <<EOF
## Finished delite process for $ACC
## `date`
#######################################################################################
EOF

echo "DONE ($@)"
