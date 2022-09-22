#!/usr/bin/env bash

set -e

ACC="$1"
TOOLPATH="$2"
PREFETCH="${TOOLPATH}/prefetch"
SAMDUMP="${TOOLPATH}/sam-dump"

echo -e "testing >$ACC<:\n"

#------------------------------------------------------------

if [[ ! -d "./$ACC" ]]; then
	$PREFETCH $ACC -p
	echo -e "\n"
fi

#------------------------------------------------------------
SAM_W="$ACC.SAM.with_qual"
if [[ ! -f "$SAM_W" ]]; then
	echo "running 'sam-dump' with qualities:"
	"$SAMDUMP" "./$ACC" > "$SAM_W"
fi
COUNTS_W=`cat $SAM_W | ./count_qual.py`
echo "counting lines in output of sam-dump with qualities (default):"
echo "	count of lines with qualities, needs to be > 0"
WQ_F1=`echo "$COUNTS_W" | cut -f1`
if [[ ! "$WQ_F1" -gt "0" ]]; then
	echo "	but it is not, it is >$WQ_F1<"
	exit 3
else
	echo "	...and it is!"
fi
echo "	count of lines without qualities, needs to be == 0"
WQ_F2=`echo "$COUNTS_W" | cut -f2`
if [[ ! "$WQ_F2" -eq "0" ]]; then
	echo "	but it is not, it is >$WQ_F2<"
	exit 3
else
	echo "	...and it is!"
fi

#------------------------------------------------------------
echo -e "\n"
SAM_O="$ACC.SAM.without_qual"
if [[ ! -f "$SAM_O" ]]; then
	echo "running 'sam-dump' without qualities:"
	"$SAMDUMP" "./$ACC" --ommit-quality > "$SAM_O"
fi
COUNTS_O=`cat $SAM_O | ./count_qual.py`
echo "counting lines in output of sam-dump without qualities ( --ommit-quality ):"
echo "	count of lines with qualities, needs to be == 0"
NQ_F1=`echo "$COUNTS_O" | cut -f1`
if [[ ! "$NQ_F1" -eq "0" ]]; then
	echo "	but it is not, it is >$NQ_F1<"
	exit 3
else
	echo "	...and it is!"
fi
echo "	count of lines without qualities, needs to be > 0"
NQ_F2=`echo "$COUNTS_O" | cut -f2`
if [[ ! "$NQ_F2" -gt "0" ]]; then
	echo "	but it is not, it is >$NQ_F2<"
	exit 3
else
	echo "	...and it is!"
fi

#------------------------------------------------------------
rm -rf "./$ACC" "$SAM_W" "$SAM_O"
