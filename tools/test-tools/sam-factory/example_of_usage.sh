
set -e

BINDIR="$HOME/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin"
SAM_FACTORY_BIN="${BINDIR}/sam-factory"

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

function insert_schema_paths() {
    local USER_SETTINGS="${HOME}/.ncbi/user-settings.mkfg"
    local SCHEMA_PATHS_KEY="/vdb/schema/paths"
    local SCHEMA_PATHS_VALUE="${HOME}/devel/ncbi-vdb/interfaces"

    local FOUND_SCHEMA_PATHS=`grep ${SCHEMA_PATHS_KEY} ${USER_SETTINGS}`
    set -- $FOUND_SCHEMA_PATHS
    if [ -z $1 ]; then
        local LINE="${SCHEMA_PATHS_KEY} = \"${SCHEMA_PATHS_VALUE}\""
        echo "inserting: $LINE"
        echo "$LINE" >> "$USER_SETTINGS"
    fi
}

function load_with_bam_load() {
    echo -e "\nrunning bam-load:"
    local BAM_LOAD="$BINDIR/bam-load"
    local SCHEMA_INC="${HOME}/devel/ncbi-vdb/interfaces"
    check_if_executable "${BAM_LOAD}"

    AFTER_BAMLOAD="SYN_ACC.DIR"
    rm -rf "${AFTER_BAMLOAD}"

    insert_schema_paths
    CMD="${BAM_LOAD} ${BAM_LOAD_SAM} --ref-file ${BAM_LOAD_REF} --output ${AFTER_BAMLOAD}"
    echo -e "\t${CMD}"
    eval "${CMD}"
    echo -e "\tbam-load return-code = $?"
}

function create_archive_with_kar() {
    echo -e "\nrunning kar:"
    local KAR="$BINDIR/kar"
    check_if_executable "${KAR}"

    AFTER_KAR="SYN_ACC"
    rm -rf "${AFTER_KAR}"

    CMD="${KAR} -c ${AFTER_KAR} -d ${AFTER_BAMLOAD}"
    echo -e "\t${CMD}"
    eval "${CMD}"
    echo -e "\tkar return-code = $?"
    rm -rf "${AFTER_BAMLOAD}"
}

function re_produce_sam_with_sam_dump() {
    echo -e "\nrunning sam-dump:"
    local SAM_DUMP="$BINDIR/sam-dump"
    check_if_executable "${SAM_DUMP}"

    local AFTER_SAM_DUMP="SYN_ACC.SAM"
    rm  -rf "${AFTER_SAM_DUMP}"

    CMD="${SAM_DUMP} ${AFTER_KAR} -u > ${AFTER_SAM_DUMP}"
    echo -e "\t${CMD}"
    eval "${CMD}"
    echo -e "\tsam-dump return-code = $?"

    #cat "${AFTER_SAM_DUMP}"
}

function test_sam_factory() {
    echo -e "\ntesting ${SAM_FACTORY_BIN}:"

    local SAM_FACTORY_CONFIG="1.sf"
    local BAM_LOAD_CONFIG="config.txt"
    local BAM_LOAD_SAM="data.sam"
    local BAM_LOAD_REF="rand-ref.fasta"

    rm -rf "${BAM_LOAD_CONFIG}" "${BAM_LOAD_SAM}" "${BAM_LOAD_REF}"
    #=======================================================
    cat "${SAM_FACTORY_CONFIG}" | "${SAM_FACTORY_BIN}"
    #=======================================================

    check_if_exists "${BAM_LOAD_CONFIG}"
    check_if_exists "${BAM_LOAD_SAM}"
    check_if_exists "${BAM_LOAD_REF}"

    load_with_bam_load
    create_archive_with_kar
    re_produce_sam_with_sam_dump
}

#----------------------------------------------------------------------

test_sam_factory
