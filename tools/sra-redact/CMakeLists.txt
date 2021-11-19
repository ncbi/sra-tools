add_compile_definitions( __mod__="sra-redact" )

if( NOT WIN32 )
set(SOURCES sra-redact.cpp sra-redact.hpp)
add_executable( sra-redact ${SOURCES} )
target_include_directories(sra-redact PRIVATE "." )
target_link_libraries( sra-redact ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_WRITE} )
MakeLinksExe( sra-redact false )
endif()
