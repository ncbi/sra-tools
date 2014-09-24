unit jobdef;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils; 

type
  TCmdType = ( CMD_OPEN, CMD_CLOSE, CMD_TABLENAME,
               CMD_OPENED, CMD_COLCOUNT, CMD_COLNAME, CMD_COLSDONE,
               CMD_OPENTAB, CMD_CELL, CMD_SEARCH );

  { TJob }
  PJob = ^TJob;
  TJob = record
      cmd : TCmdType;
      I1, I2, I3, I4, I5, I6 : Integer;
      S1, S2 : String;
      next : PJob;
   end;

implementation

end.

