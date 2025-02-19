#pragma once

#include <cstdint>
#include <cstdarg>
#include <string>

#include <iostream>
#include <sstream>

namespace vdb {

extern "C" {

typedef uint32_t rc_t;
typedef uint32_t ver_t;

typedef struct String String;
struct String {
    const char *addr;
    std::size_t size;
    uint32_t len;
};

#define StringInit( s, val, sz, length ) \
    ( void ) \
        ( ( s ) -> addr = ( val ), \
          ( s ) -> size = ( sz ), \
          ( s ) -> len = ( length ) )

void StringWhack ( const String* self );

typedef struct KFile KFile;
typedef struct KDirectory KDirectory;
typedef struct VFSManager VFSManager;
typedef struct VPath VPath;
typedef struct KStream KStream;
typedef struct KNSManager KNSManager;
typedef struct KEndPoint KEndPoint;
typedef struct KClientHttp KClientHttp, KHttp;
typedef struct CloudMgr CloudMgr;
typedef struct Cloud Cloud;
typedef struct KClientHttpRequest KClientHttpRequest;
typedef struct KClientHttpResult KClientHttpResult;

rc_t KDirectoryNativeDir_v1( KDirectory **dir );
#define KDirectoryNativeDir KDirectoryNativeDir_v1

rc_t KDirectoryRelease_v1( const KDirectory *self );
#define KDirectoryRelease KDirectoryRelease_v1

rc_t KDirectoryOpenFileRead_v1( const KDirectory *self, KFile const **f, const char *path, ... );
#define KDirectoryOpenFileRead KDirectoryOpenFileRead_v1

rc_t KFileRelease_v1( const KFile *self );
#define KFileRelease KFileRelease_v1

rc_t KFileRead( const KFile *self, uint64_t pos, void *buffer, std::size_t bsize, std::size_t *num_read );

rc_t VFSManagerMake( VFSManager ** pmanager );
rc_t VFSManagerRelease ( const VFSManager *self );

rc_t VFSManagerOpenFileRead( const VFSManager *self,
                             struct KFile const **f,
                             const struct VPath * path );

rc_t VFSManagerMakePath( struct VFSManager const * self,
                         VPath ** new_path,
                         const char * path_str, ... );

rc_t VPathReadUri ( const VPath * self, char * buffer, size_t buffer_size, size_t * num_read );
rc_t VPathGetPayRequired( const VPath * self, bool * required );
rc_t VPathGetCeRequired( const VPath * self, bool * required );

rc_t VPathRelease( const VPath *self );

rc_t KStreamRelease ( const KStream *self );
rc_t KStreamRead ( const KStream *self, void *buffer, size_t bsize, size_t *num_read );

rc_t KNSManagerMake( KNSManager **mgr );
rc_t KNSManagerRelease( const KNSManager *self );

rc_t CloudMgrMake( CloudMgr ** mgr, struct KConfig const * kfg, struct KNSManager const * kns );
rc_t CloudMgrGetCurrentCloud( const CloudMgr * self, Cloud ** cloud );
rc_t CloudMgrRelease( const CloudMgr * self );
rc_t CloudMakeComputeEnvironmentToken( const Cloud * self, struct String const ** ce_token );
rc_t CloudRelease( const Cloud * self );

rc_t KNSManagerMakeReliableClientRequest ( struct KNSManager const *self,
    struct KClientHttpRequest **req,
    ver_t version, struct KStream *conn, const char *url, ... );

rc_t KNSManagerMakeClientRequest( struct KNSManager const *self,
    KClientHttpRequest **req, ver_t version, struct KStream *conn, const char *url,
    ... );

rc_t KClientHttpRequestSetCloudParams( KClientHttpRequest * self, bool ceRequired, bool payRequired );
rc_t KClientHttpRequestGET( KClientHttpRequest *self, KClientHttpResult **rslt );
rc_t KClientHttpResultStatus( const KClientHttpResult *self, uint32_t *code,
                            char *msg_buff, size_t buff_size, size_t *msg_size );
rc_t KClientHttpResultGetInputStream( KClientHttpResult *self, struct KStream** s );
rc_t KClientHttpResultRelease ( const KClientHttpResult *self );

rc_t KClientHttpRequestRelease ( const KClientHttpRequest *self );

rc_t string_printf( char *dst, size_t bsize, size_t *num_writ, const char *fmt, ... );

} // end of extern "C"

class MsgFactory {
    public :
        static std::string rc_msg( const std::string& msg, rc_t rc ) {
            std::stringstream ss;
            ss << msg;

            char buffer[ 4096 ];
            size_t written;
            rc_t rc1 = string_printf( buffer, sizeof buffer, &written, "%R", rc );
            if ( 0 == rc1 && written > 0 ) {
                buffer[ written ] = 0;
                ss << buffer;
            }
            return ss . str();
        }

        static std::string code_msg( const std::string& msg, uint32_t code ) {
            std::stringstream ss;
            ss << msg << code;
            return ss . str();
        }

        static void throw_msg( const std::string& msg, rc_t rc ) {
            throw std::runtime_error( rc_msg( msg, rc ) );
        }
};

class KFileFactory {
    public:
        // takes a filesystem-path and returns a vdb::KFile or nullptr on error
        static const KFile* make_from_path( const std::string& path ) {
            KDirectory * dir = nullptr;
            rc_t rc = KDirectoryNativeDir( &dir );
            if ( 0 != rc ) {
                MsgFactory::throw_msg( "KDirectoryNativeDir() failed : ", rc );
            } else {
                const KFile * res = nullptr;
                rc = KDirectoryOpenFileRead( dir, &res, path . c_str() );
                if ( 0 != rc ) {
                    MsgFactory::throw_msg( "KDirectoryOpenFileRead() failed : ", rc );
                }
                KDirectoryRelease( dir );
                if ( 0 == rc ) {
                    return res;
                }
            }
            return nullptr;
        }

        // takes a url and returns a vdb::KFile or nullptr on error
        // the length of the url-resource has to be known ( does not work for dynamic content )
        static const KFile* make_from_vpath( const std::string& path ) {
            const KFile * res = nullptr;
            VFSManager * mgr;
            rc_t rc = VFSManagerMake( &mgr );
            if ( 0 != rc ) {
                MsgFactory::throw_msg( "VFSManagerMake() failed : ", rc );
            } else {
                VPath * vpath;
                rc = VFSManagerMakePath( mgr, &vpath, "%s", path . c_str() );
                if ( 0 != rc ) {
                    MsgFactory::throw_msg( "VFSManagerMakePath() failed : ", rc );
                } else {
                    rc = VFSManagerOpenFileRead( mgr, &res, vpath );
                    if ( 0 != rc ) {
                        MsgFactory::throw_msg( "VFSManagerOpenFileRead() failed : ", rc );
                    }
                    VPathRelease( vpath );
                }
                VFSManagerRelease( mgr );
            }
            return res;
        }
};

class KStreamFactory {
    private :
        static const VPath * uri_to_vpath( const std::string& uri ) {
            VPath * res = nullptr;
            VFSManager * mgr;
            rc_t rc = VFSManagerMake( &mgr );
            if ( 0 != rc ) {
                MsgFactory::throw_msg( "VFSManagerMake() failed : ", rc );
            } else {
                rc = VFSManagerMakePath( mgr, &res, "%s", uri . c_str() );
                if ( 0 != rc ) {
                    MsgFactory::throw_msg( "VFSManagerMakePath() failed : ", rc );
                }
            }
            return res;
        }

        static std::string VPathUri( const VPath * vpath ) {
            char buffer[ 4096 ];
            size_t num_read;
            rc_t rc = VPathReadUri( vpath, buffer, sizeof buffer, &num_read );
            if ( 0 != rc ) {
                MsgFactory::throw_msg( "VPathReadUri() failed : ", rc );
            } else {
                return std::string( buffer, num_read );
            }
            return std::string();
        }

        static bool cloud_env_req( const VPath * vpath ) {
            bool res = false;
            rc_t rc = VPathGetCeRequired( vpath, &res );
            if ( 0 != rc ) {
                MsgFactory::throw_msg( "VPathGetCeRequired() failed : ", rc );
            }
            return res;
        }

        static bool pay_req( const VPath * vpath ) {
            bool res = false;
            rc_t rc = VPathGetPayRequired( vpath, &res );
            if ( 0 != rc ) {
                MsgFactory::throw_msg( "VPathGetPayRequired() failed : ", rc );
            }
            return res;
        }

        static const String* get_cloud_env_token( bool cloud_env_required ) {
            const String* res = nullptr;
            if ( cloud_env_required ) {
                CloudMgr * cmgr = NULL;

                rc_t rc = CloudMgrMake( &cmgr, NULL, NULL );
                if ( 0 != rc ) {
                    MsgFactory::throw_msg( "CloudMgrMake() failed : ", rc );
                } else {
                    Cloud * cloud = NULL;
                    rc = CloudMgrGetCurrentCloud( cmgr, &cloud );
                    if ( rc != 0 ) {
                        MsgFactory::throw_msg( "CloudMgrGetCurrentCloud() failed : ", rc );
                    } else {
                        rc = CloudMakeComputeEnvironmentToken( cloud, &res );
                        if ( rc != 0 ) {
                            MsgFactory::throw_msg( "CloudMakeComputeEnvironmentToken() failed : ", rc );
                        }
                    }
                    CloudRelease( cloud );
                }
                CloudMgrRelease( cmgr );
            }
            return res;
        }

        static KClientHttpRequest * make_request( KNSManager * mgr, const VPath * vpath, bool reliable ) {
            KClientHttpRequest * req = nullptr;

            bool cloud_env_required = cloud_env_req( vpath );
            const String * cloud_env_token = get_cloud_env_token( cloud_env_required );
            const std::string vp_uri = VPathUri( vpath );
            String S_uri;
            StringInit( &S_uri, vp_uri.c_str(), vp_uri.length(), (uint32_t)vp_uri.length() );
            ver_t vers = 0x01010000; // HTTP 1.1

            rc_t rc = 0;
            if ( reliable ) {
                if ( cloud_env_required && cloud_env_token != NULL) {
                    rc = KNSManagerMakeReliableClientRequest( mgr, &req, vers, NULL,
                                    "%S&ident=%S", &S_uri, cloud_env_token );
                } else {
                    rc = KNSManagerMakeReliableClientRequest( mgr, &req, vers, NULL, "%S", &S_uri );
                }
                if ( 0 != rc ) {
                    MsgFactory::throw_msg( "KNSManagerMakeReliableClientRequest() failed : ", rc );
                }
            } else {
                if ( cloud_env_required && cloud_env_token != NULL) {
                    rc = KNSManagerMakeClientRequest( mgr, &req, vers, NULL,
                                    "%S&ident=%S", &S_uri, cloud_env_token );
                } else {
                    rc = KNSManagerMakeClientRequest( mgr, &req, vers, NULL, "%S", &S_uri );
                }
                if ( 0 != rc ) {
                    MsgFactory::throw_msg( "KNSManagerMakeClientRequest() failed : ", rc );
                }
            }
            if ( 0 != rc ) {
                req = nullptr;
            } else {
                bool pay_required = pay_req( vpath );
                rc = KClientHttpRequestSetCloudParams( req, cloud_env_required, pay_required );
                if ( 0 != rc ) {
                    MsgFactory::throw_msg( "KClientHttpRequestSetCloudParams() failed : ", rc );
                }
            }
            StringWhack( cloud_env_token );
            return req;
        }

    public:
        // takes a url and returns a vdb::KStream or nullptr on error
        static const KStream* make_from_uri( const std::string& uri, bool reliable = false ) {
            KStream * res = nullptr;
            KNSManager * mgr;
            rc_t rc = KNSManagerMake( &mgr );
            if ( 0 != rc ) {
                MsgFactory::throw_msg( "KNSManagerMake() failed : ", rc );
            } else {
                auto vpath = uri_to_vpath( uri );
                if ( nullptr == vpath ) {
                    throw std::runtime_error( "failed to create VPath from URI" );
                } else {
                    auto req = make_request( mgr, vpath, reliable );
                    if ( nullptr == req ) {
                        throw std::runtime_error( "failed to create request" );
                    } else {
                        KClientHttpResult * rslt = NULL;
                        rc = KClientHttpRequestGET( req, &rslt );
                        if ( 0 != rc ) {
                            MsgFactory::throw_msg( "KClientHttpRequestGET() failed : ", rc );
                        } else {
                            uint32_t code = 0;
                            rc = KClientHttpResultStatus( rslt, &code, NULL, 0, NULL );
                            if ( rc != 0 ) {
                                MsgFactory::throw_msg( "KClientHttpResultStatus() failed : ", rc );
                            } else if ( 200 != code ) {
                                throw std::runtime_error( MsgFactory::code_msg( "HTTP-returncode = ", code ) );
                            } else {
                                rc = KClientHttpResultGetInputStream( rslt, &res );
                                if ( rc != 0 ) {
                                    MsgFactory::throw_msg( "KClientHttpResultGetInputStream() failed", rc );
                                }
                            }
                            KClientHttpResultRelease( rslt );
                        }
                        KClientHttpRequestRelease( req );
                    }
                    VPathRelease( vpath );
                }
                KNSManagerRelease( mgr );
            }
            return res;
        }
};

}; // end of namespace vdb
