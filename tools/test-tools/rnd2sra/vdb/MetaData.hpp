#pragma once

#include "VdbObj.hpp"
#include <cstdint>

namespace vdb {

using namespace std;

extern "C" {

rc_t KMetadataAddRef ( const KMetadata *self );
rc_t KMetadataRelease ( const KMetadata *self );

typedef struct KDatabase KDatabase;
typedef struct KTable KTable;
typedef struct KColumn KColumn;

rc_t KDatabaseOpenMetadataRead( KDatabase const *self, const KMetadata **meta );
rc_t KTableOpenMetadataRead( KTable const *self, const KMetadata **meta );
rc_t KColumnOpenMetadataRead( KColumn const *self, const KMetadata **meta );

rc_t KDatabaseOpenMetadataUpdate( KDatabase *self, KMetadata **meta );
rc_t KTableOpenMetadataUpdate( KTable *self, KMetadata **meta );
rc_t KColumnOpenMetadataUpdate( KColumn *self, KMetadata **meta );

rc_t VTableOpenMetadataRead( const VTable *self, KMetadata const **meta );
rc_t VTableOpenMetadataUpdate( VTable *self, struct KMetadata **meta );

rc_t KMetadataVersion( const KMetadata *self, uint32_t *version );
rc_t KMetadataByteOrder( const KMetadata *self, bool *reversed );
rc_t KMetadataRevision( const KMetadata *self, uint32_t *revision );
rc_t KMetadataMaxRevision( const KMetadata *self, uint32_t *revision );

rc_t KMetadataCommit( KMetadata *self );
rc_t KMetadataFreeze( KMetadata *self );

rc_t KMetadataOpenRevision( const KMetadata *self, const KMetadata **meta, uint32_t revision );

rc_t KMetadataGetSequence( const KMetadata *self, const char *seq, int64_t *val );
rc_t KMetadataSetSequence( KMetadata *self, const char *seq, int64_t val );
rc_t KMetadataNextSequence( KMetadata *self, const char *seq, int64_t *val );

typedef struct KMDataNode KMDataNode;

rc_t KMDataNodeAddRef( const KMDataNode *self );
rc_t KMDataNodeRelease( const KMDataNode *self );

rc_t KMetadataOpenNodeRead( const KMetadata *self, const KMDataNode **node, const char *path, ... );
rc_t KMDataNodeOpenNodeRead( const KMDataNode *self, const KMDataNode **node, const char *path, ... );
rc_t KMetadataVOpenNodeRead( const KMetadata *self, const KMDataNode **node, const char *path, va_list args );
rc_t KMDataNodeVOpenNodeRead( const KMDataNode *self, const KMDataNode **node, const char *path, va_list args );

rc_t KMetadataOpenNodeUpdate( KMetadata *self, KMDataNode **node, const char *path, ... );
rc_t KMDataNodeOpenNodeUpdate( KMDataNode *self, KMDataNode **node, const char *path, ... );
rc_t KMetadataVOpenNodeUpdate( KMetadata *self, KMDataNode **node, const char *path, va_list args );
rc_t KMDataNodeVOpenNodeUpdate( KMDataNode *self, KMDataNode **node, const char *path, va_list args );

rc_t KMDataNodeByteOrder( const KMDataNode *self, bool *reversed );

rc_t KMDataNodeRead( const KMDataNode *self, size_t offset, void *buffer, size_t bsize,
                     size_t *num_read, size_t *remaining );
rc_t KMDataNodeWrite( KMDataNode *self, const void *buffer, size_t size );
rc_t KMDataNodeAppend( KMDataNode *self, const void *buffer, size_t size );

rc_t KMDataNodeReadB8( const KMDataNode *self, void *b8 );
rc_t KMDataNodeReadB16( const KMDataNode *self, void *b16 );
rc_t KMDataNodeReadB32( const KMDataNode *self, void *b32 );
rc_t KMDataNodeReadB64( const KMDataNode *self, void *b64 );
rc_t KMDataNodeReadB128( const KMDataNode *self, void *b128 );

rc_t KMDataNodeReadAsI16( const KMDataNode *self, int16_t *i );
rc_t KMDataNodeReadAsU16( const KMDataNode *self, uint16_t *u );
rc_t KMDataNodeReadAsI32( const KMDataNode *self, int32_t *i );
rc_t KMDataNodeReadAsU32( const KMDataNode *self, uint32_t *u );
rc_t KMDataNodeReadAsI64( const KMDataNode *self, int64_t *i );
rc_t KMDataNodeReadAsU64( const KMDataNode *self, uint64_t *u );
rc_t KMDataNodeReadAsF64( const KMDataNode *self, double *f );
rc_t KMDataNodeReadCString( const KMDataNode *self, char *buffer, size_t bsize, size_t *size );

rc_t KMDataNodeWriteB8( KMDataNode *self, const void *b8 );
rc_t KMDataNodeWriteB16( KMDataNode *self, const void *b16 );
rc_t KMDataNodeWriteB32( KMDataNode *self, const void *b32 );
rc_t KMDataNodeWriteB64( KMDataNode *self, const void *b64 );
rc_t KMDataNodeWriteB128( KMDataNode *self, const void *b128 );
rc_t KMDataNodeWriteCString( KMDataNode *self, const char *str );

rc_t KMDataNodeReadAttr( const KMDataNode *self, const char *name, char *buffer, size_t bsize, size_t *size );
rc_t KMDataNodeWriteAttr( KMDataNode *self, const char *name, const char *value );

rc_t KMDataNodeReadAttrAsI16( const KMDataNode *self, const char *attr, int16_t *i );
rc_t KMDataNodeReadAttrAsU16( const KMDataNode *self, const char *attr, uint16_t *u );
rc_t KMDataNodeReadAttrAsI32( const KMDataNode *self, const char *attr, int32_t *i );
rc_t KMDataNodeReadAttrAsU32( const KMDataNode *self, const char *attr, uint32_t *u );
rc_t KMDataNodeReadAttrAsI64( const KMDataNode *self, const char *attr, int64_t *i );
rc_t KMDataNodeReadAttrAsU64( const KMDataNode *self, const char *attr, uint64_t *u );
rc_t KMDataNodeReadAttrAsF64( const KMDataNode *self, const char *attr, double *f );

rc_t KMDataNodeDropAll( KMDataNode *self );
rc_t KMDataNodeDropAttr( KMDataNode *self, const char *attr );
rc_t KMDataNodeDropChild( KMDataNode *self, const char *path, ... );
rc_t KMDataNodeVDropChild( KMDataNode *self, const char *path, va_list args );

rc_t KMDataNodeRenameAttr( KMDataNode *self, const char *from, const char *to );
rc_t KMDataNodeRenameChild( KMDataNode *self, const char *from, const char *to );

rc_t KMDataNodeCopy( KMDataNode *self, KMDataNode const *source );
rc_t KMDataNodeCompare( const KMDataNode *self, KMDataNode const *other, bool *equal );
rc_t KTableMetaCopy( struct KTable *self, const struct KTable *src,
                     const char * path, bool src_node_has_to_exist );
rc_t KTableMetaCompare( const struct KTable *self, const struct KTable *other,
                        const char * path, bool * equal );

}; // end of extern "C"

class MetaNode;
typedef std::shared_ptr< MetaNode > MetaNodePtr;
class MetaNode : public VDBObj {
    private :
#ifdef VDB_WRITE
        KMDataNode * f_node;
#else
        const KMDataNode * f_node;
#endif

        MetaNode( const KMetadata * meta, const string& path ) {
            set_rc( KMetadataOpenNodeRead( meta, (const KMDataNode **)&f_node, "%s", path . c_str() ) );
        }

        MetaNode( const KMDataNode * parent, const string& path ) {
            set_rc( KMDataNodeOpenNodeRead( parent, (const KMDataNode **)&f_node, "%s", path . c_str() ) );
        }

#ifdef VDB_WRITE
        MetaNode( KMetadata * meta, const string& path ) {
            set_rc( KMetadataOpenNodeUpdate( meta, &f_node, "%s", path . c_str() ) );
        }

        MetaNode( KMDataNode * parent, const string& path ) {
            set_rc( KMDataNodeOpenNodeUpdate( parent, &f_node, "%s", path . c_str() ) );
        }

#endif

    public :
        ~MetaNode() { KMDataNodeRelease( f_node ); }

        static MetaNodePtr open( const KMetadata * meta, const string& path ) {
            return MetaNodePtr( new MetaNode( meta, path ) );
        }

        MetaNodePtr open( const string& path ) {
            return MetaNodePtr( new MetaNode( f_node, path ) );
        }

        bool read( int16_t& value ) const {
            rc_t rc1 = KMDataNodeReadAsI16( f_node, &value );
            return ( 0 == rc1 );
        }

        bool read( uint16_t& value ) const {
            rc_t rc1 = KMDataNodeReadAsU16( f_node, &value );
            return ( 0 == rc1 );
        }

        bool read( int32_t& value ) const {
            rc_t rc1 = KMDataNodeReadAsI32( f_node, &value );
            return ( 0 == rc1 );
        }

        bool read( uint32_t& value ) const {
            rc_t rc1 = KMDataNodeReadAsU32( f_node, &value );
            return ( 0 == rc1 );
        }

        bool read( int64_t& value ) const {
            rc_t rc1 = KMDataNodeReadAsI64( f_node, &value );
            return ( 0 == rc1 );
        }

        bool read( uint64_t& value ) const {
            rc_t rc1 = KMDataNodeReadAsU64( f_node, &value );
            return ( 0 == rc1 );
        }

        bool read( double value ) const {
            rc_t rc1 = KMDataNodeReadAsF64( f_node, &value );
            return ( 0 == rc1 );
        }

        bool read( string& value ) const {
            char buffer[ 512 ];
            size_t read;
            rc_t rc1 = KMDataNodeReadCString( f_node, buffer, sizeof buffer, &read );
            if ( 0 == rc1 ) {
                value = string( buffer, read );
                return true;
            }
            return false;
        }

#ifdef VDB_WRITE
        static MetaNodePtr open_update( KMetadata * meta, const string& path ) {
            return MetaNodePtr( new MetaNode( meta, path ) );
        }

        MetaNodePtr open_update( const string& path ) {
            return MetaNodePtr( new MetaNode( f_node, path ) );
        }

        bool write( int16_t value ) {
            rc_t rc1 = KMDataNodeWriteB16( f_node, &value );
            return ( 0 == rc1 );
        }

        bool write( uint16_t value ) {
            rc_t rc1 = KMDataNodeWriteB16( f_node, &value );
            return ( 0 == rc1 );
        }

        bool write( int32_t value ) {
            rc_t rc1 = KMDataNodeWriteB32( f_node, &value );
            return ( 0 == rc1 );
        }

        bool write( uint32_t value ) {
            rc_t rc1 = KMDataNodeWriteB32( f_node, &value );
            return ( 0 == rc1 );
        }

        bool write( int64_t value ) {
            rc_t rc1 = KMDataNodeWriteB64( f_node, &value );
            return ( 0 == rc1 );
        }

        bool write( uint64_t value ) {
            rc_t rc1 = KMDataNodeWriteB64( f_node, &value );
            return ( 0 == rc1 );
        }

        bool write( const string& value ) {
            rc_t rc1 = KMDataNodeWriteCString( f_node, value . c_str() );
            return ( 0 == rc1 );
        }
#endif

};

class Meta;
typedef std::shared_ptr< Meta > MetaPtr;
class Meta : public VDBObj {
    private :
#ifdef VDB_WRITE
        KMetadata * f_meta;
#else
        const KMetadata * f_meta;
#endif

        Meta( const VTable * tbl ) { set_rc( VTableOpenMetadataRead( tbl, ( const KMetadata ** )&f_meta ) ); }

#ifdef VDB_WRITE
        Meta( VTable * tbl ) { set_rc( VTableOpenMetadataUpdate( tbl, &f_meta ) ); }
#endif

    public:
        ~Meta() { KMetadataRelease( f_meta ); }

        static MetaPtr open( const VTable * tbl ) { return MetaPtr( new Meta( tbl ) ); }

        MetaNodePtr open_node( const string& path ) {
            return MetaNode::open( f_meta, path );
        }

#ifdef VDB_WRITE
        static MetaPtr open( VTable * tbl ) { return MetaPtr( new Meta( tbl ) ); }

        MetaNodePtr open_node_update( const string& path ) {
            return MetaNode::open_update( f_meta, path );
        }

        bool commit( void ) {
            return set_rc( KMetadataCommit( f_meta ) );
        }

        bool freeze( void ) {
            return set_rc( KMetadataFreeze( f_meta ) );
        }

#endif

        uint32_t get_version( void ) const {
            uint32_t version;
            rc_t rc1 = KMetadataVersion( f_meta, &version );
            if ( 0 != rc1 ) { version = 0; }
            return version;
        }

        bool get_byte_order( void ) const {
            bool res;
            rc_t rc1 = KMetadataByteOrder( f_meta, &res );
            if ( 0 != rc1 ) { res = false; }
            return res;
        }

        uint32_t get_revision( void ) const {
            uint32_t revision;
            rc_t rc1 = KMetadataRevision( f_meta, &revision );
            if ( 0 != rc1 ) { revision = 0; }
            return revision;
        }

        uint32_t get_max_revision( void ) const {
            uint32_t revision;
            rc_t rc1 = KMetadataMaxRevision( f_meta, &revision );
            if ( 0 != rc1 ) { revision = 0; }
            return revision;
        }

};

}; // end of namespace vdb
