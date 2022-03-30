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

add_compile_definitions( __mod__="tools/sra-load" )

if( LibXml2_FOUND )

	set( LOADER_SRC
		pstring
		common-xml
		experiment-xml
		run-xml
		loader-file
		loader-fmt
		sra-writer
		loader
	)

   if (SINGLE_CONFIG )
	set( LOADER_LIB
        ${COMMON_LINK_LIBRARIES}
		${COMMON_LIBS_WRITE}
		kxml
		${LIBXML2_LIBRARIES}
	)

   else()
	set( LOADER_LIB
        ${COMMON_LINK_LIBRARIES}
		${COMMON_LIBS_WRITE}
		kxml
		xml2
	)
   endif()

   if ( "mac" STREQUAL ${OS} )
      list( APPEND LOADER_LIB iconv )
   endif()

	# External

	set( SFF_LOAD_SRC
		${LOADER_SRC}
		writer-454
		writer-ion-torrent
		sff-fmt
	)

	add_executable( sff-load ${SFF_LOAD_SRC} )
	target_link_libraries( sff-load ${LOADER_LIB} )
	MakeLinksExe( sff-load false )


	set( FASTQ_LOAD_SRC
		${LOADER_SRC}
		writer-illumina
		writer-454
		writer-ion-torrent
		fastq-fmt
	)

	add_executable( fastq-load ${FASTQ_LOAD_SRC} )
	target_link_libraries( fastq-load ${LOADER_LIB} )
	MakeLinksExe( fastq-load false )


	set( SRF_LOAD_SRC
		${LOADER_SRC}
		srf
		srf-fmt
		writer-illumina
		ztr-illumina
		srf-illumina
		writer-absolid
		ztr-absolid
		ztr-huffman
		srf-absolid
	)

	add_executable( srf-load ${SRF_LOAD_SRC} )
	target_link_libraries( srf-load ${LOADER_LIB} )
	MakeLinksExe( srf-load false )


	set( ABSOLID_LOAD_SRC
		${LOADER_SRC}
		writer-absolid
		absolid-fmt
	)

	add_executable( abi-load ${ABSOLID_LOAD_SRC} )
	target_link_libraries( abi-load ${LOADER_LIB} )
	MakeLinksExe( abi-load false )


	set( ILLUMINA_LOAD_SRC
		${LOADER_SRC}
		writer-illumina
		illumina-fmt
	)

	add_executable( illumina-load ${ILLUMINA_LOAD_SRC} )
	target_link_libraries( illumina-load ${LOADER_LIB} )
	MakeLinksExe( illumina-load false )


	set( HELICOS_LOAD_SRC
		${LOADER_SRC}
		writer-helicos
		helicos-fmt
	)

	add_executable( helicos-load ${HELICOS_LOAD_SRC} )
	target_link_libraries( helicos-load ${LOADER_LIB} )
	MakeLinksExe( helicos-load false )


	set( PACBIO_LOADXML_SRC
		${LOADER_SRC}
		pacbio-fmt
	)

	add_executable( pacbio-loadxml ${PACBIO_LOADXML_SRC} )
	target_link_libraries( pacbio-loadxml ${LOADER_LIB} )
	MakeLinksExe( pacbio-loadxml false )

endif()
