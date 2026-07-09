set -e

BINDIR="$1"

ACC="actual/2.acc"
INI="actual/2.ini"

run_and_check_flat() {
echo "CHECK FLAT"
rm -rf $ACC
echo "seed = 10101
rows = 10
product = flat
checksum = md5
with_name = yes
layout = T5 : B30-50 : T5 : B44
layout = T5 : B55 T5 : B32
layout = T5 : B45 T5 : B47
spotgroup = SG1
spotgroup = SG2" > $INI
$BINDIR/rnd2sra $INI --out $ACC
$BINDIR/fasterq-dump $ACC -O actual
$BINDIR/vdb-dump $ACC -R1 -C BASE_COUNT
echo "----------------------------------
"
}

run_and_check_flat

run_and_check_db() {
echo "CHECK DB"
rm -rf $ACC
echo "seed = 10101
rows = 10
product = db
checksum = md5
with_name = yes
layout = T5 : B30-50 : T5 : B44
layout = T5 : B55 T5 : B32
layout = T5 : B45 T5 : B47
spotgroup = SG1
spotgroup = SG2" > $INI
$BINDIR/rnd2sra $INI --out $ACC
$BINDIR/fasterq-dump $ACC -O actual
$BINDIR/vdb-dump $ACC -R1 -C BASE_COUNT
echo "----------------------------------
"
}

run_and_check_db

run_and_check_csra() {
echo "CHECK cSRA"
rm -rf $ACC
echo "seed = 10101
rows = 10
product = csra
checksum = md5
spots = F : 12 : 55 : 120
spots = N : 7 : 50 : 100
spots = 1 : 3 : 57 : 102
spots = 2 : 4 : 56 : 103
spotgroup = SG1
spotgroup = SG2" > $INI
$BINDIR/rnd2sra $INI --out $ACC
$BINDIR/fasterq-dump $ACC -O actual
$BINDIR/vdb-dump $ACC -R1 -C BASE_COUNT
$BINDIR/vdb-dump $ACC -T PRIM -R1 -C BASE_COUNT
echo "----------------------------------
"
}

run_and_check_csra

echo "success!"
