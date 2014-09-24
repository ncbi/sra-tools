unit support_unit;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Grids, XMLCfg, DataProxy, CheckLst,
  Menus, Graphics, StrUtils;

type
 TCellType = ( ct_topleft, ct_row, ct_col, ct_content );
 TOnLRUClick = procedure( Sender : TObject ) of Object;

 TDrawCtx = record
   w, h : Integer;
   g    : TDrawGrid;
 end;

procedure setup_position( M : TForm; cfg : TXMLConfig; S : TScreen );
procedure store_position( M : TForm; cfg : TXMLConfig );
procedure adjust_gridrowcount_to( G : TDrawGrid; P : TProxy; h : Integer );
procedure adjust_gridrowcount( G : TDrawGrid; P : TProxy );
function get_celltype( aCol, aRow: Integer ) : TCellType;
procedure draw_cell( ctx : TDrawCtx; aRect : TRect;
                     center, clip : boolean; s : String );
function CheckListBox2String( aBox : TCheckListBox ) : String;

procedure SaveLRU( path : string; cfg : TXMLConfig );
procedure PopulateLRUMenue( cfg : TXMLConfig; mm : TMainMenu;
                            item : TMenuItem; event : TOnLRUClick );
procedure ClearLRU( cfg : TXMLConfig; item : TMenuItem );

implementation

procedure setup_position( M : TForm; cfg : TXMLConfig; S : TScreen );
begin
  M.Left := cfg.GetValue( 'xpos', M.Left );
  M.Width:= cfg.GetValue( 'width', M.Width );
  if ( M.Left + M.Width > S.Width )
    then M.Left := S.Width - M.Width;
  if ( M.Left < 0 ) then M.Left := 0;
  if ( M.Width > S.Width ) then M.Width := S.Width;
  M.Top  := cfg.GetValue( 'ypos', M.Top );
  M.Height:= cfg.Getvalue( 'height', M.Height );
  if ( M.Top + M.Height > S.Height )
    then M.Top := S.Height - M.Height;
  if ( M.Top < 0 ) then M.Top := 0;
  if ( M.Height > S.Height ) then M.Height := S.Height;
  if ( cfg.GetValue( 'windowstate', 'N' ) = 'M' )
    then M.WindowState := wsMaximized;
end;

procedure store_position( M : TForm; cfg : TXMLConfig );
begin
  case M.WindowState of
    wsNormal : begin
               cfg.SetValue( 'xpos', M.Left );
               cfg.SetValue( 'width', M.Width );
               cfg.SetValue( 'ypos', M.Top );
               cfg.SetValue( 'height', M.Height );
               cfg.SetValue( 'windowstate', 'N' );
               end;
    wsMaximized : cfg.SetValue( 'windowstate', 'M' );
    wsMinimized : cfg.SetValue( 'windowstate', 'N' );
   end;
end;

procedure adjust_gridrowcount_to( G : TDrawGrid; P : TProxy; h : Integer );
begin
  G.RowCount := ( G.ClientHeight div h ) + 1;
  G.RowCount := G.VisibleRowCount + 2;
  P.AdjustCacheRows( G.RowCount );
end;

procedure adjust_gridrowcount( G : TDrawGrid; P : TProxy );
begin
  adjust_gridrowcount_to( G, P, G.DefaultRowHeight );
end;

function get_celltype( aCol, aRow : Integer ) : TCellType;
begin
  if ( aCol = 0 )
    then if ( aRow = 0 )
           then Result := ct_topleft
           else Result := ct_row
    else if ( aRow = 0 )
           then Result := ct_col
           else Result := ct_content;
end;

procedure draw_multi_line( ctx : TDrawCtx; aRect : TRect;
          center : boolean; s : String; cw, tw, ch, cpl : Integer );
var n_lines, v_lines, i : Integer;
begin
  if ( cpl > 0 )
    then begin
    n_lines := ( Length( s ) div cpl );
    v_lines := ( ch div ctx.h );
    if ( v_lines < n_lines )
      then begin
      { not all lines are visible ... }
      if ( v_lines > 0 )
        then begin
        for i := 0 to v_lines - 2
          do ctx.g.Canvas.TextOut( aRect.Left + 5,
                                   aRect.Top + 3 + ( i * ctx.h ),
                                   MidStr( s, i * cpl, cpl ) );
        i := v_lines - 1;
        ctx.g.Canvas.TextOut( aRect.Left + 5,
                              aRect.Top + 3 + ( i * ctx.h ),
                              MidStr( s, i * cpl, cpl - 2 ) + '..' );

        end;
      end
      else begin
      { all lines are visible ... }
      if ( n_lines > 0 )
        then for i := 0 to n_lines
          do ctx.g.Canvas.TextOut( aRect.Left + 5,
                                   aRect.Top + 3 + ( i * ctx.h ),
                                   MidStr( s, i * cpl, cpl ) );
      end;
    end;
end;

procedure draw_single_line( ctx : TDrawCtx; aRect : TRect;
          center, clip : boolean; s : String; cw, tw, cpl : Integer );
var x, y : Integer;
begin
  y := aRect.top + 3;
  if ( center )
    then x := aRect.Left + ( ( cw - tw ) div 2 )
    else x := aRect.left + 5;
  if ( ( Length( s ) > cpl )and( clip ) )
    then ctx.g.Canvas.TextRect( aRect, x, y, MidStr( s, 0, cpl-2 )+'..' )
    else ctx.g.Canvas.TextRect( aRect, x, y, s );
end;

procedure draw_cell( ctx : TDrawCtx; aRect : TRect;
                     center, clip : boolean; s : String );
var ch, cw, tw, cpl : Integer;
begin
  ch := aRect.Bottom - aRect.Top; { cell height }
  cw := aRect.Right - aRect.Left; { cell width }
  tw := ctx.g.Canvas.TextWidth( s ); { text width }
  cpl := ( ( cw - 10 ) div ctx.w ); { chars per line }

  if ( tw > ( cw - 10 ) )and( ch > ( ctx.h + ctx.h ) )
    then draw_multi_line( ctx, aRect, center, s, cw, tw, ch, cpl )
    else draw_single_line( ctx, aRect, center, clip, s, cw, tw, cpl );
end;

function CheckListBox2String( aBox : TCheckListBox ) : String;
var i : Integer;
begin
  Result := '';
  if Assigned( aBox )
    then begin
    for i := 0  to aBox.Items.Count - 1
      do if ( aBox.Checked[ i ] )
           then Result := Result + 'X'
           else Result := Result + '-';
    end;
end;

procedure SaveLRU( path : string; cfg : TXMLConfig );
var L : TStringList;
begin
  L := TStringList.Create;
  if Assigned( L )
    then try
    L.Text := cfg.GetValue( 'LRU/path', '' );
    if ( L.IndexOf( path ) < 0 )
      then begin
      if ( L.Count > 20 )
        then L.Delete( 0 );
      L.Add( path );
      cfg.SetValue( 'LRU/path', L.Text );
      end;
    finally
    L.Free;
    end;
end;

procedure PopulateLRUMenue( cfg : TXMLConfig; mm : TMainMenu;
                            item : TMenuItem; event : TOnLRUClick );
var M : TMenuItem;
    L : TStringList;
    I : Integer;
begin
  L := TStringList.Create;
  if Assigned( L )
    then try
      L.Text := cfg.GetValue( 'LRU/path', '' );
      item.Clear;
      for I := 0 to L.Count-1
        do begin
        M := TMenuItem.Create( mm );
        M.Name := Format( 'LRU_Item_%d', [ I ] );
        M.Caption := L.Strings[ I ];
        M.OnClick := event;
        item.Add( M );
        end;
    finally
      L.Free;
    end;
end;

procedure ClearLRU( cfg : TXMLConfig; item : TMenuItem );
begin
  cfg.SetValue( 'LRU/path', '' );
  item.Clear;
end;

end.

