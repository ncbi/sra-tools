include_directories ( ${CMAKE_SOURCE_DIR}/ngs )

add_library ( ngs-vdb
                VdbReadCollection.cpp
                FragmentBlobIterator.cpp
                FragmentBlob.cpp
                VdbReference.cpp
                ReferenceBlob.cpp
                ReferenceBlobIterator.cpp
                VdbAlignment.cpp
                NGS-VDB.cpp )

add_subdirectory ( test )
