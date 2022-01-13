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

add_compile_definitions( __mod__="tools/sra-sort" )

# Internal
if( NOT WIN32 )
	add_executable( dump-blob-boundaries dump-blob-boundaries )
	target_link_libraries( dump-blob-boundaries ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} )
	MakeLinksExe( dump-blob-boundaries false )
endif()

# External

set( SRA_SORT_SRC
	caps
	mem
	membank
	paged-membank
	paged-mmapbank
	except
	idx-mapping
	map-file
	col-pair
	row-set
	simple-row-set
	mapping-row-set
	sorting-row-set
	meta-pair
	dir-pair
	tbl-pair
	db-pair
	glob-poslen
	poslen-col-pair
	ref-alignid-col
	buff-writer
	id-mapper-col
	capture-first-half-aligned
	run
	sra-sort
	xcheck-ref-align

	csra-tbl
	csra-pair
)

add_executable( sra-sort ${SRA_SORT_SRC} )
target_link_libraries( sra-sort ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( sra-sort false )

add_executable( sra-sort-cg ${SRA_SORT_SRC} )
target_link_libraries( sra-sort-cg ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( sra-sort-cg false )
target_compile_definitions( sra-sort-cg PUBLIC SEQUENCE_BEFORE_SECONDARY=0 )
