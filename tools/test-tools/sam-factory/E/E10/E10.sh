
#this script assumes that the path for the neccessary schema-files is set in the settings

#the directory for the binaries needs to be given as parameter to this script
BINDIR=$1

#if the BINDIR cannot be found ( or not given ) use this as fallback...
if [[ ! -d $BINDIR ]]; then
    BINDIR="$HOME/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin"
fi

SAM_FACTORY_BIN="${BINDIR}/sam-factory"
BAM_LOAD="${BINDIR}/bam-load"
ERROR_TEST="E10"

#----------------------------------------------------------------------

function check_if_executable() {
    if [[ ! -x $1 ]]; then
        echo -e "\tfailed to build or not found: $1"
        exit 3
    else
        echo -e "\t$1 found!"
    fi
}

function check_if_exists() {
    if [[ ! -f $1 ]]; then
        echo -e "\tfailed to build $1"
        exit 3
    else
        echo -e "\t$1 created!"
    fi
}

#----------------------------------------------------------------------

echo -e "\ntesting ${SAM_FACTORY_BIN}: >${ERROR_TEST}<"

SAM_FACTORY_CONFIG="${ERROR_TEST}.sf"
BAM_LOAD_CONFIG="${ERROR_TEST}.config"
BAM_LOAD_SAM="${ERROR_TEST}.sam"
BAM_LOAD_REF="${ERROR_TEST}.fasta"

rm -rf "${BAM_LOAD_CONFIG}" "${BAM_LOAD_SAM}" "${BAM_LOAD_REF}"
#=======================================================
cat "${SAM_FACTORY_CONFIG}" | "${SAM_FACTORY_BIN}"
#=======================================================

check_if_exists "${BAM_LOAD_CONFIG}"
check_if_exists "${BAM_LOAD_SAM}"
check_if_exists "${BAM_LOAD_REF}"

echo -e "\nrunning bam-load:"
check_if_executable "${BAM_LOAD}"

BAM_LOAD_OUTPUT="${ERROR_TEST}.DIR"
rm -rf "${BAM_LOAD_OUTPUT}"

BAM_LOAD_STDERR="${ERROR_TEST}.stderr"
rm -rf "${BAM_LOAD_STDERR}"

CMD="${BAM_LOAD} ${BAM_LOAD_SAM} --ref-file ${BAM_LOAD_REF} --output ${BAM_LOAD_OUTPUT} 2>${BAM_LOAD_STDERR}"
echo -e "\t${CMD}"
eval "${CMD}"
echo -e "\tbam-load return-code = $?"

#clean up of the directory created by bam-load
rm -rf "${BAM_LOAD_OUTPUT}"

#cut off the date/time/tool/version for each line in STDERR
BAM_LOAD_STDERR_CUT="${ERROR_TEST}.stderr.cut"
cat "${BAM_LOAD_STDERR}" | cut -c 36- > "${BAM_LOAD_STDERR_CUT}"

#compare produced stderr with expected stderr
BAM_LOAD_STDERR_EXP="${ERROR_TEST}.stderr.expected"
if ( diff -q "${BAM_LOAD_STDERR_CUT}" "${BAM_LOAD_STDERR_EXP}" ) then
    rm -rf "${BAM_LOAD_STDERR_CUT}"
    rm -rf "${BAM_LOAD_CONFIG}" "${BAM_LOAD_SAM}" "${BAM_LOAD_REF}"
    rm -rf "${BAM_LOAD_STDERR}"
    echo -e "\nbam-load stderr output is identical to expected output"
    exit 0
else
    echo -e "\nbam-load stderr output is NOT identical to expected output"
    exit 3
fi
