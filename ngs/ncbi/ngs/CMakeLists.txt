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

GenerateStaticLibs( ngs "${SRC}" )
target_link_libraries( ngs ${COMMON_LIBS_READ} )

add_compile_definitions (__mod__="ngs/ncbi/ngs")