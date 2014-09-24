unit ColumnSel;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, Forms, Controls, Graphics, Dialogs, CheckLst,
  ExtCtrls, Buttons, StdCtrls, DataProxy, selector_types, support_unit;

type

  { TColumnform }

  TColumnform = class(TForm)
    BOK: TBitBtn;
    BCancel: TBitBtn;
    BClear: TButton;
    BSetAll: TButton;
    BNonStatic: TButton;
    ColumnBox: TCheckListBox;
    Footer: TPanel;
    Panel1: TPanel;
    procedure BClearClick(Sender: TObject);
    procedure BNonStaticClick(Sender: TObject);
    procedure BSetAllClick(Sender: TObject);
    procedure ColumnBoxItemClick( Sender : TObject; Index : integer );
  private
    { private declarations }
    FOrgChecked : String;
    FNonStatic  : String;
    procedure setup_columns( aProxy : TProxy );
    procedure enable_bok;
  public
    { public declarations }
    function present( const p : TPoint; const h : Integer;
                      aProxy : TProxy ) : boolean;
    function checked_items : String;
  end;

var
  Columnform: TColumnform;


implementation

{$R *.lfm}

{ TColumnform }

procedure TColumnform.BClearClick( Sender : TObject );
var i : Integer;
begin
  for i:=0 to ColumnBox.Items.Count -1
    do ColumnBox.Checked[ i ] := false;
  enable_bok;
end;

procedure TColumnform.BNonStaticClick( Sender : TObject );
var i : Integer;
begin
  for i:=0 to ColumnBox.Items.Count -1
    do ColumnBox.Checked[ i ] := ( FNonStatic[ i + 1 ] = 'X' );
  enable_bok;
end;

procedure TColumnform.BSetAllClick( Sender : TObject );
var i : Integer;
begin
  for i:=0 to ColumnBox.Items.Count -1
    do ColumnBox.Checked[ i ] := true;
  enable_bok;
end;

procedure TColumnform.ColumnBoxItemClick( Sender : TObject; Index : integer );
begin
  enable_bok;
end;

procedure TColumnform.setup_columns( aProxy : TProxy );
var i : Integer;
begin
  ColumnBox.Clear;
  for i:=0 to aProxy.ColCount[ ALL_COLUMNS ] - 1
    do begin
    ColumnBox.Items.Add( aProxy.ColName[ ALL_COLUMNS, i ] );
    ColumnBox.Checked[ i ] := aProxy.ColVisibleI[ i ];
    end;
  FNonStatic := aProxy.NonStaticColumns;
end;

procedure TColumnform.enable_bok;
begin
  BOK.Enabled := ( FOrgChecked <> checked_items );
end;

function TColumnform.present( const p : TPoint; const h : Integer;
                              aProxy : TProxy ) : boolean;
begin
  Left := p.x;
  Top  := p.y;
  Height := h;
  setup_columns( aProxy );
  FOrgChecked := CheckListBox2String( ColumnBox );
  BOK.Enabled := False;
  Result := ( ShowModal = mrOK );
end;

function TColumnform.checked_items: String;
begin
  Result := CheckListBox2String( ColumnBox );
end;

end.

