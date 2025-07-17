#pragma once

#include "VdbObj.hpp"

namespace vdb {

extern "C" {

rc_t VSchemaAddRef( const VSchema *self );
rc_t VSchemaRelease( const VSchema *self );

rc_t VDBManagerMakeSchema(  VDBManager const *self, VSchema **schema );
rc_t VSchemaAddIncludePath( VSchema *self, const char *path, ... );
rc_t VSchemaVAddIncludePath( VSchema *self, const char *path, va_list args );
rc_t VSchemaAddIncludePaths( VSchema *self, std::size_t length, const char *paths );

rc_t VSchemaParseText( VSchema *self, const char *name, const char *text, std::size_t bytes );
rc_t VSchemaParseFile( VSchema *self, const char *name, ... );
rc_t VSchemaVParseFile( VSchema *self, const char *name, va_list args );

enum VSchemaDumpMode { sdmPrint, sdmCompact };
rc_t VSchemaDump( const VSchema *self, uint32_t mode, const char *decl,
        rc_t ( * flush ) ( void *dst, const void *buffer, std::size_t bsize ), void *dst );

rc_t VSchemaIncludeFiles( const VSchema *self, KNamelist const **list );

typedef struct VTypedecl VTypedecl;
struct VTypedecl {
    uint32_t type_id;
    uint32_t dim;
};
rc_t VSchemaResolveTypedecl( const VSchema *self, VTypedecl *resolved, const char *typedecl, ... );
rc_t VSchemaVResolveTypedecl( const VSchema *self, VTypedecl *resolved, const char *typedecl, va_list args );
rc_t VTypedeclToText( const VTypedecl *self, const VSchema *schema, char *buffer, std::size_t bsize );
bool VTypedeclToSupertype( const VTypedecl *self, const VSchema *schema, VTypedecl *cast );
bool VTypedeclToType( const VTypedecl *self, const VSchema *schema,  uint32_t ancestor,
        VTypedecl *cast, uint32_t *distance );
bool VTypedeclToTypedecl( const VTypedecl *self, const VSchema *schema, const VTypedecl *ancestor,
        VTypedecl *cast, uint32_t *distance );
bool VTypedeclCommonAncestor( const VTypedecl *self, const VSchema *schema,
        const VTypedecl *peer, VTypedecl *ancestor, uint32_t *distance );

enum { vtdBool = 1, vtdUint, vtdInt, vtdFloat, vtdAscii, vtdUnicode };
typedef struct VTypedesc VTypedesc;
struct VTypedesc {
	uint32_t intrinsic_bits;
	uint32_t intrinsic_dim;
	uint32_t domain;
};
uint32_t VTypedescSizeof( const VTypedesc *self );
rc_t VSchemaDescribeTypedecl( const VSchema *self, VTypedesc *desc, const VTypedecl *td );

}; // end of extern "C"

struct SchemaPrinter {
        std::string s;

        static rc_t callback( void * obj, const void *buffer, std::size_t bsize ) {
            SchemaPrinter * printer = static_cast< SchemaPrinter *>( obj );
            std::string tmp( ( const char * ) buffer, bsize );
            printer -> s += tmp;
            return 0;
        }
};

class VSch;
typedef std::shared_ptr<VSch> VSchPtr;
class VSch : public VDBObj {
    private:
        VSchema * f_schema;

        VSch( const VDBManager * mgr ) { set_rc( VDBManagerMakeSchema( mgr, &f_schema ) ); }

    public:
        ~VSch() { VSchemaRelease( f_schema ); }

        static VSchPtr make( const VDBManager * mgr ) { return VSchPtr( new VSch( mgr ) ); }

        VSchema * get_schema( void ) const { return f_schema; }

        bool AddInclucePath( const std::string& path ) {
            return set_rc( VSchemaAddIncludePath( f_schema, "%s", path.c_str() ) );
        }

        bool ParseText( const std::string& schema_txt ) {
            return set_rc( VSchemaParseText( f_schema, nullptr,
                                      schema_txt . c_str(), schema_txt .length() ) );
        }

        bool ParseFile( const std::string& filename ) {
            return set_rc( VSchemaParseFile( f_schema, "%s", filename . c_str() ) );
        }

        rc_t print_callback( const void *buffer, std::size_t bsize ) {
            return 0;
        }

        std::string print( void ) {
            SchemaPrinter printer;
            rc_t rc = VSchemaDump( f_schema, sdmPrint, nullptr, SchemaPrinter::callback, &printer );
            if ( 0 == rc ) {
                return printer . s;
            }
            return std::string( "err" );
        }
};

}; // end of namesapce vdb
