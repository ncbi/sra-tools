#! /bin/bash

# ================================================================
#
#   the purpose of this test is to verify that the read-id
#   is populated by fasterq-dump in the "--fasta-unsorted" - mode
#
#   this has to be tested for flat tables as well as for cSRA
#
# ================================================================

set -e

BINDIR="$1"
if [[ ! -d $BINDIR ]]; then
    BINDIR="${HOME}/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin"
fi

if [[ ! -d $BINDIR ]]; then
    echo "cannot find ${BINDIR} --> exit"
    exit 3
fi

./test_for_read_id_flat.sh "${BINDIR}"
./test_for_read_id_cSRA.sh "${BINDIR}"
