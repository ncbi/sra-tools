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

add_compile_definitions( __mod__="tools/sra-pileup" )

set(LIBS "kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_READ}" )

# External
set( SRA_PILEUP_SRC
	dyn_string
	cmdline_cmn
	out_redir
	perf_log
	reref
	cg_tools
	report_deletes
	ref_regions
	4na_ascii
	ref_walker_0
	ref_walker
	walk_debug
	pileup_counters
	pileup_index
	pileup_indels
	pileup_varcount
	pileup_stat
	pileup_v2
	sra-pileup
)
GenerateExecutableWithDefs( sra-pileup "${SRA_PILEUP_SRC}" "" "" "${LIBS}" )
MakeLinksExe( sra-pileup true )

set( SAM_DUMP_SRC
	inputfiles
	perf_log
	rna_splice_log
	sam-dump-opts
	out_redir
	sam-hdr
	sam-hdr1
	matecache
	read_fkt
	sam-aligned
	sam-unaligned
	md_flag
	cg_tools
	sam-dump
	sam-dump3
	dyn_string
)
GenerateExecutableWithDefs( sam-dump "${SAM_DUMP_SRC}" "" "" "${LIBS}" )
MakeLinksExe( sam-dump true )
