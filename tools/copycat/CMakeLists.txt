add_compile_definitions( __mod__="tools/copycat" )

# External
if( NOT WIN32 )
    add_executable( copycat
        copycat
        ccfileformat
        cccat
        cctree
        cctree-dump
        cctar
        ccsra
        ccsubchunk
        ccfile
    )
    target_link_libraries( copycat ${COMMON_LINK_LIBRARIES} ncbi-vdb kff magic-static )
endif()
