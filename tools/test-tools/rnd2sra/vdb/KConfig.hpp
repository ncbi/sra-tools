#pragma once

#include "VdbObj.hpp"
#include "VdbString.hpp"
#include "KDir.hpp"

namespace vdb {

extern "C" {

rc_t KConfigMake( KConfig **cfg, KDirectory const * optional_search_base );
rc_t KConfigAddRef( const KConfig *self );
rc_t KConfigRelease( const KConfig *self );
rc_t KConfigLoadFile( KConfig * self, const char * path, KFile const * file );
rc_t KConfigCommit( KConfig *self );
rc_t KConfigRead( const KConfig *self, const char *path, std::size_t offset, char *buffer,
		std::size_t bsize, std::size_t *num_read, std::size_t *remaining );
rc_t KConfigReadBool( const KConfig* self, const char* path, bool* result );
rc_t KConfigWriteBool( KConfig *self, const char * path, bool value );
rc_t KConfigReadI64( const KConfig* self, const char* path, int64_t* result );
rc_t KConfigReadU64( const KConfig* self, const char* path, uint64_t* result );
rc_t KConfigReadF64( const KConfig* self, const char* path, double* result );
rc_t KConfigReadString( const KConfig* self, const char* path, String** result );
rc_t KConfigWriteString( KConfig *self, const char * path, const char * value );
rc_t KConfigWriteSString( KConfig *self, const char * path, String const * value );
rc_t KConfigPrint( const KConfig * self, int indent );
rc_t KConfigToFile( const KConfig * self, KFile * file );
void KConfigDisableUserSettings ( void );
void KConfigSetNgcFile(const char * path);

/* ========================================================================================= */

rc_t KConfigNodeAddRef( const KConfigNode *self );
rc_t KConfigNodeRelease( const KConfigNode *self );

rc_t KConfigNodeGetMgr( const KConfigNode * self, KConfig ** mgr );
rc_t KConfigOpenNodeRead ( const KConfig *self, const KConfigNode **node, const char *path, ... );
rc_t KConfigNodeOpenNodeRead ( const KConfigNode *self, const KConfigNode **node, const char *path, ... );
rc_t KConfigVOpenNodeRead( const KConfig *self, const KConfigNode **node, const char *path, va_list args );
rc_t KConfigNodeVOpenNodeRead( const KConfigNode *self, const KConfigNode **node, const char *path, va_list args );
rc_t KConfigOpenNodeUpdate( KConfig *self, KConfigNode **node, const char *path, ... );
rc_t KConfigNodeOpenNodeUpdate( KConfigNode *self, KConfigNode **node, const char *path, ... );
rc_t KConfigVOpenNodeUpdate( KConfig *self, KConfigNode **node, const char *path, va_list args );
rc_t KConfigNodeVOpenNodeUpdate( KConfigNode *self, KConfigNode **node, const char *path, va_list args );
rc_t KConfigNodeRead( const KConfigNode *self, std::size_t offset, char *buffer, std::size_t bsize,
        std::size_t *num_read, std::size_t *remaining );
rc_t KConfigNodeReadBool( const KConfigNode *self, bool* result );
rc_t KConfigNodeReadI64( const KConfigNode *self, int64_t* result );
rc_t KConfigNodeReadU64( const KConfigNode *self, uint64_t* result );
rc_t KConfigNodeReadF64( const KConfigNode *self, double* result );
rc_t KConfigNodeReadString( const KConfigNode *self, struct String** result );
#define KConfigNodeListChild KConfigNodeListChildren
rc_t KConfigNodeListChildren( const KConfigNode *self, KNamelist **names );
rc_t KConfigNodeWrite( KConfigNode *self, const char *buffer, std::size_t size );
rc_t KConfigNodeWriteBool( KConfigNode *self, bool state );
rc_t KConfigNodeAppend( KConfigNode *self, const char *buffer, std::size_t size );
rc_t KConfigNodeReadAttr( const KConfigNode *self, const char *name, char *buffer, std::size_t bsize, std::size_t *size );
rc_t KConfigNodeWriteAttr( KConfigNode *self, const char *name, const char *value );
rc_t KConfigNodeDropAll( KConfigNode *self );
rc_t KConfigNodeDropAttr( KConfigNode *self, const char *attr );
rc_t KConfigNodeDropChild( KConfigNode *self, const char *path, ... );
rc_t KConfigNodeVDropChild( KConfigNode *self, const char *path, va_list args );
rc_t KConfigNodeRenameAttr( KConfigNode *self, const char *from, const char *to );
rc_t KConfigNodeRenameChild( KConfigNode *self, const char *from, const char *to );

}; // end of extern "C"

}; // end of namesapce vdb
