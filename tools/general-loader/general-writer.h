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
#include <stddef.h>

enum gw_evt_id
{
    evt_bad_event,

    /* version 1 events */
    evt_end_stream,
    evt_new_table,
    evt_new_column,
    evt_open_stream,
    evt_cell_default, 
    evt_cell_data, 
    evt_next_row,
    evt_errmsg,

    /* version 2 events */
    evt_remote_path,
    evt_use_schema,
    evt_cell2_default,
    evt_cell2_data
};

#define GW_SIGNATURE "NCBIgnld"
#define GW_GOOD_ENDIAN 1
#define GW_REVERSE_ENDIAN ( 1 << 24 )
#define GW_CURRENT_VERSION 1


/*----------------------------------------------------------------------
 * version 1
 */

#define GW_HDR_MBRS                                                            \
    char signature [ 8 ]; /* 8 characters to identify file type */             \
    uint32_t endian;      /* an internally known pattern to identify endian */ \
    uint32_t version;     /* a single-integer version number */
    
struct gw_header
{
#ifdef __cplusplus
    inline gw_header ( uint32_t vers = 0 );
    inline gw_header ( const gw_header & hdr );
#endif

    GW_HDR_MBRS
};

/* more defines to create inheritance in C++,
   and simulate it in C structures. */
#ifdef __cplusplus
#define GW_HDR_PARENT : gw_header
#undef GW_HDR_MBRS
#define GW_HDR_MBRS
#else
#define GW_HDR_PARENT
#endif

struct gw_header_v1 GW_HDR_PARENT
{
#ifdef __cplusplus
    inline gw_header_v1 ();
    inline gw_header_v1 ( const gw_header & hdr );
#endif

    GW_HDR_MBRS

    uint32_t remote_db_name_size;
    uint32_t schema_file_name_size;
    uint32_t schema_db_spec_size;
    /* string data follows: 3 strings plus 1 NUL byte for each, 4-byte aligned
     uint32_t data [ ( ( remote_db_name_size + schema_file_name_size + schema_spec_size + 3 ) + 3 ) / 4 ]; */
};

/* ugly define to simulate inheritance in C */
#define GW_EVT_HDR_MBR uint32_t id_evt;
struct gw_evt_hdr_v1
{
#ifdef __cplusplus
    inline uint32_t id () const;
    inline gw_evt_id evt () const;
    inline gw_evt_hdr_v1 ( uint32_t id, gw_evt_id evt );
    inline gw_evt_hdr_v1 ( const gw_evt_hdr_v1 & hdr );
#endif

    GW_EVT_HDR_MBR
};

/* more defines to create inheritance in C++,
   and simulate it in C structures. */
#ifdef __cplusplus
#define GW_EVT_HDR_PARENT : gw_evt_hdr_v1
#undef GW_EVT_HDR_MBR
#define GW_EVT_HDR_MBR
#else
#define GW_EVT_HDR_PARENT
#endif

struct gw_table_hdr_v1 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline gw_table_hdr_v1 ( uint32_t id, gw_evt_id evt );
    inline gw_table_hdr_v1 ( const gw_evt_hdr_v1 & hdr );
#endif
    GW_EVT_HDR_MBR
    uint32_t table_name_size;
    /* uint32_t data [ ( ( table_name_size + 1 ) + 3 ) / 4 ]; */
};

struct gw_column_hdr_v1 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline gw_column_hdr_v1 ( uint32_t id, gw_evt_id evt );
    inline gw_column_hdr_v1 ( const gw_evt_hdr_v1 & hdr );
#endif
    GW_EVT_HDR_MBR
    uint32_t table_id;
    uint32_t column_name_size;
    /* uint32_t data [ ( ( column_name_size + 1 ) + 3 ) / 4 ]; */
};

struct gw_cell_hdr_v1 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline gw_cell_hdr_v1 ( uint32_t id, gw_evt_id evt );
    inline gw_cell_hdr_v1 ( const gw_evt_hdr_v1 & hdr );
#endif
    GW_EVT_HDR_MBR
    uint32_t elem_bits;
    uint32_t elem_count;
    /* uint32_t data [ ( elem_bits * elem_count + 31 ) / 32 ]; */
};

struct gw_errmsg_hdr_v1 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline gw_errmsg_hdr_v1 ( uint32_t id, gw_evt_id evt );
    inline gw_errmsg_hdr_v1 ( const gw_evt_hdr_v1 & hdr );
#endif
    GW_EVT_HDR_MBR
    uint32_t msg_size;
    /* uint32_t data [ ( ( msg_size + 1 ) + 3 ) / 4 ]; */
};

/*----------------------------------------------------------------------
 * version 2
 */

struct gw_header_v2 GW_HDR_PARENT
{
#ifdef __cplusplus
    inline gw_header_v2 ();
    inline gw_header_v2 ( const gw_header & hdr );
#endif

    GW_HDR_MBRS

    uint32_t hdr_size;         /* the size of the entire header, including alignment */
};

#undef GW_EVT_HDR_MBR
#define GW_EVT_HDR_MBR \
    uint8_t _evt;      \
    uint8_t _id;

struct gw_evt_hdr_v2
{
#ifdef __cplusplus
    inline uint32_t id () const;
    inline gw_evt_id evt () const;
    inline gw_evt_hdr_v2 ( uint32_t id, gw_evt_id evt );
    inline gw_evt_hdr_v2 ( const gw_evt_hdr_v2 & hdr );
#endif

    GW_EVT_HDR_MBR
};

/* more defines to create inheritance in C++,
   and simulate it in C structures. */
#undef GW_EVT_HDR_PARENT
#ifdef __cplusplus
#define GW_EVT_HDR_PARENT : gw_evt_hdr_v2
#undef GW_EVT_HDR_MBR
#define GW_EVT_HDR_MBR
#else
#define GW_EVT_HDR_PARENT
#endif

struct gw_string_hdr_U8_v2 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline size_t size () const;
    inline gw_string_hdr_U8_v2 ( uint32_t id, gw_evt_id evt );
    inline gw_string_hdr_U8_v2 ( const gw_evt_hdr_v2 & hdr );
#endif

    GW_EVT_HDR_MBR
    uint8_t string_size;
    /* char data [ string_size + 1 ]; */
};

struct gw_string_hdr_U16_v2 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline size_t size () const;
    inline gw_string_hdr_U16_v2 ( uint32_t id, gw_evt_id evt );
    inline gw_string_hdr_U16_v2 ( const gw_evt_hdr_v2 & hdr );
#endif

    GW_EVT_HDR_MBR
    uint16_t string_size;
    /* char data [ string_size + 1 ]; */
};

struct gw_string2_hdr_U8_v2 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline size_t size1 () const;
    inline size_t size2 () const;
    inline gw_string2_hdr_U8_v2 ( uint32_t id, gw_evt_id evt );
    inline gw_string2_hdr_U8_v2 ( const gw_evt_hdr_v2 & hdr );
#endif

    GW_EVT_HDR_MBR
    uint8_t string1_size;
    uint8_t string2_size;
    /* char data [ string1_size + 1 + string2_size + 1 ]; */
};

struct gw_string2_hdr_U16_v2 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline size_t size1 () const;
    inline size_t size2 () const;
    inline gw_string2_hdr_U16_v2 ( uint32_t id, gw_evt_id evt );
    inline gw_string2_hdr_U16_v2 ( const gw_evt_hdr_v2 & hdr );
#endif

    GW_EVT_HDR_MBR
    uint16_t string1_size;
    uint16_t string2_size;
    /* char data [ string1_size + 1 + string2_size + 1 ]; */
};

struct gw_column_hdr_v2 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline uint32_t table () const;
    inline size_t size () const;
    inline gw_column_hdr_v2 ( uint32_t id, gw_evt_id evt );
    inline gw_column_hdr_v2 ( const gw_evt_hdr_v2 & hdr );
#endif

    GW_EVT_HDR_MBR
    uint8_t table_id;
    uint8_t elem_bits;
    uint8_t flag_bits;                  /* 1 = integer packing */
    uint8_t column_name_size;
};

struct gw_data_hdr_U8_v2 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline size_t size () const;
    inline gw_data_hdr_U8_v2 ( uint32_t id, gw_evt_id evt );
    inline gw_data_hdr_U8_v2 ( const gw_evt_hdr_v2 & hdr );
#endif

    GW_EVT_HDR_MBR
    uint8_t data_size;                  /* = data_size - 1, i.e. "0" means 1 byte */
    /* uint8_t data [ string_size ]; */
};

struct gw_data_hdr_U16_v2 GW_EVT_HDR_PARENT
{
#ifdef __cplusplus
    inline size_t size () const;
    inline gw_data_hdr_U16_v2 ( uint32_t id, gw_evt_id evt );
    inline gw_data_hdr_U16_v2 ( const gw_evt_hdr_v2 & hdr );
#endif

    GW_EVT_HDR_MBR
    uint16_t data_size;                 /* = data_size - 1, i.e. "0" means 1 byte */
    /* uint8_t data [ string_size ]; */
};

#endif /*_h_general_writer_*/
