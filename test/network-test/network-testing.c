/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include <klib/out.h>
#include <klib/rc.h>
#include <klib/printf.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/data-buffer.h>
#include <klib/time.h>

#include <kns/manager.h>
#include <kns/kns-mgr-priv.h> /* KNSManagerMakeReliableHttpFile */
#include <kns/endpoint.h>
#include <kns/http.h>
#include <kns/stream.h>

#include <kfs/file.h>

#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/resolver.h>

#include <os-native.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


static rc_t ipv4_endpoint_to_string( char * buffer, size_t buflen, KEndPoint * ep )
{
	uint32_t b[4];
	b[0] = ( ep->u.ipv4.addr & 0xFF000000 ) >> 24;
	b[1] = ( ep->u.ipv4.addr & 0xFF0000 ) >> 16;
	b[2] = ( ep->u.ipv4.addr & 0xFF00 ) >> 8;
	b[3] = ( ep->u.ipv4.addr & 0xFF );
	return string_printf( buffer, buflen, NULL, "ipv4: %d.%d.%d.%d : %d",
						   b[0], b[1], b[2], b[3], ep->u.ipv4.port );
}

static rc_t ipv6_endpoint_to_string( char * buffer, size_t buflen, KEndPoint * ep )
{
	uint32_t b[8];
	b[0] = ( ep->u.ipv6.addr[ 0  ] << 8 ) | ep->u.ipv6.addr[ 1  ];
	b[1] = ( ep->u.ipv6.addr[ 2  ] << 8 ) | ep->u.ipv6.addr[ 3  ];
	b[2] = ( ep->u.ipv6.addr[ 4  ] << 8 ) | ep->u.ipv6.addr[ 5  ];
	b[3] = ( ep->u.ipv6.addr[ 6  ] << 8 ) | ep->u.ipv6.addr[ 7  ];
	b[4] = ( ep->u.ipv6.addr[ 8  ] << 8 ) | ep->u.ipv6.addr[ 9  ];
	b[5] = ( ep->u.ipv6.addr[ 10 ] << 8 ) | ep->u.ipv6.addr[ 11 ];
	b[6] = ( ep->u.ipv6.addr[ 12 ] << 8 ) | ep->u.ipv6.addr[ 13 ];
	b[7] = ( ep->u.ipv6.addr[ 14 ] << 8 ) | ep->u.ipv6.addr[ 15 ];
	return string_printf( buffer, buflen, NULL, "ipv6: %.04X:%.04X:%.04X:%.04X:%.04X:%.04X:%.04X:%.04X: :%d", 
						   b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], ep->u.ipv6.port );
}

static rc_t ipc_endpoint_to_string( char * buffer, size_t buflen, KEndPoint * ep )
{
	return string_printf( buffer, buflen, NULL, "ipc: %s", ep->u.ipc_name );
}

static rc_t endpoint_to_string( char * buffer, size_t buflen, KEndPoint * ep )
{
	rc_t rc;
	switch( ep->type )
	{
		case epIPV4 : rc = ipv4_endpoint_to_string( buffer, buflen, ep ); break;
		case epIPV6 : rc = ipv6_endpoint_to_string( buffer, buflen, ep ); break;
		case epIPC  : rc = ipc_endpoint_to_string( buffer, buflen, ep ); break;
		default     : rc = string_printf( buffer, buflen, NULL, "unknown endpoint-tyep %d", ep->type ); break;
	}
	return rc;
}

static rc_t perfrom_dns_test( KNSManager const * kns_mgr, const char * domain, uint16_t port )
{
	rc_t rc;
	KEndPoint ep;
	String s_domain;

	KTimeMs_t start_time = KTimeMsStamp();
	StringInitCString( &s_domain, domain );
	rc = KNSManagerInitDNSEndpoint( kns_mgr, &ep, &s_domain, port );
	if ( rc != 0 )
		pLogErr( klogErr, rc, "cannot init endpoint for $(URL):$(PORT)", "URL=%s,PORT=%d", domain, port );
	else
	{
		char s_endpoint[ 1024 ];
		rc = endpoint_to_string( s_endpoint, sizeof s_endpoint, &ep );
		KOutMsg( "\nendpoint for %s:%d is: '%s'\n", domain, port, s_endpoint );
	}
	KOutMsg( "in %d milliseconds\n", KTimeMsStamp() - start_time );
	
	return rc;
}


static rc_t read_stream_into_databuffer( KStream * stream, KDataBuffer * databuffer )
{
	rc_t rc;
	
	size_t total = 0;
	KDataBufferMakeBytes( databuffer, 4096 );
	while ( 1 )
	{
		size_t num_read;
		uint8_t * base;
		uint64_t avail = databuffer->elem_count - total;
		if ( avail < 256 )
		{
			rc = KDataBufferResize( databuffer, databuffer->elem_count + 4096 );
			if ( rc != 0 )
			{
				LogErr( klogErr, rc, "CGI: KDataBufferResize failed" );
				break;
			}
		}
		
		base = databuffer->base;
		rc = KStreamRead( stream, & base [ total ], ( size_t ) databuffer->elem_count - total, &num_read );
		if ( rc != 0 )
		{
			/* TBD - look more closely at rc */
			if ( num_read > 0 )
			{
				LogErr( klogErr, rc, "CGI: KStreamRead failed" );
				rc = 0;
			}
			else
				break;
		}

		if ( num_read == 0 )
			break;

		total += num_read;
	}

	if ( rc == 0 )
		databuffer->elem_count = total;
	return rc;
}


static rc_t call_cgi( KNSManager const * kns_mgr, const char * cgi_url, uint32_t ver_major, uint32_t ver_minor,
			   const char * protocol, const char * acc, KDataBuffer * databuffer )
{
	KHttpRequest * req;
	rc_t rc = KNSManagerMakeReliableClientRequest( kns_mgr, &req, 0x01000000, NULL, cgi_url );
	if ( rc != 0 )
		pLogErr( klogErr, rc, "CGI: cannot make ReliableClientRequest $(URL)", "URL=%s", cgi_url );
	else
	{
        rc = KHttpRequestAddPostParam( req, "version=%u.%u", ver_major, ver_minor );
		if ( rc != 0 )
			pLogErr( klogErr, rc, "CGI: KHttpRequestAddPostParam version=$(V1).$(V2) failed", "V1=%d,V2=%d", ver_major, ver_minor );
		
		if ( rc == 0 )
		{
			rc = KHttpRequestAddPostParam( req, "acc=%s", acc );
			if ( rc != 0 )
				pLogErr( klogErr, rc, "CGI: KHttpRequestAddPostParam acc=$(ACC) failed", "ACC=%s", acc );
		}
		
		if ( rc == 0 )
		{
			rc = KHttpRequestAddPostParam ( req, "accept-proto=%s", protocol );
			if ( rc != 0 )
				pLogErr( klogErr, rc, "CGI: KHttpRequestAddPostParam accept-proto=$(PROTO) failed", "PROTO=%s", protocol );
		}

        if ( rc == 0 )
        {
            KHttpResult *rslt;
            rc = KHttpRequestPOST( req, &rslt );
			if ( rc != 0 )
				LogErr( klogErr, rc, "CGI: KHttpRequestPOST failed" );
			else
			{
                uint32_t code;
                rc = KHttpResultStatus ( rslt, &code, NULL, 0, NULL );
				if ( rc != 0 )
					LogErr( klogErr, rc, "CGI: KHttpResultStatus failed" );
				else
				{
					if ( code != 200 )
						pLogErr( klogErr, rc, "CGI: unexpected result-code of $(RESCODE)", "RESCODE=%d", code );
					else
					{
						KStream *response;
						
						rc = KHttpResultGetInputStream ( rslt, &response );
						if ( rc != 0 )
							LogErr( klogErr, rc, "CGI: KHttpResultGetInputStream failed" );
						else
						{
							rc = read_stream_into_databuffer( response, databuffer );
							KStreamRelease ( response );
						}
					}
				}
				KHttpResultRelease ( rslt );
			}
		}
		KHttpRequestRelease ( req );
	}
	return rc;
}


static rc_t perform_cgi_test( KNSManager const * kns_mgr, const char * acc )
{
	rc_t rc;
	KDataBuffer databuffer;
	KTimeMs_t start_time = KTimeMsStamp();

	memset( &databuffer, 0, sizeof databuffer );
	rc = call_cgi( kns_mgr, "https://www.ncbi.nlm.nih.gov/Traces/names/names.fcgi", 1, 1, "http,https", acc, &databuffer );
	if ( rc == 0 )
	{
	    const char *start = ( const void* ) databuffer.base;
		size_t size = KDataBufferBytes( &databuffer );
		KOutMsg( "\nCGI: Response = %.*s\n", size, start );
	}
	KOutMsg( "in %d milliseconds\n\n", KTimeMsStamp() - start_time );
	
	return rc;
}


static rc_t print_vpath( const char * prefix, const VPath * vpath, char * buffer, size_t buflen )
{
	const String * s_path;
	rc_t rc = VPathMakeString( vpath, &s_path );
	if ( rc != 0 )
		pLogErr( klogErr, rc, "VPathMakeString() for $(PREFIX) failed", "PREFIX=%s", prefix );
	else
	{
		KOutMsg( "resolved (%s) : %S\n", prefix, s_path );
		if ( buffer != NULL && buflen > 0 )
			string_printf( buffer, buflen, NULL, "%S", s_path );
		StringWhack( s_path );
	}
	return rc;
}


static rc_t perform_resolve_test( const char * acc, char * buffer, size_t buflen )
{
    VFSManager * vfs_mgr;
    rc_t rc = VFSManagerMake( &vfs_mgr );
	if ( rc != 0 )
		LogErr( klogErr, rc, "VFSManagerMake failed" );
    else
    {
        VResolver * resolver;
        rc = VFSManagerGetResolver( vfs_mgr, &resolver );
		if ( rc != 0 )
			LogErr( klogErr, rc, "VFSManagerGetResolver failed" );
        else
        {
            VPath * query_path;
            rc = VFSManagerMakePath( vfs_mgr, &query_path, "ncbi-acc:%s", acc );
			if ( rc != 0 )
				LogErr( klogErr, rc, "VFSManagerMakePath failed" );
            else
            {
                const VPath * local = NULL;
                const VPath * remote = NULL;
				const VPath * cache = NULL;
				
				KTimeMs_t start_time = KTimeMsStamp();
				
				rc = VResolverQuery( resolver, 0, query_path, &local, NULL, NULL );
				if ( rc != 0 )
					LogErr( klogErr, rc, "VResolverQuery (local) failed" );
                else
                {
					rc = print_vpath( "local ", local, NULL, 0 );
					VPathRelease ( local );
                }
				rc = VResolverQuery( resolver, 0, query_path, NULL, &remote, &cache );
				if ( rc != 0 )
					LogErr( klogErr, rc, "VResolverQuery (remote/cache) failed" );
                else
				{
					if ( remote != NULL )
					{
						rc = print_vpath( "remote", remote, buffer, buflen );
						VPathRelease ( remote );
					}
					if ( rc == 0 && cache != NULL )
					{
						rc = print_vpath( "cache ", cache, NULL, 0 );
						VPathRelease ( cache );
					}
				}
		
				KOutMsg( "in %d milliseconds\n", KTimeMsStamp() - start_time );
                VPathRelease ( query_path );
            }
            VResolverRelease( resolver );
        }
        VFSManagerRelease ( vfs_mgr );
    }
	return rc;
}


static rc_t read_blocks( const KFile * remote_file, uint32_t block_count, uint64_t * total )
{
	rc_t rc = 0;
	size_t block_size = 1024 * 1024 * 32;
	
	*total = 0;
	uint8_t * block = malloc( block_size );
	if ( block == NULL )
	{
		rc = RC( rcExe, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
		LogErr( klogErr, rc, "allocation of block failed" );
	}
	else
	{
		uint32_t i;
		uint64_t pos = 0;
		for ( i = 0; rc == 0 && i < block_count; ++i )
		{
			size_t num_read;
			rc = KFileReadAll( remote_file, pos, block, block_size, &num_read );
			if ( rc != 0 )
				pLogErr( klogErr, rc, "KFileReadAll() at '$(POS)' failed", "POS=%ld", pos );
			else
				pos += num_read;
		}
		*total = pos;
		free( (void *) block );
	}
	return rc;
}

static rc_t perform_read_test( KNSManager const * kns_mgr, const char * remote_path )
{
	
	const KFile * remote_file;
	KTimeMs_t start_time = KTimeMsStamp();
	rc_t rc = KNSManagerMakeHttpFile ( kns_mgr, &remote_file, NULL, 0x0101, "%s", remote_path );
	if ( rc != 0 )
		pLogErr( klogErr, rc, "KNSManagerMakeHttpFile() '$(FILENAME)' failed", "FILENAME=%s", remote_path );
	else
	{
		uint64_t total;
		KOutMsg( "\n'%s' opened\nin %d milliseconds\n", remote_path, KTimeMsStamp() - start_time );
		start_time = KTimeMsStamp();
		rc = read_blocks( remote_file, 10, &total );
		KOutMsg( "total of %ld bytes read in %d milliseconds\n\n", total, KTimeMsStamp() - start_time );
		KFileRelease( remote_file );
	}
	return rc;
}

rc_t perfrom_network_test( const char * acc )
{
	KNSManager * kns_mgr;
	rc_t rc =  KNSManagerMake( &kns_mgr );
	if ( rc != 0 )
		LogErr( klogErr, rc, "cannot make KNS-Manager" );
	else
	{
		if ( KNSManagerGetHTTPProxyEnabled( kns_mgr ) )
		{
			KOutMsg( "\nKNSManager: proxy enabled\n" );
			{
				const String * proxy;
				rc = KNSManagerGetHTTPProxyPath( kns_mgr, &proxy );
				if ( rc != 0 )
					LogErr( klogErr, rc, "cannot request proxy-path from KNS-Manager" );
				else
					KOutMsg( "KNSManager: proxy at : %S\n", proxy );
			}
		}
		else
			KOutMsg( "\nKNSManager: proxy disabled\n" );
			
		if ( rc == 0 )
		{
			const char * user_agent;
			rc = KNSManagerGetUserAgent( &user_agent );
			if ( rc != 0 )
				LogErr( klogErr, rc, "cannot request user-agent from KNS-Manager" );
			else
				KOutMsg( "KNSManager: user-agent = '%s'\n", user_agent );
		}

		if ( rc == 0 )
			rc = perfrom_dns_test( kns_mgr, "www.ncbi.nlm.nih.gov", 80 );

		if ( rc == 0 )
			rc = perform_cgi_test( kns_mgr, acc );

		if ( rc == 0 )
		{
			char remote_path[ 4096 ];
			remote_path[ 0 ] = 0;
			rc = perform_resolve_test( acc, remote_path, sizeof remote_path );
			if ( rc == 0 && remote_path[ 0 ] != 0 )
				rc = perform_read_test( kns_mgr, remote_path );
		}
			
		KNSManagerRelease( kns_mgr );
	}
	return rc;
}
