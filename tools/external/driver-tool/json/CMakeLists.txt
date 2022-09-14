#add_compile_definitions( __mod__="tools/driver-tool/json" )

include_directories( ../inc )
#include_directories( ../secure )
#include_directories( ../ut )

set( JSON_SRC
	json
	json-array
	json-object
	json-primitive
	json-value
	json-wrapper
	$<TARGET_OBJECTS:utf8proc>
)

GenerateStaticLibs( json "${JSON_SRC}" )

