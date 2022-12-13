
set -e

SAM_FACTORY_BIN="sam-factory"
SAM_ANALYZE_BIN="sam-analyze"
BINDIR="$HOME/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin"

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

#just create a SAM-file with sam-factory...

SAM_FILE="data.sam"

${BINDIR}/${SAM_FACTORY_BIN} << EOF
r:type=random,name=R1,length=5000
r:type=random,name=R2,length=4000
r:type=random,name=R3,length=3000

ref-out:rand-ref.fasta
sam-out:${SAM_FILE}

p:name=A,ref=R1,repeat=1000
p:name=A,ref=R1,repeat=1000
s:name=A,ref=R1,repeat=10

p:name=B,ref=R2,repeat=1000
p:name=B,ref=R2,repeat=1000

p:name=C,ref=R3,repeat=1000
p:name=C,ref=R3,repeat=1000
EOF

#and analyze it...

${BINDIR}/${SAM_ANALYZE_BIN} -i ${SAM_FILE} -r -a -g finger.txt
