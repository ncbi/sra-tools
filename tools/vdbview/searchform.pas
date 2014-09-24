unit searchform;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, Forms, Controls, Graphics, Dialogs, ExtCtrls,
  Buttons, StdCtrls, DataProxy;

type

  { TSForm }

  TSForm = class(TForm)
    B_OK: TBitBtn;
    BCancel: TBitBtn;
    EPattern: TEdit;
    Panel1: TPanel;
    RB_Backward: TRadioButton;
    RB_Forward: TRadioButton;
    procedure BCancelClick(Sender: TObject);
    procedure B_OKClick(Sender: TObject);
  private
    { private declarations }
    fRow, fCol : Integer;
    fProxy : TProxy;
  public
    { public declarations }
    procedure present( const p : TPoint;
                       aCol, aRow : Integer;
                       pattern : String;
                       aProxy : TProxy );
    procedure update_row( aRow : Integer );
  end; 

var
  SForm: TSForm;

implementation

{$R *.lfm}

{ TSForm }

procedure TSForm.B_OKClick( Sender : TObject );
begin
  if ( RB_Forward.Checked )
    then fProxy.Search( true, fCol, fRow + 1, EPattern.Text )
    else fProxy.Search( false, fCol, fRow - 1, EPattern.Text );
end;

procedure TSForm.BCancelClick(Sender: TObject);
begin
  fProxy.CancelSearch;
  Hide;
end;

procedure TSForm.present( const p : TPoint;
                          aCol, aRow : Integer;
                          pattern : String;
                          aProxy : TProxy );
begin
  Left := p.x;
  Top  := p.y;
  EPattern.Text := pattern;
  fRow := aRow;
  fCol := aCol;
  fProxy := aProxy;
  Show;
end;

procedure TSForm.update_row( aRow : Integer );
begin
  fRow := aRow;
end;

end.

