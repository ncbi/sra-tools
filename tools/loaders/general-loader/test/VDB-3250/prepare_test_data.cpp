#include <iostream>
#include <stdexcept>

#include <kfs/mmap.h>
#include <kfs/directory.h>
#include <kfs/file.h>

#include <string.h>
#include <stdlib.h>

#include "../../../tools/general-loader/general-writer.hpp"

using namespace std;
using namespace ncbi;

/************************************************
 * Syntax: program -d data -s schema -o outfile *
 ************************************************/
string _PR = "prepare_test_data";
string _DF = "test_data.bin";
string _SF = "test_data.vschema";
string _OF = "test_data.gw";

#define _DF_T    "-d"
#define _DF_TH   "data_file"

#define _SF_T    "-s"
#define _SF_TH   "schema_file"

#define _OF_T    "-o"
#define _OF_TH   "output_file"

void
usage ()
{
    cerr << endl;
    cerr << "USAGE:";
    cerr << " " << _PR;
    cerr << " [ " << _DF_T << " " << _DF_TH << " ]";
    cerr << " [ " << _SF_T << " " << _SF_TH << " ]";
    cerr << " [ " << _OF_T << " " << _OF_TH << " ]";
    cerr << endl;
    cerr << endl;
}   /* usage () */

bool
parse_args ( int argc, char ** argv )
{
    if ( argc < 1 ) {
        cerr << "ERROR: invalid usage of parse_args ()" << endl;
        exit ( 1 );
    }

    char * Ch = strrchr ( * argv, '/' );
    _PR = ( Ch == NULL ) ? ( * argv ) : ( Ch + 1 );

    for ( int llp = 1; llp < argc; llp ++ ) {
        if ( strcmp ( argv [ llp ], _DF_T ) == 0 ) {
            if ( argc <= llp + 1 ) {
                cerr << "ERROR: '" << _DF_T << "' requests parameter" << endl;
                return false;
            }
            llp ++;
            _DF = argv [ llp ];

            continue;
        }

        if ( strcmp ( argv [ llp ], _SF_T ) == 0 ) {
            if ( argc <= llp + 1 ) {
                cerr << "ERROR: '" << _SF_T << "' requests parameter" << endl;
                return false;
            }
            llp ++;
            _SF = argv [ llp ];

            continue;
        }

        if ( strcmp ( argv [ llp ], _OF_T ) == 0 ) {
            if ( argc <= llp + 1 ) {
                cerr << "ERROR: '" << _OF_T << "' requests parameter" << endl;
                return false;
            }
            llp ++;
            _OF = argv [ llp ];

            continue;
        }

        cerr << "ERROR: invalid parameter '" << argv [ llp ] << "'" << endl;
        return false;
    }

    cerr << "  DATA: " << _DF << endl;;
    cerr << "SCHEMA: " << _SF << endl;;
    cerr << "   OUT: " << _OF << endl;;

    return true;
}   /* parse_args () */

int bobrabotka ();

int
main ( int argc, char ** argv )
{
    if ( ! parse_args ( argc, argv ) ) {
        usage ();
        return 1;
    }

    int rc = bobrabotka ();

    if ( rc == 0 ) {
        cout << "DONE (^_^)" << endl;
    }
    else {
        cerr << "FAILED (o_O)" << endl;
    }

    return rc;
}   /* main () */

class LoA { /* LoA - LOaded uint64_t Array */
public:
    LoA ( const string & File );
    ~LoA ();

    inline const uint64_t * Data () const { return _data; };
    inline size_t Qty () const { return _qty; };

private:
    KMMap * _map;

    string _path;
    uint64_t * _data;
    size_t _qty;
};

LoA :: LoA ( const string & Nm )
:   _map ( NULL )
,   _path ( Nm )
,   _data ( NULL )
,   _qty ( 0 )
{
    cout << "#=======================" << endl;
    cout << "Reading uint64_t array from " << _path << endl;

    struct KDirectory * Nat = NULL;
    if ( KDirectoryNativeDir ( & Nat ) != 0 ) {
        throw runtime_error ( "Can not get NativeDir!" );
    }

    const struct KFile * File = NULL;
    if ( KDirectoryOpenFileRead ( Nat, & File, Nm . c_str () ) != 0 ) {
        throw runtime_error ( string ( "Can not open File [" ) + Nm + "]!" );
    }

    if ( KMMapMakeRead ( ( const KMMap ** ) & _map, File ) != 0 ) {
        throw runtime_error ( string ( "Can not mmap File [" ) + Nm + "]!" );
    }

    const void * Addr = NULL;
    if ( KMMapAddrRead ( _map, & Addr ) != 0 ) {
        throw runtime_error ( string ( "Can not mmap -> addr File [" ) + Nm + "]!" );
    }

    size_t Size = 0;
    if ( KMMapSize ( _map, & Size ) != 0 ) {
        throw runtime_error ( string ( "Can not mmap -> size File [" ) + Nm + "]!" );
    }

    if ( Size % sizeof ( uint64_t ) != 0 ) {
        throw runtime_error ( string ( "Invalid file size [" ) + Nm + "]!" );
    }

    _data = ( uint64_t * ) Addr;
    _qty = Size / sizeof ( uint64_t );

    KFileRelease ( File );
    KDirectoryRelease ( Nat );

    cout << "Read " << _qty << " uint64_t integers" << endl;
    cout << "#=======================" << endl;
}   /* LoA :: LoA () */

LoA :: ~LoA ()
{
    if ( _map != NULL ) {
        KMMapRelease ( _map );
        _map = NULL;
    }
    _path . clear ();
    _data = NULL;
    _qty = 0;
}   /* LoA :: ~LoA () */

void
write_data ( const LoA & Loa )
{
    cout << "#=======================" << endl;
    cout << "Writing to file " << _OF << endl;

    GeneralWriter GW ( _OF );

        /*  Some formalities
         */
    GW . setSoftwareName ( "general-loader-test", "01.01.01" );
    GW . setRemotePath ( "TEST" );
    GW . useSchema ( _SF, "NCBI:sra:db:trace #1" );

    size_t EBits = sizeof ( uint64_t ) * 8;

        /*  Adding table and column
         */
    int TableId = GW . addTable ( "SEQUENCE" );
    int ColumnId = GW . addIntegerColumn ( TableId, "SIGNAL", EBits );

        /*  Opening writer and setting defaults
         */
    GW . open ();
    GW . columnDefault ( ColumnId, EBits, "", 0 );

        /*  Writing and flusing buffers
         */
    GW . write ( ColumnId, EBits, Loa . Data (), Loa . Qty () );
    GW . nextRow ( TableId );

        /*  Here writer should die by itself and close all streams :LOL:
         */
    cout << "Wrote " << Loa . Qty () << " 64-bit integers ( " << ( Loa . Qty () * sizeof ( uint64_t ) ) << " bytes ) to file " << _OF << endl;
    cout << "#=======================" << endl;
}   /* write_data () */

int
bobrabotka ()
{
    try {
            /*  First we do need something extra cool
             */
        LoA Loa ( _DF );

            /*  Second we do prepare writer
             */
        write_data ( Loa );
    }
    catch ( exception & E ) {
        cerr << "Exception handled : " << E . what () << endl;
        return 1;
    }
    catch ( ... ) {
        cerr << "Unknown exception handled" << endl;
        return 1;
    }

    return 0;
}
