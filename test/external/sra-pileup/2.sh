TOOLDIR="$HOME/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin"
SAMDUMP="${TOOLDIR}/sam-dump"
VDBDUMP="${TOOLDIR}/vdb-dump"
SQLITE3="sqlite3"
ACC="SRR3697723"
DB="$ACC.db"
SAMTAB="SAM"
PRIMTAB="PRIM"

load_data() {

SAM_CMD="./run_samdump.py --exe ${SAMDUMP} --acc ${ACC} --opt primary"

#read the columns in the same order they appear in the SAM-format
#when importing into sqlite, use the same names for each column as in the SAM-format
#( that means renaming them into sam-columns )
PRIM_COLS="ALIGN_ID,SEQ_SPOT_ID,SAM_FLAGS,REF_NAME,REF_POS,MAPQ,CIGAR_SHORT,MATE_REF_NAME,MATE_REF_POS,TEMPLATE_LEN,READ,SAM_QUALITY,MATE_ALIGN_ID"
PRIM_CMD="${VDBDUMP} ${ACC} -f csv -T PRIM -C ${PRIM_COLS}"

#start clean, remove the database
rm -f ${DB}

#create the SAM- and the SRC-table
${SQLITE3} ${DB} <<- HEREDOC_MARKER
create table if not exists ${SAMTAB} (
    "ROW_ID" INT,
    "QNAME" TEXT,
    "FLAGS" INT,
    "REF" TEXT,
    "POS" INT,
    "MAPQ" INT,
    "CIGAR" TEXT,
    "MREF" TEXT,
    "MPOS" INT,
    "TLEN" INT,
    "SEQ" TEXT,
    "QUAL" TEXT
);

.shell echo "importing output of sam-dump"
.mode csv
.import '|${SAM_CMD}' ${SAMTAB}

create table if not exists ${PRIMTAB} (
    "ROW_ID" INT,
    "QNAME" INT,
    "FLAGS" INT,
    "REF" TEXT,
    "POS" INT,
    "MAPQ" INT,
    "CIGAR" TEXT,
    "MREF" TEXT,
    "MPOS" INT,
    "TLEN" INT,
    "SEQ" TEXT,    
    "QUAL" TEXT,
    "MATE_ID" INT
);

.shell echo "importing PRIMARY_ALIGNMENT-table via vdb-dump"
.mode csv
.import '|${PRIM_CMD}' ${PRIMTAB}

HEREDOC_MARKER
}

compare_rows() {
    COL="$1"
    echo -e "\ncomparing ${SAMTAB}.${COL} vs ${PRIMTAB}.${COL}"
    SRC1="${SQLITE3} ${DB} --csv 'select ROW_ID,${COL} from ${SAMTAB} order by ROW_ID;'"
    SRC2="${SQLITE3} ${DB} --csv 'select ROW_ID,${COL} from ${PRIMTAB} order by ROW_ID;'"
    diff -qs <( eval ${SRC1} ) <( eval ${SRC2} )
}

compare_data() {
    for col in QNAME FLAGS REF POS MAPQ CIGAR MREF MPOS TLEN SEQ QUAL
    do
        compare_rows ${col}
    done
}

load_data
compare_data

echo -e "\ndone."
