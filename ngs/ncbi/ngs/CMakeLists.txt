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

set( SRC
	NCBI-NGS
	CSRA1_PileupEvent
	CSRA1_Pileup
	CSRA1_Alignment
	CSRA1_ReferenceWindow
	CSRA1_Reference
	CSRA1_ReadCollection
	CSRA1_Read
	EBI_ReferenceSequence
	SRA_Statistics
	SRA_DB_ReadCollection
	SRA_Read
	SRA_ReadGroup
	SRA_ReadGroupInfo
	SRA_ReadCollection
	SRA_ReferenceSequence
	NGS_Statistics
	NGS_ReadCollection
	NGS_PileupEvent
	NGS_Pileup
	NGS_ReadGroup
	NGS_Alignment
	NGS_Reference
	NGS_ReferenceSequence
	NGS_Read
	NGS_Fragment
	NGS_Cursor
	NGS_Refcount
	NGS_String
	NGS_Id
	NGS_ErrBlock
	NGS_FragmentBlob
	NGS_FragmentBlobIterator
	NGS_ReferenceBlob
	NGS_ReferenceBlobIterator
	VByteBlob
)

# suppress certain compiler warnings
if  ( "Clang" STREQUAL "${CMAKE_C_COMPILER_ID}")
    set_source_files_properties( SRA_Statistics PROPERTIES COMPILE_FLAGS -Wno-implicit-int-float-conversion )
endif()

GenerateStaticLibsWithDefs( ngs "${SRC}" "__mod__=\"ngs/ncbi/ngs\"" "" )
target_link_libraries( ngs ${COMMON_LIBS_READ} )
if( RUN_SANITIZER_TESTS )
	target_link_libraries( ngs-asan ${COMMON_LIBS_READ} )
	target_link_libraries( ngs-tsan ${COMMON_LIBS_READ} )
endif()


