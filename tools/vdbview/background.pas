unit background;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, jobdef, linkedlist, svdb_obj, selector_types;

Type

    TOnJobDone = procedure( aJob : PJob ) of Object;

    { TBackground }

    TBackground = class( TThread )
    private
      FOnJobDone : TOnJobDone;
      FInputQ    : TMutexJobQ;
      FJob       : PJob;
      FSvdb      : TSvdb_obj;
      FIsOpen    : boolean;

      procedure exec_cmd_cell;
      procedure exec_cmd_colcount;
      procedure exec_cmd_colname;
      procedure exec_cmd_colsdone;
      procedure exec_cmd_open;
      procedure exec_cmd_close;
      procedure exec_cmd_opened;
      procedure exec_cmd_opentab;
      procedure exec_cmd_search;
      procedure exec_cmd_tablename;

    protected
      procedure Execute; override;
      procedure Notify;
    public
      constructor Create( OnJobDone : TOnJobDone );
      destructor Destroy; override;
      function PutJob( aJob : PJob ) : boolean;
      procedure FlushJobs( q : TJobQ );
    end;

implementation

function mask_the_backslash( const s : string ) : string;
var i : Integer;
begin
  Result := '';
  for i:= 1 to Length( s )
    do begin
    if ( s[ i ] = '\' )
      then Result := Result + '/'
      else Result := Result + s[ i ];
    end;
end;

{ TBackground }

{ **************************************************
  IN : S1...acc, S2...path,
  OUT: I1...open( 0=no, 1=yes )
       I2...isDb( 0=no, 1=yes )
       I3...Tablecount
  ************************************************** }
procedure TBackground.exec_cmd_open;
begin
  FJob^.I1 := 0;
  FJob^.I2 := 0;
  if ( FIsOpen )
    then exec_cmd_close;
  if ( Length( FJob^.S1 ) > 0 )
    then begin
    FJob^.S2 := FSvdb.Acc2Path( FJob^.S1 );
    end;
  if ( Length( FJob^.S2 ) > 0 )
    then if FSvdb.OpenPath( mask_the_backslash( FJob^.S2 ) )
           then FIsOpen := True;
  if FIsOpen
    then begin
    FJob^.I1 := 1;
    if FSvdb.IsDb
      then FJob^.I2 := 1;
    FJob^.I3 := FSvdb.CountTabs;
    end;
end;


procedure TBackground.exec_cmd_close;
begin
  if ( FIsOpen )
    then begin
    FSvdb.Close;
    FIsOpen := false;
    end;
end;


{ **************************************************
  IN : I1...index
  OUT: S1...Tablename
  ************************************************** }
procedure TBackground.exec_cmd_tablename;
begin
  if ( FIsOpen )
    then FJob^.S1 := FSvdb.TabName( FJob^.I1 );
end;


{ **************************************************
  IN : I1...tab-idx
     : I2...selector (all,visible)
  OUT: I3...count
  ************************************************** }
procedure TBackground.exec_cmd_colcount;
begin
  if ( FIsOpen )
    then FJob^.I3 := FSvdb.CountCols( FJob^.I1, FJob^.I2 );
end;


{ *** do nothing, just pass through as a signal *** }
procedure TBackground.exec_cmd_opened; begin end;


{ **************************************************
  IN : I1...tab-idx
       I2...selector (all,visible)
       I3...idx
  OUT: S1...name
  ************************************************** }
procedure TBackground.exec_cmd_colname;
begin
  if ( FIsOpen )
    then FJob^.S1 := FSvdb.ColName( FJob^.I1, FJob^.I2, FJob^.I3 );
end;


{ **************************************************
  IN : I1...tab-idx
  OUT: I2...result-code
       I3...RowRange
       S1...Non-Static-Columns as String 'XX-X-X--'
  ************************************************** }
procedure TBackground.exec_cmd_opentab;
var i, n : Integer;
    s : String;
begin
  if ( FIsOpen )
    then begin
    FJob^.I2 := FSvdb.OpenTable( FJob^.I1, '' );
    FJob^.I3 := FSvdb.RowRange( FJob^.I1 );
    n := FSvdb.CountCols( FJob^.I1, ALL_COLUMNS );
    for i := 0 to n - 1
      do begin
      s := FSvdb.ColName( FJob^.I1, ALL_COLUMNS, i );
      if ( FSvdb.ColIdx( FJob^.I1, NON_STATIC_COLUMNS, s ) >= 0 )
        then FJob^.S1 := FJob^.S1 + 'X'
        else FJob^.S1 := FJob^.S1 + '-';
      end;
    end;
end;


{ **************************************************
  IN : I1...tab-idx
       I2...selector (all,visible)
       I3...aCol
       I4...aRow
  OUT: S1...name
  ************************************************** }
procedure TBackground.exec_cmd_cell;
begin
  if ( FIsOpen )
    then FJob^.S1 := FSvdb.GetCell( FJob^.I1, FJob^.I2, FJob^.I3, FJob^.I4 );
end;


{ **************************************************
  IN : I1...tab-idx
       I2...column to search in
       I3...row to start searching at
       I4...forward or backward
       I5...search-chunk
       S1...text to search for
  OUT: I6... >= 0 ... pattern found at this row,
             -1 ... pattern not found
             -2 ... chunk exhausted, pattern not found
  ************************************************** }
procedure TBackground.exec_cmd_search;
begin
  FJob^.I6 := -1;
  if ( FIsOpen )
    then if ( FJob^.I4 = 0 )
           then FJob^.I6 := FSvdb.FindBwd( FJob^.I1, ALL_COLUMNS,
                 FJob^.I2, FJob^.I3, FJob^.I5, FJob^.S1 )
           else FJob^.I6 := FSvdb.FindFwd( FJob^.I1, ALL_COLUMNS,
                 FJob^.I2, FJob^.I3, FJob^.I5, FJob^.S1 );
end;


{ *** do nothing, just pass through as a signal *** }
procedure TBackground.exec_cmd_colsdone; begin end;

procedure TBackground.Execute;
var working : boolean;
begin
  working := false;
  while ( not Terminated ) do
    begin
    if ( FInputQ.Empty )
       then begin
       if working
         then begin
         working := false;
         FJob := Nil;
         Synchronize( @Notify );
         end;
       Sleep( 200 );
       end
       else begin
       working := true;
       FJob := FInputQ.Get;
       case FJob^.cmd of
        CMD_OPEN  : exec_cmd_open;
        CMD_CLOSE : exec_cmd_close;
        CMD_TABLENAME : exec_cmd_tablename;
        CMD_OPENED : exec_cmd_opened;
        CMD_COLCOUNT : exec_cmd_colcount;
        CMD_COLNAME : exec_cmd_colname;
        CMD_COLSDONE : exec_cmd_colsdone;
        CMD_OPENTAB : exec_cmd_opentab;
        CMD_CELL : exec_cmd_cell;
        CMD_SEARCH : exec_cmd_search;
       end;
       Synchronize( @Notify );
       end;
    end;
end;

procedure TBackground.Notify;
begin
  if Assigned( FOnJobDone )
    then FOnJobDone( FJob );
end;

constructor TBackground.Create( OnJobDone : TOnJobDone );
begin
  FreeOnTerminate := True;
  FOnJobDone  := OnJobDone;
  FInputQ     := TMutexJobQ.Create;
  FSvdb       := TSvdb_obj.Create;
  FIsOpen     := false;
  inherited Create( false );
end;

destructor TBackground.Destroy;
begin
  inherited Destroy;
  FSvdb.Destroy;
  FInputQ.Free;
end;

function TBackground.PutJob( aJob : PJob ) : boolean;
begin
  Result := FInputQ.Empty;
  FInputQ.Put( aJob );
end;

procedure TBackground.FlushJobs( q : TJobQ );
begin
  FInputQ.Flush( q );
end;

end.

