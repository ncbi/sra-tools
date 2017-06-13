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

#include <tui/tui.hpp>

using namespace tui;

void example_0( void );
void example_1( void );
void example_2( void );
void example_3( void );

int main( int argc, const char* argv[] )
{
    int selection = argc > 1 ? atoi( argv[ 1 ] ) : 0;
    switch( selection )
    {
        case 0 : example_0(); break;
        case 1 : example_1(); break;
        case 2 : example_2(); break;
        case 3 : example_3(); break;        
    }
    return 0;
};
