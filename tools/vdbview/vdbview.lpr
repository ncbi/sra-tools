program vdbview;

{$mode objfpc}{$H+}

uses
  {$IFDEF UNIX}{$IFDEF UseCThreads}
  cthreads,
  {$ENDIF}{$ENDIF}
  Interfaces, // this includes the LCL widgetset
  Forms, lazcontrols, MainWindow, ColumnSel, gotoform, searchform, version
  { you can add units after this };

{$R *.res}

begin
  Application.Initialize;
  Application.CreateForm(TMainform, Mainform);
  Application.CreateForm(TColumnform, Columnform);
  Application.CreateForm(TGotoForm, MyGotoForm);
  Application.CreateForm(TSForm, SForm);
  Application.CreateForm(TVersionForm, VersionForm);
  Application.Run;
end.

