unit svdb_obj;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, selector_types, svdb;

type

  { TSvdb_obj }

  TSvdb_obj = class
  private
    { private declarations }
    f_svdb : TSvdb;
    f_obj  : Pointer;

  public
    { public declarations }
    constructor Create;
    destructor Destroy; override;

    function Acc2Path( acc : String ) : String;
    function OpenPath( path : String ) : boolean;
    procedure Close;
    function IsDb : boolean;
    function CountTabs : Integer;
    function TabName( tabid : Integer ) : String;
    function TabIdx( name : String ) : Integer;
    function CountCols( tabid, sel : Integer ) : Integer;
    function ColName( tabid, sel, colid : Integer ) : String;
    function ColIdx( tabid, sel : Integer; name : String ) : Integer;
    function SetColVisible( tabid, sel, colid, visible : Integer ) : Integer;
    function DefTypeIdx( tabid, sel, colid : Integer ) : Integer;
    function CountTypes( tabid, sel, colid : Integer ) : Integer;
    function TypeName( tabid, sel, colid, typeid : Integer ) : String;
    function TypeIdx( tabid, sel, colid : Integer; name : String ) : Integer;
    function TypeDomain( tabid, sel, colid, typeid : Integer ) : Integer;
    function TypeBits( tabid, sel, colid, typeid : Integer ) : Integer;
    function TypeDim( tabid, sel, colid, typeid : Integer ) : Integer;

    function OpenTable( tabid : Integer; defline : String ) : Integer;
    function MaxColnameLen( tabid, sel : Integer ) : Integer;
    procedure SetElemSep( tabid, sel, colid : Integer; sep : String );
    procedure SetDimSep( tabid, sel, colid : Integer; sep : String );
    function IsColEnabled( tabid, sel, colid : Integer ) : boolean;
    function RowRange( tabid : Integer ) : Int64;
    function FirstRow( tabid : Integer ) : Int64;
    function GetCell( tabid, sel, colid : Integer; row : Int64 ) : String;
    function FindFwd( tabid, sel, colid : Integer;
                      row : Int64; chunk : Integer; pattern : String ): Int64;
    function FindBwd( tabid, sel, colid : Integer;
                      row : Int64; chunk : Integer; pattern : String ): Int64;

    function MetaRoot( tabid : Integer ) : Integer;
    function MetaName( tabid, metaid : Integer ) : String;
    function MetaValueLen( tabid, metaid : Integer ) : Integer;
    function MetaValuePrintable( tabid, metaid : Integer ) : boolean;
    function MetaValuePtr( tabid, metaid : Integer ) : Pointer;
    function MetaValue( tabid, metaid, trim : Integer ) : String;
    function MetaChildCount( tabid, metaid : Integer ) : Integer;
    function MetaChildId( tabid, metaid, childid : Integer ) : Integer;

  end;


implementation

{ TSvdb_obj }

constructor TSvdb_obj.Create;
begin
  inherited Create;
  f_svdb := TSvdb.Create;
  f_obj  := Nil;
end;

destructor TSvdb_obj.Destroy;
begin
  f_svdb.Free;
  inherited Destroy;
end;

function TSvdb_obj.Acc2Path( acc : String ) : String;
begin
  Result := f_svdb.Acc2Path( acc );
end;

function TSvdb_obj.OpenPath( path : String ) : boolean;
begin
  Close;
  f_obj := f_svdb.OpenPath( path );
  Result := ( f_obj <> Nil );
end;

procedure TSvdb_obj.Close;
begin
  if ( f_obj <> Nil )
    then begin
    f_svdb.Close( f_obj );
    f_obj := Nil;
    end;
end;

function TSvdb_obj.IsDb : boolean;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.IsDb( f_obj )
    else Result := false;
end;

function TSvdb_obj.CountTabs : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.CountTabs( f_obj )
    else Result := 0;
end;

function TSvdb_obj.TabName( tabid : Integer ) : String;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.TabName( f_obj, tabid )
    else Result := '';
end;

function TSvdb_obj.TabIdx( name : String ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.TabIdx( f_obj, name )
    else Result := -1;
end;

function TSvdb_obj.CountCols( tabid, sel : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.CountCols( f_obj, tabid, sel )
    else Result := 0;
end;

function TSvdb_obj.ColName( tabid, sel, colid : Integer ) : String;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.ColName( f_obj, tabid, sel, colid )
    else Result := '';
end;

function TSvdb_obj.ColIdx( tabid, sel : Integer; name : String ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.ColIdx( f_obj, tabid, sel, name )
    else Result := -1;
end;

function TSvdb_obj.SetColVisible( tabid, sel, colid, visible : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.SetColVisible( f_obj, tabid, sel, colid, visible )
    else Result := 0;
end;

function TSvdb_obj.DefTypeIdx( tabid, sel, colid : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.DefTypeIdx( f_obj, tabid, sel, colid )
    else Result := 0;
end;

function TSvdb_obj.CountTypes( tabid, sel, colid : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.CountTypes( f_obj, tabid, sel, colid )
    else Result := 0;
end;

function TSvdb_obj.TypeName( tabid, sel, colid, typeid : Integer ) : String;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.TypeName( f_obj, tabid, sel, colid, typeid )
    else Result := '';
end;

function TSvdb_obj.TypeIdx( tabid, sel, colid : Integer; name : String ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.TypeIdx( f_obj, tabid, sel, colid, name )
    else Result := -1;
end;

function TSvdb_obj.TypeDomain( tabid, sel, colid, typeid : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.TypeDomain( f_obj, tabid, sel, colid, typeid )
    else Result := 0;
end;

function TSvdb_obj.TypeBits( tabid, sel, colid, typeid : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.TypeBits( f_obj, tabid, sel, colid, typeid )
    else Result := 0;
end;

function TSvdb_obj.TypeDim( tabid, sel, colid, typeid : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.TypeDim( f_obj, tabid, sel, colid, typeid )
    else Result := 0;
end;

function TSvdb_obj.OpenTable( tabid : Integer; defline : String ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.OpenTable( f_obj, tabid, defline )
    else Result := 0;
end;

function TSvdb_obj.MaxColnameLen( tabid, sel : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.MaxColnameLen( f_obj, tabid, sel )
    else Result := 0;
end;

procedure TSvdb_obj.SetElemSep( tabid, sel, colid : Integer; sep : String);
begin
  if ( f_obj <> Nil )
    then f_svdb.SetElemSep( f_obj, tabid, sel, colid, sep );
end;

procedure TSvdb_obj.SetDimSep( tabid, sel, colid : Integer; sep : String );
begin
  if ( f_obj <> Nil )
    then f_svdb.SetDimSep( f_obj, tabid, sel, colid, sep );
end;

function TSvdb_obj.IsColEnabled( tabid, sel, colid : Integer ) : boolean;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.IsColEnabled( f_obj, tabid, sel, colid )
    else Result := false;
end;

function TSvdb_obj.RowRange( tabid : Integer ) : Int64;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.RowRange( f_obj, tabid )
    else Result := 0;
end;

function TSvdb_obj.FirstRow( tabid : Integer ) : Int64;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.FirstRow( f_obj, tabid )
    else Result := 0;
end;

function TSvdb_obj.GetCell( tabid, sel, colid : Integer; row : Int64 ) : String;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.GetCell( f_obj, tabid, sel, colid, row )
    else Result := '';
end;

function TSvdb_obj.FindFwd( tabid, sel, colid : Integer; row : Int64;
                            chunk : Integer; pattern : String ) : Int64;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.FindFwd( f_obj, tabid, sel, colid, row, chunk, pattern )
    else Result := -1;
end;

function TSvdb_obj.FindBwd( tabid, sel, colid : Integer; row : Int64;
                            chunk : Integer; pattern : String ) : Int64;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.FindBwd( f_obj, tabid, sel, colid, row, chunk, pattern )
    else Result := -1;
end;

function TSvdb_obj.MetaRoot( tabid : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.MetaRoot( f_obj, tabid )
    else Result := -1;
end;

function TSvdb_obj.MetaName( tabid, metaid : Integer ) : String;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.MetaName( f_obj, tabid, metaid )
    else Result := '';
end;

function TSvdb_obj.MetaValueLen( tabid, metaid : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.MetaValueLen( f_obj, tabid, metaid )
    else Result := -1;
end;

function TSvdb_obj.MetaValuePrintable( tabid, metaid : Integer ) : boolean;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.MetaValuePrintable( f_obj, tabid, metaid )
    else Result := false;
end;

function TSvdb_obj.MetaValuePtr( tabid, metaid : Integer ) : Pointer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.MetaValuePtr( f_obj, tabid, metaid )
    else Result := Nil;
end;

function TSvdb_obj.MetaValue( tabid, metaid, trim : Integer ) : String;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.MetaValue( f_obj, tabid, metaid, trim )
    else Result := '';
end;

function TSvdb_obj.MetaChildCount( tabid, metaid : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.MetaChildCount( f_obj, tabid, metaid )
    else Result := -1;
end;

function TSvdb_obj.MetaChildId( tabid, metaid, childid : Integer ) : Integer;
begin
  if ( f_obj <> Nil )
    then Result := f_svdb.MetaChildId( f_obj, tabid, metaid, childid )
    else Result := -1;
end;

end.

