#pragma once

#include "VdbObj.hpp"
#include "VdbString.hpp"

namespace vdb {

extern "C" {

typedef uint32_t VRemoteProtocols;
enum {
    /* version 1.1 protocols */
      eProtocolNone  = 0
    , eProtocolDefault = eProtocolNone
    , eProtocolHttp  = 1
    , eProtocolFasp  = 2

      /* version 1.2 protocols */
    , eProtocolHttps = 3

      /* version 3.0 protocols */
    , eProtocolFile  = 4
    , eProtocolS3    = 5 /* Amazon Simple Storage Service */
    , eProtocolGS    = 6 /* Google Cloud Storage */

      /* value 7 are available for future */

    , eProtocolLast
    , eProtocolMax   = eProtocolLast - 1
    , eProtocolMask  = 7

    , eProtocolMaxPref = 6

      /* macros for building multi-protocol constants
         ordered by preference from least to most significant bits */
#define VRemoteProtocolsMake2( p1, p2 )                                     \
      ( ( ( VRemoteProtocols ) ( p1 ) & eProtocolMask ) |                   \
        ( ( ( VRemoteProtocols ) ( p2 ) & eProtocolMask ) << ( 3 * 1 ) ) )

#define VRemoteProtocolsMake3( p1, p2, p3 )                                 \
      ( VRemoteProtocolsMake2 ( p1, p2 ) |                                  \
        ( ( ( VRemoteProtocols ) ( p3 ) & eProtocolMask ) << ( 3 * 2 ) ) )

#define VRemoteProtocolsMake4( p1, p2, p3, p4 )                                 \
      ( VRemoteProtocolsMake3 ( p1, p2, p3 ) |                                  \
        ( ( ( VRemoteProtocols ) ( p4 ) & eProtocolMask ) << ( 3 * 3 ) ) )

    , eProtocolFaspHttp         = VRemoteProtocolsMake2 ( eProtocolFasp,  eProtocolHttp  )
    , eProtocolHttpFasp         = VRemoteProtocolsMake2 ( eProtocolHttp,  eProtocolFasp  )
    , eProtocolHttpsHttp        = VRemoteProtocolsMake2 ( eProtocolHttps, eProtocolHttp  )
    , eProtocolHttpHttps        = VRemoteProtocolsMake2 ( eProtocolHttp,  eProtocolHttps )
    , eProtocolFaspHttps        = VRemoteProtocolsMake2 ( eProtocolFasp,  eProtocolHttps )
    , eProtocolHttpsFasp        = VRemoteProtocolsMake2 ( eProtocolHttps, eProtocolFasp  )
    , eProtocolFaspHttpHttps    = VRemoteProtocolsMake3 ( eProtocolFasp,  eProtocolHttp,  eProtocolHttps )
    , eProtocolHttpFaspHttps    = VRemoteProtocolsMake3 ( eProtocolHttp,  eProtocolFasp,  eProtocolHttps )
    , eProtocolFaspHttpsHttp    = VRemoteProtocolsMake3 ( eProtocolFasp,  eProtocolHttps, eProtocolHttp  )
    , eProtocolHttpHttpsFasp    = VRemoteProtocolsMake3 ( eProtocolHttp,  eProtocolHttps, eProtocolFasp  )
    , eProtocolHttpsFaspHttp    = VRemoteProtocolsMake3 ( eProtocolHttps, eProtocolFasp,  eProtocolHttp  )
    , eProtocolHttpsHttpFasp    = VRemoteProtocolsMake3 ( eProtocolHttps, eProtocolHttp,  eProtocolFasp  )
    , eProtocolFileFaspHttpHttps= VRemoteProtocolsMake4 ( eProtocolFile,  eProtocolFasp,  eProtocolHttp, eProtocolHttps  )
};

rc_t VResolverAddRef( const VResolver * self );
rc_t VResolverRelease( const VResolver * self );

VRemoteProtocols VRemoteProtocolsParse( String const * protos );
rc_t VResolverQuery( const VResolver * self, VRemoteProtocols protocols, VPath const * query,
		VPath const ** local, VPath const ** remote, VPath const ** cache );

// DEPRECATED :
rc_t VResolverLocal( const VResolver * self, VPath const * accession, VPath const ** path );
rc_t VResolverRemote( const VResolver * self, VRemoteProtocols protocols, VPath const * accession,
		VPath const ** path );
rc_t VResolverCache( const VResolver * self, VPath const * url, VPath const ** path, uint64_t file_size );

typedef uint32_t VResolverEnableState;
enum { vrUseConfig = 0, vrAlwaysEnable = 1, vrAlwaysDisable = 2 };
VResolverEnableState VResolverLocalEnable( const VResolver * self, VResolverEnableState enable );
VResolverEnableState VResolverRemoteEnable( const VResolver * self, VResolverEnableState enable );
VResolverEnableState VResolverCacheEnable( const VResolver * self, VResolverEnableState enable );

}; // end of extern "C"

}; // end of namesapce vdb
