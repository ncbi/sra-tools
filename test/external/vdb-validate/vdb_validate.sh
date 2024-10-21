#!/bin/sh
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

bin_dir=$1
vdb_validate=$2

echo "testing ${vdb_validate} ${bin_dir}"

TEMPDIR=.

rm -rf actual/
mkdir -p actual/

export NCBI_SETTINGS=/

output=$(./runtestcase.sh \
	    "${bin_dir}/${vdb_validate} db/sdc_len_mismatch.csra" no_sdc_checks 0)
res=$?
if [ "$res" != "0" ];
	then echo "${vdb_validate} no_sdc_checks FAILED, res=$res output=$output" \
      && exit 1;
fi

output=$(./runtestcase.sh \
	    "${bin_dir}/${vdb_validate} db/sdc_tmp_mismatch.csra --sdc:rows 100%" \
	                                sdc_tmp_mismatch 3)
res=$?
if [ "$res" != "0" ];
   then echo "${vdb_validate} sdc_tmp_mismatch FAILED, res=$res output=$output"\
     && exit 1;
fi

output=$(./runtestcase.sh \
	    "${bin_dir}/${vdb_validate} db/sdc_pa_longer.csra --sdc:rows 100%" \
	                                sdc_pa_longer_1 3)
res=$?
if [ "$res" != "0" ];
	then echo "${vdb_validate} sdc_pa_longer_1 FAILED, res=$res output=$output"\
      && exit 1;
fi

output=$(./runtestcase.sh \
        "${bin_dir}/${vdb_validate} db/sdc_pa_longer.csra --sdc:rows 100% \
              --sdc:plen_thold 50%" sdc_pa_longer_2 3)
res=$?
if [ "$res" != "0" ];
	then echo "${vdb_validate} sdc_pa_longer_2 FAILED, res=$res output=$output"\
      && exit 1;
fi

output=$(./runtestcase.sh \
        "${bin_dir}/${vdb_validate} db/sdc_pa_longer.csra --sdc:rows 100% \
              --sdc:plen_thold 51%" sdc_pa_longer_3 0)
res=$?
if [ "$res" != "0" ];
	then echo "${vdb_validate} sdc_pa_longer_3 FAILED, res=$res output=$output"\
      && exit 1;
fi

output=$(./runtestcase.sh \
        "${bin_dir}/${vdb_validate} db/sdc_len_mismatch.csra --sdc:rows 100% \
            --sdc:plen_thold 1%" sdc_len_mismatch_1 3)
res=$?
if [ "$res" != "0" ];
 then echo "${vdb_validate} sdc_len_mismatch_1 FAILED, res=$res output=$output"\
   && exit 1;
fi

output=$(./runtestcase.sh \
        "${bin_dir}/${vdb_validate} db/sdc_len_mismatch.csra --sdc:rows 100% \
          --sdc:plen_thold 100%" sdc_len_mismatch_2 3)
res=$?
if [ "$res" != "0" ];
 then echo "${vdb_validate} sdc_len_mismatch_2 FAILED, res=$res output=$output"\
   && exit 1;
fi

output=$(./runtestcase.sh \
        "${bin_dir}/${vdb_validate} db/sdc_seq_cmp_read_len_corrupt.csra \
          --sdc:seq-rows 100%" sdc_seq_cmp_read_len_corrupt 3)
res=$?
if [ "$res" != "0" ];
	then echo \
 "${vdb_validate} sdc_seq_cmp_read_len_corrupt FAILED, res=$res output=$output"\
        && exit 1;
fi

output=$(./runtestcase.sh \
        "${bin_dir}/${vdb_validate} db/sdc_seq_cmp_read_len_fixed.csra \
          --sdc:seq-rows 100%" sdc_seq_cmp_read_len_fixed 0)
res=$?
if [ "$res" != "0" ];
	then echo \
   "${vdb_validate} sdc_seq_cmp_read_len_fixed FAILED, res=$res output=$output"\
        && exit 1;
fi

output=$(./runtestcase.sh \
        "${bin_dir}/${vdb_validate} db/blob-row-gap.kar" ROW_GAP 0)
res=$?
if [ "$res" != "0" ];
	then echo "${vdb_validate} ROW_GAP FAILED, res=$res output=$output" \
      && exit 1;
fi

output=$(./runtestcase.sh \
        "${bin_dir}/${vdb_validate} db/SRR053990 -Cyes" CONSISTENCY 0)
res=$?
if [ "$res" != "0" ];
	then echo "${vdb_validate} CONSISTENCY FAILED, res=$res output=$output" \
    && exit 1;
fi

output=$(./runtestcase.sh \
        "${bin_dir}/${vdb_validate} db/SRR053990 -Cyes -Bno" CONSISTENCY 0)
res=$?
if [ "$res" != "0" ];
	then echo \
     "${vdb_validate} CONSISTENCY without crc FAILED, res=$res output=$output" \
    && exit 1;
fi

output=$(./runtestcase.sh \
        "${bin_dir}/${vdb_validate} db/SRR053990 -Cyes -Byes" \
        CONSISTENCY_and_CRC 0)
res=$?
if [ "$res" != "0" ];
	then echo \
         "${vdb_validate} CONSISTENCY_and_CRC FAILED, res=$res output=$output" \
    && exit 1;
fi

if [ "${TEST_DATA}" != "" ]; then
	echo ${TEST_DATA}/SRR1207586-READ_LEN-vs-READ-mismatch
	file ${TEST_DATA}/SRR1207586-READ_LEN-vs-READ-mismatch
    output=$(./runtestcase.sh \
	    "${bin_dir}/${vdb_validate} \
	            ${TEST_DATA}/SRR1207586-READ_LEN-vs-READ-mismatch \
	            -Cyes" READ_LEN 3)
    res=$?
    if [ "$res" != "0" ];
	    then echo "${vdb_validate} READ_LEN FAILED, res=$res output=$output" \
          && exit 1;
    fi

    output=$(./runtestcase.sh \
	    "${bin_dir}/${vdb_validate} \
	            ${TEST_DATA}/SRR26020762-corrupt.sralite" no_crc 3)
    res=$?
    if [ "$res" != "0" ];
	    then echo "${vdb_validate} no_crc FAILED, res=$res output=$output" \
          && exit 1;
    fi
fi

# verify failure verifying ancient no-schema run
if ${bin_dir}/${vdb_validate} db/SRR053325-no-schema 2> actual/noschema; \
	then echo ${vdb_validate} no-schema-run should fail; exit 1; fi
grep -q ' Run File is obsolete. Please download the latest' actual/noschema

db_acc=SRR1872513
output=$(./db_schema_check.sh ${bin_dir} ${vdb_validate} ${db_acc})
res=$?
if [ "$res" != "0" ];
	then echo "$output" && exit 1;
fi

db_acc=SRR11039354
output=$(./db_schema_check.sh ${bin_dir} ${vdb_validate} ${db_acc})
res=$?
if [ "$res" != "0" ];
	then echo "$output" && exit 1;
fi

db_acc=SRR5238359
output=$(./db_schema_check.sh ${bin_dir} ${vdb_validate} ${db_acc})
res=$?
if [ "$res" != "0" ];
	then echo "$output" && exit 1;
fi

db_acc=SRR8803095
output=$(./db_schema_check.sh ${bin_dir} ${vdb_validate} ${db_acc})
res=$?
if [ "$res" != "0" ];
	then echo "$output" && exit 1;
fi

db_acc=SRR15057789
output=$(./db_schema_check.sh ${bin_dir} ${vdb_validate} ${db_acc})
res=$?
if [ "$res" != "0" ];
	then echo "$output" && exit 1;
fi

db_acc=SRR11664922
output=$(./db_schema_check.sh ${bin_dir} ${vdb_validate} ${db_acc})
res=$?
if [ "$res" != "0" ];
	then echo "$output" && exit 1;
fi

echo "All ${vdb_validate} tests succeed"
rm -rf actual/
