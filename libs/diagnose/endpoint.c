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

#include <diagnose/diagnose.h> /* endpoint_to_string */

#include <klib/printf.h> /* string_printf */
#include <kns/endpoint.h> /* KEndPoint */

static rc_t ipv4_endpoint_to_string(char *buffer, size_t buflen, KEndPoint *ep)
{
    uint32_t b[4];
    b[0] = ( ep->u.ipv4.addr & 0xFF000000 ) >> 24;
    b[1] = ( ep->u.ipv4.addr & 0xFF0000 ) >> 16;
    b[2] = ( ep->u.ipv4.addr & 0xFF00 ) >> 8;
    b[3] = ( ep->u.ipv4.addr & 0xFF );
    return string_printf( buffer, buflen, NULL, "ipv4: %d.%d.%d.%d : %d",
                           b[0], b[1], b[2], b[3], ep->u.ipv4.port );
}

static rc_t ipv6_endpoint_to_string(char *buffer, size_t buflen, KEndPoint *ep)
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
    return string_printf( buffer, buflen, NULL,
        "ipv6: %.04X:%.04X:%.04X:%.04X:%.04X:%.04X:%.04X:%.04X: :%d",
        b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], ep->u.ipv6.port );
}

static rc_t ipc_endpoint_to_string(char *buffer, size_t buflen, KEndPoint *ep)
{
    return string_printf( buffer, buflen, NULL, "ipc: %s", ep->u.ipc_name );
}

rc_t endpoint_to_string( char * buffer, size_t buflen, struct KEndPoint * ep )
{
    rc_t rc;
    switch( ep->type )
    {
        case epIPV4 : rc = ipv4_endpoint_to_string( buffer, buflen, ep ); break;
        case epIPV6 : rc = ipv6_endpoint_to_string( buffer, buflen, ep ); break;
        case epIPC  : rc = ipc_endpoint_to_string( buffer, buflen, ep ); break;
        default     : rc = string_printf( buffer, buflen, NULL,
                          "unknown endpoint-type %d", ep->type ); break;
    }
    return rc;
}
