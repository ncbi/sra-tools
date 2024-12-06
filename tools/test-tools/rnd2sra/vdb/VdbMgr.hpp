#pragma once

#include "VSchema.hpp"
#include "VdbObj.hpp"
#include "VdbTable.hpp"
#include "VdbDatabase.hpp"

namespace vdb {

extern "C" {

/* ========================================================================================= */

enum {
    kptAny = 0,
    kptDatabase = kptLastDefined,
    kptTable,
    kptIndex,
    kptColumn,
    kptMetadata,
    kptPrereleaseTbl
};

rc_t VDBManagerAddRef( const VDBManager *self );
rc_t VDBManagerRelease( const VDBManager *self );
rc_t VDBManagerMakeRead( const VDBManager **mgr, KDirectory const *wd );
rc_t VDBManagerMakeUpdate( VDBManager **mgr, KDirectory *wd );
rc_t VDBManagerVersion( const VDBManager *self, uint32_t *version );

rc_t VDBManagerAddSchemaIncludePath( const VDBManager *self, const char *path, ... );
rc_t VDBManagerVAddSchemaIncludePath( const VDBManager *self, const char *path, va_list args );

rc_t VDBManagerGetObjVersion( const VDBManager *self, ver_t * version, const char *path );
rc_t VDBManagerGetObjModDate( const VDBManager *self, KTime_t * ts, const char *path );

int VDBManagerPathType( const VDBManager * self, const char *path, ... );
int VDBManagerVPathType( const VDBManager * self, const char *path, va_list args );

rc_t VDBManagerGetCacheRoot( const VDBManager * self, VPath const ** path );
rc_t VDBManagerSetCacheRoot( const VDBManager * self, VPath const * path );

rc_t VDBManagerDeleteCacheOlderThan( const VDBManager * self, uint32_t days );
rc_t VDBManagerPreferFullQuality( VDBManager * self );
rc_t VDBManagerPreferZeroQuality( VDBManager * self );

rc_t VDBManagerOpenView( VDBManager const * self, const VView **view,
        const VSchema * schema, const char * name );
rc_t VDatabaseOpenView( VDatabase const *self, const VView **view, const char *name );

}; // end of extern "C"

class VMgr;
typedef std::shared_ptr< VMgr > VMgrPtr;
class VMgr : public VDBObj {
    private :
#ifdef VDB_WRITE
        VDBManager * f_mgr;
#else
        const VDBManager * f_mgr;
#endif

#ifdef VDB_WRITE
        VMgr( KDirectory * dir ) {
            set_rc( VDBManagerMakeUpdate( &f_mgr, dir ) );
        }
#else
        VMgr( const KDirectory * dir ) {
            set_rc( VDBManagerMakeRead( &f_mgr, dir ) );
        }
#endif

    public :
        ~VMgr() { VDBManagerRelease( f_mgr ); }

#ifdef VDB_WRITE
        static VMgrPtr make( KDirectory * dir ) { return VMgrPtr( new VMgr( dir ) ); }
#else
        static VMgrPtr make( const KDirectory * dir ) { return VMgrPtr( new VMgr( dir ) ); }
#endif

        // open table or database for read
        VTblPtr open_tbl( const std::string& acc ) const { return VTbl::open( f_mgr, acc ); }
        VDbPtr open_db( const std::string& acc ) const { return VDb::make( f_mgr, acc ); }

        VSchPtr make_schema( void ) const { return VSch::make( f_mgr ); }

#ifdef VDB_WRITE
        VTblPtr create_tbl( const VSchPtr schema, const std::string& schema_type,
                            const std::string& dir_name, const Checksum_ptr checksum ) const {
            return VTbl::create( f_mgr, schema, schema_type, dir_name, checksum );
        }

        VTblPtr create_tbl( const VSchPtr schema, const char * schema_type,
                            const char * dir_name, const Checksum_ptr checksum ) const {
            return VTbl::create( f_mgr, schema, schema_type, dir_name, checksum );
        }

        VDbPtr create_db( const VSchPtr schema, const std::string& schema_type,
                          const std::string& dir_name, const Checksum_ptr checksum ) const {
            return VDb::create( f_mgr, schema, schema_type, dir_name, checksum );
        }

        VDbPtr create_db( const VSchPtr schema, const char * schema_type,
                          const char * dir_name, const Checksum_ptr checksum ) const {
            return VDb::create( f_mgr, schema, schema_type, dir_name, checksum );
        }

#endif

        uint32_t version( void ) const {
            uint32_t v = 0;
            rc_t rc1 = ( VDBManagerVersion( f_mgr, &v ) );
            return 0 == rc1 ? v : 0;
        }

        int PathType( const std::string& path ) const {
            return VDBManagerPathType( f_mgr, "%s", path.c_str() );
        }

        bool AddSchemaIncludePath( const std::string& path ) const {
            rc_t rc1 = VDBManagerAddSchemaIncludePath( f_mgr, "%s", path.c_str() );
            return ( 0 == rc1 );
        }

        uint32_t GetObjVersion( const std::string& path ) const {
            uint32_t v = 0;
            rc_t rc1 = VDBManagerGetObjVersion( f_mgr, &v, path.c_str() );
            return 0 == rc1 ? v : 0;
        }

        KTime_t GetObjModDate( const std::string& path ) const {
            KTime_t t = 0;
            rc_t rc1 = VDBManagerGetObjModDate( f_mgr, &t, path . c_str() );
            return 0 == rc1 ? t : 0;
        }
};

} // namespace vdb
