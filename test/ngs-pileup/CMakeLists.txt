
add_executable( test-ngs-pileup test-ngs-pileup.cpp )

include_directories( ${CMAKE_SOURCE_DIR} )

target_link_libraries( test-ngs-pileup
    ncbi-ngs-c++
    ngs-c++
    ngs
    ncbi-vdb-static
    kapp
    ktst
    dl
    pthread
)

add_test( NAME Test_Ngs_Pileup
          COMMAND test-ngs-pileup
)

add_test( NAME Test_Ngs_Pileup_1
          COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_SOURCE_DIR} 1.0 SRR833251
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

# -@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 1.0 SRR833251
# 	@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 2.0 SRR1166393
# 	@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 3.0 ERR334733                # ILLUMINA
# 	@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 4.0 SRR1068006
# 	@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 5.0 SRR619510                # ABSOLID REF_ROWS=36172
# 	-@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 6.0 SRR1251988               # SEC_ROWS=198
# 	@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 7.0 SRR1186012               # ION_TORRENT
# 	-@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 8.1 SRR556739 -r chrY # COMPLETE_GENOMICS
# 	-@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 8.2 SRR556739 -r chrM # COMPLETE_GENOMICS, circular reference
# 	@ ./runtestcase.sh $(BINDIR) $(SRCDIR) 9.0 SRR341578 -r NC_011752.1         #:19900-20022
