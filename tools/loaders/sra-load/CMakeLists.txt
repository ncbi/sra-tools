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

	if ( "GNU" STREQUAL "${CMAKE_C_COMPILER_ID}")
		set_source_files_properties( sra-writer PROPERTIES COMPILE_FLAGS -Wno-stringop-truncation )
	endif()

	if (SINGLE_CONFIG )
		set( LOADER_LIB
			kapp
			loader
			${COMMON_LINK_LIBRARIES}
			${COMMON_LIBS_WRITE}
			kxml
			${LIBXML2_LIBRARIES}
            ksrch
		)

	else()
		set( LOADER_LIB
			kapp
			loader
			${COMMON_LINK_LIBRARIES}
			${COMMON_LIBS_WRITE}
			kxml
			xml2
            ksrch
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

	GenerateExecutableWithDefs( sff-load "${SFF_LOAD_SRC}" "" "" "${LOADER_LIB}" )
	MakeLinksExe( sff-load false )


	set( FASTQ_LOAD_SRC
		${LOADER_SRC}
		writer-illumina
		writer-454
		writer-ion-torrent
		fastq-fmt
	)

	GenerateExecutableWithDefs( fastq-load "${FASTQ_LOAD_SRC}" "" "" "${LOADER_LIB}" )
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

	GenerateExecutableWithDefs( srf-load "${SRF_LOAD_SRC}" "" "" "${LOADER_LIB}" )
	MakeLinksExe( srf-load false )


	set( ABSOLID_LOAD_SRC
		${LOADER_SRC}
		writer-absolid
		absolid-fmt
	)

	GenerateExecutableWithDefs( abi-load "${ABSOLID_LOAD_SRC}" "" "" "${LOADER_LIB}" )
	MakeLinksExe( abi-load false )


	set( ILLUMINA_LOAD_SRC
		${LOADER_SRC}
		writer-illumina
		illumina-fmt
	)

	GenerateExecutableWithDefs( illumina-load "${ILLUMINA_LOAD_SRC}" "" "" "${LOADER_LIB}" )
	MakeLinksExe( illumina-load false )


	set( HELICOS_LOAD_SRC
		${LOADER_SRC}
		writer-helicos
		helicos-fmt
	)

	GenerateExecutableWithDefs( helicos-load "${HELICOS_LOAD_SRC}" "" "" "${LOADER_LIB}" )
	MakeLinksExe( helicos-load false )


	set( PACBIO_LOADXML_SRC
		${LOADER_SRC}
		pacbio-fmt
	)

	GenerateExecutableWithDefs( pacbio-loadxml "${PACBIO_LOADXML_SRC}" "" "" "${LOADER_LIB}" )
	MakeLinksExe( pacbio-loadxml false )

endif()
