add_compile_definitions (__mod__="ngs/ncbi/ngs-c++")

set( NGS_SRC
	NCBI-NGS
)

add_library( libncbi-ngs-c++ ${NGS_SRC} )
target_include_directories( libncbi-ngs-c++ PUBLIC ${CMAKE_SOURCE_DIR}/../ncbi-vdb/libs/klib )
target_include_directories( libncbi-ngs-c++ PUBLIC ${CMAKE_SOURCE_DIR}/ngs/ncbi/ngs )

add_executable( testy-the-bear testy-the-bear )

set( NCBI_VDB_LIBDIR "~/ncbi-outdir/ncbi-vdb/linux/gcc/x86_64/dbg/lib" )
set( NCBI_VDB_ILIBDIR "~/ncbi-outdir/ncbi-vdb/linux/gcc/x86_64/dbg/ilib" )
set( NGS_LIBDIR "~/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/lib" )
set( NGS_ILIBDIR "~/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/ilib" )
target_link_directories( testy-the-bear PUBLIC ${NCBI_VDB_LIBDIR} )
target_link_directories( testy-the-bear PUBLIC ${NCBI_VDB_ILIBDIR} )
target_link_directories( testy-the-bear PUBLIC ${NGS_LIBDIR} )
target_link_directories( testy-the-bear PUBLIC ${NGS_ILIBDIR} )
target_link_libraries( testy-the-bear kapp ncbi-ngs-c++ ncbi-vdb ngs-c++ )
