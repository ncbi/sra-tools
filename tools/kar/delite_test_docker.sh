#!/bin/bash

##
## NOTE: That script does not have permissions for execution, because it should
##       run only from docker container. It is umbrella for sra_delite.sh script
##

usage ()
{
    cat <<EOF >&2

That script will run test results of delite process on SRA run by it's accession

Syntax:

    `basename $0` < -h | --help | ACCESSION [ACCESSION ...] >

Where:

       ACCESSION - valid SRA accession
     -h | --help - print that help message

EOF

}

if [ $# -eq 0 ]
then
    usage

    echo ERROR: ACCESSION parameter is not defined >&2
    exit 1
fi

if [ $# -eq 1 ]
then
    case $1 in
        -h)
            usage
            exit 0
            ;;
        --help)
            usage
            exit 0
            ;;
    esac
fi

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

delite_test ()
{
    ACC=$1

    if [ -z "$ACC" ]
    then
        echo ERROR: invalid usage or delite_test command, ACCESSION missed >&2
        exit 1
    fi

    cat <<EOF 

#######################################################################################
## Running test on results of delite process for $ACC
## `date`
EOF

    OUTD=/output/$ACC

    run_cmd sra_delite.sh test --target $OUTD

cat <<EOF
## Finished test on results of delite process for $ACC
## `date`
#######################################################################################

EOF
}

for i in $@
do
    delite_test $i
done

echo "DONE ($@)"
