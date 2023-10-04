#!/bin/bash

vdb_dump_makedb=$1
bin_dir=$2
vdb_dump_binary=$3

echo vdb-dump-makedb: ${vdb_dump_makedb}

echo "testing ${vdb_dump_binary}"

TEMPDIR=.

echo Testing ${vdb_dump_binary} from ${bin_dir}

rm -rf actual
mkdir -p actual

echo makedb:
if [ ! -f data ]
then
	mkdir -p data; ${vdb_dump_makedb}
fi

function run_test() {
	local test_id=$1
	local test_args=$2

	local output=actual/$test_id.stdout

	${bin_dir}/${vdb_dump_binary} $test_args > $output 2>actual/$test_id.stderr
	local res=$?
	if [ "$res" != "0" ];
		then echo "${vdb_dump_binary} $test_args ($test_name $test_id) FAILED, res=$res output=$output" && exit 1;
	fi

	diff expected/$test_id.stdout $output >actual/$test_id.diff
	res=$?
	if [ "$res" != "0" ];
		then echo "${vdb_dump_binary} $test_name ($test_id) FAILED, res=$res diff=$(cat actual/$test_id.diff)" && exit 1;
	fi
	echo run_test $test_id done
}

function run_test_neg() {
	local test_id=$1
	local test_args=$2

	local output=actual/$test_id.stdout

	${bin_dir}/${vdb_dump_binary} $test_args > $output  2>actual/$test_id.stderr
	local res=$?
	if [ "$res" == "0" ];
		then echo "${vdb_dump_binary} $test_args ($test_name $test_id) unexpectedly succeeded, res=$res output=$output" && exit 1;
	fi

	echo run_test $test_id done
}

#TODO: fail if multiple tables and/or views are requested

# output format
run_test "1.0" "SRR056386 -R 1 -C READ -f tab"
run_test "1.1" "SRR056386 -R 1 -C READ -f tab -I"

# # nested databases
run_test "2.0" "-E data/NestedDatabase"
# run_test "2.1" "-T SUBDB_1.SUBSUBDB_1.TABLE1 data/NestedDatabase"
# run_test "2.2" "-T SUBDB_1.SUBSUBDB_2.TABLE2 data/NestedDatabase"
# see VDB-5367

# Views
#	the accessions used below were taken from a blackbox test run, each represents a
#  	different table type in a CSRA object.
#
run_test "3.0" "SRR1063272 -S view.vschema --view V1<SEQUENCE> -R 1-2"
run_test "3.1" "SRR1063272 -S view.vschema --view V2<SEQUENCE> -R 1-2"

# 3.2 NCBI:SRA:Nanopore:sequence
run_test "3.2" "ERR968961 -S view.nanopore.vschema --view V<SEQUENCE> -R 1-2"

# 3.3, 3.4 NCBI:SRA:PacBio:smrt:sequence (very old and the latest)
run_test "3.3" "SRR353493 -S view.pacbio.vschema --view V<SEQUENCE> -R 1-2"
run_test "3.4" "ERR976792 -S view.pacbio.vschema --view V<SEQUENCE> -R 1-2"

# 3.5 NCBI:align:db:alignment_evidence_sorted
run_test "3.5" "SRR2070515 -S view.vschema --view V1<SEQUENCE> -R 1-2"

# 3.6 NCBI:align:db:alignment_sorted
run_test "3.6" "ERR957001 -S view.vschema --view V1<SEQUENCE> -R 1-2"

# 3.7 NCBI:align:db:alignment_unsorted. this will issue some "Undeclared identifier" messages for views that rely on table types not in this accession's schema, but will do the job since V1's declaration is correct
run_test "3.7" "ERR570054 -S view.vschema --view V1<SEQUENCE> -R 1-2"

# 3.8 NULL as foreign key
run_test "3.8" "SRR1063272 --view V4<PRIMARY_ALIGNMENT,SEQUENCE> -S view.vschema -R 6"

# 3.9 join a table to itself
run_test "3.9" "SRR1063272 --view V6<PRIMARY_ALIGNMENT,SEQUENCE> -S view.vschema  -R 36"

# 3.10 view on view
run_test "3.10" "SRR1063272 --view V7<V6<PRIMARY_ALIGNMENT,SEQUENCE>> -S view.vschema  -R 36"

# 3.11 same table as different parameters of a view
run_test "3.11" "SRR1063272 --view V8<PRIMARY_ALIGNMENT,PRIMARY_ALIGNMENT,PRIMARY_ALIGNMENT> -S view.vschema  -R 1-5"

# 4.0 table object - views not supported
run_test_neg "4.0" "ERR997444 -S view.vschema --view V1<SEQUENCE>"
#TODO: support stand-alone tables

# 5.0 a bigger view on SEQUENCE
run_test "5.0" "SRR1063272 --view V9<SEQUENCE> -S view.vschema -R 1"

# 6.0 an aliased view from a database
run_test "6.0" "data/ViewDatabase -T VIEW1"
run_test "6.1" "data/ViewDatabase -T VIEW3"

# 7.0 symbolic names for various platforms
run_test "7.0" "input/platforms -C PLATFORM"

rm -rf actual
# keep the test database for the other tests that might follow (e.g. Test_Vdb_dump_view-alias - see CMakeLists.txt)
#rm -rf data

./test_buffer_insufficient.sh ${bin_dir}/${vdb_dump_binary} VDB-3937.kar
res=$?
if [ "$res" != "0" ];
	then echo "test_buffer_insufficient FAILED, res=$res output=$output" && exit 1;
fi

echo "...all tests passed"
echo vdb_dump.sh is finished
