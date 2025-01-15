#pragma once

#include "VdbObj.hpp"
#include "VdbTable.hpp"

namespace vdb {

extern "C" {

rc_t VDatabaseAddRef( const VDatabase *self );
rc_t VDatabaseRelease( const VDatabase *self );

rc_t VDatabaseOpenSchema( const VDatabase *self, VSchema const **schema );
rc_t VDatabaseListTbl( const VDatabase *self, KNamelist **names );
rc_t VDatabaseListDB( const VDatabase *self, KNamelist **names );

rc_t VDatabaseTypespec( const VDatabase *self, char *ts_buff, std::size_t ts_size );
rc_t VDatabaseGetQualityCapability( const VDatabase *self, bool *fullQuality, bool *synthQuality );
rc_t VDatabaseOpenMetadataRead( const VDatabase *self, KMetadata const **meta );

rc_t VDBManagerOpenDBRead( VDBManager const *self, const VDatabase **db,
        VSchema const *schema, const char *path, ... );
rc_t VDBManagerVOpenDBRead( VDBManager const *self, const VDatabase **db,
        VSchema const *schema, const char *path, va_list args );
rc_t VDBManagerOpenDBReadVPath( VDBManager const *self, const VDatabase **db,
        VSchema const *schema, VPath *path );

rc_t VDBManagerCreateDB( VDBManager *self, VDatabase **db,
        VSchema const *schema, const char *typespec,
        KCreateMode cmode, const char *path, ... );

rc_t VDatabaseCreateDB( VDatabase *self, VDatabase **db,
        const char *member, KCreateMode cmode, const char *name, ... );

};  // end of extern "C"

class VDb;
typedef std::shared_ptr< VDb > VDbPtr;
class VDb : public VDBObj {
    private :

#ifdef VDB_WRITE
         VDatabase * f_db;
#else
        const VDatabase * f_db;
#endif

        VDb( const VDBManager * mgr, const std::string& acc ) {
            set_rc( VDBManagerOpenDBRead( mgr, ( const VDatabase** )&f_db, nullptr, "%s", acc.c_str() ) );
        }

        VDb( const VDatabase * db, const std::string& name ) {
            set_rc( VDatabaseOpenDBRead( db, ( const VDatabase** )&f_db, "%s", name.c_str() ) );
        }

#ifdef VDB_WRITE
        VDb( VDBManager * mgr, const VSchPtr schema,
             const std::string& schema_type, const std::string& dir_name, const Checksum_ptr checksum ) {
            KCreateMode mode = checksum -> mode( kcmInit );
            set_rc( VDBManagerCreateDB( mgr, &f_db, schema -> get_schema(),
                    schema_type . c_str(), mode, "%s", dir_name . c_str() ) );
        }

        VDb( VDatabase * parent, const std::string& schema_member, const std::string& name,
             const Checksum_ptr checksum ) {
            KCreateMode mode = checksum -> mode( kcmInit );
            set_rc( VDatabaseCreateDB( parent, &f_db,
                    schema_member . c_str(), mode, "%s", name . c_str() ) );
        }

#endif

    public :
        ~VDb() { VDatabaseRelease( f_db ); }

        static VDbPtr make( const VDBManager * mgr, const std::string& acc ) {
            return VDbPtr( new VDb( mgr, acc ) );
        }

#ifdef VDB_WRITE
        static VDbPtr create( VDBManager * mgr, const VSchPtr schema,
                const std::string& schema_type, const std::string& dir_name,
                const Checksum_ptr checksum ) {
            return VDbPtr( new VDb( mgr, schema, schema_type, dir_name, checksum ) );
        }

        static VDbPtr create( VDBManager * mgr, const VSchPtr schema,
                const char * schema_type, const char * dir_name,
                const Checksum_ptr checksum ) {
            return VDbPtr( new VDb( mgr, schema, schema_type, dir_name, checksum ) );
        }

        VDbPtr create_child( const std::string& schema_member, const std::string& name,
                             const Checksum_ptr checksum ) {
            return VDbPtr( new VDb( f_db, schema_member, name, checksum ) );
        }

        VDbPtr create_child( const char * schema_member, const char * name,
                             const Checksum_ptr checksum) {
            return VDbPtr( new VDb( f_db, schema_member, name, checksum ) );
        }

        VTblPtr create_tbl( const std::string& schema_member, const std::string& name,
                            const Checksum_ptr checksum ) {
            return VTbl::create( f_db, schema_member, name, checksum );
        }

        VTblPtr create_tbl( const char * schema_member, const char * name,
                            const Checksum_ptr checksum ) {
            return VTbl::create( f_db, schema_member, name, checksum );
        }

#endif

        list_of_str tables( void ) {
            KNamelist * t;
            set_rc( VDatabaseListTbl( f_db, &t ) );
            return into_list( t );
        }

        list_of_str dbs( void ) {
            KNamelist * t;
            set_rc( VDatabaseListDB( f_db, &t ) );
            return into_list( t );
        }

        VDbPtr open_db( const std::string& name ) const {
            return VDbPtr( new VDb( f_db, name ) );
        }

        VTblPtr open_tbl( const std::string& name ) const {
            return VTbl::open( f_db, name );
        }

        RowRange range( const std::string tbl_name, const std::string& col_name ) const {
            auto tbl = open_tbl( tbl_name );
            if ( *tbl ) { return tbl -> row_range( col_name ); }
            return RowRange();
        }
};

}; // end of namespace vdb
