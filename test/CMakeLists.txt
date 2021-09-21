if(NOT CMAKE_TEST_OUTPUT_DIRECTORY)
    set(CMAKE_TEST_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/test-bin)
endif()

# this is where test executables will be created
# to access tools themselves, use ${CMAKE_BINARY_DIR}/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_TEST_OUTPUT_DIRECTORY} )

set( COMMON_LINK_LIBRARIES ktst kapp )

# for the tests that link against libncbi-[w]vdb
if( WIN32 )
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup")
    set( COMMON_LIBS_READ  ${COMMON_LINK_LIBRARIES} ncbi-vdb Ws2_32 Crypt32 )
    set( COMMON_LIBS_WRITE ${COMMON_LINK_LIBRARIES} ncbi-wvdb Ws2_32 Crypt32 )
else()
    link_libraries( ${COMMON_LINK_LIBRARIES} )
    set( COMMON_LIBS_READ   ncbi-vdb  dl pthread )
    set( COMMON_LIBS_WRITE  ncbi-wvdb dl pthread )
endif()

add_subdirectory(align)
add_subdirectory(align-cache)
add_subdirectory(align-info)
add_subdirectory(copycat) # fails
add_subdirectory(driver-tool)
add_subdirectory(fasterq-dump)
add_subdirectory(fastq-loader)
add_subdirectory(kar)
add_subdirectory(kdbmeta) # project not yet converted
add_subdirectory(kget)
add_subdirectory(make-read-filter) # project not yet converted
# add_subdirectory(ngs-java) # project not yet converted
# add_subdirectory(ngs-pileup) # project not yet converted
# add_subdirectory(ngs-python) # project not yet converted
# add_subdirectory(prefetch)
# add_subdirectory(qual-recalib-stat)
# add_subdirectory(read-filter-redact)
# # add_subdirectory(samline) # command line tool; needs arguments to become a test?
# add_subdirectory(scripts)
# add_subdirectory(sra-pileup)
# add_subdirectory(sra-sort)
add_subdirectory(sra-stat)
add_subdirectory(srapath) # TODO: some variables have no effect on the test
# # add_subdirectory(tarballs) # called directly from TeamCity, do not invoke from CMake?
# # add_subdirectory(tui-example) # command line demos
add_subdirectory(vcf-loader) # TODO: for some reason it works even without setting config and env
add_subdirectory(vdb-config)
add_subdirectory(vdb-copy)
add_subdirectory(vdb-diff) # was not included in runtests
# add_subdirectory(vdb-dump) # TODO - ??
add_subdirectory(vdb-validate)

# add_subdirectory(fuse) # retired
# add_subdirectory(network-test) # broken now
# add_subdirectory(dump-test) # tools/fastq-dump is abandoned
# add_subdirectory(pileup-stats) # moved to ngs-tools, delete
