
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

#TODO: fix
# add_test( NAME Test_Ngs_Pileup_1
#           COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR} 1.0 SRR833251
#           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

add_test( NAME Test_Ngs_Pileup_2
          COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR} 2.0 SRR1166393
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

# Illumina
add_test( NAME Test_Ngs_Pileup_3
          COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR} 3.0 ERR334733
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

add_test( NAME Test_Ngs_Pileup_4
          COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR} 4.0 SRR1068006
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

# ABSOLID REF_ROWS=36172
add_test( NAME SlowTest_Ngs_Pileup_5
          COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR} 5.0 SRR619510
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

#TODO: fix
# SEC_ROWS=198
# add_test( NAME Test_Ngs_Pileup_6
#           COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR} 6.0 SRR1251988
#           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

# ION_TORRENT
add_test( NAME Test_Ngs_Pileup_7
          COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR} 7.0 SRR1186012
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

#TODO: fix
# COMPLETE_GENOMICS
# add_test( NAME Test_Ngs_Pileup_8.1
#           COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR} 8.1 SRR556739 -r chrY
#           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

#TODO: fix
# COMPLETE_GENOMICS, circular reference
# add_test( NAME Test_Ngs_Pileup_8.2
#           COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR} 8.2 SRR556739 -r chrM
#           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

#:19900-20022
add_test( NAME Test_Ngs_Pileup_9
          COMMAND ./runtestcase.sh ${CMAKE_BINARY_DIR}/bin ${CMAKE_CURRENT_SOURCE_DIR} 9.0 SRR341578 -r NC_011752.1
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
