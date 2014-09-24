unit svdb;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, dynlibs;

type

  TAcc2Path = function( acc : PChar ) : PChar; cdecl;
  TOpenPath = function( path : PChar ) : Pointer; cdecl;
  TClose = procedure( obj : Pointer ); cdecl;
  TLastErr = function : PChar; cdecl;
  TIsDb = function( obj : Pointer ) : Integer; cdecl;
  TCountTabs = function( obj : Pointer ) : Integer; cdecl;
  TTabName = function( obj : Pointer; tabid : Integer ) : PChar; cdecl;
  TTabIdx = function( obj : Pointer; name : PChar ) : Integer; cdecl;
  TCountCols = function( obj : Pointer; tabid, sel : Integer ) : Integer; cdecl;
  TColName = function( obj : Pointer;
                       tabid, sel, colid : Integer ) : PChar; cdecl;
  TColIdx = function( obj : Pointer; tabid, sel : Integer;
                      name : PChar ) : Integer; cdecl;
  TColVisibility = function( obj : Pointer; tabid, sel,
                             col_id, visible : Integer ) : Integer; cdecl;
  TDefTypeIdx = function( obj : Pointer; tabid, sel,
                          colid : Integer ) : Integer; cdecl;
  TCountTypes = function( obj : Pointer; tabid, sel,
                          colid : Integer ) : Integer; cdecl;
  TTypeName = function( obj : Pointer; tabid, sel,
                        colid, typeid : Integer ) : PChar; cdecl;
  TTypeIdx = function( obj : Pointer; tabid, sel, colid : Integer;
                       name : PChar ) : Integer; cdecl;
  TTypeDomain = function( obj : Pointer; tabid, sel,
                          colid, typeid : Integer ) : Integer; cdecl;
  TTypeBits = function( obj : Pointer; tabid, sel,
                        colid, typeid : Integer ) : Integer; cdecl;
  TTypeDim = function( obj : Pointer; tabid, sel,
                       colid, typeid : Integer ) : Integer; cdecl;

  TOpenTable = function( obj : Pointer; tabid : Integer;
                         defline : PChar ) : Integer; cdecl;
  TMaxColnameLen = function( obj : Pointer;
                             tabid, sel : Integer ) : Integer; cdecl;
  TSetElemSep = procedure( obj : Pointer; tabid, sel,
                           colid : Integer; sep : PChar ); cdecl;
  TSetDimSep = procedure( obj : Pointer; tabid, sel,
                          colid : Integer; sep : PChar ); cdecl;
  TIsColEnabled = function( obj : Pointer; tabid, sel,
                            colid : Integer ) : Integer; cdecl;
  TGetRowRange = function( obj : Pointer; tabid : Integer ) : Int64; cdecl;
  TGetFirstRow = function( obj : Pointer; tabid : Integer ) : Int64; cdecl;
  TGetCell = function( obj : Pointer; dst : PChar;
                       dst_len, tab_id, sel, col_id : Integer;
                       row : Int64 ) : Integer; cdecl;

  TFindFwd = function( obj : Pointer; tabid, sel, colid : Integer;
                       row : Int64; chunk : Integer; pattern : PChar ): Int64; cdecl;
  TFindBwd = function( obj : Pointer; tabid, sel, colid : Integer;
                       row : Int64; chunk : Integer; pattern : PChar ): Int64; cdecl;

  TMetaRoot = function( obj : Pointer; tabid : Integer ) : Integer; cdecl;
  TMetaName = function( obj : Pointer; tabid, metaid : Integer;
                        dst :PChar; dst_len : Integer ) : Integer; cdecl;
  TMetaValueLen = function( obj : Pointer;
                            tabid, metaid : Integer ) : Integer; cdecl;
  TMetaValuePrintable = function( obj : Pointer;
                            tabid, metaid : Integer ) : Integer; cdecl;
  TMetaValuePtr = function( obj : Pointer;
                            tabid, metaid : Integer ) : Pointer; cdecl;
  TMetaValue = function( obj : Pointer; tabid, metaid : Integer;
                         dst :PChar; dst_len, trim : Integer ) : Integer; cdecl;
  TMetaChildCount = function( obj : Pointer;
                         tabid, metaid : Integer ) : Integer; cdecl;
  TMetaChildId = function( obj : Pointer;
                         tabid, metaid, childid : Integer ) : Integer; cdecl;

  { TSvdb }

  TSvdb = class
  private
    { private declarations }
    f_mylib : TLibHandle;

    f_acc2path   : TAcc2Path;
    f_openpath   : TOpenPath;
    f_lasterr    : TLastErr;
    f_close      : TClose;
    f_isdb       : TIsDb;
    f_countTabs  : TCountTabs;
    f_tabName    : TTabName;
    f_tabIdx     : TTabIdx;
    f_countCols  : TCountCols;
    f_colName    : TColName;
    f_colIdx     : TColIdx;
    f_colVisible : TColVisibility;
    f_defTypeIdx : TDefTypeIdx;
    f_countTypes : TCountTypes;
    f_typeName   : TTypeName;
    f_typeIdx    : TTypeIdx;
    f_typeDomain : TTypeDomain;
    f_typeBits   : TTypeBits;
    f_typeDim    : TTypeDim;

    f_openTab      : TOpenTable;
    f_maxColNameLen : TMaxColnameLen;
    f_setElemSep   : TSetElemSep;
    f_setDimSep    : TSetDimSep;
    f_isColEnabled : TIsColEnabled;
    f_getRowRange  : TGetRowRange;
    f_getFirstRow  : TGetFirstRow;
    f_getCell      : TGetCell;
    f_findFwd      : TFindFwd;
    f_findBwd      : TFindBwd;

    f_metaRoot     : TMetaRoot;
    f_metaName     : TMetaName;
    f_metaValueLen : TMetaValueLen;
    f_metaValuePrintable : TMetaValuePrintable;
    f_metaValuePtr : TMetaValuePtr;
    f_metaValue    : TMetaValue;
    f_metaChildCount : TMetaChildCount;
    f_metaChildId  : TMetaChildId;

    function connect( dll_name : String ) : boolean;
    procedure disconnect;
    procedure clear;
    function GetCellN( obj : Pointer; tabid, sel, colid, size : Integer;
                       row : Int64; var needed : Integer ) : String;
    function MetaNameN( obj : Pointer; tabid, metaid, size : Integer) : String;
    function MetaValueN( obj : Pointer; tabid, metaid, size, trim : Integer ) : String;
  public
    { public declarations }
    constructor Create;
    destructor Destroy; override;

    function LastErr : String;
    function Acc2Path( acc : String ) : String;
    function OpenPath( path : String ) : Pointer;
    procedure Close( obj : Pointer );
    function IsDb( obj : Pointer ) : boolean;
    function CountTabs( obj : Pointer ) : Integer;
    function TabName( obj : Pointer; tabid : Integer ) : String;
    function TabIdx( obj : Pointer; name : String ) : Integer;
    function CountCols( obj : Pointer; tabid, sel : Integer ) : Integer;
    function ColName( obj : Pointer; tabid, sel, colid : Integer ) : String;
    function ColIdx( obj : Pointer; tabid, sel : Integer; name : String ) : Integer;
    function SetColVisible( obj : Pointer; tabid, sel, colid, visible : Integer ) : Integer;
    function DefTypeIdx( obj : Pointer; tabid, sel, colid : Integer ) : Integer;
    function CountTypes( obj : Pointer; tabid, sel, colid : Integer ) : Integer;
    function TypeName( obj : Pointer; tabid, sel, colid, typeid : Integer ) : String;
    function TypeIdx( obj : Pointer; tabid, sel, colid : Integer; name : String ) : Integer;
    function TypeDomain( obj : Pointer; tabid, sel, colid, typeid : Integer ) : Integer;
    function TypeBits( obj : Pointer; tabid, sel, colid, typeid : Integer ) : Integer;
    function TypeDim( obj : Pointer; tabid, sel, colid, typeid : Integer ) : Integer;

    function OpenTable( obj : Pointer; tabid : Integer; defline : String ) : Integer;
    function MaxColnameLen( obj : Pointer; tabid, sel : Integer ) : Integer;
    procedure SetElemSep( obj : Pointer; tabid, sel, colid : Integer; sep : String );
    procedure SetDimSep( obj : Pointer; tabid, sel, colid : Integer; sep : String );
    function IsColEnabled( obj : Pointer; tabid, sel, colid : Integer ) : boolean;
    function RowRange( obj : Pointer; tabid : Integer ) : Int64;
    function FirstRow( obj : Pointer; tabid : Integer ) : Int64;
    function GetCell( obj : Pointer; tabid, sel, colid : Integer; row : Int64 ) : String;
    function FindFwd( obj : Pointer;  tabid, sel, colid : Integer;
                      row : Int64; chunk : Integer; pattern : String ) : Int64;
    function FindBwd( obj : Pointer;  tabid, sel, colid : Integer;
                      row : Int64; chunk : Integer; pattern : String ) : Int64;

    function MetaRoot( obj : Pointer; tabid : Integer ) : Integer;
    function MetaName( obj : Pointer; tabid, metaid : Integer ) : String;
    function MetaValueLen( obj : Pointer; tabid, metaid : Integer ) : Integer;
    function MetaValuePrintable( obj : Pointer; tabid, metaid : Integer ) : boolean;
    function MetaValuePtr( obj : Pointer; tabid, metaid : Integer ) : Pointer;
    function MetaValue( obj : Pointer; tabid, metaid, trim : Integer ) : String;
    function MetaChildCount( obj : Pointer; tabid, metaid : Integer ) : Integer;
    function MetaChildId( obj : Pointer; tabid, metaid, childid : Integer ) : Integer;

  end;

implementation

{ TSvdb }

constructor TSvdb.Create;
begin
  inherited;
  clear;
  connect( 'libsvdb.dll' );
end;

destructor TSvdb.Destroy;
begin
  disconnect;
  inherited Destroy;
end;

function TSvdb.LastErr : String;
var s : PChar;
begin
  Result := '';
  if ( f_lasterr <> Nil )
    then begin
    s := f_lasterr();
    if ( s <> Nil )
      then Result := String( s );
    end;
end;

function TSvdb.Acc2Path( acc : String ) : String;
var s : PChar;
begin
  Result := '';
  if ( f_acc2path <> Nil )
    then begin
    s := f_acc2path( PChar( acc ) );
    if ( s <> Nil )
       then Result := String( s );
  end;
end;

function TSvdb.OpenPath( path : String ) : Pointer;
begin
  Result := Nil;
  if ( f_openpath <> Nil )
    then Result := f_openpath( PChar( path ) );
end;

procedure TSvdb.Close( obj : Pointer );
begin
  if ( f_close <> Nil )
    then f_close( obj );
end;

function TSvdb.IsDb( obj : Pointer ) : boolean;
begin
  Result := false;
  if ( f_isdb <> Nil )
    then Result := ( f_isdb( obj ) <> 0 );
end;

function TSvdb.CountTabs( obj : Pointer ) : Integer;
begin
  Result := 0;
  if ( f_countTabs <> Nil )
    then Result := f_countTabs( obj );
end;

function TSvdb.TabName( obj : Pointer; tabid : Integer ) : String;
var p : PChar;
begin
  Result := '';
  if ( f_tabName <> Nil )
    then begin
    p := f_tabName( obj, tabid );
    if ( p <> Nil )
      then Result := String( p );
    end;
end;

function TSvdb.TabIdx( obj : Pointer; name : String ) : Integer;
begin
  Result := -1;
  if ( f_tabIdx <> Nil )
    then Result := f_tabIdx( obj, PChar( name ) );
end;

function TSvdb.CountCols( obj : Pointer; tabid, sel : Integer ) : Integer;
begin
  Result := 0;
  if ( f_countCols <> Nil )
    then Result := f_countCols( obj, tabid, sel );
end;

function TSvdb.ColName( obj : Pointer; tabid, sel, colid : Integer ) : String;
var p : PChar;
begin
  Result := '';
  if ( f_colName <> Nil )
    then begin
    p := f_colName( obj, tabid, sel, colid );
    if ( p <> Nil )
      then Result := String( p );
    end;
end;

function TSvdb.ColIdx( obj : Pointer; tabid, sel : Integer; name : String ) : Integer;
begin
  Result := -1;
  if ( f_colIdx <> Nil )
    then Result := f_colIdx( obj, tabid, sel, PChar( name ) );
end;

function TSvdb.SetColVisible( obj : Pointer; tabid, sel, colid, visible : Integer ) : Integer;
begin
  Result := -1;
  if ( f_colVisible <> Nil )
    then Result := f_colVisible( obj, tabid, sel, colid, visible );
end;

function TSvdb.DefTypeIdx( obj : Pointer; tabid, sel, colid : Integer ) : Integer;
begin
  Result := -1;
  if ( f_defTypeIdx <> Nil )
    then Result := f_defTypeIdx( obj, tabid, sel, colid );
end;

function TSvdb.CountTypes( obj : Pointer; tabid, sel, colid : Integer ) : Integer;
begin
  Result := -1;
  if ( f_countTypes <> Nil )
    then Result := f_countTypes( obj, tabid, sel, colid );
end;

function TSvdb.TypeName( obj : Pointer; tabid, sel, colid, typeid : Integer ) : String;
var p : PChar;
begin
  Result := '';
  if ( f_typeName <> Nil )
    then begin
    p := f_typeName( obj, tabid, sel, colid, typeid );
    if ( p <> Nil )
      then Result := String( p );
    end;
end;

function TSvdb.TypeIdx( obj : Pointer; tabid, sel, colid : Integer; name : String ) : Integer;
begin
  Result := -1;
  if ( f_typeIdx <> Nil )
    then Result := f_typeIdx( obj, tabid, sel, colid, PChar( name ) );
end;

function TSvdb.TypeDomain( obj : Pointer; tabid, sel, colid, typeid : Integer ) : Integer;
begin
  Result := -1;
  if ( f_typeDomain <> Nil )
    then Result := f_typeDomain( obj, tabid, sel, colid, typeid );
end;

function TSvdb.TypeBits( obj : Pointer; tabid, sel, colid, typeid : Integer ) : Integer;
begin
  Result := -1;
  if ( f_typeBits <> Nil )
    then Result := f_typeBits( obj, tabid, sel, colid, typeid );
end;

function TSvdb.TypeDim( obj : Pointer; tabid, sel, colid, typeid : Integer ) : Integer;
begin
  Result := -1;
  if ( f_typeDim <> Nil )
    then Result := f_typeDim( obj, tabid, sel, colid, typeid );
end;

function TSvdb.OpenTable( obj : Pointer; tabid : Integer; defline : String ) : Integer;
begin
  Result := -1;
  if ( f_openTab <> Nil )
    then Result := f_openTab( obj, tabid, PChar( defline ) );
end;

function TSvdb.MaxColnameLen( obj : Pointer; tabid, sel : Integer ) : Integer;
begin
  Result := -1;
  if ( f_maxColNameLen <> Nil )
    then Result := f_maxColNameLen( obj, tabid, sel );
end;

procedure TSvdb.SetElemSep( obj : Pointer; tabid, sel, colid : Integer; sep : String );
begin
  if ( f_setElemSep <> Nil )
    then f_setElemSep( obj, tabid, sel, colid, PChar( sep ) );
end;

procedure TSvdb.SetDimSep( obj : Pointer; tabid, sel, colid : Integer; sep : String );
begin
  if ( f_setDimSep <> Nil )
    then f_setDimSep( obj, tabid, sel, colid, PChar( sep ) );
end;

function TSvdb.IsColEnabled( obj : Pointer; tabid, sel, colid : Integer ) : boolean;
begin
  Result := false;
  if ( f_isColEnabled <> Nil )
    then Result := ( f_isColEnabled( obj, tabid, sel, colid ) <> 0 );
end;

function TSvdb.RowRange( obj : Pointer; tabid : Integer ) : Int64;
begin
  Result := 0;
  if ( f_getRowRange <> Nil )
    then Result := f_getRowRange( obj, tabid );
end;

function TSvdb.FirstRow( obj : Pointer; tabid : Integer ) : Int64;
begin
  Result := 0;
  if ( f_getFirstRow <> Nil )
    then Result := f_getFirstRow( obj, tabid );
end;

function TSvdb.GetCellN( obj : Pointer; tabid, sel, colid, size : Integer;
                         row : Int64; var needed : Integer ) : String;
var p : PChar;
begin
  Result := '';
  needed := size;
  p := StrAlloc( size );
  if ( p <> Nil )
    then begin
    needed := f_getCell( obj, p, size, tabid, sel, colid, row );
    Result := String( p );
    StrDispose( p );
    end;
end;


function TSvdb.GetCell( obj : Pointer; tabid, sel, colid : Integer;
                        row : Int64 ) : String;
var bufsize, needed : Integer;
    p : PChar;
begin
  Result := '';
  bufsize := 250;
  needed := bufsize;
  if ( f_getCell <> Nil )
    then begin
    p := StrAlloc( bufsize );
    if ( p <> Nil )
      then begin
      needed := f_getCell( obj, p, bufsize, tabid, sel, colid, row );
      if ( needed >= bufsize )
        then begin
        StrDispose( p );
        p := StrAlloc( needed + 1 );
        if ( p <> Nil )
          then f_getCell( obj, p, needed + 1, tabid, sel, colid, row );
        end;
      end;
    if ( p <> Nil )
      then begin
      Result := String( p );
      StrDispose( p );
      end;
    end;
end;

function TSvdb.FindFwd( obj : Pointer; tabid, sel, colid : Integer;
                        row : Int64; chunk : Integer; pattern : String ) : Int64;
begin
  Result := -1;
  if ( f_findFwd <> Nil )
    then Result := f_findFwd( obj, tabid, sel, colid, row, chunk, PChar( pattern ) );
end;

function TSvdb.FindBwd( obj : Pointer; tabid, sel, colid : Integer;
                        row : Int64; chunk : Integer; pattern : String ) : Int64;
begin
  Result := -1;
  if ( f_findBwd <> Nil )
    then Result := f_findBwd( obj, tabid, sel, colid, row, chunk, PChar( pattern ) );
end;

function TSvdb.MetaRoot( obj : Pointer; tabid : Integer ) : Integer;
begin
  Result := -1;
  if ( f_metaRoot <> Nil )
    then Result := f_metaRoot( obj, tabid );
end;


function TSvdb.MetaNameN( obj : Pointer; tabid, metaid, size : Integer ) : String;
var p : PChar;
    written : Integer;
begin
  Result := '';
  p := StrAlloc( size );
  if ( p <> Nil )
    then begin
    written := f_metaName( obj, tabid, metaid, p, size );
    if ( written < ( size - 1 ) )
      then Result := String( p );
    StrDispose( p );
    end;
end;


function TSvdb.MetaName( obj: Pointer ; tabid, metaid : Integer ) : String;
var size : Integer;
begin
  Result := '';
  size := 512;
  if ( f_metaName <> Nil )
    then while ( ( Length( Result ) = 0 )and( size < 128000 ) )
      do begin
      size := size + size;
      Result := MetaNameN( obj, tabid, metaid, size );
      end;
end;

function TSvdb.MetaValueLen( obj : Pointer;
                             tabid, metaid : Integer ) : Integer;
begin
  Result := -1;
  if ( f_metaValueLen <> Nil )
    then Result := f_metaValueLen( obj, tabid, metaid );
end;

function TSvdb.MetaValuePrintable( obj : Pointer;
                                   tabid, metaid : Integer ) : boolean;
begin
  Result := false;
  if ( f_metaValuePrintable <> Nil )
    then Result := ( f_metaValuePrintable( obj, tabid, metaid ) <> 0 );
end;

function TSvdb.MetaValuePtr( obj : Pointer; tabid, metaid : Integer ) : Pointer;
begin
  Result := Nil;
  if ( f_metaValuePtr <> Nil )
    then Result := f_metaValuePtr( obj, tabid, metaid );
end;

function TSvdb.MetaValueN( obj : Pointer; tabid, metaid, size, trim : Integer ) : String;
var p : PChar;
    written : Integer;
begin
  Result := '';
  p := StrAlloc( size );
  if ( p <> Nil )
    then begin
    written := f_metaValue( obj, tabid, metaid, p, size, trim );
    if ( written < ( size - 1 ) )
      then Result := String( p );
    StrDispose( p );
    end;
end;

function TSvdb.MetaValue( obj : Pointer; tabid, metaid, trim : Integer ) : String;
var size : Integer;
begin
  Result := '';
  size := 512;
  if ( f_metaValue <> Nil )
    then while ( ( Length( Result ) = 0 )and( size < 128000 ) )
      do begin
      size := size + size;
      Result := MetaValueN( obj, tabid, metaid, size, trim );
      end;
end;

function TSvdb.MetaChildCount( obj : Pointer;
                               tabid, metaid : Integer ) : Integer;
begin
  Result := -1;
  if ( f_metaChildCount <> Nil )
    then Result := f_metaChildCount( obj, tabid, metaid );
end;

function TSvdb.MetaChildId( obj : Pointer;
                            tabid, metaid, childid : Integer ) : Integer;
begin
  Result := -1;
  if ( f_metaChildId <> Nil )
    then Result := f_metaChildId( obj, tabid, metaid, childid );

end;


function TSvdb.connect( dll_name: String ) : boolean;
begin
  f_mylib  := LoadLibrary( dll_name );
  Result := ( f_mylib <> NilHandle );
  if ( Result )
    then begin
    f_acc2path   := TAcc2Path( GetProcedureAddress( f_mylib, 'svdb_accession_2_path' ) );
    f_openpath   := TOpenPath( GetProcedureAddress( f_mylib, 'svdb_open_path' ) );
    f_lasterr    := TLastErr( GetProcedureAddress( f_mylib, 'svdb_last_err' ) );
    f_close      := TClose( GetProcedureAddress( f_mylib, 'svdb_close' ) );
    f_isdb       := TIsDb( GetProcedureAddress( f_mylib, 'svdb_is_db' ) );
    f_countTabs  := TCountTabs( GetProcedureAddress( f_mylib, 'svdb_count_tabs' ) );
    f_tabName    := TTabName( GetProcedureAddress( f_mylib, 'svdb_tabname' ) );
    f_tabIdx     := TTabIdx( GetProcedureAddress( f_mylib, 'svdb_tab_idx' ) );
    f_countCols  := TCountCols( GetProcedureAddress( f_mylib, 'svdb_count_cols' ) );
    f_colName    := TColName( GetProcedureAddress( f_mylib, 'svdb_colname' ) );
    f_colIdx     := TColIdx( GetProcedureAddress( f_mylib, 'svdb_col_idx' ) );
    f_colVisible := TColVisibility( GetProcedureAddress( f_mylib, 'svdb_set_column_visibility' ) );
    f_defTypeIdx := TDefTypeIdx( GetProcedureAddress( f_mylib, 'svdb_dflt_type_idx' ) );
    f_countTypes := TCountTypes( GetProcedureAddress( f_mylib, 'svdb_count_types' ) );
    f_typeName   := TTypeName( GetProcedureAddress( f_mylib, 'svdb_typename' ) );
    f_typeIdx    := TTypeIdx( GetProcedureAddress( f_mylib, 'svdb_type_idx' ) );
    f_typeDomain := TTypeDomain( GetProcedureAddress( f_mylib, 'svdb_typedomain' ) );
    f_typeBits   := TTypeBits( GetProcedureAddress( f_mylib, 'svdb_typebits' ) );
    f_typeDim    := TTypeDim( GetProcedureAddress( f_mylib, 'svdb_typedim' ) );

    f_openTab       := TOpenTable( GetProcedureAddress( f_mylib, 'svdb_open_table' ) );
    f_maxColNameLen := TMaxColnameLen( GetProcedureAddress( f_mylib, 'svdb_max_colname_length' ) );
    f_setElemSep    := TSetElemSep( GetProcedureAddress( f_mylib, 'svdb_set_elem_separator' ) );
    f_setDimSep     := TSetDimSep( GetProcedureAddress( f_mylib, 'svdb_set_dim_separator' ) );
    f_isColEnabled  := TIsColEnabled( GetProcedureAddress( f_mylib, 'svdb_is_enabled' ) );
    f_getRowRange   := TGetRowRange( GetProcedureAddress( f_mylib, 'svdb_row_range' ) );
    f_getFirstRow   := TGetFirstRow( GetProcedureAddress( f_mylib, 'svdb_first_row' ) );
    f_getCell       := TGetCell( GetProcedureAddress( f_mylib, 'svdb_cell' ) );
    f_findFwd       := TFindFwd( GetProcedureAddress( f_mylib, 'svdb_find_fwd' ) );
    f_findBwd       := TFindBwd( GetProcedureAddress( f_mylib, 'svdb_find_bwd' ) );

    f_metaRoot     := TMetaRoot( GetProcedureAddress( f_mylib, 'svdb_tab_meta_root' ) );
    f_metaName     := TMetaName( GetProcedureAddress( f_mylib, 'svdb_tab_meta_name' ) );
    f_metaValueLen := TMetaValueLen( GetProcedureAddress( f_mylib, 'svdb_tab_meta_value_len' ) );
    f_metaValuePrintable := TMetaValuePrintable( GetProcedureAddress( f_mylib, 'svdb_tab_meta_value_printable' ) );
    f_metaValuePtr := TMetaValuePtr( GetProcedureAddress( f_mylib, 'svdb_tab_meta_value_ptr' ) );
    f_metaValue    := TMetaValue( GetProcedureAddress( f_mylib, 'svdb_tab_meta_value' ) );
    f_metaChildCount := TMetaChildCount( GetProcedureAddress( f_mylib, 'svdb_tab_meta_child_count' ) );
    f_metaChildId  := TMetaChildId( GetProcedureAddress( f_mylib, 'svdb_tab_meta_child_id' ) );

    end;
end;

procedure TSvdb.disconnect;
begin
  if ( f_mylib <> NilHandle )
    then begin
    FreeLibrary( f_mylib );
    clear;
    end;
end;

procedure TSvdb.clear;
begin
  f_mylib := NilHandle;

  f_acc2path   := Nil;
  f_openpath   := Nil;
  f_lasterr    := Nil;
  f_close      := Nil;
  f_isdb       := Nil;
  f_countTabs  := Nil;
  f_tabName    := Nil;
  f_tabIdx     := Nil;
  f_countCols  := Nil;
  f_colName    := Nil;
  f_colIdx     := Nil;
  f_colVisible := Nil;
  f_defTypeIdx := Nil;
  f_countTypes := Nil;
  f_typeName   := Nil;
  f_typeIdx    := Nil;
  f_typeDomain := Nil;
  f_typeBits   := Nil;
  f_typeDim    := Nil;

  f_openTab       := Nil;
  f_maxColNameLen := Nil;
  f_setElemSep    := Nil;
  f_setDimSep     := Nil;
  f_isColEnabled  := Nil;
  f_getRowRange   := Nil;
  f_getFirstRow   := Nil;
  f_getCell       := Nil;
  f_findFwd       := Nil;
  f_findBwd       := Nil;

  f_metaRoot     := Nil;
  f_metaName     := Nil;
  f_metaValueLen := Nil;
  f_metaValuePrintable := Nil;
  f_metaValuePtr := Nil;
  f_metaValue    := Nil;
  f_metaChildCount := Nil;
  f_metaChildId  := Nil;

end;

end.

