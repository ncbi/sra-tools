# ==============================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
# ==============================================================================

echo -n Testing --delete option of kdbmeta...

bin_dir=$1
kdbmeta=$2
VERBOSE=$3
tool=$1/$2

rm -rf actual tmp || exit 2

if [ ! -f "$bin_dir/kar" ] ; then
    echo "kar not found in $bin_dir"
    exit 1
fi

if [ ! -f "$tool" ] ; then
    echo "$kdbmeta not found in $bin_dir"
    exit 1
fi

mkdir actual || exit 2
if [ "$VERBOSE" != "" ] ; then
    echo "$bin_dir/kar --extract test.sra --directory tmp"
fi
$bin_dir/kar --extract test.sra --directory tmp || exit 3

###################################### DB ######################################

$tool tmp > actual/db || exit 4
diff actual/db expected/db || exit 5

if [ "$VERBOSE" != "" ] ; then echo "$tool tmp A/B=1" ; fi
$tool tmp A/B=1 || exit 6
$tool tmp > actual/db.AB || exit 7
diff actual/db.AB expected/db.AB || exit 8

if [ "$VERBOSE" != "" ] ; then echo "$tool tmp --delete schema" ; fi
$tool tmp --delete schema || exit 9
$tool tmp > actual/db.schema || exit 10
diff actual/db.schema expected/db.schema || exit 11

$tool tmp --delete SOFTWARE || exit 12
$tool tmp > actual/db.SOFTWARE || exit 13
diff actual/db.SOFTWARE expected/db.SOFTWARE || exit 14

$tool tmp --delete LOAD || exit 15
$tool tmp > actual/A_B || exit 16
diff actual/A_B expected/A_B || exit 17

$tool tmp --delete A/B || exit 18
$tool tmp > actual/B || exit 19
diff actual/B expected/B || exit 20

$tool tmp --delete A || exit 21
$tool tmp > actual/A || exit 22
diff actual/A expected/A || exit 23

################################# Table via -T #################################

$tool tmp -TSEQUENCE > actual/tbl || exit 24
diff actual/tbl expected/tbl || exit 25

$tool tmp -TSEQUENCE A/B=1 || exit 26
$tool tmp -TSEQUENCE > actual/tbl.AB || exit 27
diff actual/tbl.AB expected/tbl.AB || exit 28

$tool tmp -TSEQUENCE --delete STATS || exit 29
$tool tmp -TSEQUENCE > actual/tbl.STATS || exit 30
diff actual/tbl.STATS expected/tbl.STATS || exit 31

$tool tmp -TSEQUENCE --delete col || exit 32
$tool tmp -TSEQUENCE > actual/tbl.col || exit 33
diff actual/tbl.col expected/tbl.col || exit 34

$tool tmp -TSEQUENCE --delete schema || exit 35
$tool tmp -TSEQUENCE > actual/tbl.schema || exit 36
diff actual/tbl.schema expected/tbl.schema || exit 37

$tool tmp -TSEQUENCE --delete RNA_FLAG || exit 38
$tool tmp -TSEQUENCE > actual/A_B || exit 39
diff actual/A_B expected/A_B || exit 40

$tool tmp -TSEQUENCE --delete A/B || exit 41
$tool tmp -TSEQUENCE > actual/B || exit 42
diff actual/B expected/B || exit 43

$tool tmp -TSEQUENCE --delete A || exit 44
$tool tmp -TSEQUENCE > actual/A || exit 45
diff actual/A expected/A || exit 46

##################################### Table ####################################

$bin_dir/kar --extract test.sra --directory tmp || exit 47

$tool tmp/tbl/SEQUENCE > actual/tbl || exit 48
diff actual/tbl expected/tbl || exit 49

$tool tmp/tbl/SEQUENCE A/B=1 || exit 50
$tool tmp/tbl/SEQUENCE > actual/tbl.AB || exit 51
diff actual/tbl.AB expected/tbl.AB || exit 52

$tool tmp/tbl/SEQUENCE --delete STATS || exit 53
$tool tmp/tbl/SEQUENCE > actual/tbl.STATS || exit 54
diff actual/tbl.STATS expected/tbl.STATS || exit 55

$tool tmp/tbl/SEQUENCE --delete col || exit 56
$tool tmp/tbl/SEQUENCE > actual/tbl.col || exit 57
diff actual/tbl.col expected/tbl.col || exit 58

$tool tmp/tbl/SEQUENCE --delete schema || exit 59
$tool tmp/tbl/SEQUENCE > actual/tbl.schema || exit 60
diff actual/tbl.schema expected/tbl.schema || exit 61

$tool tmp/tbl/SEQUENCE --delete RNA_FLAG || exit 62
$tool tmp/tbl/SEQUENCE > actual/A_B || exit 63
diff actual/A_B expected/A_B || exit 64

$tool tmp/tbl/SEQUENCE --delete A/B || exit 65
$tool tmp/tbl/SEQUENCE > actual/B || exit 66
diff actual/B expected/B || exit 67

$tool tmp/tbl/SEQUENCE --delete A || exit 68
$tool tmp/tbl/SEQUENCE > actual/A || exit 69
diff actual/A expected/A || exit 70

#################################### Column ####################################

$tool tmp/tbl/SEQUENCE/col/READ > actual/col || exit 71
diff actual/col expected/col || exit 72

$tool tmp/tbl/SEQUENCE/col/READ A/B=1 || exit 73
$tool tmp/tbl/SEQUENCE/col/READ > actual/col.AB || exit 74
diff actual/tbl.AB expected/col.AB || exit 75

$tool tmp/tbl/SEQUENCE/col/READ --delete schema || exit 76
$tool tmp/tbl/SEQUENCE/col/READ > actual/col.schema || exit 77
diff actual/col.schema expected/col.schema || exit 78

$tool tmp/tbl/SEQUENCE/col/READ --delete row-len || exit 79
$tool tmp/tbl/SEQUENCE/col/READ > actual/A_B || exit 80
diff actual/A_B expected/A_B || exit 81

$tool tmp/tbl/SEQUENCE/col/READ --delete A/B || exit 82
$tool tmp/tbl/SEQUENCE/col/READ > actual/B || exit 83
diff actual/B expected/B || exit 84

$tool tmp/tbl/SEQUENCE/col/READ --delete A || exit 85
$tool tmp/tbl/SEQUENCE/col/READ > actual/A || exit 86
diff actual/A expected/A || exit 87

################################################################################

rm -r actual tmp || exit 88

echo
echo ...success
