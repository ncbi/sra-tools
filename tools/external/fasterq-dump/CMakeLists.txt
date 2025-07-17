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

set( TOOLS_SRC
	helper
	arg_helper
	dflt_defline
	tool_ctx
	inspector
	sbuffer
	err_msg
	file_tools
	var_fmt
	flex_printer
	temp_dir
	progress_thread
	cleanup_task
	index
	lookup_writer
	lookup_reader
	locked_file_list
	locked_value
	file_printer
	merge_sorter
	sorter
	cmn_iter
	raw_read_iter
	special_iter
	fq_seq_ua_iter
	fq_seq_csra_iter
	align_iter
	simple_fasta_iter
	db_join
	tbl_join
	temp_registry
	copy_machine
	multi_writer
	concatenator
	ref_inventory
	fasterq-dump
)

GenerateExecutableWithDefs( fasterq-dump "${TOOLS_SRC}" "__mod__=\"tools/fasterq-dump\"" "" "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ};ksrch" )
MakeLinksExe( fasterq-dump true )
