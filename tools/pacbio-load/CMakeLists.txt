find_package( HDF5 COMPONENTS )

# External
if( HDF5_FOUND )
	add_compile_definitions( __mod__="tools/pacbio-load" )
	add_executable( pacbio-load
		pl-context
		pl-tools
		pl-zmw
		pl-basecalls_cmn
		pl-sequence
		pl-consensus
		pl-passes
		pl-metrics
		pl-regions
		pl-progress
		pacbio-load
	)

	set( PACBIO_LOAD_LIB
		kapp
		tk-version
		kdf5
		load
		${HDF5_LIBRARIES}
		ncbi-wvdb.${STLX}
		pthread
		dl
		m
	)

	target_link_libraries( pacbio-load ${PACBIO_LOAD_LIB} )
	MakeLinksExe( pacbio-load true )
endif()
