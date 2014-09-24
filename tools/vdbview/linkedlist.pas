unit linkedlist;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, jobdef;

type

  { TJobQ }
  TJobQ = class
  private
    FHead, FTail : PJob;
  public
    constructor Create;
    destructor Destroy; override;
    function Empty : Boolean; virtual;
    procedure Put( job : PJob ); virtual;
    function Get : PJob; virtual;
    function GetOrMake : PJob; virtual;
    procedure Flush( dst : TJobQ ); virtual;
    procedure Clear;
  end;

  { TMutexJobQ }

  TMutexJobQ = class( TJobQ )
  private
    cs : TRTLCriticalSection;
  public
    constructor Create;
    destructor Destroy; override;
    function Empty : Boolean; override;
    procedure Put( job : PJob ); override;
    function Get : PJob; override;
    function GetOrMake : PJob; override;
    procedure Flush( dst : TJobQ ); override;
  end;

implementation

{ TJobQ }

constructor TJobQ.Create;
begin
  inherited Create;
  FHead := Nil;
  FTail := Nil;
end;

destructor TJobQ.Destroy;
begin
  Clear;
  inherited Destroy;
end;

function TJobQ.Empty : Boolean;
begin
  Result := ( FHead = Nil );
end;

procedure TJobQ.Put( job : PJob );
begin
  job^.next := Nil;
  if ( FHead = Nil )
    then FHead := job
    else FTail^.next := job;
  FTail := job;
end;

function TJobQ.Get : PJob;
begin
  if ( FHead <> Nil )
    then begin
    Result := FHead;
    FHead := Result^.next;
    end
    else begin
    Result := Nil;
    end;
end;

function TJobQ.GetOrMake : PJob;
begin
  if ( FHead <> Nil )
    then begin
    Result := FHead;
    FHead := Result^.next;
    end
    else begin
    new ( Result );
    end;
end;

procedure TJobQ.Flush( dst : TJobQ );
var job : PJob;
begin
  while ( not Empty )
    do begin
    job := Get;
    if Assigned( dst )
      then dst.Put( job )
      else Dispose( job );
    end;
end;

procedure TJobQ.Clear;
var job : PJob;
begin
  while ( not Empty )
    do begin
    job := Get;
    Dispose( job );
    end;
end;

{ TMutexJobQ }

constructor TMutexJobQ.Create;
begin
  inherited Create;
  InitCriticalSection( cs );
end;

destructor TMutexJobQ.Destroy;
begin
  inherited Destroy;
  DoneCriticalsection( cs );
end;

function TMutexJobQ.Empty : Boolean;
begin
  EnterCriticalSection( cs );
  try
    Result := inherited Empty;
  finally
    LeaveCriticalSection( cs );
  end;
end;

procedure TMutexJobQ.Put( job : PJob );
begin
  EnterCriticalSection( cs );
  try
    inherited Put( job );
  finally
    LeaveCriticalSection( cs );
  end;
end;

function TMutexJobQ.Get : PJob;
begin
  Result := Nil;
  EnterCriticalSection( cs );
  try
    Result := inherited Get;
  finally
    LeaveCriticalSection( cs );
  end
end;

function TMutexJobQ.GetOrMake: PJob;
begin
  Result := Nil;
  EnterCriticalSection( cs );
  try
    Result := inherited GetOrMake;
  finally
    LeaveCriticalSection( cs );
  end
end;

procedure TMutexJobQ.Flush( dst : TJobQ );
begin
  EnterCriticalSection( cs );
  try
    inherited Flush( dst );
  finally
    LeaveCriticalSection( cs );
  end
end;

end.

