unit DataProxy;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Dialogs, math, jobdef, background, linkedlist,
  selector_types;

type

  cell = record
    value : String;
    valid : boolean;
  end;

  row = record
    cells : array of cell;
  end;

  TOnCellValid = procedure( const aCol, aRow : Integer ) of Object;
  TOnSearchDone = procedure( const aCol, aRow : Integer;
                             const pattern : String ) of Object;
  TOnOpened = procedure( const path : String; success : Boolean ) of Object;
  TOnTableSwitched = procedure of Object;
  TOnClosed = procedure of Object;
  TOnLED = procedure( state : boolean ) of Object;
  TOnLog = procedure( const s : String ) of Object;

  { TProxy }

  TProxy = class
  private
    rows : array of row;
    FOffset  : Integer;
    FRows    : Integer;
    FCols    : Integer;
    FVisCols : Integer;

    FBackground  : TBackground;
    FJobStock    : TJobQ;
    FTableNames  : TStringList;

    FAllColumnNames : TStringList;
    FVisIdx2AllIdx : array of Integer;
    FAllIdx2VisIdx : array of Integer;

    FOnCellValid  : TOnCellValid;
    FOnOpened     : TOnOpened;
    FOnTableSwitched : TOnTableSwitched;
    FOnClosed     : TOnClosed;
    FOnSearchDone : TOnSearchDone;
    FOnLED        : TOnLED;

    FAccession   : String;
    FOpenPath    : String;
    FNonStatic   : String;
    FTabId       : Integer;
    FIsOpen      : boolean;
    FIsDb        : boolean;
    FTables      : Integer;
    FTableRows   : Integer;
    FSearchChunk : Integer;
    FSearchCancel: boolean;

    procedure CopyRow( const dst, src : Integer );
    function GetCell( aCol, aRow : Integer ): String;
    function GetColCount( sel : Integer ): Integer;
    function GetColName( sel, idx : Integer ): String;
    function GetColVisibleI( idx : Integer ): boolean;
    function GetColVisibleS( aName : String ): boolean;
    function GetTableName( idx : Integer ): String;
    procedure Invalidate( const firstrow, lastrow : Integer );
    procedure SetColVisibleI( idx : Integer ; const AValue : boolean );
    procedure SetColVisibleS( aName : String ; const AValue : boolean );
    procedure SetOffset( const n : Integer );
    procedure AdjustCacheColumnCount( const n : Integer );

    procedure SendJob( cmd : TCmdType;
                       i1, i2, i3, i4, i5, i6 : Integer;
                       s1, s2 : String );
    procedure QueryColName( sel, idx : Integer );
    procedure QueryOpened;
    procedure QueryTablename( idx : Integer );
    procedure QueryColCount( sel : Integer );
    procedure QueryColsDone( sel : Integer );
    procedure QueryCell( sel, aCol, aRow : Integer );
    procedure QueryOpenTab;

    procedure OnJobDone( job : PJob );
    procedure OnOpenJobDone( job : PJob );
    procedure OnOpenedReceived;
    procedure OnCloseJobDone;
    procedure OnColCountReceived( job : PJob );
    procedure OnColNameReceived( job : PJob );
    procedure OnColsDoneReceived( job : PJob );
    procedure OnTableName( job : PJob );
    procedure OnOpenTabReceived( job : PJob );
    procedure OnCellReceived( job : PJob );
    procedure OnSearchReceived( job : PJob );

    procedure SetTabId( const AValue : Integer );
    procedure ShiftRows( const by : Integer );
  public
    constructor Create;
    destructor Destroy; override;
    procedure AdjustCacheRows( const n : Integer );
    procedure Open( path, acc : String );
    procedure AjustVisibleColumns( checked : String );
    procedure Search( fwd : boolean; colidx, startrow : Integer;
                      pattern : String );
    procedure CancelSearch;

    property OnCellValid : TOnCellValid read FOnCellValid write FOnCellValid;
    property OnOpened : TOnOpened read FOnOpened write FOnOpened;
    property OnTableSwitched : TOnTableSwitched read FOnTableSwitched write FOnTableSwitched;
    property OnClosed : TOnClosed read FOnClosed write FOnClosed;
    property OnLED : TOnLED read FOnLED write FOnLED;
    property OnSearchDone : TOnSearchDone read FOnSearchDone write FOnSearchDone;

    property Tables : Integer read FTables;
    property TableName[ idx : Integer ] : String read GetTableName;
    property TableRows : Integer read FTableRows;
    property IsDb : Boolean read FIsDb;
    property Accession : String read FAccession;
    property IsOpen : Boolean read FIsOpen;
    property Path : String read FOpenPath;
    property TabId : Integer read FTabId write SetTabId;
    property ColCount[ sel : Integer ] : Integer read GetColCount;
    property ColName[ sel, idx : Integer ] : String read GetColName;
    property Cell[ aCol, aRow : Integer ] : String read GetCell;
    property RowCount : Integer read FRows;
    property Offset : Integer read FOffset write SetOffset;
    property ColVisibleS[ aName : String ] : boolean read GetColVisibleS write SetColVisibleS;
    property ColVisibleI[ idx : Integer ] : boolean read GetColVisibleI write SetColVisibleI;
    property NonStaticColumns : String read FNonStatic;
  end;


implementation

{ TProxy }

constructor TProxy.Create;
begin
  FOnCellValid := NIL;
  FOnOpened := NIL;
  FOnTableSwitched := NIL;
  FOnClosed := NIL;
  FOnLED := NIL;
  FOnSearchDone := NIL;

  FOffset := 0;
  FRows   := 0;
  FCols   := 1;
  FVisCols:= 1;
  FTabId  := 0;
  FTableRows := 0;
  FOpenPath  := '';
  FAccession := '';
  FNonStatic := '';
  FSearchChunk := 1000;
  AdjustCacheRows( 10 );

  FTableNames  := TStringList.Create;
  FAllColumnNames := TStringList.Create;

  SetLength( FVisIdx2AllIdx, 1 );
  FVisIdx2AllIdx[ 0 ] := 0;
  SetLength( FAllIdx2VisIdx, 1 );
  FAllIdx2VisIdx[ 0 ] := 0;

  FJobStock    := TJobQ.Create;
  FBackground  := TBackground.Create( @OnJobDone );
end;

destructor TProxy.Destroy;
begin
  FBackground.Terminate;
  FJobStock.Free;
  FTableNames.Free;
  FAllColumnNames.Free;
  SetLength( FVisIdx2AllIdx, 0 );
  SetLength( FAllIdx2VisIdx, 0 );
  inherited Destroy;
end;

procedure TProxy.AdjustCacheRows( const n : Integer );
var i : Integer;
begin
  if ( n < FRows )
     then for i := n to FRows-1
       do SetLength( rows[ i ].cells, 0 );
  SetLength( rows, n );
  if ( n > FRows )
     then for i := FRows to n-1
       do SetLength( rows[ i ].cells, FCols );
  FRows := n;
end;

procedure TProxy.AdjustCacheColumnCount( const n : Integer );
var i : Integer;
begin
  for i := 0 to FRows - 1
    do SetLength( rows[i].cells, n );
  SetLength( FVisIdx2AllIdx, n );
  SetLength( FAllIdx2VisIdx, n );
  for i := 0 to n - 1
    do begin
    FVisIdx2AllIdx[ i ] := i;
    FAllIdx2VisIdx[ i ] := i;
    end;
  FCols := n;
  FVisCols := n;
end;

procedure TProxy.Invalidate( const firstrow, lastrow : Integer );
var i, j : Integer;
begin
  for i := firstrow to lastrow
    do begin
    for j:= 0 to FCols - 1
      do begin
      rows[ i ].cells[ j ].value := '';
      rows[ i ].cells[ j ].valid := false;
      end;
    end;
end;

procedure TProxy.SetColVisibleI( idx : Integer ; const AValue: boolean );
var i, j : Integer;
begin
  if ( idx >= 0 )and( idx < FCols )
    then begin
    if ( AValue )
      then FAllIdx2VisIdx[ idx ] := idx
      else FAllIdx2VisIdx[ idx ] := -1;
    j := 0;
    for i := 0 to FCols - 1
      do begin
      if ( FAllIdx2VisIdx[ i ] >= 0 )
        then begin
        FAllIdx2VisIdx[ i ] := j;
        FVisIdx2AllIdx[ j ] := i;
        inc( j );
        end;
      end;
    FVisCols := j;
    end;
end;

procedure TProxy.SetColVisibleS( aName : String ; const AValue : boolean );
begin
  SetColVisibleI( FAllColumnNames.IndexOf( aName ), AValue );
end;

procedure TProxy.CopyRow( const dst, src : Integer );
var i : Integer;
begin
  for i := 0 to ( FCols - 1 )
    do rows[ dst ].cells[ i ] := rows[ src ].cells[ i ];
end;

function TProxy.GetCell( aCol, aRow : Integer ): String;
var eRow, absCol : Integer;
begin
  Result := '';
  if ( aCol >= 0 )and( aCol < FCols )
    then begin
    eRow := aRow - FOffset;
    absCol := FVisIdx2AllIdx[ aCol ];
    if ( absCol >= 0 )and( absCol < Length( rows[0].cells ) )and
       ( eRow >= 0 )and( eRow < Length( rows ) )
      then if ( rows[ eRow ].cells[ absCol ].valid )
             then Result := rows[ eRow ].cells[ absCol ].value
             else QueryCell( ALL_COLUMNS, absCol, aRow );
    end;
end;

function TProxy.GetColCount( sel : Integer ) : Integer;
begin
  Result := 0;
  case sel of
    ALL_COLUMNS : Result := FCols;
    VISIBLE_COLUMNS : Result := FVisCols;
  end;
end;

function TProxy.GetColName( sel, idx : Integer ) : String;
begin
  Result := '';
  if ( idx >= 0 )and ( idx < FCols )
    then begin
    if ( sel = VISIBLE_COLUMNS )
      then idx := FVisIdx2AllIdx[ idx ];
    if ( idx >= 0 )and( idx < FAllColumnNames.Count )
      then Result := FAllColumnNames[ idx ];
    end;
end;

function TProxy.GetColVisibleI( idx : Integer ): boolean;
begin
  if ( idx >= 0 ) and ( idx < FCols )
    then Result := ( FAllIdx2VisIdx[ idx ] >= 0 )
    else Result := false;
end;

function TProxy.GetColVisibleS( aName : String ) : boolean;
begin
  Result := GetColVisibleI( FAllColumnNames.IndexOf( aName ) );
end;

function TProxy.GetTableName( idx : Integer ) : String;
begin
  if ( idx >= 0 )and( idx < FTableNames.Count )
     then Result := FTableNames[ idx ]
     else Result := '?';
end;

procedure TProxy.ShiftRows( const by : Integer );
var i : Integer;
begin
  if ( by > 0 )
    then for i := 0 to ( ( FRows - 1 ) - by )
           do CopyRow( i, i + by )
    else for i := FRows - 1 downto ( - by )
           do CopyRow( i, i + by )
end;

procedure TProxy.SetOffset( const n : Integer );
var diff : Integer;
begin
  diff := ( n - FOffset );
  if ( diff <> 0 )
    then begin
    FOffset := n;
    if ( abs( diff ) >= FRows )
      then begin
      FBackground.FlushJobs( FJobStock );
      Invalidate( 0, FRows - 1 ); { invalidate all }
      end
      else begin
      ShiftRows( diff );
      if ( diff > 0 )
        then Invalidate( FRows - diff, FRows - 1 )
        else Invalidate( 0, ( -diff ) - 1 );
      end;
    end;
end;

procedure TProxy.SendJob( cmd : TCmdType;
                          i1, i2, i3, i4, i5, i6 : Integer;
                          s1, s2 : String );
var job : PJob;
begin
  job := FJobStock.GetOrMake;
  if Assigned( job )
    then begin
    job^.cmd := cmd;
    job^.I1  := i1;
    job^.I2  := i2;
    job^.I3  := i3;
    job^.I4  := i4;
    job^.I5  := i5;
    job^.I6  := i6;
    job^.S1  := s1;
    job^.S2  := s2;
    if FBackground.PutJob( job )
      then if Assigned( FOnLED )
             then FOnLED( true );
    end;
end;

procedure TProxy.Open( path, acc : String );
begin SendJob( CMD_OPEN, 0, 0, 0, 0, 0, 0, acc, path ); end;

procedure TProxy.AjustVisibleColumns( checked : String );
var i, l : Integer;
    vis  : boolean;
begin
  l := Min( Length( checked ), FCols );
  for i := 0 to l - 1
    do begin
    vis := ( checked[ i + 1 ] = 'X' );
    if ( vis <> ColVisibleI[ i ] )
      then ColVisibleI[ i ] := vis;
    end;
end;

procedure TProxy.Search( fwd : boolean;
                         colidx, startrow : Integer;
                         pattern: String );
var i_fwd, absCol : Integer;
begin
  if fwd then i_fwd := 1 else i_fwd := 0;
  if ( colidx >= 0 )and( colidx < FCols )
    then begin
    absCol := FVisIdx2AllIdx[ colidx ];
    if ( absCol >= 0 )
      then begin
      SendJob( CMD_SEARCH,
               FTabId, absCol, startrow, i_fwd, FSearchChunk, 0,
               pattern, '' );
      FSearchCancel := false;
      end;
    end;
end;

procedure TProxy.CancelSearch;
begin
  FSearchCancel := true;
end;

procedure TProxy.QueryTablename( idx : Integer );
begin SendJob( CMD_TABLENAME, idx, 0, 0, 0, 0, 0, '', '' ); end;

procedure TProxy.QueryColCount( sel : Integer );
begin SendJob( CMD_COLCOUNT, FTabId, sel, 0, 0, 0, 0, '', '' ); end;

procedure TProxy.QueryColsDone( sel : Integer );
begin SendJob( CMD_COLSDONE, sel, 0, 0, 0, 0, 0, '', '' ); end;

procedure TProxy.QueryCell( sel, aCol, aRow : Integer );
begin SendJob( CMD_CELL, FTabId, sel, aCol, aRow, 0, 0, '', '' ); end;

procedure TProxy.QueryOpenTab;
begin SendJob( CMD_OPENTAB, FTabId, 0, 0, 0, 0, 0, '', '' ); end;

procedure TProxy.QueryOpened;
begin SendJob( CMD_OPENED, 0, 0, 0, 0, 0, 0, '', '' ); end;

procedure TProxy.QueryColName( sel, idx : Integer );
begin SendJob( CMD_COLNAME, FTabId, sel, idx, 0, 0, 0, '', '' ); end;


{ **************************************************
  S1...acc, S2...path,
  I1...open( 0=no, 1=yes )
  I2...isDb( 0=no, 1=yes )
  I3...number of tables
  ************************************************** }
procedure TProxy.OnOpenJobDone( job : PJob );
var i : Integer;
begin
  FTableNames.Clear;
  if ( job^.I1 > 0 )
    then begin
      FAccession := job^.S1;
      FOpenPath  := job^.S2;
      FIsOpen    := true;
      FIsDb      := ( job^.I2 > 0 );
      FTables    := job^.I3;
      for i := 0 to FTables-1
        do QueryTablename( i );
      QueryOpened;
    end
    else begin
      FAccession := '';
      FOpenPath  := '';
      FIsOpen    := false;
      FIsDb      := false;
      FTables    := 0;
      if Assigned( FOnOpened )
        then FOnOpened( job^.S2, false );
    end;
end;

procedure TProxy.OnCloseJobDone;
begin
  if Assigned( FOnClosed )
    then FOnClosed;
end;

procedure TProxy.OnTableName( job : PJob );
begin
  FTableNames.Add( job^.S1 );
end;

procedure TProxy.OnOpenedReceived;
begin
  if Assigned( FOnOpened )
    then FOnOpened( FOpenPath, true );
end;

procedure TProxy.OnColCountReceived( job : PJob );
var i : Integer;
begin
  for i:=0 to job^.I3 - 1
    do QueryColName( job^.I2, i );
  QueryColsDone( job^.I2 );
end;

procedure TProxy.OnColNameReceived( job : PJob );
begin
  case job^.I2 of
    ALL_COLUMNS : FAllColumnNames.Add( job^.S1 );
  end;
end;

procedure TProxy.OnColsDoneReceived( job : PJob );
begin
  case job^.I1 of
    ALL_COLUMNS : QueryOpenTab;
  end;
end;

procedure TProxy.OnOpenTabReceived( job : PJob );
begin
  AdjustCacheColumnCount( FAllColumnNames.Count );
  FTableRows := job^.I3;
  FNonStatic := job^.S1;
  if Assigned( FOnTableSwitched )
    then FOnTableSwitched;
end;

procedure TProxy.OnCellReceived( job : PJob );
var aRow, aCol, visCol : Integer;
begin
  aCol := job^.I3; { is in all-columns-index }
  aRow := job^.I4 - FOffset;
  if ( aRow >= 0 )and( aRow < FRows )and( aCol >= 0 )and( aCol < FCols )
    then begin
    rows[ aRow ].cells[ aCol ].value := job^.S1;
    rows[ aRow ].cells[ aCol ].valid := true;
    visCol := FAllIdx2VisIdx[ aCol ];
    if Assigned( FOnCellValid ) and ( visCol >= 0 )
      then FOnCellValid( visCol, aRow );
    end;
end;

procedure TProxy.OnSearchReceived( job : PJob );
var aCol, visCol, aRow : Integer;
    fwd, call_handler : boolean;
begin
  if ( not FSearchCancel )
    then begin
    visCol := 0;
    aRow   := -1;
    call_handler := true;
    aCol := job^.I2; { is in all-columns-index }
    if ( aCol >= 0 )and( aCol < FCols )
      then begin
      visCol := FAllIdx2VisIdx[ aCol ];
      if ( visCol >= 0 )
        then aRow := job^.I6;
      if ( aRow = -2 )
        then begin
        { search of next chunk if not found... }
        fwd := ( job^.I4 = 1 );
        if ( fwd )
          then aRow := job^.I3 + FSearchChunk
          else aRow := job^.I3 - FSearchChunk;
        if ( aRow >= 0 )and( aRow < FTableRows )
          then begin
          Search( fwd, visCol, aRow, job^.S1 );
          call_handler := false;
          end
          else begin
          aRow := -1; { not found... }
          end;
        end;
      end;
    if Assigned( FOnSearchDone ) and ( call_handler )
      then FOnSearchDone( visCol, aRow, job^.S1 );
    end;
end;

procedure TProxy.SetTabId( const AValue : Integer );
begin
  FTabId := AValue;
  FAllColumnNames.Clear;
  Invalidate( 0, FRows - 1 ); { InvalidateAll }
  QueryColCount( ALL_COLUMNS );
end;

procedure TProxy.OnJobDone( job : PJob );
begin
  if Assigned( job )
    then begin
    case job^.cmd of
      CMD_OPEN : OnOpenJobDone( job );
      CMD_CLOSE : OnCloseJobDone;
      CMD_TABLENAME : OnTableName( job );
      CMD_OPENED : OnOpenedReceived;
      CMD_COLCOUNT : OnColCountReceived( job );
      CMD_COLNAME : OnColNameReceived( job );
      CMD_COLSDONE : OnColsDoneReceived( job );
      CMD_OPENTAB : OnOpenTabReceived( job );
      CMD_CELL : OnCellReceived( job );
      CMD_SEARCH : OnSearchReceived( job );
    end;
    FJobStock.Put( job );
    end
    else begin
    if Assigned( FOnLED )
      then FOnLED( false );
    end;
end;

end.

