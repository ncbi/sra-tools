#pragma once

#include <algorithm>

#include "VdbObj.hpp"
#include "VCursor.hpp"
#include "MetaData.hpp"
#include "checksum.hpp"

namespace vdb {

extern "C" {

rc_t VDatabaseOpenDBRead( const VDatabase *self, const VDatabase **db, const char *name, ... );
rc_t VDatabaseVOpenDBRead( const VDatabase *self, const VDatabase **db, const char *name, va_list args );

rc_t VDBManagerOpenTableRead( VDBManager const *self, const VTable **tbl,
        VSchema const *schema, const char *path, ... );
rc_t VDBManagerVOpenTableRead( VDBManager const *self, const VTable **tbl,
        VSchema const *schema, const char *path, va_list args );
rc_t VDBManagerOpenTableReadVPath( VDBManager const *self, const VTable **tbl,
        VSchema const *schema, struct VPath *path );

rc_t VDBManagerCreateTable( VDBManager *self, VTable **tbl,
        VSchema const *schema, const char *typespec,
        KCreateMode cmode, const char *path, ... );

rc_t VDatabaseOpenTableRead( VDatabase const *self, const VTable **tbl,
        const char *name, ... );
rc_t VDatabaseVOpenTableRead( VDatabase const *self, const VTable **tbl,
        const char *name, va_list args );

rc_t VDatabaseCreateTableByMask( VDatabase *self, VTable **tbl,
        const char *member, KCreateMode cmode, KCreateMode cmode_mask, const char *name, ... );
rc_t VDatabaseVCreateTableByMask( VDatabase *self, VTable **tbl,
        const char *member, KCreateMode cmode, KCreateMode cmode_mask, const char *name, va_list args );
/* the following function is DEPRECATED, it's left for backward compatibility only */
rc_t VDatabaseCreateTable( VDatabase *self, VTable **tbl,
        const char *member, KCreateMode cmode, const char *name, ... );
/* the following function is DEPRECATED, it's left for backward compatibility only */
rc_t VDatabaseVCreateTable( VDatabase *self, VTable **tbl,
        const char *member, KCreateMode cmode, const char *name, va_list args );

rc_t VTableAddRef( const VTable *self );
rc_t VTableRelease( const VTable *self );

rc_t VTableTypespec( const VTable *self, char *ts_buff, std::size_t ts_size );

rc_t VTableOpenIndexRead( const VTable *self, const KIndex **idx, const char *name, ... );
rc_t VTableVOpenIndexRead( const VTable *self, const KIndex **idx, const char *name, va_list args );

rc_t VTableListReadableColumns( const VTable *self, KNamelist **names );
rc_t VTableListCol( const VTable *self, KNamelist **names );

rc_t VTableListReadableDatatypes( const VTable *self, const char *col,
        uint32_t *dflt_idx, KNamelist **typedecls );

rc_t VTableColumnDatatypes( const VTable *self, const char *col,
        uint32_t *dflt_idx, KNamelist **typedecls );

rc_t VTableOpenSchema( const VTable *self, VSchema const **schema );

rc_t VTableIsEmpty( const VTable *self, bool * empty );

rc_t VTableGetQualityCapability( const VTable *self, bool *fullQuality, bool *synthQuality );

};  // end of extern "C"

class VTbl;
typedef std::shared_ptr<VTbl> VTblPtr;
class VTbl : public VDBObj {
    private :
#ifdef VDB_WRITE
        VTable * f_tbl;
#else
        const VTable * f_tbl;
#endif

        VTbl( const VDBManager * mgr, const std::string& acc ) {
            set_rc( VDBManagerOpenTableRead( mgr, ( const VTable** )&f_tbl, nullptr, "%s", acc.c_str() ) );
        }

        VTbl( const VDatabase * db, const std::string& name ) {
            set_rc( VDatabaseOpenTableRead( db, ( const VTable** )&f_tbl, "%s", name.c_str() ) );
        }

#ifdef VDB_WRITE
        VTbl( VDBManager * mgr, const VSchPtr schema,
              const std::string& schema_type, const std::string& dir_name,
              const Checksum_ptr checksum
        ) {
            KCreateMode mode = checksum -> mode( kcmInit );
            set_rc( VDBManagerCreateTable( mgr, &f_tbl, schema -> get_schema(),
                    schema_type . c_str(), mode, "%s", dir_name . c_str() ) );
        }

        VTbl( VDatabase * db, const std::string& schema_member, const std::string& name,
              const Checksum_ptr checksum ) {
            KCreateMode mode = checksum -> mode( kcmInit );
            set_rc( VDatabaseCreateTable( db, &f_tbl,
                    schema_member . c_str(), mode, "%s", name . c_str() ) );
        }
#endif

    public :
        ~VTbl() { VTableRelease( f_tbl ); }

        static VTblPtr open( const VDBManager * mgr, const std::string& acc ) {
            return VTblPtr( new VTbl( mgr, acc ) );
        }

        static VTblPtr open( const VDatabase * db, const std::string& name ) {
            return VTblPtr( new VTbl( db, name ) );
        }

        MetaPtr open_meta( void ) const { return Meta::open( f_tbl ); }

#ifdef VDB_WRITE
        static VTblPtr create( VDBManager * mgr, const VSchPtr schema,
                              const std::string& schema_type, const std::string& dir_name,
                              const Checksum_ptr checksum ) {
            return VTblPtr( new VTbl( mgr, schema, schema_type, dir_name, checksum ) );
        }

        static VTblPtr create( VDBManager * mgr, const VSchPtr schema,
                              const char * schema_type, const char * dir_name,
                              const Checksum_ptr checksum ) {
            return VTblPtr( new VTbl( mgr, schema, schema_type, dir_name, checksum ) );
        }

        static VTblPtr create( VDatabase * db, const std::string& schema_member,
                               const std::string& name, const Checksum_ptr checksum ) {
            return VTblPtr( new VTbl( db, schema_member, name, checksum ) );
        }

        static VTblPtr create( VDatabase * db, const char * schema_member,
                               const char * name, const Checksum_ptr checksum ) {
            return VTblPtr( new VTbl( db, schema_member, name, checksum ) );
        }

        MetaPtr open_meta_for_update( void ) const { return Meta::open( f_tbl ); }

#endif

        list_of_str columns( bool readable = true ) {
            KNamelist * t;
            if ( readable ) {
                set_rc( VTableListReadableColumns( f_tbl, &t ) );
            } else {
                set_rc( VTableListCol( f_tbl, &t  ) );
            }
            return into_list( t );
        }

        bool has_column( const std::string& col_name )  {
            auto available = columns();
            auto found = std::find( available.begin(), available.end(), col_name );
            return ( found != available . end() );
        }

        RowRange row_range( const std::string& col_name ) {
            auto cur = cursor();
            if ( *cur  ) {
                if ( cur -> add_col( 1, col_name ) ) {
                    if ( cur -> open() ) {
                        return cur -> row_range( 1 );
                    }
                }
            }
            return RowRange();
        }

        bool empty( void ) {
            bool b = true;
            set_rc( VTableIsEmpty( f_tbl, &b ) );
            return b;
        }

        std::string get_type_spec( void ) const {
            char buff[ 64 ];
            rc_t rc = VTableTypespec( f_tbl, buff, sizeof( buff ) );
            return 0 == rc ? std::string( buff ) : std::string( "" );
        }

        VCurPtr cursor( std::size_t capacity = 0 ) const {
            return VCur::make( f_tbl, capacity );
        }

#ifdef VDB_WRITE
        VCurPtr writable_cursor( void ) const { return VCur::make( f_tbl ); }
#endif
};

}; // end of namespace vdb
