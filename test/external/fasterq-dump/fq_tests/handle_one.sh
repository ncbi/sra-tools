#!/bin/bash

ACC=$1

#skip comments
if [[ "${ACC}" == "#"* ]]; then
    exit
fi

#skip empty lines
if [[ "${ACC}" == "" ]]; then
    exit
fi

echo "start handling $ACC"

./whole_spot.sh $ACC
retcode=$?
if [[ $retcode -ne 0 ]]; then
	echo "done handling $ACC ---> ERROR"
    exit 3
fi

./split_spot.sh $ACC
retcode=$?
if [[ $retcode -ne 0 ]]; then
	echo "done handling $ACC ---> ERROR"
    exit 3
fi

./split_files.sh $ACC
retcode=$?
if [[ $retcode -ne 0 ]]; then
	echo "done handling $ACC ---> ERROR"
    exit 3
fi

./split3.sh $ACC
retcode=$?
if [[ $retcode -ne 0 ]]; then
	echo "done handling $ACC ---> ERROR"
    exit 3
fi

./unsorted.sh $ACC
retcode=$?
if [[ $retcode -ne 0 ]]; then
	echo "done handling $ACC ---> ERROR"
    exit 3
else
	echo "done handling $ACC ---> OK"
fi

