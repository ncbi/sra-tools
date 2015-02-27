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

#ifndef _sra_tools_hpp_general_loader_
#define _sra_tools_hpp_general_loader_

#include <klib/defs.h>

#include <string>
#include <vector>

struct KStream;
struct VCursor;
struct VDatabase;

#define GeneralLoaderSignatureString "wheeeeee"

class GeneralLoader
{
public:
    
    struct Header
    {
        char        signature[8];
        uint32_t    endian;
        uint32_t    version;
    };

    struct Metadata
    {
        uint32_t    md_size;
        uint32_t    str_size;
        uint32_t    schema_offset;
        uint32_t    schema_file_offset;
        uint32_t    database_name_offset;
        uint32_t    num_columns;
    };
    
    struct Column
    {
        uint32_t    table_name_offset;
        uint32_t    column_name_offset;
    };

public:
    GeneralLoader ( const struct KStream& p_input );
    ~GeneralLoader ();
    
    rc_t Run ();
    
    void Reset();
    
private:
    // Active cursors
    // value_type::first    : table name 
    // value_type::second   : cursor
    typedef std::vector < std::pair < std::string, struct VCursor * > > Cursors;

    // From column id to VCursor.
    // value_type::first    : index into Cursors
    // value_type::second   : colIdx in the VCursor
    typedef std::vector < std::pair < uint32_t, uint32_t > > ColumnToCursor; 

    rc_t ReadHeader ();
    rc_t ReadMetadata ();
    rc_t ReadColumns ();
    rc_t ReadStringTable ();
    rc_t MakeDatabase ();
    rc_t MakeCursor ( const char * p_table );
    rc_t MakeCursors ();
    rc_t OpenCursors ();
    rc_t ReadData ();
    
    const struct KStream& m_input;
    
    Metadata    m_md;
    Column*     m_columns;
    char*       m_strings;
    
    struct VDatabase*   m_db;
    Cursors             m_cursors;
    ColumnToCursor      m_columnToCursor;
};

#endif
