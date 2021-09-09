add_compile_definitions( __mod__="test/align-cache" )

if ( NOT WIN32 )
    add_test( NAME Test_Align_Cache
          COMMAND runtest.sh ${VDB_SRCDIR}/interfaces ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}}
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
else()
#TODO: make run on Windows
endif()

# align-cache: $(DIRTOTEST)/align-cache
# 	@ rm -rf CSRA_file.cache
# 	@ echo "vdb/schema/paths = \"$(VDB_INCDIR)\"" > tmp.kfg
# 	@ VDB_CONFIG=`pwd` $(DIRTOTEST)/align-cache \
# 	                          -t 10 --min-cache-count 1 CSRA_file CSRA_file.cache
# 	@ $(DIRTOTEST)/vdb-validate CSRA_file.cache/ 2>&1 \
# 	                          | grep --quiet "is consistent"
# 	@ rm tmp.kfg
# 	@ rm -rf CSRA_file.cache

# vg: $(DIRTOTEST)/align-cache
# 	valgrind --ncbi --suppressions=$(SRCDIR)/valgrind.suppress \
# 	 $(DIRTOTEST)/align-cache -t 10 --min-cache-count 1 CSRA_file CSRA_file.cache
