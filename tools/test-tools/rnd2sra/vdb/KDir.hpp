#pragma once

#include "VdbObj.hpp"
#include "VdbMgr.hpp"

namespace vdb {

extern "C" {

rc_t KDirectoryAddRef_v1( const KDirectory *self );
#define KDirectoryAddRef KDirectoryAddRef_v1
rc_t KDirectoryRelease_v1( const KDirectory *self );
#define KDirectoryRelease KDirectoryRelease_v1

rc_t KDirectoryList_v1( const KDirectory *self, KNamelist **list,
        bool ( * f ) ( const KDirectory *dir, const char *name, void *data ),
        void *data, const char *path, ... );
#define KDirectoryList KDirectoryList_v1
rc_t KDirectoryVList( const KDirectory *self, KNamelist **list,
        bool ( * f ) ( const KDirectory *dir, const char *name, void *data ),
        void *data, const char *path, va_list args );

rc_t KDirectoryVisit_v1( const KDirectory *self, bool recur,
        rc_t ( * f ) ( const KDirectory *dir, uint32_t type, const char *name, void *data ),
        void *data, const char *path, ... );
#define KDirectoryVisit KDirectoryVisit_v1
rc_t KDirectoryVVisit( const KDirectory *self, bool recur,
        rc_t ( * f ) ( const KDirectory *dir, uint32_t type, const char *name, void *data ),
        void *data, const char *path, va_list args );

rc_t KDirectoryVisitUpdate_v1( KDirectory *self, bool recur,
        rc_t ( * f ) ( KDirectory *dir, uint32_t type, const char *name, void *data ),
        void *data, const char *path, ... );
#define KDirectoryVisitUpdate KDirectoryVisitUpdate_v1
rc_t KDirectoryVVisitUpdate( KDirectory *self, bool recur,
        rc_t ( * f ) ( KDirectory *dir, uint32_t type, const char *name, void *data ),
        void *data, const char *path, va_list args );

uint32_t KDirectoryPathType_v1( const KDirectory *self, const char *path, ... );
#define KDirectoryPathType KDirectoryPathType_v1
uint32_t KDirectoryVPathType( const KDirectory *self, const char *path, va_list args );

rc_t KDirectoryResolvePath_v1( const KDirectory *self, bool absolute,
		char *resolved, std::size_t rsize, const char *path, ... );
#define KDirectoryResolvePath KDirectoryResolvePath_v1

rc_t KDirectoryVResolvePath( const KDirectory *self, bool absolute,
        char *resolved, std::size_t rsize, const char *path, va_list args );

rc_t KDirectoryResolveAlias_v1( const KDirectory *self, bool absolute,
        char *resolved, std::size_t rsize, const char *alias, ... );
#define KDirectoryResolveAlias KDirectoryResolveAlias_v1
rc_t KDirectoryVResolveAlias( const KDirectory *self, bool absolute,
        char *resolved, std::size_t rsize, const char *alias, va_list args );

rc_t KDirectoryRename_v1( KDirectory *self, bool force, const char *from, const char *to );
#define KDirectoryRename KDirectoryRename_v1

rc_t KDirectoryRemove_v1( KDirectory *self, bool force, const char *path, ... );
#define KDirectoryRemove KDirectoryRemove_v1
rc_t KDirectoryVRemove( KDirectory *self, bool force, const char *path, va_list args );

rc_t KDirectoryClearDir_v1( KDirectory *self, bool force, const char *path, ... );
#define KDirectoryClearDir KDirectoryClearDir_v1
rc_t KDirectoryVClearDir( KDirectory *self, bool force, const char *path, va_list args );

rc_t KDirectoryAccess_v1( const KDirectory *self, uint32_t *access, const char *path, ... );
#define KDirectoryAccess KDirectoryAccess_v1
rc_t KDirectoryVAccess( const KDirectory *self, uint32_t *access, const char *path, va_list args );

rc_t KDirectorySetAccess_v1( KDirectory *self, bool recur, uint32_t access,
        uint32_t mask, const char *path, ... );
#define KDirectorySetAccess KDirectorySetAccess_v1
rc_t KDirectoryVSetAccess( KDirectory *self, bool recur, uint32_t access,
        uint32_t mask, const char *path, va_list args );

rc_t KDirectoryDate_v1( const KDirectory *self, KTime_t *date, const char *path, ... );
#define KDirectoryDate KDirectoryDate_v1
rc_t KDirectoryVDate( const KDirectory *self, KTime_t *date, const char *path, va_list args );

rc_t KDirectorySetDate_v1( KDirectory *self, bool recur, KTime_t date, const char *path, ... );
#define KDirectorySetDate KDirectorySetDate_v1
rc_t KDirectoryVSetDate( KDirectory *self, bool recur, KTime_t date, const char *path, va_list args );

rc_t KDirectoryCreateAlias_v1( KDirectory *self, uint32_t access, KCreateMode mode,
        const char *targ, const char *alias );
#define KDirectoryCreateAlias KDirectoryCreateAlias_v1

rc_t KDirectoryCreateLink_v1( KDirectory *self, uint32_t access, KCreateMode mode,
        const char *oldpath, const char *newpath );
#define KDirectoryCreateLink KDirectoryCreateLink_v1

rc_t KDirectoryOpenFileRead_v1( const KDirectory *self, KFile const **f, const char *path, ... );
#define KDirectoryOpenFileRead KDirectoryOpenFileRead_v1
rc_t KDirectoryVOpenFileRead( const KDirectory *self, KFile const **f, const char *path, va_list args );

rc_t KDirectoryOpenFileWrite_v1( KDirectory *self, KFile **f, bool update, const char *path, ... );
#define KDirectoryOpenFileWrite KDirectoryOpenFileWrite_v1
rc_t KDirectoryVOpenFileWrite( KDirectory *self, KFile **f, bool update, const char *path, va_list args );

rc_t KDirectoryOpenFileSharedWrite_v1( KDirectory *self, KFile **f, bool update, const char *path, ... );
#define KDirectoryOpenFileSharedWrite KDirectoryOpenFileSharedWrite_v1
rc_t KDirectoryVOpenFileSharedWrite( KDirectory *self, KFile **f, bool update, const char *path, va_list args );

rc_t KDirectoryCreateFile( KDirectory *self, KFile **f, bool update, uint32_t access,
        KCreateMode mode, const char *path, ... );
rc_t KDirectoryVCreateFile( KDirectory *self, KFile **f, bool update, uint32_t access,
        KCreateMode mode, const char *path, va_list args );

rc_t KDirectoryFileSize_v1( const KDirectory *self, uint64_t *size, const char *path, ... );
#define KDirectoryFileSize KDirectoryFileSize_v1
rc_t KDirectoryVFileSize( const KDirectory *self, uint64_t *size, const char *path, va_list args );

rc_t KDirectoryFilePhysicalSize_v1( const KDirectory *self, uint64_t *size, const char *path, ... );
#define KDirectoryFilePhysicalSize KDirectoryFilePhysicalSize_v1
rc_t KDirectoryVFilePhysicalSize ( const KDirectory *self, uint64_t *size, const char *path, va_list args );

rc_t KDirectorySetFileSize_v1( KDirectory *self, uint64_t size, const char *path, ... );
#define KDirectorySetFileSize KDirectorySetFileSize_v1
rc_t KDirectoryVSetFileSize ( KDirectory *self, uint64_t size, const char *path, va_list args );

rc_t KDirectoryFileLocator_v1( const KDirectory *self, uint64_t *locator, const char *path, ... );
#define KDirectoryFileLocator KDirectoryFileLocator_v1
rc_t KDirectoryVFileLocator ( const KDirectory *self, uint64_t *locator, const char *path, va_list args );

rc_t KDirectoryFileContiguous_v1( const KDirectory *self, bool *contiguous, const char *path, ... );
#define KDirectoryFileContiguous KDirectoryFileContiguous_v1
rc_t KDirectoryVFileContiguous( const KDirectory *self, bool *contiguous, const char *path, va_list args );

rc_t KDirectoryOpenDirRead_v1( const KDirectory *self, const KDirectory **sub, bool chroot, const char *path, ... );
#define KDirectoryOpenDirRead KDirectoryOpenDirRead_v1
rc_t KDirectoryVOpenDirRead( const KDirectory *self, const KDirectory **sub, bool chroot, const char *path, va_list args );

rc_t KDirectoryOpenDirUpdate_v1( KDirectory *self, KDirectory **sub, bool chroot, const char *path, ... );
#define KDirectoryOpenDirUpdate KDirectoryOpenDirUpdate_v1
rc_t KDirectoryVOpenDirUpdate( KDirectory *self, KDirectory **sub, bool chroot, const char *path, va_list args );

rc_t KDirectoryCreateDir_v1( KDirectory *self, uint32_t access, KCreateMode mode, const char *path, ... );
#define KDirectoryCreateDir KDirectoryCreateDir_v1
rc_t KDirectoryVCreateDir( KDirectory *self, uint32_t access, KCreateMode mode, const char *path, va_list args );

rc_t KDirectoryCopyPath_v1( const KDirectory *src_dir, KDirectory *dst_dir, const char *src_path, const char *dst_path );
#define KDirectoryCopyPath KDirectoryCopyPath_v1
rc_t KDirectoryCopyPaths_v1( const KDirectory * src_dir, KDirectory *dst_dir,
        bool recursive, const char *src, const char *dst );
#define KDirectoryCopyPaths KDirectoryCopyPaths_v1

rc_t KDirectoryCopy_v1( const KDirectory *src_dir, KDirectory *dst_dir, bool recursive, const char *src, const char *dst );
#define KDirectoryCopy KDirectoryCopy_v1

rc_t KDirectoryGetDiskFreeSpace_v1( const KDirectory * self, uint64_t * free_bytes_available, uint64_t * total_number_of_bytes );
#define KDirectoryGetDiskFreeSpace KDirectoryGetDiskFreeSpace_v1

rc_t KDirectoryNativeDir_v1( KDirectory **dir );
#define KDirectoryNativeDir KDirectoryNativeDir_v1

rc_t KFileAddRef( const KFile *self );
rc_t KFileRelease( const KFile *self );
rc_t KFileRandomAccess( const KFile *self );
uint32_t KFileType( const KFile *self );
rc_t KFileSize( const KFile *self, uint64_t *size );
rc_t KFileSetSize( KFile *self, uint64_t size );
rc_t KFileRead( const KFile *self, uint64_t pos, void *buffer, std::size_t bsize, std::size_t *num_read );
rc_t KFileTimedRead( const KFile *self, uint64_t pos,void *buffer, std::size_t bsize,
        std::size_t *num_read, timeout_t *tm );
rc_t KFileReadAll( const KFile *self, uint64_t pos, void *buffer, std::size_t bsize, std::size_t *num_read );
rc_t KFileTimedReadAll( const KFile *self, uint64_t pos, void *buffer, std::size_t bsize,
        std::size_t *num_read, timeout_t *tm );
rc_t KFileReadExactly( const KFile *self, uint64_t pos, void *buffer, std::size_t bytes );
rc_t KFileTimedReadExactly( const KFile *self, uint64_t pos, void *buffer, std::size_t bytes, timeout_t *tm );
rc_t KFileReadChunked( const KFile *self, uint64_t pos, KChunkReader * chunks,
        std::size_t bytes, std::size_t * num_read );
rc_t KFileTimedReadChunked( const KFile *self, uint64_t pos, KChunkReader * chunks,
        std::size_t bytes, std::size_t * num_read, timeout_t *tm );
rc_t KFileWrite( KFile *self, uint64_t pos, const void *buffer, std::size_t size, std::size_t *num_writ );
rc_t KFileTimedWrite( KFile *self, uint64_t pos, const void *buffer, std::size_t size,
        std::size_t *num_writ, timeout_t *tm );
rc_t KFileWriteAll( KFile *self, uint64_t pos, const void *buffer, std::size_t size, std::size_t *num_writ );
rc_t KFileTimedWriteAll( KFile *self, uint64_t pos, const void *buffer, std::size_t size,
        std::size_t *num_writ, struct timeout_t *tm );
rc_t KFileWriteExactly( KFile *self, uint64_t pos, const void *buffer, std::size_t bytes );
rc_t KFileTimedWriteExactly( KFile *self, uint64_t pos, const void *buffer, std::size_t bytes, timeout_t *tm );
rc_t KFileMakeStdIn( const KFile **std_in );
rc_t KFileMakeStdOut( KFile **std_out );
rc_t KFileMakeStdErr( KFile **std_err );

}; // end of extern "C"

class KDir;
typedef std::shared_ptr<KDir> KDirPtr;
class KDir : public VDBObj {
    private :
        KDirectory * f_dir;

        KDir() : f_dir( nullptr ) {
            set_rc( KDirectoryNativeDir( &f_dir ) );
        }

        KDir( const string& path ) : f_dir( nullptr ) {
            KDirectory * f_root;
            if ( set_rc( KDirectoryNativeDir( &f_root ) ) ) {
                if ( path . empty() ) {
                    f_dir = f_root;
                } else {
                    set_rc( KDirectoryOpenDirUpdate( f_root, &f_dir, true, "%s", path.c_str() ) );
                    KDirectoryRelease( f_root );
                }
            }
        }

    public :
        ~KDir() { KDirectoryRelease( f_dir ); }

        static KDirPtr make( void ) { return KDirPtr( new KDir() ); }
        static KDirPtr make( const string& path ) { return KDirPtr( new KDir( path ) ); }

        VMgrPtr make_mgr( void ) { return VMgr::make( f_dir ); }

        KDirectory * get_dir( void ) const { return f_dir; }
};

}; // end of namespace vdb
