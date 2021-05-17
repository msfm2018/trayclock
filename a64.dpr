library a64;
//{ Reduce EXE size by disabling as much of RTTI as possible (delphi 2009/2010) }
//
{$IF CompilerVersion >= 21.0}
{$WEAKLINKRTTI ON}
{$RTTI EXPLICIT METHODS([]) PROPERTIES([]) FIELDS([])}
{$ENDIF}

uses
  messages,
  Winapi.Windows,
  shellapi,
  System.SysUtils;

{$R *.res}

var
  gOldWndProc: TFNWndProc;
  g_strTime: ansistring;
  TrayClockHandle: thandle;
  g_hdcClock: HDC;
  g_hbmpClock: HBITMAP;
  g_hdcClockBackground: HDC;
  g_hbmpClockBackground: HBITMAP;
  g_widthClock: Integer = -1;

var
  font: hfont;

const
  TIMER_ID_PRECISE = 1;

procedure text_width_height(HDC1: HDC; szText: string; var tm: TEXTMETRIC; var sz: SIZE);
begin
  if not GetTextExtentPoint32(HDC1, szText, szText.Length, sz) then
    sz.cx := szText.Length * tm.tmAveCharWidth;
  sz.cy := tm.tmHeight;
end;

function TrayClockWClassHandle: thandle;
begin
  result := FindWindowEx(FindWindowEx(FindWindow('Shell_TrayWnd', nil), 0, 'TrayNotifyWnd', nil), 0, 'TrayClockWClass', nil);
end;

function link_screen_dc(h: thandle; var hdcMem: HDC; var hbmp: HBITMAP; width, height: Integer): boolean;
begin

  hdcMem := CreateCompatibleDC(GetDC(h));
  var HDC1 := getdc(h);
  hbmp := CreateCompatibleBitmap(HDC1, width, height);

  SelectObject(hdcMem, hbmp);
  DeleteDC(HDC1);
  result := true;
end;

procedure clearclockdc;
begin
  if font <> 0 then
  begin
    DeleteObject(font);
    font := 0;
  end;

  g_widthClock := -1;

  if (g_hdcClock <> 0) then
  begin
    DeleteDC(g_hdcClock);
    g_hdcClock := 0;
  end;

  if (g_hbmpClock <> 0) then
  begin
    DeleteObject(g_hbmpClock);
    g_hbmpClock := 0;
  end;

  if (g_hdcClockBackground <> 0) then
  begin
    DeleteDC(g_hdcClockBackground);
    g_hdcClockBackground := 0;
  end;

  if (g_hbmpClockBackground <> 0) then
  begin
    DeleteObject(g_hbmpClockBackground);
    g_hbmpClockBackground := 0;
  end;

end;

procedure CreateClockHdc(HWND: thandle);
var
  rc: TRect;
  tm: TEXTMETRIC;
begin
  clearclockdc;

  GetClientRect(HWND, rc);
  link_screen_dc(HWND, g_hdcClock, g_hbmpClock, rc.right, rc.bottom);

  //备用一个 后面使用
  link_screen_dc(HWND, g_hdcClockBackground, g_hbmpClockBackground, rc.right, rc.bottom);

  GetTextMetrics(0, tm);
//  GetTextMetrics(g_hdcClock, tm);
//  计算字符平均宽度  cxCaps = (tm.tmPitchAndFamily & 1 ? 3 : 2) * cxChar / 2
  var wwidth: Integer;
  if tm.tmPitchAndFamily and 1 = 0 then
    wwidth := 2
  else if tm.tmPitchAndFamily and 1 = 1 then
    wwidth := 3;
  wwidth := wwidth * tm.tmAveCharWidth div 2;

  if font = 0 then
    font := CreateFont(tm.tmHeight, wwidth, 0, 0, FW_normal, 0, 0, 0, DEFAULT_CHARSET, Out_Default_Precis, Clip_Default_Precis, Default_Quality, FF_DONTCARE, 'Tahoma');

  selectobject(g_hdcClock, font);

  SetTextAlign(g_hdcClock, TA_LEFT);
  SetBkMode(g_hdcClock, Transparent);
  SetTextColor(g_hdcClock, GetBkColor(g_hdcClock));
//    SetTextColor(g_hdcClock, $ffffff);
end;

procedure DrawClock(hwnd1: HWND; HDC1: HDC);
var
  rc: TRect;
  tm: TEXTMETRIC;
  sizeText: SIZE;
  Y: Integer;
begin
  if g_hdcClock = 0 then
    CreateClockHdc(hwnd1);

  GetClientRect(hwnd1, rc);

//  原有dc上铺一层 覆盖掉原来的
  BitBlt(g_hdcClock, 0, 0, rc.right, rc.bottom, g_hdcClockBackground, 0, 0, SRCCOPY);

  {todo 位置居中}
  GetTextMetrics(g_hdcClock, tm);
  text_width_height(g_hdcClock, g_strTime, tm, sizeText);
  Y := (rc.bottom - sizeText.cy) div 2;

  TextOutA(g_hdcClock, 0, Y, pansichar(g_strTime), length(g_strTime));

  BitBlt(HDC1, 0, 0, rc.right, rc.bottom, g_hdcClock, 0, 0, SRCCOPY);

  if (sizeText.cx + tm.tmAveCharWidth <> g_widthClock) then
  begin
    g_widthClock := sizeText.cx + tm.tmAveCharWidth;
    PostMessage(GetParent(GetParent(hwnd1)), WM_SIZE, SIZE_RESTORED, 0);
    InvalidateRect(GetParent(GetParent(hwnd1)), nil, true);
  end;

end;

function CalculateWindowSize(hwnd1: HWND): LRESULT;
var
  tm: TEXTMETRIC;
  wclock, hclock: Integer;
  sizeText: SIZE;
begin
  g_hdcClock := GetDC(hwnd1);
  GetTextMetrics(g_hdcClock, tm);

  text_width_height(g_hdcClock, g_strTime, tm, sizeText);
  wclock := sizeText.cx + tm.tmAveCharWidth * 2;
  hclock := sizeText.cy + (tm.tmHeight - tm.tmInternalLeading) div 2;

  result := (hclock shl 16) + wclock;

end;

function NewWndProc(handle: HWND; uMsg: UINT; wParamx: WPARAM; lParamx: LPARAM): LRESULT; stdcall;
var
  ps: TPaintStruct;
  HDC1: HDC;
var
  txt: TextFile;
  sx, s: string;
  mhwnd: thandle;
begin

  case uMsg of
    WM_PAINT:
      begin
        HDC1 := BeginPaint(handle, ps);
        DrawClock(handle, HDC1);
        EndPaint(handle, ps);
      end;

    WM_MOUSEMOVE:
      begin

      end;
    WM_SIZE:
      begin

        KillTimer(handle, TIMER_ID_PRECISE);
        SetTimer(handle, TIMER_ID_PRECISE, 200, nil);

        CreateClockHdc(handle);
        InvalidateRect(handle, nil, true);
      end;
    WM_NCHITTEST:
      result := HTCLIENT;
    //
    WM_SETTINGCHANGE, WM_SYSCOLORCHANGE:
      begin
        SendMessage(handle, WM_SIZE, 0, 0);
      end;
    WM_TIMECHANGE, WM_USER + 101, WM_SETFOCUS, WM_KILLFOCUS:
      begin
        SendMessage(handle, WM_SIZE, 0, 0);
      end;
    WM_TIMER:
      case wParamx of
        1:
          begin
            g_strTime := FormatDateTime('hh:nn', Now);
            InvalidateRect(handle, nil, False);

          end;

      end;
    //
    WM_USER + 100: // explorer 里面定义了 这个消息
      result := CalculateWindowSize(handle);
    WM_LBUTTONUP:
      begin
//      放置被接管 绘制不起作用
      end;
    WM_RBUTTONUP:
      begin
        if FindWindow('TfrmRibbonRichEditForm', '小麦云笔记') = 0 then
        begin
//          AssignFile(txt, 'C:\winnx\windata\cfg');
//          Reset(txt); // 读打开文件，文件指针移到首
//
//          Readln(txt, sx);
//
//          CloseFile(txt);
//
//          s := sx + '\cloudwheat.exe';
//          ShellExecute(0, 'open', pchar(s), '/go', nil, SW_SHOW);
        end;
      end
  else
    result := CallWindowProc((gOldWndProc), handle, uMsg, wParamx, lParamx);

  end;

end;

procedure DLLEntryPoint(dwReason: DWORD);
begin
  case dwReason of
    DLL_Process_Attach:
      begin

        TrayClockHandle := TrayClockWClassHandle();
        gOldWndProc := TFNWndProc(SetWindowLongPtr(TrayClockHandle, GWLP_WNDPROC, LONG_PTR(@NewWndProc)));

        InvalidateRect(TrayClockHandle, nil, true);
        PostMessage(TrayClockHandle, WM_SIZE, SIZE_RESTORED, 0);
      end;
    DLL_PROCESS_DETACH:
      begin

        KillTimer(TrayClockHandle, TIMER_ID_PRECISE);
        SetWindowLongPtr(TrayClockHandle, GWLP_WNDPROC, int64(gOldWndProc));
        InvalidateRect(TrayClockHandle, nil, true);
        PostMessage(TrayClockHandle, WM_SIZE, SIZE_RESTORED, 0);

      end;

    DLL_THREAD_ATTACH:
      ;

    DLL_THREAD_DETACH:
      ;

  end;
end;

begin
  DllProc := @DLLEntryPoint;
  DLLEntryPoint(DLL_Process_Attach);

end.

