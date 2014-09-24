#!/bin/sh

# test gcc for support of supplied parameters

if test $# -ge 4
then
    if ( echo "typedef int x;" | gcc -c -xc - -o /dev/null $1 $2 $3 $4 > /dev/null 2>&1 )
    then
	echo $1 $2 $3 $4
	exit 0
    fi
fi

if test $# -ge 3
then
    if ( echo "typedef int x;" | gcc -c -xc - -o /dev/null $1 $2 $3 > /dev/null 2>&1 )
    then
	echo $1 $2 $3
	exit 0
    fi
fi

if test $# -ge 2
then
    if ( echo "typedef int x;" | gcc -c -xc - -o /dev/null $1 $2 > /dev/null 2>&1 )
    then
	echo $1 $2
	exit 0
    fi
fi

if test $# -ge 1
then
    if ( echo "typedef int x;" | gcc -c -xc - -o /dev/null $1 > /dev/null 2>&1 )
    then
	echo $1
	exit 0
    fi
fi
