TOOLPATH="$HOME/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin"
TOOL="${TOOLPATH}/fasterq-dump"
REFTOOL="${TOOLPATH}/fastq-dump"
MD5_DB="ref_md5.sqlite"

function echo_verbose {
	if [[ $VERBOSE -ne 0 ]]; then
		echo "$1"
	fi
}

function run {
	if [[ $VERBOSE -ne 0 ]]; then
    	echo "$1"
    	eval "time $1"
	else
		eval "$1"
	fi
}

function gen_options {
	if [[ $VERBOSE -ne 0 ]]; then
    	OPTIONS="$1 -p"
	else
    	OPTIONS="$1"
	fi
}

function check_tools {
	TOOL_LOC=`which $TOOL`
	if [ ! -f $TOOL_LOC ]; then
    	echo "cannot find $TOOL! aborting"
    	exit 3
	fi
	echo_verbose "$TOOL is located at $TOOL_LOC"

	TOOL_LOC=`which $REFTOOL`
	if [ ! -f $TOOL_LOC ]; then
    	echo "cannot find $REFTOOL! aborting"
    	exit 3
	fi
	echo_verbose "$REFTOOL is located at $TOOL_LOC"

	TOOL_VER=`$TOOL -V`
	retcode=$?
	if [[ $retcode -ne 0 ]]; then
    	echo "cannot execute $TOOL! aborting"
    	exit 3
	fi

	REFTOOL_VER=`$REFTOOL -V`
	retcode=$?
	if [[ $retcode -ne 0 ]]; then
    	echo "cannot execute $REFTOOL! aborting"
    	exit 3
	fi
}

function get_md5 {
	CMD="./get_ref.py $MD5_DB $1"
	eval $CMD
}

function set_md5 {
	CMD="./store_ref.py $MD5_DB $1 $2"
	eval $CMD
}

function md5_of_file {
	if [ -f "$1" ]; then
		md5sum "$1" | cut -d ' ' -f 1
		rm "$1"
	else
		echo "-"
	fi
}

function compare_md5 {
	if [[ "$1" != "$2" ]]; then
        echo "difference in md5-sum for $3 ! aborting"
        exit 3
	fi
}
