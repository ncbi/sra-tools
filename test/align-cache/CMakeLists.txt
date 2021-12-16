add_compile_definitions( __mod__="test/align-cache" )

if ( NOT WIN32 )
    add_test( NAME Test_Align_Cache
          COMMAND runtest.sh ${VDB_INTERFACES_DIR} ${BINDIR}
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
else()
#TODO: make run on Windows
endif()

# vg: $(DIRTOTEST)/align-cache
# 	valgrind --ncbi --suppressions=$(SRCDIR)/valgrind.suppress \
# 	 $(DIRTOTEST)/align-cache -t 10 --min-cache-count 1 CSRA_file CSRA_file.cache
