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

#ifndef _cpp_gw_dumper_test_
#define _cpp_gw_dumper_test_

#ifndef _hpp_general_writer_
#include "general-writer.hpp"
#endif

#include <kfs/defs.h>

namespace ncbi
{
    static
    void run ()
    {
        GeneralWriter gw ( "test-gw-dumper.gw" );

        gw . setRemotePath ( "remote-path" );
        gw . setSoftwareName ( "gw-dumper-test", "0.9.0" );
        gw . useSchema ( "schemafile.vschema", "db_spec.schema" );

        int db_id = gw . dbAddDatabase ( 0, "db1", "db1", kcmCreate );
        gw . setDBMetadataNode ( db_id, "node/path", "value01" );

        int tbl_id = gw . dbAddTable ( db_id, "tbl1", "tbl1", kcmCreate );
        gw . setTblMetadataNode ( tbl_id, "tbl/path", "value01" );

        int col_id = gw . addColumn ( tbl_id, "col1", 64, 0 );
        gw . setColMetadataNode ( col_id, "col/path", "value01" );

        tbl_id = gw . addTable ( "tbl2" );
        gw . setTblMetadataNode ( tbl_id, "tbl2/path", "value02" );
    }
}


int main ( int argc, char * argv [] )
{
    try
    {
        ncbi :: run ();
    }
    catch ( const char x [] )
    {
        std :: cerr << "caught exception: " << x << '\n';
    }
    catch ( ... )
    {
        std :: cerr << "caught exception\n";
    }

    return 0;
}

#endif //_cpp_gw_dumper_test_
