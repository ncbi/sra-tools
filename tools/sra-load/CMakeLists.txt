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

	set( LOADER_LIB
		kapp
		tk-version
		load
		ncbi-wvdb.${STLX}
		pthread
		dl
		m
		kxfs
		kxml
		xml2
	)

	if( NOT WIN32 )
		list( APPEND LOADER_LIB m )
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
	MakeLinksExe( sff-load true )


	set( FASTQ_LOAD_SRC
		${LOADER_SRC}
		writer-illumina
		writer-454
		writer-ion-torrent
		fastq-fmt
	)

	add_executable( fastq-load ${FASTQ_LOAD_SRC} )
	target_link_libraries( fastq-load ${LOADER_LIB} )
	MakeLinksExe( fastq-load true )


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
	MakeLinksExe( srf-load true )


	set( ABSOLID_LOAD_SRC
		${LOADER_SRC}
		writer-absolid
		absolid-fmt
	)

	add_executable( abi-load ${ABSOLID_LOAD_SRC} )
	target_link_libraries( abi-load ${LOADER_LIB} )
	MakeLinksExe( abi-load true )


	set( ILLUMINA_LOAD_SRC
		${LOADER_SRC}
		writer-illumina
		illumina-fmt
	)

	add_executable( illumina-load ${ILLUMINA_LOAD_SRC} )
	target_link_libraries( illumina-load ${LOADER_LIB} )
	MakeLinksExe( illumina-load true )


	set( HELICOS_LOAD_SRC
		${LOADER_SRC}
		writer-helicos
		helicos-fmt
	)

	add_executable( helicos-load ${HELICOS_LOAD_SRC} )
	target_link_libraries( helicos-load ${LOADER_LIB} )
	MakeLinksExe( helicos-load true )


	set( PACBIO_LOADXML_SRC
		${LOADER_SRC}
		pacbio-fmt
	)

	add_executable( pacbio-loadxml ${PACBIO_LOADXML_SRC} )
	target_link_libraries( pacbio-loadxml ${LOADER_LIB} )
	MakeLinksExe( pacbio-loadxml true )

endif()
