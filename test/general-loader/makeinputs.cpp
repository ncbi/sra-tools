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

/**
* Unit tests for General Loader
*/
#include "testsource.hpp"

#include "../../tools/general-loader/utf8-like-int-codec.c"

using namespace std;

const char* tableName = "REFERENCE";
const char* asciiColumnName = "SPOT_GROUP";
const char* i64ColumnName = "PRIMARY_ALIGNMENT_IDS";
const char* u32ColumnName = "SPOT_ID";
  
void
make_1( const string& p_caseId, bool p_packed )
{   // One table, one column, default values changed mid-data
    TestSource source;
    TestSource::packed = p_packed;
    
    source . SchemaEvent ( "align/align.vschema", "NCBI:align:db:alignment_sorted" );
    source . DatabaseEvent ( string ( "actual/" ) + p_caseId + "/db" );
    
    source . NewTableEvent ( 1, tableName ); 
    source . NewColumnEvent ( 1, 1, asciiColumnName, 8 );
    source . OpenStreamEvent();
    
    string value1 = "first value";
    source . CellDataEvent( 1, value1 );
    source . NextRowEvent ( 1 );
    
    string default1 = "first default";
    source . CellDefaultEvent( 1, default1 );
    source . NextRowEvent ( 1 );
    
    source . NextRowEvent ( 1 );
    
    string value2 = "second value";
    source . CellDefaultEvent( 1, value2 );
    source . NextRowEvent ( 1 );
    
    string default2 = "second default";
    source . CellDefaultEvent( 1, default2 );
    source . NextRowEvent ( 1 );
    
    source . CloseStreamEvent();
    string filename = string ( "input/" ) + p_caseId + ".gl";
    source . SaveBuffer ( filename . c_str ()  );
}

void
make_2( const string& p_caseId, bool p_packed )
{   // Error message
    TestSource source;
    TestSource::packed = p_packed;
    
    source . SchemaEvent ( "align/align.vschema", "NCBI:align:db:alignment_sorted" );
    source . DatabaseEvent ( string ( "actual/" ) + p_caseId + "/db" );
    
    source . NewTableEvent ( 1, tableName ); 
    source . NewColumnEvent ( 1, 1, asciiColumnName, 8 );
    source . OpenStreamEvent();
    
    source . ErrorMessageEvent( "something is wrong" );
    
    source . CloseStreamEvent();
    string filename = string ( "input/" ) + p_caseId + ".gl";
    source . SaveBuffer ( filename . c_str ()  );
}

void
make_3( const string& p_caseId )
{   // Empty default value
    TestSource source;
    
    source . SchemaEvent ( "align/align.vschema", "NCBI:align:db:alignment_sorted" );
    source . DatabaseEvent ( string ( "actual/" ) + p_caseId + "/db" );
    
    source . NewTableEvent ( 1, tableName ); 
    source . NewColumnEvent ( 1, 1, i64ColumnName, 64 );
    source . OpenStreamEvent();
    
    source . CellEmptyDefaultEvent( 1 );
    source . NextRowEvent ( 1 );
    
    source . CloseStreamEvent();
    string filename = string ( "input/" ) + p_caseId + ".gl";
    source . SaveBuffer ( filename . c_str ()  );
}

void
make_4( const string& p_caseId )
{   // Move Ahead event
    TestSource source;
    
    source . SchemaEvent ( "align/align.vschema", "NCBI:align:db:alignment_sorted" );
    source . DatabaseEvent ( string ( "actual/" ) + p_caseId + "/db" );
    
    source . NewTableEvent ( 1, tableName ); 
    source . NewColumnEvent ( 1, 1, i64ColumnName, 64 );
    source . OpenStreamEvent();
    
    source . CellEmptyDefaultEvent( 1 );
    source . MoveAheadEvent ( 1, 5 );
    
    source . CellDataEvent( 1, (int64_t)100 );
    source . MoveAheadEvent ( 1, 5 );
    
    // expected: 5 empty cells, 100, 4 empty cells
    source . CloseStreamEvent();
    string filename = string ( "input/" ) + p_caseId + ".gl";
    source . SaveBuffer ( filename . c_str ()  );
}

void
make_5( const string& p_caseId )
{   // Integer compression (packed mode only)
    TestSource source;
    TestSource::packed = true;
    
    source . SchemaEvent ( "align/align.vschema", "NCBI:align:db:alignment_sorted" );
    source . DatabaseEvent ( string ( "actual/" ) + p_caseId + "/db" );
    
    source . NewTableEvent ( 1, tableName ); 
    source . NewColumnEvent ( 1, 1, i64ColumnName, 64, true );
    source . OpenStreamEvent();
    
    uint8_t buf[128];
    int bytes = encode_uint64 ( 0x0F0F0F0F0F0F0F0FLL, buf, buf + sizeof buf );
    source . CellDataEventRaw ( 1, 1, buf, bytes );
    source . NextRowEvent ( 1 );
    
    source . CloseStreamEvent();
    string filename = string ( "input/" ) + p_caseId + ".gl";
    source . SaveBuffer ( filename . c_str ()  );
}

int main()
{
    make_1( "1", false );
    make_1( "1packed", true );
    make_2( "2", false );
    make_2( "2packed", true );
    make_3( "3" );
    make_4( "4" );
    make_5( "5" );
    
    return 0;
}
