#!/bin/sh
# ===========================================================================
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
#
# ===========================================================================
#echo "$0 $*"

bin_dir=$1
vdb_validate=$2
db_acc=$3

local_acc=actual/db_schema_check_${db_acc}
output=$(NCBI_VDB_PREFETCH_USES_OUTPUT_TO_FILE= \
         ${bin_dir}/prefetch -o ${local_acc} ${db_acc} 2>&1)
res=$?
if [ "$res" != "0" ];
	then echo "prefetch db_schema_check_${db_acc} FAILED, res=$res output=$output" && exit 1;
else
	output=$(${bin_dir}/${vdb_validate} ${local_acc} 2>&1)
	res=$?
	#rm ${local_acc}
	if [ "$res" != "0" ];
		then echo "${vdb_validate} db_schema_check_${db_acc} FAILED because of RC: $res, output=$output" && exit 1;
	fi

	output_grep=$(echo "${output}" | grep "verify_database: type unrecognized while validating database" 2>&1)
	res_grep=$?
	if [ "$res_grep" = "0" ];
		then echo "${vdb_validate} db_schema_check_${db_acc} FAILED because of the line: ${output_grep}" && exit 1;
	fi
fi
