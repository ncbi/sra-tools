#include <iostream>
#include <stdexcept>

#include <kfs/mmap.h>
#include <kfs/directory.h>
#include <kfs/file.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/database.h>
#include <vdb/cursor.h>

#include <string.h>
#include <stdlib.h>

using namespace std;

/************************************************
 * Syntax: program -d data -s schema -o outfile *
 ************************************************/
string _PR = "check_test_result";
string _DF = "test_data.bin";
string _SF = "test_data.vschema";
string _TF = "test";

#define _DF_T    "-d"
#define _DF_TH   "data_file"

#define _SF_T    "-s"
#define _SF_TH   "schema_file"

#define _TF_T    "-T"
#define _TF_TH   "target"

void
usage ()
{
    cerr << endl;
    cerr << "USAGE:";
    cerr << " " << _PR;
    cerr << " [ " << _DF_T << " " << _DF_TH << " ]";
    cerr << " [ " << _SF_T << " " << _SF_TH << " ]";
    cerr << " [ " << _TF_T << " " << _TF_TH << " ]";
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

        if ( strcmp ( argv [ llp ], _TF_T ) == 0 ) {
            if ( argc <= llp + 1 ) {
                cerr << "ERROR: '" << _TF_T << "' requests parameter" << endl;
                return false;
            }
            llp ++;
            _TF = argv [ llp ];

            continue;
        }

        cerr << "ERROR: invalid parameter '" << argv [ llp ] << "'" << endl;
        return false;
    }

    cerr << "  DATA: " << _DF << endl;;
    cerr << "SCHEMA: " << _SF << endl;;
    cerr << "TARGET: " << _TF << endl;;

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

class DbR {
public :
    DbR ();
    ~DbR ();

    inline const uint64_t * Data () const { return _data; };
    inline size_t Qty () const { return _qty; };

private :
    void open_db ();
    void read_data ();

    const struct VDBManager * _man;
    struct VSchema * _schema;
    const struct VDatabase * _database;
    const struct VTable * _table;
    const struct VCursor * _cursor;

    uint64_t * _data;
    size_t _qty;
};

DbR :: DbR ()
:   _man ( NULL )
,   _schema ( NULL )
,   _database ( NULL )
,   _table ( NULL )
,   _cursor ( NULL )
,   _data ( NULL )
,   _qty ( 0 )
{
    cout << "#=======================" << endl;
    cout << "Reading uint64_t array from database " << _TF << endl;
    open_db ();
    read_data ();
    cout << "Read " << _qty << " uint64_t integers" << endl;
    cout << "#=======================" << endl;
}   /* DbR :: DbR () */

DbR :: ~DbR ()
{
    if ( _data != NULL ) {
        delete [] _data;
        _data = NULL;
    }
    _qty = 0;

    if ( _cursor != NULL ) {
        VCursorRelease ( _cursor );
        _cursor = NULL;
    }
    if ( _table != NULL ) {
        VTableRelease ( _table );
        _table = NULL;
    }
    if ( _database != NULL ) {
        VDatabaseRelease ( _database );
        _database = NULL;
    }
    if ( _schema != NULL ) {
        VSchemaRelease ( _schema );
        _schema = NULL;
    }
    if ( _man != NULL ) {
        VDBManagerRelease ( _man );
        _man = NULL;
    }
}   /* DbR :: ~DbR () */

void
DbR :: open_db ()
{
    struct KDirectory * Nat;
    if ( KDirectoryNativeDir ( & Nat ) != 0 ) {
        throw runtime_error ( "Can not get NativeDir!" );
    }

    if ( VDBManagerMakeRead ( & _man,  Nat ) != 0 ) {
        throw runtime_error ( "Can not create VDB manager!" );
    }

    KDirectoryRelease ( Nat );

    if ( VDBManagerMakeSchema ( _man, & _schema ) != 0 ) {
        throw runtime_error ( "Can not create V-Schema!" );
    }

    if ( VSchemaParseFile ( _schema, _SF . c_str () ) != 0 ) {
        throw runtime_error ( "Can not parse V-Schema!" );
    }

    if ( VDBManagerOpenDBRead ( _man, & _database, _schema, _TF . c_str () ) ) {
        throw runtime_error ( "Can not open V-Database!" );
    }

    if ( VDatabaseOpenTableRead ( _database, & _table, "SEQUENCE" ) != 0 ) {
        throw runtime_error ( "Can not open V-Table!" );
    }

    if ( VTableCreateCursorRead ( _table, & _cursor ) != 0 ) {
        throw runtime_error ( "Can not create V-Cursor!" );
    }
}   /* DbR :: open_db () */

void
DbR :: read_data ()
{
    uint32_t ColumnId = 0;
    if ( VCursorAddColumn ( _cursor, & ColumnId, "SIGNAL" ) != 0 ) { 
        throw runtime_error ( "Can not add column to V-Cursor!" );
    }

    if ( VCursorOpen ( _cursor ) != 0 ) {
        throw runtime_error ( "Can not open V-Cursor!" );
    }

    uint32_t EBits = 0;
    uint32_t RLen = 0;

    const void * Base = NULL;

    if ( VCursorCellDataDirect (
                            _cursor,
                            1,
                            ColumnId,
                            & EBits,
                            & Base,
                            NULL,
                            & RLen
                            ) != 0 ) {
        throw runtime_error ( "Can not read V-Cursor!" );
    }

    if ( EBits != ( sizeof ( uint64_t ) * 8 ) ) {
        throw runtime_error ( "Invalid size of readed elements" );
    }

    cerr << "read [" << RLen << "] elems of size [" << EBits << "]" << endl;

    _data = new uint64_t [ RLen ];
    memmove ( _data, Base, sizeof ( uint64_t ) * RLen );
    _qty = RLen;

    VCursorRelease ( _cursor );
    _cursor = NULL;

}   /* DbR :: read_data () */

int
bobrabotka ()
{
    try {
            /*  First we do need something extra cool
             */
        LoA Loa ( _DF );

        DbR Dbr;

        cout << "#=======================" << endl;
        cout << "Comareing reuslts" << endl;

        if ( Loa . Qty () != Dbr . Qty () ) {
            throw runtime_error ( "Different size of readed arrays" );
        }

        for ( size_t llp = 0; llp < Loa . Qty (); llp ++ ) {
            if ( Loa . Data () [ llp ] != Dbr . Data () [ llp ] ) {
                throw runtime_error ( "Data is dirrer" );
            }
        }

        cout << "Comarision was adequate #lol#" << endl;
        cout << "#=======================" << endl;
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
