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
#include <iostream>

using namespace tui;

void example_0( void );
void example_1( void );
void example_2( void );
void example_3( void );
void example_4( void );
void example_5( void );
void example_6( void );
void example_7( void );
void example_8( void );
void example_9( void );
void example_10( void );

int main( int argc, const char* argv[] )
{
    int selection = argc > 1 ? atoi( argv[ 1 ] ) : 0;
    switch( selection )
    {
        case 0 : example_0(); break;    // most minimal example: just an empty screen
        case 1 : example_1(); break;    // empty screen with caption
        case 2 : example_2(); break;    // empty screen with caption and status-line
        case 3 : example_3(); break;    // empty screen showing screen - resolution in status line
        case 4 : example_4(); break;    // show one label
        case 5 : example_5(); break;    // show one label and one input-field
        case 6 : example_6(); break;    // with controller now
        case 7 : example_7(); break;    // with button added
        case 8 : example_8(); break;    // with check-box
        case 9 : example_9(); break;    // visual design for SRA-STRIDES
        case 10 : example_10(); break;  // try to use multi-tabbed one screen
        
        default : std::cout << "unknown test: " << selection << std::endl;
    }
    return 0;
};
