set( INC_DIRS ${CMAKE_SOURCE_DIR}/ngs )

set( SRC
	VdbReadCollection.cpp
	FragmentBlobIterator.cpp
	FragmentBlob.cpp
	VdbReference.cpp
	ReferenceBlob.cpp
	ReferenceBlobIterator.cpp
	VdbAlignment.cpp
	NGS-VDB.cpp
)

GenerateStaticLibsWithDefs( ngs-vdb "${SRC}" "" "${INC_DIRS}" )

add_subdirectory ( test )
