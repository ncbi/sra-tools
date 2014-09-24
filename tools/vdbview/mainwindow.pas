unit MainWindow;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, XMLCfg, FileUtil, Forms, Controls, Graphics, Dialogs,
  ComCtrls, ExtCtrls, Grids, StdCtrls, Buttons, Menus, ActnList, LCLType,
  support_unit, math, selector_types, DataProxy, columnSel, gotoform,
  searchform, clipbrd, types, version;

type

  { TMainform }

  TMainform = class(TForm)
    A_Version: TAction;
    A_ClearHistory: TAction;
    ApplicationProperties1: TApplicationProperties;
    A_Copy: TAction;
    A_Adjust_All_Cellwidths: TAction;
    A_AdjustCellWidth: TAction;
    A_Adjust_All_Headers: TAction;
    A_AdjustHeader: TAction;
    A_Search: TAction;
    A_Goto: TAction;
    A_First: TAction;
    A_Last: TAction;
    A_PageDn: TAction;
    A_PageUp: TAction;
    A_Up: TAction;
    A_Down: TAction;
    A_Columns: TAction;
    A_OpenDir: TAction;
    A_OpenFile: TAction;
    A_OpenAcc: TAction;
    A_Exit: TAction;
    A_ActivateRecord: TAction;
    A_ActivateGrid: TAction;
    ActionList: TActionList;
    LED: TShape;
    MenuColumns: TMenuItem;
    MenuItem1: TMenuItem;
    MenuItem10: TMenuItem;
    MenuItem11: TMenuItem;
    MenuItem2: TMenuItem;
    MI_Recent: TMenuItem;
    MenuItem4: TMenuItem;
    MenuItem5: TMenuItem;
    MenuItem6: TMenuItem;
    MenuItem7: TMenuItem;
    MenuItem8: TMenuItem;
    MenuItem9: TMenuItem;
    PopupMenu1: TPopupMenu;
    B_First: TSpeedButton;
    B_Prev: TSpeedButton;
    B_Up: TSpeedButton;
    B_Down: TSpeedButton;
    B_Next: TSpeedButton;
    B_Last: TSpeedButton;
    B_Search: TSpeedButton;
    B_Goto: TSpeedButton;
    Tables: TComboBox;
    Grid: TDrawGrid;
    MainMenu: TMainMenu;
    MenueFile: TMenuItem;
    MenueView: TMenuItem;
    MI_Exit: TMenuItem;
    MenuGrid: TMenuItem;
    MI_Open_Accession: TMenuItem;
    MI_Open_File: TMenuItem;
    MI_Open_Directory: TMenuItem;
    MenuRecord: TMenuItem;
    SelFileDlg: TOpenDialog;
    Panel1: TPanel;
    RowScroll: TScrollBar;
    ActivateRecord: TSpeedButton;
    ActivateGrid: TSpeedButton;
    cfg: TXMLConfig;
    SelDirDlg: TSelectDirectoryDialog;
    StatusBar: TStatusBar;
    LEDTimer: TTimer;
    procedure A_AdjustCellWidthExecute(Sender: TObject);
    procedure A_AdjustHeaderExecute(Sender: TObject);
    procedure A_Adjust_All_CellwidthsExecute(Sender: TObject);
    procedure A_Adjust_All_HeadersExecute(Sender: TObject);
    procedure A_ClearHistoryExecute(Sender: TObject);
    procedure A_CopyExecute(Sender: TObject);
    procedure A_SearchExecute(Sender: TObject);
    procedure A_ActivateGridExecute(Sender: TObject);
    procedure A_ActivateRecordExecute(Sender: TObject);
    procedure A_ColumnsExecute(Sender: TObject);
    procedure A_DownExecute(Sender: TObject);
    procedure A_ExitExecute(Sender: TObject);
    procedure A_FirstExecute(Sender: TObject);
    procedure A_GotoExecute(Sender: TObject);
    procedure A_LastExecute(Sender: TObject);
    procedure A_OpenAccExecute(Sender: TObject);
    procedure A_OpenDirExecute(Sender: TObject);
    procedure A_OpenFileExecute(Sender: TObject);
    procedure A_PageDnExecute(Sender: TObject);
    procedure A_PageUpExecute(Sender: TObject);
    procedure A_UpExecute(Sender: TObject);
    procedure A_VersionExecute(Sender: TObject);
    procedure FormActivate(Sender: TObject);
    procedure FormClose(Sender: TObject; var CloseAction: TCloseAction);
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure FormResize(Sender: TObject);
    procedure GridDrawCell(Sender: TObject; aCol, aRow: Integer; aRect: TRect;
      aState: TGridDrawState);
    procedure GridHeaderSized(Sender: TObject; IsColumn: Boolean; Index: Integer
      );
    procedure GridKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure GridMouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure GridMouseWheelDown(Sender: TObject; Shift: TShiftState;
      MousePos: TPoint; var Handled: Boolean);
    procedure GridMouseWheelUp(Sender: TObject; Shift: TShiftState;
      MousePos: TPoint; var Handled: Boolean);
    procedure GridSelectCell(Sender: TObject; aCol, aRow: Integer;
      var CanSelect: Boolean);
    procedure LEDTimerTimer(Sender: TObject);
    procedure RowScrollChange(Sender: TObject);
    procedure TablesSelect(Sender: TObject);
  private
    { private declarations }
    fProxy   : TProxy;
    fGridRow : Integer;
    fGridCol : Integer;
    fRecRow  : Integer;
    fRecCol  : Integer;
    fSearch  : String;
    fDrawCtx : TDrawCtx;
    fMouseShift : TShiftState;
    fColWidths : TStringList;

    function CopyGrid: String;
    function CopyRecord: String;
    procedure DrawDataCell( aCol, aRow : Integer; ct: TCellType; aRect: TRect );
    procedure DrawRecordCell( aRow : Integer; ct: TCellType; aRect: TRect );
    procedure GotoRow( const aRow : Integer );
    procedure OnLRUClick( Sender : TObject );
    procedure OnOpened( const path : String; success : boolean );
    procedure OnTableSwitched;
    procedure OnCellInvalidate( const aCol, aRow : Integer );
    procedure OnLED( state : boolean );
    procedure OnSearchDone( const aCol, aRow : Integer;
                            const pattern : String );
    procedure PerformOpen( const path, acc : String );
    procedure AdjustColWidth( aCol : Integer; header : boolean );
    procedure SaveColWidths;
    procedure RestoreColWidths;
  public
    { public declarations }
  end; 

var
  Mainform: TMainform;

implementation

{$R *.lfm}

{ TMainform }

procedure TMainform.FormCreate(Sender: TObject);
var s : String;
begin
  s := ExtractFilePath( Application.ExeName );
  cfg.Filename := s + '\' + cfg.Filename;
  SelDirDlg.InitialDir := s;
  SelFileDlg.InitialDir := s;
  fColWidths := TStringList.Create;
  fProxy := TProxy.Create;
  fProxy.OnOpened := @OnOpened;
  fProxy.OnTableSwitched := @OnTableSwitched;
  fProxy.OnCellValid := @OnCellInvalidate;
  fProxy.OnLED := @OnLED;
  fProxy.OnSearchDone := @OnSearchDone;
  fGridRow := 1;
  fGridCol := 1;
  fRecRow := 1;
  fRecCol := 1;
  fSearch := '';
  A_ActivateGrid.Execute;
end;

procedure TMainform.FormDestroy(Sender: TObject);
begin
  fProxy.Free;
  fColWidths.Free;
end;

procedure TMainform.FormActivate( Sender : TObject );
begin
  setup_position( Mainform, cfg, Screen ); {support_unit}
  PopulateLRUMenue( cfg, MainMenu, MI_Recent, @OnLRUClick );
  fDrawCtx.h := Grid.Canvas.TextHeight( 'X' );
  fDrawCtx.w := Grid.Canvas.TextWidth( 'W' );
  fDrawCtx.g := Grid;
  if ( ParamCount > 0 )
    then PerformOpen( ParamStr( 1 ), '' );
end;

procedure TMainform.FormClose(Sender: TObject; var CloseAction: TCloseAction);
begin
  store_position( Mainform, cfg ); {support_unit}
end;

procedure TMainform.FormResize( Sender : TObject );
var n : Integer;
begin
  if ( ActivateGrid.Down )
    then begin
    adjust_gridrowcount( Grid, fProxy ); {support_unit}
    n := fProxy.TableRows - Grid.RowCount + 1;
    if ( n < Grid.RowCount )
      then n := Grid.RowCount;
    RowScroll.Max := n;
    end
    else Grid.ColWidths[ 1 ] := Grid.ClientWidth - Grid.ColWidths[ 0 ];
end;

procedure TMainform.A_ActivateGridExecute( Sender : TObject );
begin
  { save the row/col for record-view }
  fRecRow := Grid.Row;
  fRecCol := Grid.Col;
  adjust_gridrowcount( Grid, fProxy ); {support_unit}
  Grid.ColCount := fProxy.ColCount[ VISIBLE_COLUMNS ] + 1;
  Grid.ScrollBars := ssHorizontal;
  RowScroll.Visible := True;
  { restore row/col for grid-view }
  Grid.Row := fGridRow;
  Grid.Col := fGridCol;
  Grid.ColWidths[ 0 ] := 96;
  A_Adjust_All_HeadersExecute( Sender );
  RestoreColWidths;
  ActivateGrid.Down := True;
end;

procedure TMainform.A_SearchExecute( Sender : TObject );
var aCol, aRow : Integer;
    p : TPoint;
begin
  if ActivateGrid.Down
    then begin
    aCol := Grid.Col - 1;
    aRow := Grid.Row + RowScroll.Position - 1;
    end
    else begin
    aCol := Grid.Row - 1;
    aRow := fRecRow;
    end;
  if ( Length( fSearch ) < 1 )
    then fSearch := fProxy.Cell[ aCol, aRow ];
  p := Point( Grid.Left, Grid.Top );
  SForm.present( ClientToScreen( p ), aCol, aRow, fSearch, fProxy );
end;

procedure TMainform.SaveColWidths;
var i, idx : Integer;
    s : String;
begin
  for i := 1 to Grid.ColCount - 1
    do begin
    s := fProxy.ColName[ VISIBLE_COLUMNS, i - 1 ];
    idx := fColWidths.IndexOf( s );
    if ( idx < 0 )
      then begin
      fColWidths.Add( s );
      idx := fColWidths.IndexOf( s );
      end;
    if ( idx >= 0 )
      then fColWidths.Objects[ idx ] := TObject( Grid.ColWidths[ i ] );
    end;
end;

procedure TMainform.RestoreColWidths;
var i, idx : Integer;
begin
  if ( ActivateGrid.Down )
    then for i := 1 to Grid.ColCount - 1
    do begin
    idx := fColWidths.IndexOf( fProxy.ColName[ VISIBLE_COLUMNS, i - 1 ] );
    if ( idx >= 0 )
      then Grid.ColWidths[ i ] := Integer( fColWidths.Objects[ idx ] );
    end;
end;

procedure TMainform.A_AdjustHeaderExecute( Sender : TObject );
begin
  if ActivateGrid.Down
    then AdjustColWidth( Grid.Col, true );
end;

procedure TMainform.A_Adjust_All_CellwidthsExecute(Sender: TObject);
var i : Integer;
begin
  if ActivateGrid.Down
    then for i:= 1 to Grid.ColCount - 1
           do AdjustColWidth( i, false );
end;

procedure TMainform.A_AdjustCellWidthExecute( Sender : TObject );
begin
  if ActivateGrid.Down
    then AdjustColWidth( Grid.Col, false );
end;

procedure TMainform.A_Adjust_All_HeadersExecute( Sender : TObject );
var i : Integer;
begin
  if ActivateGrid.Down
    then for i:= 1 to Grid.ColCount - 1
           do AdjustColWidth( i, true );
end;

procedure TMainform.A_ClearHistoryExecute( Sender : TObject );
begin
  ClearLRU( cfg, MI_Recent ); {support_unit}
end;

function TMainform.CopyGrid : String;
var gr : TGridRect;
    row, col, n : Integer;
begin
  Result  := '';
  gr := Grid.Selection;
  for row := gr.Top to gr.Bottom
    do begin
    if ( row > gr.Top )
      then Result := Result + #13;
    for col := gr.Left to gr.Right
      do begin
      if ( col > gr.Left )
        then Result := Result + #9;
      if ( col = 0 )
        then begin
        n := row + RowScroll.Position;
        if ( n <= RowScroll.Max )
           then Result := Result + FloatToStrF( n, ffnumber, 0, 0 );
        end
        else Result := Result + fProxy.Cell[ col - 1, row - 1 ];
      end;
    end;
end;

function TMainform.CopyRecord : String;
var gr : TGridRect;
    row, col : Integer;
begin
  Result := '';
  gr := Grid.Selection;
  for row := gr.Top to gr.Bottom
    do begin
    if ( row > gr.Top )
      then Result := Result + #13;
    for col := gr.Left to gr.Right
      do begin
      if ( col > gr.Left )
        then Result := Result + #9;
      case col of
        0 : Result := Result + fProxy.ColName[ VISIBLE_COLUMNS, row - 1 ];
        1 : Result := Result + fProxy.Cell[ row -1, fGridRow + RowScroll.Position - 1 ];
      end;
      end;
    end;
end;

procedure TMainform.A_CopyExecute( Sender : TObject );
var  s : String;
begin
  if ActivateGrid.Down
    then s := CopyGrid
    else s := CopyRecord;
  if ( Length( s ) > 0 )
    then Clipboard.AsText := s;
end;

procedure TMainform.A_ActivateRecordExecute(Sender: TObject);
begin
  { save the row/col for record-view }
  fGridRow := Grid.Row;
  fGridCol := Grid.Col;
  SaveColWidths;
  Grid.RowCount := fProxy.ColCount[ VISIBLE_COLUMNS ] + 1;
  Grid.ColCount := 2;
  Grid.ScrollBars := ssVertical;
  RowScroll.Visible := False;
  { restore row/col for record-view }
  Grid.Row := fRecRow;
  Grid.Col := fRecCol;
  Grid.ColWidths[ 0 ] := 120;
  Grid.ColWidths[ 1 ] := Grid.ClientWidth - Grid.ColWidths[ 0 ];
  ActivateRecord.Down := True;
  Grid.Invalidate;
end;

procedure TMainform.A_ColumnsExecute( Sender : TObject );
var p : TPoint;
begin
  p := Point( Grid.Left, Grid.Top );
  if ( Columnform.present( ClientToScreen( p ), Grid.ClientHeight, fProxy ) )
    then begin
    fProxy.AjustVisibleColumns( Columnform.checked_items );
    if ( ActivateGrid.Down )
      then Grid.ColCount := FProxy.ColCount[ VISIBLE_COLUMNS ] + 1
      else Grid.RowCount := FProxy.ColCount[ VISIBLE_COLUMNS ] + 1;
    Grid.Invalidate;
    end;
end;

procedure TMainform.A_DownExecute( Sender : TObject );
begin
  if ( Grid.Row < ( Grid.RowCount - 2 ) )
    then Grid.Row := Grid.Row + 1
    else if ( RowScroll.Position < RowScroll.Max )
           then begin
           RowScroll.Position := RowScroll.Position + 1;
           Grid.Invalidate;
           end

end;

procedure TMainform.A_ExitExecute(Sender: TObject);
begin
  Close;
end;

procedure TMainform.A_FirstExecute( Sender : TObject );
begin
  RowScroll.Position := 0;
  Grid.Row := 1;
  Grid.Invalidate;
end;

procedure TMainform.GotoRow( const aRow : Integer );
var nr, n1 : Integer;
begin
  nr := Min( aRow, fProxy.TableRows );
  n1 := ( nr - Grid.Row );
  if ( n1 <= RowScroll.Max )and( n1 >= 0 )
    then RowScroll.Position := n1
    else if ( n1 < 0 )
           then begin
           RowScroll.Position := 0;
           Grid.Row := nr;
           end
           else begin
           RowScroll.Position := RowScroll.Max;
           Grid.Row := nr - RowScroll.Position;
           end;
  Grid.Invalidate;
end;

procedure TMainform.A_GotoExecute( Sender : TObject );
var aRow : Integer;
    p : TPoint;
begin
  p := Point( Grid.Left, Grid.Top );
  aRow := Grid.Row + RowScroll.Position;
  if ( MyGotoForm.present( ClientToScreen( p ), aRow ) )
    then GotoRow( aRow );
end;

procedure TMainform.A_LastExecute( Sender : TObject );
begin
  RowScroll.Position := RowScroll.Max;
  Grid.Row := Grid.RowCount - 1;
  Grid.Invalidate;
end;

procedure TMainform.PerformOpen( const path, acc : String );
begin
  Tables.Clear; { the table-selector ... }
  fProxy.Open( path, acc );
end;

procedure TMainform.OnLRUClick( Sender : TObject );
var s : String;
begin
  s := ( Sender as TMenuItem ).Caption;
  PerformOpen( s, '' );
end;

procedure TMainform.AdjustColWidth( aCol : Integer; header : boolean );
var s : String;
    r, w : Integer;
begin
  w := 0;
  if ( header )
    then begin
    if ( aCol > 0 )
      then begin
      s := fProxy.ColName[ VISIBLE_COLUMNS, aCol - 1 ];
      w := Grid.Canvas.TextWidth( s );
      end;
    end
    else begin
    for r := 0 to Grid.RowCount - 1
      do begin
      s := fProxy.Cell[ aCol-1, r + RowScroll.Position ];
      w := Max( w, Grid.Canvas.TextWidth( s ) );
      end;
    end;
  if ( w > 0 )
    then Grid.ColWidths[ aCol ] := w + 10;
end;

procedure TMainform.A_OpenAccExecute( Sender : TObject );
var acc : String;
begin
  acc := '';
  if ( InputQuery( 'Open accession', 'accession', acc ) )
    then PerformOpen( '', acc );
end;

procedure TMainform.A_OpenDirExecute(Sender: TObject);
begin
  if SelDirDlg.Execute
    then PerformOpen( SelDirDlg.FileName, '' );
end;

procedure TMainform.A_OpenFileExecute(Sender: TObject);
begin
  if SelFileDlg.Execute
    then PerformOpen( SelFileDlg.FileName, '' );
end;

procedure TMainform.A_PageDnExecute( Sender : TObject );
begin
  if ( Grid.Row + 10 <= Grid.RowCount )
    then Grid.Row := Grid.Row + 10
    else if ( RowScroll.Position < ( RowScroll.Max - 10 ) )
           then begin
           RowScroll.Position := RowScroll.Position + 10;
           Grid.Invalidate;
           end
           else begin
           RowScroll.Position := RowScroll.Max;
           Grid.Row := Grid.RowCount - 1;
           Grid.Invalidate;
           end;
end;

procedure TMainform.A_PageUpExecute( Sender : TObject );
begin
  if Grid.Row > 10
    then Grid.Row := Grid.Row - 10
    else if ( RowScroll.Position > 10 )
           then begin
           RowScroll.Position := RowScroll.Position - 10;
           Grid.Invalidate;
           end
           else begin
           RowScroll.Position := 0;
           Grid.Row := 0;
           Grid.Invalidate;
           end;
end;

procedure TMainform.A_UpExecute( Sender : TObject );
begin
  if Grid.Row > 1
    then Grid.Row := Grid.Row - 1
    else if ( RowScroll.Position > 0 )
           then begin
           RowScroll.Position := RowScroll.Position - 1;
           Grid.Invalidate;
           end;
end;

procedure TMainform.A_VersionExecute(Sender: TObject);
begin
  VersionForm.ShowModal;
end;

procedure TMainform.DrawDataCell( aCol, aRow : Integer;
                                  ct : TCellType; aRect : TRect );
var s : String;
    effective_row : Integer;
    center, clip : boolean;
begin
  effective_row := aRow + RowScroll.Position - 1;
  case ct of
    ct_topleft : s := 'row';
    ct_row     : s := Format( '%s', [ FloatToStrF(
                        effective_row + 1, ffnumber, 0, 0 ) ] );
    ct_col     : s := FProxy.ColName[ VISIBLE_COLUMNS, aCol - 1 ];
    ct_content : s := FProxy.Cell[ aCol - 1, effective_row ];
  end;
  clip := ( ct = ct_content );
  center := ( ct = ct_topleft )or( ct = ct_row );
  draw_cell( fDrawCtx, aRect, center, clip, s );
end;

procedure TMainform.DrawRecordCell( aRow : Integer;
                                    ct : TCellType; aRect : TRect );
var s  : String;
    clip : boolean;
begin
  case ct of
    ct_topleft : s := 'column';
    ct_row     : s := FProxy.ColName[ VISIBLE_COLUMNS, aRow - 1 ];
    ct_col     : s := 'value';
    ct_content : s := FProxy.Cell[ aRow - 1, fGridRow + RowScroll.Position - 1 ];
  end;
  clip := ( ct = ct_content );
  draw_cell( fDrawCtx, aRect, false, clip, s );
end;

procedure TMainform.GridDrawCell(Sender: TObject; aCol, aRow: Integer;
  aRect: TRect; aState: TGridDrawState);
begin
  if ActivateGrid.Down
    then DrawDataCell( aCol, aRow, get_celltype( aCol, aRow ), aRect )
    else DrawRecordCell( aRow, get_celltype( aCol, aRow ), aRect );
end;

procedure TMainform.GridHeaderSized( Sender : TObject; IsColumn : Boolean;
                                     Index : Integer );
var h, i : Integer;
begin
  if ( ( not IsColumn )and ( ActivateGrid.Down ) )
    then begin
    if ( ssShift in fMouseShift )
      then begin
      h := Grid.RowHeights[ Index ];
      Grid.RowCount := ( Grid.ClientHeight div h ) + 1;
      for i := 1 to Grid.RowCount - 1
        do Grid.RowHeights[ i ] := h;
      adjust_gridrowcount_to( Grid, fProxy, h );
      end
      else begin
      adjust_gridrowcount( Grid, fProxy );
      end;
    end;
end;

procedure TMainform.GridKeyDown(Sender: TObject; var Key: Word;
  Shift: TShiftState);
begin
  case Key of
    vk_up    : begin A_Up.Execute; Key := 0; end;
    vk_down  : begin A_Down.Execute; Key := 0; end;
    vk_prior : begin A_PageUp.Execute; Key := 0; end;
    vk_next  : begin A_PageDn.Execute; Key := 0; end;
    vk_home  : begin A_First.Execute; Key := 0; end;
    vk_end   : begin A_Last.Execute; Key := 0; end;
  end;
end;

procedure TMainform.GridMouseDown(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
begin
  fMouseShift := Shift;
end;

procedure TMainform.GridMouseWheelDown( Sender : TObject; Shift : TShiftState;
  MousePos : TPoint; var Handled : Boolean );
begin
  A_DownExecute( Sender );
  Handled := True;
end;

procedure TMainform.GridMouseWheelUp( Sender : TObject; Shift : TShiftState;
  MousePos : TPoint; var Handled : Boolean );
begin
  A_UpExecute( Sender );
  Handled := True;
end;

procedure TMainform.GridSelectCell(Sender: TObject; aCol, aRow: Integer;
  var CanSelect: Boolean);
begin
  if ActivateGrid.Down
    then StatusBar.Panels[ 0 ].Text := Format( '%s',
    [ FloatToStrF( aRow + RowScroll.Position, ffnumber, 0, 0 ) ] );

end;

procedure TMainform.LEDTimerTimer( Sender : TObject );
begin
  if ( LEDTimer.Tag = 0 )
    then begin
    LED.Brush.Color := clLime;
    LEDTimer.Tag := 1;
    end
    else begin
    LED.Brush.Color := clGreen;
    LEDTimer.Tag := 0;
    end
end;

procedure TMainform.RowScrollChange( Sender : TObject );
begin
  FProxy.Offset := RowScroll.Position;
  StatusBar.Panels[ 0 ].Text := Format( '%s',
    [ FloatToStrF( Grid.Row + RowScroll.Position, ffnumber, 0, 0 ) ] );
  Grid.Invalidate;
end;

procedure TMainform.TablesSelect( Sender : TObject );
begin
  FProxy.TabId := Tables.ItemIndex; { triggers table-open... }
  Tables.Enabled := false;
end;

procedure TMainform.OnOpened( const path : String; success : boolean );
var i : Integer;
begin
  if ( success )
    then begin
    Mainform.Caption := path;
    Tables.Clear;
    for i:=0 to FProxy.Tables - 1
      do Tables.Items.Add( FProxy.TableName[ i ] );
    Tables.ItemIndex := 0;
    TablesSelect( Tables );
    SaveLRU( path, cfg );
    PopulateLRUMenue( cfg, MainMenu, MI_Recent, @OnLRUClick );
    end
    else begin
    ShowMessage( Format( 'open "%s" failed', [ path ] ) );
    end;
end;

procedure TMainform.OnTableSwitched;
begin
  if ( ActivateGrid.Down )
    then begin
    Grid.ColCount := fProxy.ColCount[ VISIBLE_COLUMNS ] + 1;
    adjust_gridrowcount( Grid, fProxy ); {support_unit}
    end
    else begin
    Grid.ColCount := 2;
    Grid.RowCount := fProxy.ColCount[ VISIBLE_COLUMNS ] + 1;
    end;
  RowScroll.Max := fProxy.TableRows - Grid.RowCount + 1;
  StatusBar.Panels[ 1 ].Text := Format( 'of %s',
    [ FloatToStrF( fProxy.TableRows, ffnumber, 0, 0 ) ] );
  Grid.Invalidate;
  Tables.Enabled := true;
end;

procedure TMainform.OnCellInvalidate( const aCol, aRow : Integer );
var eCol, eRow : Integer;
begin
  if ( ActivateGrid.Down )
    then begin
    eRow := aRow + 1;
    eCol := aCol + 1;
    end
    else begin
    eRow := aCol + 1;
    eCol := 1;
    end;
  if ( eRow > 0 )and( eRow < Grid.RowCount )and
     ( eCol > 0 )and( eCol < Grid.ColCount )
    then Grid.InvalidateCell( eCol, eRow );
end;

procedure TMainform.OnLED( state : boolean );
begin
  if State
    then begin
    LEDTimer.Tag := 1;
    LED.Brush.Color := clLime;
    LEDTimer.Enabled:= true;
    end
    else begin
    LEDTimer.Tag := 0;
    LEDTimer.Enabled:= false;
    LED.Brush.Color := clDefault;
    end;
end;

procedure TMainform.OnSearchDone( const aCol, aRow : Integer;
                                  const pattern : String );
begin
  if ( aRow < 0 )
    then ShowMessage( Format( '"%s" not found', [ pattern ] ) )
    else begin
    SForm.update_row( aRow );
    GotoRow( aRow + 1 );
    end;
end;

end.

