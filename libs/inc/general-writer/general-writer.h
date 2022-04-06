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

#ifdef __cplusplus
#include <string.h>
#endif


/*----------------------------------------------------------------------
 * event codes
 */

enum gw_evt_id
{
    evt_bad_event,

    evt_errmsg,                           /* convey processing error msg */
    evt_end_stream,                       /* cleanly terminates a stream */

    evt_remote_path,                      /* sets remote output path     */
    evt_use_schema,                       /* conveys schema usage        */
    evt_new_table,                        /* create a new table          */
    evt_new_column,                       /* create a new column in tbl  */
    evt_open_stream,                      /* open stream for data flow   */

    evt_cell_default,                     /* set/reset cell default val  */
    evt_cell_data,                        /* write/append data cell      */
    evt_next_row,                         /* move to next row in table   */
    evt_move_ahead,                       /* move ahead by N rows        */

    evt_errmsg2,
    evt_remote_path2,
    evt_use_schema2,
    evt_new_table2,
    evt_cell_default2,                    /* packed default <= 64K bytes */
    evt_cell_data2,                       /* packed data <= 64K bytes    */
    evt_empty_default,                    /* set cell default to empty   */

    /* BEGIN VERSION 2 MESSAGES */
    evt_software_name,                    /* sets software name          */
    evt_db_metadata_node,                    /* uses gw(p)_2string_evt_v1   */
    evt_tbl_metadata_node,
    evt_col_metadata_node,
    evt_db_metadata_node2,                   /* uses gwp_2string_evt_U16    */
    evt_tbl_metadata_node2,
    evt_col_metadata_node2,

    evt_add_mbr_db,
    evt_add_mbr_tbl,

    evt_logmsg,
    evt_progmsg,

    evt_max_id                            /* must be last                */
};

#define GW_SIGNATURE "NCBIgnld"
#define GW_GOOD_ENDIAN 1
#define GW_REVERSE_ENDIAN ( 1 << 24 )
#define GW_CURRENT_VERSION 2

//These are not to change
#define STRING_LIMIT_8 0x100
#define STRING_LIMIT_16 0x10000
#define ID_LOWER_LIMIT 0
#define ID_UPPER_LIMIT 255

/********************************
 * DESCRIPTION OF STREAM EVENTS *
 ********************************

 *. BYTE ORDER
    All binary data are in origination-host-native byte order.
    The originating host indicates its byte order in the stream header.
    Two byte-orders are supported: little-endian and big-endian.

 *. PACKING
    The preferred mode of operation for reduction of network bandwidth
    is "packed". In this mode, the event structures use single-byte members
    whenever possible, and there is no word alignment within the stream.
    In addition, events with integer data can utilize a packing algorithm
    to reduce the bandwidth consumed when the values are typically small
    with regard to the stated type.

    Packed mode is useful from C and C++ where there is ample support
    from the language itself. Non-packed mode is more natural for languages
    such as Python and Java and uses word-size members within structs and
    does not perform any integer data packing.

    NB - packed mode makes use of knowing that zero-length items
    are invalid and should not be sent, and so stores length-1 wherever
    a length is required, thus giving a range of 1..256 rather than
    0..255 for a single byte. The same is true for ids.

 1. STREAM HEADER
    The stream starts with a header appropriate for the protocol
    version in use. For version 1, use a "gw_header_v1".

    Every stream header must have the standard "gw_header" as its
    base definition. This portion presents an 8-character signature
    to identify the type of stream, followed immediately by an integer
    indicating originating-host byte order. The receiver must verify that
    this value is either GW_GOOD_ENDIAN or GW_REVERSE_ENDIAN; any other
    value means the stream is either not of this protocol or corrupt.
    A value of GW_REVERSE_ENDIAN implies that the receiver must perform
    byte-swapping on all word values within all events and their data.
    This is likely to be a nearly untenable task by any but the
    general-loader, where the latter has access to type information.

    The value presented for version must be >= 1 and <= GW_CURRENT_VERSION.
    A value of 0 must be rejected as not conforming to protocol while a
    value > GW_CURRENT_VERSION cannot be processed.

    The v1 value for "packing" currently allows for two values: 0 and 1,
    where 0 means that no packing will be used and 1 indicates packed
    events will be used. This affects how all subsequent events are to
    be sent and received.

 2. SET REMOTE PATH [ OPTIONAL ]
    In the not-packed case, use "gw_1string_evt".
      GW_SET_ID_EVT ( & evt.dad, 0, evt_remote_path );
      evt.sz = strlen ( path );
    follow with the bytes in path
      write ( path, strlen ( path ) );
    end with 0..3 bytes of value 0 to realign stream to 4-byte boundary.

    In the packed case where strlen ( path ) <= 256, use "gwp_1string_evt".
      GWP_SET_ID_EVT ( & evt.dad, 0, evt_remote_path );
      evt.sz = strlen ( path ) - 1;
    follow with the bytes in path
      write ( path, strlen ( path ) );

    In the packed case where strlen ( path ) > 0x100 but <= 0x10000, use "gwp_1string_evt_U16"
      GWP_SET_ID_EVT ( & evt.dad, 0, evt_remote_path2 );
      evt.sz = strlen ( path ) - 1;
    follow with the bytes in path
      write ( path, strlen ( path ) );

  MORE TO COME...

 */


/*======================================================================
 * version 1
 */

/* gw_header
 *  common to all versions
 */
struct gw_header
{
    char signature [ 8 ]; /* = GW_SIGNATURE                                     */
    uint32_t endian;      /* = GW_GOOD_ENDIAN or GW_REVERSE_ENDIAN              */
    uint32_t version;     /* 0 < version <= GW_CURRENT_VERSION                  */
    uint32_t hdr_size;    /* the size of the entire header, including alignment */
};

/* gw_header_v1
 *  v1 header
 */
struct gw_header_v1
{
    gw_header dad;
    uint32_t packing;     /* 0 = no packing, 1 = byte packing */
};


/*----------------------------------------------------------------------
 * full-size ( not packed ) events
 */

/* gw_evt_hdr
 *  common header to not-packed v1 events
 *
 *  used as-is for events:
 *    { evt_end_stream, evt_open_stream, evt_next_row }
 */
struct gw_evt_hdr_v1
{
    uint32_t id_evt;      /* bits 0..23 = id, bits 24..31 = event code */
};

/* gw_1string_evt_v1
 *  event to convey a single string of information
 *
 *  used for events:
 *    { evt_errmsg, evt_remote_path, evt_new_table }
 */
struct gw_1string_evt_v1
{
    gw_evt_hdr_v1 dad;    /* common header : id = 0 or new table id > 0       */
    uint32_t sz;          /* size of string in bytes - NO trailing NUL byte   */
 /* char str [ sz ];       * string data.                                     *
    char align [ 0..3 ];   * ( ( 4 - sizeof str % 4 ) % 4 ) zeros to align    */
};

/* gw_2string_evt
 *  event used to send a pair of strings
 *
 *  used for events:
 *    { evt_use_schema }
 */
struct gw_2string_evt_v1
{
    gw_evt_hdr_v1 dad;    /* common header : id = 0                           */
    uint32_t sz1;         /* size of string 1 in bytes, NO trailing NUL byte  */
    uint32_t sz2;         /* size of string 2 in bytes, NO trailing NUL byte  */
 /* char str [ sz1+sz2 ];  * string data.                                     *
    char align [ 0..3 ];   * ( ( 4 - sizeof str % 4 ) % 4 ) zeros             */
};

/* gw_column_evt
 *  event used to create a new column
 *
 *  used for events:
 *    { evt_new_column }
 */
struct gw_column_evt_v1
{
    gw_evt_hdr_v1 dad;    /* common header : id = new column id > 0           */
    uint32_t table_id;    /* id of column's table                             */
    uint32_t elem_bits;   /* the size of each element in data type            */
    uint32_t name_sz;     /* the size in bytes of column "name" ( spec )      */
 /* char name [ name_sz ]; * the column name/spec.                            *
    char align [ 0..3 ];   * ( ( 4 - sizeof name % 4 ) % 4 ) zeros            */
};

/* gw_data_evt
 *  event used to transfer cell data
 *
 *  used for events:
 *    { evt_cell_default, evt_cell_data }
 */
struct gw_data_evt_v1
{
    gw_evt_hdr_v1 dad;    /* common header : id = column id                   */
    uint32_t elem_count;  /* the number of elements in data                   */
 /* uint8_t data [ x ];    * event data. actual size is:                      *
                           * ( ( elem_count * col . elem_bits ) + 7 ) / 8     *
    char align [ 0..3 ];   * ( ( 4 - sizeof data % 4 ) % 4 ) zeros            */
};

/* gw_move_ahead_evt_v1
 */
struct gw_move_ahead_evt_v1
{
    gw_evt_hdr_v1 dad;    /* common header : id = column id                   */
    uint32_t nrows [ 2 ]; /* the number of rows to move ahead                 */
};

struct gw_add_mbr_evt_v1
{
    gw_evt_hdr_v1 dad;
    uint32_t db_id;
    uint32_t mbr_sz;
    uint32_t name_sz;
    uint8_t create_mode;
};

struct gw_status_evt_v1
{
    gw_evt_hdr_v1 dad;
    uint32_t version;
    uint32_t timestamp;
    uint32_t pid;
    uint32_t name_sz;
    uint32_t percent;
};

/*----------------------------------------------------------------------
 * packed events
 *   used for C/C++ level operations
 */

/* gwp_evt_hdr
 *  common header to packed v1 events
 *
 *  given that id 0 is illegal, this structure does not allocate a
 *  code for it, but rather stores all ids as id-1, i.e. 1..256 => 0..255.
 *
 *  used as-is for events:
 *    { evt_end_stream, evt_open_stream, evt_next_row }
 */
struct gwp_evt_hdr_v1
{
    uint8_t _evt;         /* event code from enum              */
    uint8_t _id;          /* ids 1..256 represented as 0..255  */
};

/* gwp_1string_evt
 *  event to convey a single string of information
 *
 *  used for events:
 *    { evt_errmsg, evt_remote_path, evt_new_table }
 */
struct gwp_1string_evt_v1
{
    gwp_evt_hdr_v1 dad;   /* common header : id = 0 or new table id > 0       */
    uint8_t sz;           /* size of string - 1 in bytes - NO trailing NUL    */
 /* char str [ sz+1 ];     * string data                                      */
};

/* gwp_2string_evt
 *  event used to send a pair of strings
 *
 *  used for events:
 *    { evt_use_schema }
 */
struct gwp_2string_evt_v1
{
    gwp_evt_hdr_v1 dad;   /* common header : id = 0                           */
    uint8_t sz1;          /* size of string 1 - 1 in bytes, NO trailing NUL   */
    uint8_t sz2;          /* size of string 2 - 1 in bytes, NO trailing NUL   */
 /* char str[ sz1+sz2+2 ]; * string data.                                     */
};

/* gwp_column_evt
 *  event used to create a new column
 *
 *  used for events:
 *    { evt_new_column }
 */
struct gwp_column_evt_v1
{
    gwp_evt_hdr_v1 dad;   /* common header : id = new column id > 0           */
    uint8_t table_id;     /* id - 1 of column's table                         */
    uint8_t elem_bits;    /* the size - 1 of each element in data type        */
    uint8_t flag_bits;    /* bit[0] = 1 means uses integer element packing    */
    uint8_t name_sz;      /* the size - 1 in bytes of column "name" ( spec )  */
 /* char name [ name_sz+1 ]; the column name/spec.                            */
};

/* gwp_data_evt
 *  event used to transfer cell data
 *
 *  used for events:
 *    { evt_cell_default, evt_cell_data }
 */
struct gwp_data_evt_v1
{
    gwp_evt_hdr_v1 dad;   /* common header : id = column id                   */
    uint8_t sz;           /* the size - 1 of data in bytes                    */
 /* uint8_t data [ sz+1 ]; * event data.                                      */
};

/* gwp_move_ahead_evt_v1
 */
struct gwp_move_ahead_evt_v1
{
    gwp_evt_hdr_v1 dad;   /* common header : id = column id                   */
    uint16_t nrows [ 4 ]; /* the number of rows to move ahead                 */
};


/* SPECIAL VERSIONS WITH 16-BIT SIZE FIELDS */

/* gwp_1string_evt_U16
 *  event to convey a single string of information
 *
 *  used for events:
 *    { evt_errmsg2, evt_remote_path2, evt_new_table2 }
 *
 *  ...whenever size of string > 256
 */
struct gwp_1string_evt_U16_v1
{
    gwp_evt_hdr_v1 dad;   /* common header : id = 0 or new table id > 0       */
    uint16_t sz;          /* size of string - 1 in bytes - NO trailing NUL    */
 /* char str [ sz+1 ];     * string data.                                     */
};

/* gwp_2string_evt_U16
 *  event used to send a pair of strings
 *
 *  used for events:
 *    { evt_use_schema2 }
 *
 *  ...whenever size of any string > 256
 */
struct gwp_2string_evt_U16_v1
{
    gwp_evt_hdr_v1 dad;   /* common header : id = 0                           */
    uint16_t sz1;         /* size of string 1 - 1 in bytes, NO trailing NUL   */
    uint16_t sz2;         /* size of string 2 - 1 in bytes, NO trailing NUL   */
 /* char str[ sz1+sz2+2 ]; * string data.                                     */
};


/* gwp_data_evt_16
 *  event used to transfer cell data
 *
 *  used for events:
 *    { evt_cell_default2, evt_cell_data2 }
 */
struct gwp_data_evt_U16_v1
{
    gwp_evt_hdr_v1 dad;   /* common header : id = column id                   */
    uint16_t sz;          /* the size - 1 of data in bytes                    */
 /* uint8_t data [ sz+1 ]; * event data.                                      */
};

struct gwp_add_mbr_evt_v1
{
    gwp_evt_hdr_v1 dad;
    uint8_t db_id;
    uint8_t mbr_sz;
    uint8_t name_sz;
    uint8_t create_mode;
};

struct gwp_status_evt_v1
{
    gwp_evt_hdr_v1 dad;
    uint32_t version;
    uint32_t timestamp;
    uint32_t pid;
    uint8_t name_sz;
    uint8_t percent;
};

#ifdef __cplusplus
/*======================================================================
 * support for C++
 */

#include <string.h>
#include <assert.h>

namespace ncbi
{
    // gw_header
    inline void init ( :: gw_header & hdr )
    {
        memmove ( hdr . signature, GW_SIGNATURE, sizeof hdr . signature );
        hdr . endian = GW_GOOD_ENDIAN;
        hdr . version = GW_CURRENT_VERSION;
        hdr . hdr_size = sizeof ( :: gw_header );
    }

    inline void init ( :: gw_header & hdr, size_t hdr_size )
    {
        init ( hdr );
        hdr . hdr_size = ( uint32_t ) hdr_size;
    }

    // gw_header_v1
    inline void init ( :: gw_header_v1 & hdr )
    { init ( hdr . dad, sizeof ( :: gw_header_v1 ) ); hdr . packing = 1; }

    inline void init ( :: gw_header_v1 & hdr, const :: gw_header & dad )
    { hdr . dad = dad; hdr . packing = 0; }



    // gw_evt_hdr
    inline void init ( :: gw_evt_hdr_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        assert ( id < 0x1000000 );
        assert ( evt != evt_bad_event );
        assert ( evt < evt_max_id );

        hdr . id_evt = ( id & 0xFFFFFF ) | ( ( uint32_t ) evt << 24 );
    }

    inline uint32_t id ( const :: gw_evt_hdr_v1 & self )
    { return self . id_evt & 0xFFFFFF; }

    inline gw_evt_id evt ( const :: gw_evt_hdr_v1 & self )
    { return ( gw_evt_id ) ( self . id_evt >> 24 ); }


    // recording string size
    inline void set_string_size ( uint32_t & sz, size_t bytes )
    {
        assert ( bytes != 0 );
        assert ( sizeof bytes == 4 || ( bytes >> 32 ) == 0 );
        sz = ( uint32_t ) bytes;
    }

    // gw_1string_evt
    inline void init ( :: gw_1string_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    { init ( hdr . dad, id, evt ); hdr . sz = 0; }

    inline void init ( :: gw_1string_evt_v1 & hdr, const :: gw_evt_hdr_v1 & dad )
    { hdr . dad = dad; hdr . sz = 0; }

    inline size_t size ( const :: gw_1string_evt_v1 & self )
    { return self . sz; }

    inline void set_size ( :: gw_1string_evt_v1 & self, size_t bytes )
    { set_string_size ( self . sz, bytes ); }


    // gw_2string_evt
    inline void init ( :: gw_2string_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . sz1 = hdr . sz2 = 0;
    }

    inline void init ( :: gw_2string_evt_v1 & hdr, const :: gw_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . sz1 = hdr . sz2 = 0;
    }

    inline size_t size1 ( const gw_2string_evt_v1 & self )
    { return self . sz1; }

    inline size_t size2 ( const gw_2string_evt_v1 & self )
    { return self . sz2; }

    inline void set_size1 ( :: gw_2string_evt_v1 & self, size_t bytes )
    { set_string_size ( self . sz1, bytes ); }

    inline void set_size2 ( :: gw_2string_evt_v1 & self, size_t bytes )
    { set_string_size ( self . sz2, bytes ); }

    // gw_column_evt
    inline void init ( :: gw_column_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . table_id = hdr . elem_bits = hdr . name_sz = 0;
    }

    inline void init ( :: gw_column_evt_v1 & hdr, const :: gw_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . table_id = hdr . elem_bits = hdr . name_sz = 0;
    }

    inline uint32_t table_id ( const :: gw_column_evt_v1 & self )
    { return self . table_id; }

    inline uint32_t elem_bits ( const :: gw_column_evt_v1 & self )
    { return self . elem_bits; }

    inline uint8_t flag_bits ( const :: gw_column_evt_v1 & self )
    { return 0; }

    inline size_t name_size ( const :: gw_column_evt_v1 & self )
    { return self . name_sz; }

    inline void set_table_id ( :: gw_column_evt_v1 & self, uint32_t table_id )
    {
        assert ( table_id != 0 );
        self . table_id = table_id;
    }

    inline void set_elem_bits ( :: gw_column_evt_v1 & self, uint32_t elem_bits )
    {
        assert ( elem_bits != 0 );
        self . elem_bits = elem_bits;
    }

    inline void set_name_size ( :: gw_column_evt_v1 & self, size_t name_size )
    { set_string_size ( self . name_sz, name_size ); }


    // gw_data_evt
    inline void init ( :: gw_data_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    { init ( hdr . dad, id, evt ); hdr . elem_count = 0; }

    inline void init ( :: gw_data_evt_v1 & hdr, const :: gw_evt_hdr_v1 & dad )
    { hdr . dad = dad; hdr . elem_count = 0; }

    inline uint32_t elem_count ( const :: gw_data_evt_v1 & self )
    { return self . elem_count; }

    inline void set_elem_count ( :: gw_data_evt_v1 & self, uint32_t elem_count )
    {
        assert ( elem_count != 0 );
        self . elem_count = elem_count;
    }

    // gw_move_ahead_evt_v1
    inline void init ( :: gw_move_ahead_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        memset ( & hdr . nrows, 0, sizeof hdr . nrows );
    }

    inline void init ( :: gw_move_ahead_evt_v1 & hdr, const :: gw_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        memset ( & hdr . nrows, 0, sizeof hdr . nrows );
    }

    inline uint64_t get_nrows ( const :: gw_move_ahead_evt_v1 & self )
    {
        uint64_t nrows;
        memmove ( & nrows, & self . nrows, sizeof nrows );
        return nrows;
    }

    inline void set_nrows ( :: gw_move_ahead_evt_v1 & self, uint64_t nrows )
    {
        memmove ( & self . nrows, & nrows, sizeof self . nrows );
    }

    // gw_add_mbr_evt
    inline void init ( :: gw_add_mbr_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . db_id = hdr . mbr_sz = hdr . name_sz = hdr . create_mode = 0;
    }

    inline void init ( :: gw_add_mbr_evt_v1 & hdr, const :: gw_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . db_id = hdr . mbr_sz = hdr . name_sz = hdr . create_mode = 0;
    }

    inline uint32_t db_id ( const gw_add_mbr_evt_v1 & self )
    { return self . db_id; }

    inline void set_db_id ( :: gw_add_mbr_evt_v1 & self, uint8_t db_id )
    {
        assert ( db_id >= 0 );
        self . db_id = db_id;
    }

    inline size_t size1 ( const gw_add_mbr_evt_v1 & self )
    { return ( size_t ) self . mbr_sz; }

    inline size_t size2 ( const gw_add_mbr_evt_v1 & self )
    { return ( size_t ) self . name_sz; }

    inline void set_size1 ( :: gw_add_mbr_evt_v1 & self, size_t bytes )
    {
        set_string_size ( self . mbr_sz, bytes );
    }

    inline void set_size2 ( :: gw_add_mbr_evt_v1 & self, size_t bytes )
    {
        set_string_size ( self . name_sz, bytes );
    }

    inline uint8_t create_mode ( const gw_add_mbr_evt_v1 & self )
    { return self . create_mode; }

    inline void set_create_mode ( :: gw_add_mbr_evt_v1 & self, uint8_t mode )
    {
        self . create_mode = mode;
    }

    // gw_status_evt
    inline void init ( :: gw_status_evt_v1 &hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . version = hdr . timestamp = hdr . pid = hdr . name_sz = hdr . percent = 0;
    }

    inline void init ( :: gw_status_evt_v1 &hdr, const :: gw_evt_hdr_v1 &dad )
    {
        hdr . dad = dad;
        hdr . version = hdr . timestamp = hdr . pid = hdr . name_sz = hdr . percent = 0;
    }

    inline void set_version ( :: gw_status_evt_v1 &self, uint32_t version )
    {
        assert ( version > 0 );
        self . version = version;
    }

    inline uint32_t version ( const :: gw_status_evt_v1 &self )
    { return self . version; }

    inline void set_timestamp ( :: gw_status_evt_v1 &self, uint32_t timestamp )
    {
        assert ( timestamp > 0 );
        self . timestamp = timestamp;
    }

    inline uint32_t timestamp ( const :: gw_status_evt_v1 &self )
    { return self . timestamp; }

    inline void set_pid ( :: gw_status_evt_v1 &self, uint32_t pid )
    {
        assert ( pid > 0 );
        self . pid = pid;
    }

    inline uint32_t pid ( const :: gw_status_evt_v1 &self )
    { return self . pid; }

    inline void set_size ( :: gw_status_evt_v1 &self, size_t bytes )
    {
        set_string_size ( self . name_sz, bytes );
    }

    inline size_t size ( const :: gw_status_evt_v1 &self )
    { return ( size_t ) self . name_sz; }

    inline void set_percent ( :: gw_status_evt_v1 &self, uint32_t percent )
    {
        self . percent = percent;
    }

    inline uint32_t percent ( const :: gw_status_evt_v1 &self )
    { return self . percent; }

    ////////// packed events //////////

    // gwp_evt_hdr
    inline void init ( :: gwp_evt_hdr_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        // allow zero, but treat as 256
        assert ( id <= 0x100 );
        assert ( evt != evt_bad_event );
        assert ( evt < evt_max_id );

        hdr . _evt = ( uint8_t ) evt;
        hdr . _id = ( uint8_t ) ( id - 1 );
    }

    inline uint32_t id ( const :: gwp_evt_hdr_v1 & self )
    { return ( uint32_t ) self . _id + 1; }

    inline gw_evt_id evt ( const :: gwp_evt_hdr_v1 & self )
    { return ( gw_evt_id ) self . _evt; }


    // recording string size
    inline void set_string_size ( uint8_t & sz, size_t bytes )
    {
        assert ( bytes != 0 );
        assert ( bytes <= 0x100 );
        sz = ( uint8_t ) ( bytes - 1 );
    }

    // gwp_1string_evt
    inline void init ( :: gwp_1string_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . sz = 0;
    }

    inline void init ( :: gwp_1string_evt_v1 & hdr, const :: gwp_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . sz = 0;
    }

    inline size_t size ( const :: gwp_1string_evt_v1 & self )
    { return ( size_t ) self . sz + 1; }

    inline void set_size ( :: gwp_1string_evt_v1 & self, size_t bytes )
    { set_string_size ( self . sz, bytes ); }


    // gwp_2string_evt
    inline void init ( :: gwp_2string_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . sz1 = hdr . sz2 = 0;
    }

    inline void init ( :: gwp_2string_evt_v1 & hdr, const :: gwp_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . sz1 = hdr . sz2 = 0;
    }

    inline size_t size1 ( const gwp_2string_evt_v1 & self )
    { return ( size_t ) self . sz1 + 1; }

    inline size_t size2 ( const gwp_2string_evt_v1 & self )
    { return ( size_t ) self . sz2 + 1; }

    inline void set_size1 ( :: gwp_2string_evt_v1 & self, size_t bytes )
    { set_string_size ( self . sz1, bytes ); }

    inline void set_size2 ( :: gwp_2string_evt_v1 & self, size_t bytes )
    { set_string_size ( self . sz2, bytes ); }


    // gwp_column_evt
    inline void init ( :: gwp_column_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . table_id = hdr . elem_bits = hdr . name_sz = 0;
    }

    inline void init ( :: gwp_column_evt_v1 & hdr, const :: gwp_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . table_id = hdr . elem_bits = hdr . name_sz = 0;
    }

    inline uint32_t table_id ( const :: gwp_column_evt_v1 & self )
    { return ( uint32_t ) self . table_id + 1; }

    inline uint32_t elem_bits ( const :: gwp_column_evt_v1 & self )
    { return ( uint32_t ) self . elem_bits + 1; }

    inline uint8_t flag_bits ( const :: gwp_column_evt_v1 & self )
    { return self . flag_bits; }

    inline size_t name_size ( const :: gwp_column_evt_v1 & self )
    { return ( size_t ) self . name_sz + 1; }

    inline void set_table_id ( :: gwp_column_evt_v1 & self, uint32_t table_id )
    {
        assert ( table_id != 0 );
        assert ( table_id <= 0x100 );
        self . table_id = ( uint8_t ) ( table_id - 1 );
    }

    inline void set_elem_bits ( :: gwp_column_evt_v1 & self, uint32_t elem_bits )
    {
        assert ( elem_bits != 0 );
        assert ( elem_bits <= 0x100 );
        self . elem_bits = ( uint8_t ) ( elem_bits - 1 );
    }

    inline void set_name_size ( :: gwp_column_evt_v1 & self, size_t name_size )
    { set_string_size ( self . name_sz, name_size ); }


    // gwp_data_evt
    inline void init ( :: gwp_data_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . sz = 0;
    }

    inline void init ( :: gwp_data_evt_v1 & hdr, const :: gwp_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . sz = 0;
    }

    inline uint32_t size ( const :: gwp_data_evt_v1 & self )
    { return ( uint32_t ) self . sz + 1; }

    inline void set_size ( :: gwp_data_evt_v1 & self, size_t bytes )
    { set_string_size ( self . sz, bytes ); }


    // gwp_move_ahead_evt_v1
    inline void init ( :: gwp_move_ahead_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        memset ( & hdr . nrows, 0, sizeof hdr . nrows );
    }

    inline void init ( :: gwp_move_ahead_evt_v1 & hdr, const :: gwp_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        memset ( & hdr . nrows, 0, sizeof hdr . nrows );
    }

    inline uint64_t get_nrows ( const :: gwp_move_ahead_evt_v1 & self )
    {
        uint64_t nrows;
        memmove ( & nrows, & self . nrows, sizeof nrows );
        return nrows;
    }

    inline void set_nrows ( :: gwp_move_ahead_evt_v1 & self, uint64_t nrows )
    {
        memmove ( & self . nrows, & nrows, sizeof self . nrows );
    }


    // recording string size
    inline void set_string_size ( uint16_t & sz, size_t bytes )
    {
        assert ( bytes != 0 );
        assert ( bytes <= 0x10000 );
        sz = ( uint16_t ) ( bytes - 1 );
    }

    // gwp_1string_evt_U16
    inline void init ( :: gwp_1string_evt_U16_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . sz = 0;
    }

    inline void init ( :: gwp_1string_evt_U16_v1 & hdr, const :: gwp_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . sz = 0;
    }

    inline size_t size ( const :: gwp_1string_evt_U16_v1 & self )
    { return ( size_t ) self . sz + 1; }

    inline void set_size ( :: gwp_1string_evt_U16_v1 & self, size_t bytes )
    { set_string_size ( self . sz, bytes ); }


    // gwp_2string_evt
    inline void init ( :: gwp_2string_evt_U16_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . sz1 = hdr . sz2 = 0;
    }

    inline void init ( :: gwp_2string_evt_U16_v1 & hdr, const :: gwp_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . sz1 = hdr . sz2 = 0;
    }

    inline size_t size1 ( const gwp_2string_evt_U16_v1 & self )
    { return ( size_t ) self . sz1 + 1; }

    inline size_t size2 ( const gwp_2string_evt_U16_v1 & self )
    { return ( size_t ) self . sz2 + 1; }

    inline void set_size1 ( :: gwp_2string_evt_U16_v1 & self, size_t bytes )
    { set_string_size ( self . sz1, bytes ); }

    inline void set_size2 ( :: gwp_2string_evt_U16_v1 & self, size_t bytes )
    { set_string_size ( self . sz2, bytes ); }


    // gwp_data_evt
    inline void init ( :: gwp_data_evt_U16_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . sz = 0;
    }

    inline void init ( :: gwp_data_evt_U16_v1 & hdr, const :: gwp_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . sz = 0;
    }

    inline uint32_t size ( const :: gwp_data_evt_U16_v1 & self )
    { return ( uint32_t ) self . sz + 1; }

    inline void set_size ( :: gwp_data_evt_U16_v1 & self, size_t bytes )
    { set_string_size ( self . sz, bytes ); }


    // gwp_add_mbr_evt
    inline void init ( :: gwp_add_mbr_evt_v1 & hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . db_id = hdr . mbr_sz = hdr . name_sz = hdr . create_mode = 0;
    }

    inline void init ( :: gwp_add_mbr_evt_v1 & hdr, const :: gwp_evt_hdr_v1 & dad )
    {
        hdr . dad = dad;
        hdr . db_id = hdr . mbr_sz = hdr . name_sz = hdr . create_mode = 0;
    }

    inline uint8_t db_id ( const gwp_add_mbr_evt_v1 & self )
    { return self . db_id; }


    inline void set_db_id ( :: gwp_add_mbr_evt_v1 & self, uint8_t db_id )
    {
        assert ( db_id >= 0 );
        self . db_id = db_id;
    }

    inline size_t size1 ( const gwp_add_mbr_evt_v1 & self )
    { return ( size_t ) self . mbr_sz + 1; }

    inline size_t size2 ( const gwp_add_mbr_evt_v1 & self )
    { return ( size_t ) self . name_sz + 1; }

    inline void set_size1 ( :: gwp_add_mbr_evt_v1 & self, size_t bytes )
    {
        set_string_size ( self . mbr_sz, bytes );
    }

    inline void set_size2 ( :: gwp_add_mbr_evt_v1 & self, size_t bytes )
    {
        set_string_size ( self . name_sz, bytes );
    }

    inline uint8_t create_mode ( const gwp_add_mbr_evt_v1 & self )
    { return self . create_mode; }

    inline void set_create_mode ( :: gwp_add_mbr_evt_v1 & self, uint8_t mode )
    {
        self . create_mode = mode;
    }

    // gwp_status_evt
    inline void init ( :: gwp_status_evt_v1 &hdr, uint32_t id, gw_evt_id evt )
    {
        init ( hdr . dad, id, evt );
        hdr . version = hdr . timestamp  = 0;
        hdr . name_sz = hdr . percent = 0;
    }

    inline void init ( :: gwp_status_evt_v1 &hdr, const :: gwp_evt_hdr_v1 &dad )
    {
        hdr . dad = dad;
        hdr . version = hdr . timestamp = 0;
        hdr . name_sz = hdr . percent = 0;
    }

    inline void set_pid ( :: gwp_status_evt_v1 &self, int pid )
    {
        assert ( pid > 0 );
        self . pid = ( uint32_t ) pid;
    }

    inline uint32_t pid ( const :: gwp_status_evt_v1 &self )
    { return self . pid; }


    inline void set_version ( :: gwp_status_evt_v1 &self, uint32_t version )
    {
        assert ( version != 0 );
        self . version = version;
    }

    inline uint32_t version ( const :: gwp_status_evt_v1 &self )
    { return self . version; }

    inline void set_timestamp ( :: gwp_status_evt_v1 &self, uint32_t timestamp )
    {
        assert ( timestamp != 0 );
        self . timestamp = timestamp;
    }

    inline uint32_t timestamp ( const :: gwp_status_evt_v1 &self )
    { return self . timestamp; }

    inline void set_size ( :: gwp_status_evt_v1 &self, size_t bytes )
    {
        set_string_size ( self . name_sz, bytes );
    }

    inline size_t size ( const :: gwp_status_evt_v1 &self )
    { return ( size_t ) self . name_sz + 1; }

    inline void set_percent ( :: gwp_status_evt_v1 &self, uint32_t percent )
    {
        self . percent = percent;
    }

    inline uint32_t percent ( const :: gwp_status_evt_v1 &self )
    { return self . percent; }

}
#endif


#endif /*_h_general_writer_*/
