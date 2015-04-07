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

#ifndef _h_general_writer_
#define _h_general_writer_

#include <stdint.h>

enum gw_evt_id
{
    evt_bad_event,
    evt_end_stream,
    evt_new_table,
    evt_new_column,
    evt_open_stream,
    evt_cell_default, 
    evt_cell_data, 
    evt_next_row,
    evt_errmsg
};

#define GW_SIGNATURE "NCBIgnld"
#define GW_GOOD_ENDIAN 1
#define GW_REVERSE_ENDIAN ( 1 << 24 )
#define GW_CURRENT_VERSION 1

struct gw_header
{
    char signature [ 8 ];      /* 8 characters to identify file type */
    uint32_t endian;           /* an internally known pattern to identify endian */
    uint32_t version;          /* a single-integer version number */
    
    uint32_t remote_db_name_size;
    uint32_t schema_file_name_size;
    uint32_t schema_db_spec_size;
    /* string data follows: 3 strings plus 1 NUL byte for each, 4-byte aligned
     uint32_t data [ ( ( remote_db_name_size + schema_file_name_size + schema_spec_size + 3 ) + 3 ) / 4 ]; */
};

/* ugly define to simulate inheritance in C */
#define GW_EVT_HDR_MBR uint32_t id_evt;
struct gw_evt_hdr
{
#ifdef __cplusplus
    inline uint32_t id () const
    { return id_evt & 0xffffff; }

    inline gw_evt_id evt ()
    { return ( gw_evt_id ) ( id_evt >> 24 ); }

    gw_evt_hdr ( uint32_t id, gw_evt_id evt )
    : id_evt ( ( ( uint32_t ) evt << 24 ) | ( id & 0xffffff ) ) {} 

    gw_evt_hdr ( const gw_evt_hdr & hdr )
    : id_evt ( hdr . id_evt ) {}
#endif

    GW_EVT_HDR_MBR
};

/* more defines to create inheritance in C++,
   and simulate it in C structures. */
#ifdef __cplusplus
#define GW_EVT_HDR_PARENT : gw_evt_hdr
#undef GW_EVT_HDR_MBR
#define GW_EVT_HDR_MBR
#else
#define GW_EVT_HDR_PARENT
#endif

struct gw_table_hdr GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    gw_table_hdr ( uint32_t id, gw_evt_id evt )
        : gw_evt_hdr ( id, evt ) {}

    gw_table_hdr ( const gw_evt_hdr & hdr )
        : gw_evt_hdr ( hdr ) {}
#endif
    GW_EVT_HDR_MBR
    uint32_t table_name_size;
    /* uint32_t data [ ( ( table_name_size + 1 ) + 3 ) / 4 ]; */
};

struct gw_column_hdr GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    gw_column_hdr ( uint32_t id, gw_evt_id evt )
        : gw_evt_hdr ( id, evt ) {}

    gw_column_hdr ( const gw_evt_hdr & hdr )
        : gw_evt_hdr ( hdr ) {}
#endif
    GW_EVT_HDR_MBR
    uint32_t table_id;
    uint32_t column_name_size;
    /* uint32_t data [ ( ( column_name_size + 1 ) + 3 ) / 4 ]; */
};

struct gw_cell_hdr GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    gw_cell_hdr ( uint32_t id, gw_evt_id evt )
        : gw_evt_hdr ( id, evt ) {}

    gw_cell_hdr ( const gw_evt_hdr & hdr )
        : gw_evt_hdr ( hdr ) {}
#endif
    GW_EVT_HDR_MBR
    uint32_t elem_bits;
    uint32_t elem_count;
    /* uint32_t data [ ( elem_bits * elem_count + 31 ) / 32 ]; */
};

struct gw_errmsg_hdr GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    gw_errmsg_hdr ( uint32_t id, gw_evt_id evt )
        : gw_evt_hdr ( id, evt ) {}

    gw_errmsg_hdr ( const gw_evt_hdr & hdr )
        : gw_evt_hdr ( hdr ) {}
#endif
    GW_EVT_HDR_MBR
    uint32_t msg_size;
    /* uint32_t data [ ( ( msg_size + 1 ) + 3 ) / 4 ]; */
};

#endif /*_h_general_writer_*/
