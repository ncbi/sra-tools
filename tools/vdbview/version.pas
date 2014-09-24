unit version;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, Forms, Controls, Graphics, Dialogs, ExtCtrls,
  Buttons, StdCtrls;

type

  { TVersionForm }

  TVersionForm = class(TForm)
    BitBtn1: TBitBtn;
    Panel1: TPanel;
    StaticText1: TStaticText;
  private
    { private declarations }
  public
    { public declarations }
  end; 

var
  VersionForm: TVersionForm;

implementation

{$R *.lfm}

end.

