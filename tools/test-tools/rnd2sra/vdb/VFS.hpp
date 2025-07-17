#pragma once

#include "VdbObj.hpp"
#include "KConfig.hpp"
#include "Resolver.hpp"

namespace vdb {

extern "C" {

rc_t VFSManagerAddRef( const VFSManager *self );
rc_t VFSManagerRelease( const VFSManager *self );
rc_t VFSManagerMake ( VFSManager ** pmanager );

rc_t VFSManagerMakeResolver( VFSManager const * self, VResolver ** new_resolver, KConfig const * cfg );

//rc_t KRepositoryMakeResolver( KRepository const * self, VResolver ** new_resolver, KConfig const * cfg );

rc_t VFSManagerOpenFileRead ( const VFSManager *self, KFile const **f, const VPath * path );
rc_t VFSManagerOpenFileReadWithBlocksize ( const VFSManager *self, KFile const **f,
        const VPath * path, uint32_t blocksize, bool promote );
rc_t VFSManagerOpenDirectoryRead( const VFSManager *self, KDirectory const **d, const VPath * path );
rc_t VFSManagerOpenDirectoryReadDecrypt( const VFSManager *self, KDirectory const **d, const VPath * path );
rc_t VFSManagerOpenDirectoryReadDecryptRemote( const VFSManager *self, KDirectory const **d,
        const VPath * path, const VPath * cache );
rc_t VFSManagerOpenDirectoryUpdate( const VFSManager *self, KDirectory **d, const VPath * path );
rc_t VFSManagerOpenFileWrite( const VFSManager *self, KFile **f, bool update, const VPath * path);
rc_t VFSManagerCreateFile( const VFSManager *self,  KFile **f, bool update, uint32_t access,
        KCreateMode mode, const VPath * path );
rc_t VFSManagerRemove( const VFSManager *self, bool force, const VPath * path );
rc_t VFSManagerGetCWD( const VFSManager * self, KDirectory ** cwd );
rc_t VFSManagerGetResolver( const VFSManager * self, VResolver ** resolver );
//rc_t VFSManagerGetKNSMgr( const VFSManager * self, KNSManager ** kns );
rc_t VFSManagerGetKryptoPassword( const VFSManager * self, char * new_password, std::size_t max_size, std::size_t * size);
rc_t VFSManagerUpdateKryptoPassword( const VFSManager * self, const char * password,
        std::size_t size, char * pwd_dir, std::size_t pwd_dir_size );
rc_t VFSManagerResolveSpec( const VFSManager * self, const char * spec, VPath ** path_to_build,
        const KFile ** remote_file, const VPath ** local_cache, bool resolve_acc );
rc_t VFSManagerResolveSpecIntoDir( const VFSManager * self, const char * spec,
        const KDirectory ** dir, bool resolve_acc );

rc_t KConfigReadVPath( KConfig const* self, const char* path, VPath** result );
rc_t KConfigNodeReadVPath( KConfigNode const *self, VPath** result );

#define vfsmgr_rflag_no_acc_local ( 1 << 0 )
#define vfsmgr_rflag_no_acc_remote ( 1 << 1 )
#define vfsmgr_rflag_no_acc (vfsmgr_rflag_no_acc_local | vfsmgr_rflag_no_acc_remote )

rc_t VFSManagerResolvePath( const VFSManager * self, uint32_t flags,
        const VPath * in_path, VPath ** out_path);
rc_t VFSManagerResolvePathRelative( const VFSManager * self, uint32_t flags,
        const  VPath * base_path, const VPath * in_path, struct VPath ** out_path );

rc_t VFSManagerRegisterObject( VFSManager* self, uint32_t oid, const VPath* obj );
rc_t VFSManagerGetObjectId( const VFSManager* self, const VPath* obj, uint32_t* oid );
rc_t VFSManagerGetObject( const VFSManager* self, uint32_t oid, struct VPath** obj );
rc_t VFSManagerSetAdCaching( VFSManager* self, bool enabled );
bool VFSManagerCheckAd( const VFSManager * self, const VPath * inPath, const VPath ** outPath );
bool VFSManagerCheckEnvAndAd( const VFSManager * self, const VPath * inPath, const VPath ** outPath);
rc_t VFSManagerLogNamesServiceErrors(  VFSManager* self, bool enabled );
rc_t VFSManagerGetLogNamesServiceErrors( VFSManager * self, bool * enabled);

rc_t VFSManagerMakePath( VFSManager const * self, VPath ** new_path, const char * path_str, ... );
rc_t VFSManagerVMakePath( VFSManager const * self, VPath ** new_path, const char * path_fmt, va_list args );
rc_t VFSManagerMakeSysPath( VFSManager const * self, VPath ** new_path, const char * sys_path );
rc_t VFSManagerWMakeSysPath( VFSManager const * self, VPath ** new_path, const wchar_t * wide_sys_path );
rc_t VFSManagerMakeAccPath( VFSManager const * self, VPath ** new_path, const char * acc, ... );
rc_t VFSManagerVMakeAccPath( VFSManager const * self, VPath ** new_path, const char * fmt, va_list args );
rc_t VFSManagerMakeOidPath( VFSManager const * self, VPath ** new_path, uint32_t oid );
rc_t VFSManagerMakePathWithExtension( VFSManager const * self, VPath ** new_path,
        const VPath * orig, const char * extension );
rc_t VFSManagerExtractAccessionOrOID( VFSManager const * self, VPath ** acc_or_oid, const VPath * orig );

rc_t VPathAddRef( const VPath *self );
rc_t VPathRelease( const VPath *self );
bool VPathIsAccessionOrOID( const VPath * self );
bool VPathIsFSCompatible( const VPath * self );
bool VPathFromUri( const VPath * self );
rc_t VPathMarkHighReliability( VPath * self, bool high_reliability );
bool VPathIsHighlyReliable( const VPath * self );
rc_t VPathReadUri( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadScheme( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadAuth( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadHost( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadPortName( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadPath( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadSysPath( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadQuery( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadParam( const VPath * self, const char * param, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathReadFragment( const VPath * self, char * buffer, std::size_t buffer_size, std::size_t * num_read );
rc_t VPathMakeUri( const VPath * self, struct String const ** uri );
rc_t VPathMakeSysPath( const VPath * self, struct String const ** sys_path );
rc_t VPathMakeString( const VPath * self, struct String const ** str );
rc_t VPathGetVdbcache(const VPath * self, const VPath ** vdbcache, bool * vdbcacheChecked );
rc_t VPathGetScheme( const VPath * self, struct String * str );
rc_t VPathGetAuth( const VPath * self, struct String * str );
rc_t VPathGetHost( const VPath * self, struct String * str );
rc_t VPathGetPortName( const VPath * self, struct String * str );
uint16_t VPathGetPortNum( const VPath * self );
rc_t VPathGetPath( const VPath * self, struct String * str );
rc_t VPathGetQuery( const VPath * self, struct String * str );
rc_t VPathGetParam( const VPath * self, const char * param, struct String * str );
rc_t VPathGetFragment( const VPath * self, struct String * str );
uint32_t VPathGetOid( const VPath * self );
rc_t VPathGetId( const VPath * self, struct String * str );
rc_t VPathGetTicket( const VPath * self, struct String * str );
rc_t VPathGetAcc( const VPath * self, struct String * str );
rc_t VPathGetService( const VPath * self, struct String * str );
rc_t VPathGetType( const VPath * self, struct String * str );
rc_t VPathGetObjectType( const VPath * self, struct String * str );
rc_t VPathGetName( const VPath * self, struct String * str );
rc_t VPathGetNameExt( const VPath * self, struct String * str );
rc_t VPathGetCeRequired( const VPath * self, bool * required );
rc_t VPathGetPayRequired( const VPath * self, bool * required );
KTime_t VPathGetModDate( const VPath * self );
uint64_t VPathGetSize( const VPath * self );
const uint8_t * VPathGetMd5( const VPath * self );
bool VPathGetProjectId( const VPath * self, uint32_t * projectId );
// VQuality VPathGetQuality( const VPath * self );

}; // end of extern "C"

class VPathObj;
typedef std::shared_ptr<VPathObj> VPathObjPtr;
class VPathObj {
    private :
        const VPath * f_path;

        VPathObj( const VPath * path ) : f_path( path ) {}

    public :
        static VPathObjPtr make( VFSManager * mgr, const std::string& s ) {
            VPath * p;
            rc_t rc = VFSManagerMakePath( mgr, &p, "%s", s . c_str() );
            if ( 0 == rc ) {
                return VPathObjPtr( new VPathObj( p ) );
            }
            return nullptr;
        }

        VPathObjPtr extract_acc_or_oid( VFSManager * mgr ) const {
            VPath * p;
            rc_t rc = VFSManagerExtractAccessionOrOID( mgr, &p, f_path );
            if ( 0 == rc ) {
                return VPathObjPtr( new VPathObj( p ) );
            }
            return nullptr;
        }

        std::string to_string( void ) const {
            std::string res;
            const String * S;
            rc_t rc = VPathMakeString( f_path, &S );
            if ( 0 == rc ) {
                res = std::string( S -> addr, S -> len );
                StringWhack( S );
            }
            return res;
        }

        ~VPathObj() { VPathRelease( f_path ); }
};

class VFSMgr;
typedef std::shared_ptr<VFSMgr> VFSMgrPtr;
class VFSMgr {
    private :
        VFSManager * f_mgr;

        VFSMgr ( void ) { VFSManagerMake( &f_mgr ); }

        std::string extract_acc_or_oid( const std::string& acc ) {
            std::string res;
            auto input_path = VPathObj::make( f_mgr, acc );
            if ( nullptr != input_path ) {
                auto output_path = input_path -> extract_acc_or_oid( f_mgr );
                if ( nullptr != output_path ) {
                    res = output_path -> to_string();
                }
            }
            return res;
        }

    public :
        static VFSMgrPtr make( void ) { return VFSMgrPtr( new VFSMgr ); }

        static std::string extract_acc( const std::string& acc ) {
            return make() -> extract_acc_or_oid( acc );
        }

        ~VFSMgr( ) { VFSManagerRelease( f_mgr ); }
};

}; // end of namesapce vdb
