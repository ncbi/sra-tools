#! /usr/bin/bash

set -e

ORG="SRR341578"
ACC="./${ORG}a"

if [ ! -d $ACC ]; then
    PREFETCH=`./prefetch.sh`
    CMD="$PREFETCH $ORG > /dev/null 2> /dev/null"
    eval "$CMD"
fi

if [ -f $ORG/$ORG.sra.vdbcache ]; then
    rm -f $ORG/$ORG.sra.vdbcache
fi

if [ -f $ORG/$ORG.sra ]; then
    mv $ORG/$ORG.sra $ORG/${ORG}a.sra 
fi

if [ -d $ORG ]; then
    mv $ORG $ACC
fi

echo "$ACC"
