add_compile_definitions( __mod__="tools/sra-pileup" )

# External
add_executable( sra-pileup
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
target_link_libraries( sra-pileup ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( sra-pileup true )

add_executable( sam-dump
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
)
target_link_libraries( sam-dump ${COMMON_LINK_LIBRARIES} ncbi-vdb.${STLX} pthread dl m )
MakeLinksExe( sam-dump true )
