TOOLDIR="$HOME/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin"
VDBDUMP="${TOOLDIR}/vdb-dump"
SQLITE3="sqlite3"
ACC="SRR3697723"
DB="${ACC}_qual.db"
QTAB="QUAL"
AVGTAB="AVG_QUAL"

QUAL_CMD="./qual_split.py --exe ${VDBDUMP} --acc ${ACC}"

load_data() {

#read the columns in the same order they appear in the SAM-format
#when importing into sqlite, use the same names for each column as in the SAM-format
#( that means renaming them into sam-columns )
PRIM_COLS=""
PRIM_CMD="${VDBDUMP} ${ACC} -f csv -T PRIM -C ${PRIM_COLS}"

#start clean, remove the database
rm -f ${DB}

#create the SAM- and the SRC-table
${SQLITE3} ${DB} <<- HEREDOC_MARKER
.shell echo "importing quality-output of vdb-dump"
create table if not exists ${QTAB} (
    "ROW_ID" INT,
    "ROW_LEN" INT,
    "IDX" INT,
    "QUAL" INT
);
.mode csv
.import '|${QUAL_CMD}' ${QTAB}

.shell echo "populating averages"
create table if not exists ${AVGTAB} (
    "IDX" INT,
    "ROW_LEN" INT,
    "AVG_QUAL" INT,
    "CNT" INT
);
insert into ${AVGTAB} ( IDX, ROW_LEN, AVG_QUAL, CNT )
    select IDX, ROW_LEN, round( avg( QUAL ) ), count( QUAL )
    from ${QTAB}
    group by IDX, ROW_LEN;

.shell echo "creating indexes on ${AVGTAB}"
CREATE INDEX ROW_LEN_IDX ON ${AVGTAB} ( ROW_LEN );
CREATE INDEX IDX_IDX ON ${AVGTAB} ( IDX );

.shell echo "updating quality-events with averages"
alter table ${QTAB} add column "AVG_QUAL" INT;
update ${QTAB}
    set AVG_QUAL = ( select AVG_QUAL from ${AVGTAB}
                        where
                            ${QTAB}.IDX = ${AVGTAB}.IDX
                        and
                            ${QTAB}.ROW_LEN = ${AVGTAB}.ROW_LEN
                    );

.shell echo "updating quality-events with difference"
alter table ${QTAB} add column "DIFF" INT;
update ${QTAB} set DIFF = QUAL - AVG_QUAL;

HEREDOC_MARKER
}

load_data
