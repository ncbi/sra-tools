unit gotoform;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, Forms, Controls, Graphics, Dialogs, ExtCtrls,
  Buttons, MaskEdit;

type

  { TGotoForm }

  TGotoForm = class(TForm)
    BOK: TBitBtn;
    BCancel: TBitBtn;
    ERow: TMaskEdit;
    Panel1: TPanel;
  private
    { private declarations }
  public
    { public declarations }
    function present( const p : TPoint; var row_nr : Integer ) : boolean;
  end; 

var
  MyGotoForm : TGotoForm;

implementation

{$R *.lfm}

{ TGotoForm }

function TGotoForm.present( const p : TPoint; var row_nr : Integer ) : boolean;
begin
  Left := p.x;
  Top  := p.y;
  ERow.Text := IntToStr( row_nr );
  Result := ( ShowModal = mrOK );
  if ( Result )
    then row_nr := StrToIntDef( ERow.Text, row_nr );
end;

end.

