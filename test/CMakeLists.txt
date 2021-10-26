# this is where test executables will be created
# to access tools themselves, use ${BINDIR}
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TESTBINDIR} )

set( COMMON_LINK_LIBRARIES ${COMMON_LINK_LIBRARIES} ktst )
#link_libraries( ${COMMON_LINK_LIBRARIES} )

add_subdirectory(align)
add_subdirectory(align-cache)
add_subdirectory(align-info)
add_subdirectory(copycat) # fails
add_subdirectory(driver-tool)
add_subdirectory(fasterq-dump)
add_subdirectory(fastq-loader)
add_subdirectory(kar)
add_subdirectory(kdbmeta)
add_subdirectory(kget)
add_subdirectory(make-read-filter)
add_subdirectory(ngs-java)
add_subdirectory(ngs-pileup)
add_subdirectory(ngs-python)
add_subdirectory(prefetch) # TODO ascp ?
add_subdirectory(qual-recalib-stat)
add_subdirectory(read-filter-redact)
# # add_subdirectory(samline) # command line tool; needs arguments to become a test?
add_subdirectory(scripts)
add_subdirectory(sra-pileup)
add_subdirectory(sra-sort) # TODO: it's not clear if the test itself was running, now it's not.
add_subdirectory(sra-stat)
add_subdirectory(srapath) # TODO: some variables have no effect on the test
# # add_subdirectory(tarballs) # called directly from TeamCity, do not invoke from CMake?
# # add_subdirectory(tui-example) # command line demos
add_subdirectory(vcf-loader) # TODO: for some reason it works even without setting config and env
add_subdirectory(vdb-config)
add_subdirectory(vdb-copy)
add_subdirectory(vdb-diff) # was not included in runtests
add_subdirectory(vdb-dump)
add_subdirectory(vdb-validate)

# add_subdirectory(fuse) # retired
# add_subdirectory(network-test) # broken now
# add_subdirectory(dump-test) # tools/fastq-dump is abandoned
# add_subdirectory(pileup-stats) # moved to ngs-tools, delete
