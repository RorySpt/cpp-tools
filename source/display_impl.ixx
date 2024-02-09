

// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
module;

//
// Copyright (c) Microsoft Corporation. All rights reserved

// we need commctrl v6 for LoadIconMetric()
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")

#include <array>

#include "resource.h"
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <functional>
#include <strsafe.h>
#include <tchar.h>

#include "code_trans.h"
#include "json.hpp"
#include "print.h"
export module display_impl;

import config;
import utils;

HINSTANCE g_hInst = NULL;

UINT const WMAPP_NOTIFYCALLBACK = WM_APP + 1;
UINT const WMAPP_HIDEFLYOUT = WM_APP + 2;

UINT_PTR const HIDEFLYOUT_TIMER_ID = 1;

wchar_t const szWindowClass[] = L"NotificationIconTest";
wchar_t const szFlyoutWindowClass[] = L"NotificationFlyout";

// Use a guid to uniquely identify our icon
class __declspec(uuid("9D0B8B92-4E1C-488e-A1E1-2331AFCE2CB5")) PrinterIcon;


HWND main_window_hwnd;
export {

	// Forward declarations of functions included in this code module:
	void                RegisterWindowClass(PCWSTR pszClassName, PCWSTR pszMenuName, WNDPROC lpfnWndProc);
	LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
	ATOM                RegisterFlyoutClass(HINSTANCE hInstance);
	LRESULT CALLBACK    FlyoutWndProc(HWND, UINT, WPARAM, LPARAM);
	HWND                ShowFlyout(HWND hwnd);
	void                HideFlyout(HWND hwndMainWindow, HWND hwndFlyout);
	void                PositionFlyout(HWND hwnd, REFGUID guidIcon);
	void                ShowContextMenu(HWND hwnd, POINT pt);
	BOOL                AddNotificationIcon(HWND hwnd);
	BOOL                DeleteNotificationIcon();
	BOOL                ShowLowInkBalloon();
	BOOL                ShowNoInkBalloon();
	BOOL                ShowPrintJobBalloon();
	BOOL                RestoreTooltip();
}
export std::wstring LoadResourceString(
    _In_ UINT uID)
{
    static std::array<wchar_t, 2048> buffer;
    int Count;
    if((Count = LoadString(GetModuleHandle(nullptr), uID, buffer.data(), static_cast<int>(buffer.size()))) > 0)
    {
        return { buffer.data(), buffer.data() + Count };
    }
    return {};
}
bool SetStartWithWindows(bool b_cond)
{
    HKEY hKey;
    LPCTSTR lpSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");

    const auto title_name = LoadResourceString(IDS_APP_TITLE);
    const LPCTSTR lpValueName = title_name.data();
    const auto wAppPath = AnsiToWideChar(get_config().app_path);
    const LPCTSTR lpData = wAppPath.c_str();

    bool succeed = false;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, lpSubKey, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
    {
        if(b_cond)
        {
            // 将程序路径写入注册表项
            if (RegSetValueEx(hKey, lpValueName, 0, REG_SZ, LPBYTE(lpData), _tcslen(lpData) * sizeof(TCHAR)) == ERROR_SUCCESS)
            {
                println("程序成功添加到开机启动项");
                succeed = true;
            }
        }
        else
        {
            if (RegDeleteValue(hKey, lpValueName))
            {
                println("程序开机启动项已移除");
                succeed = true;
            }
        }
        RegCloseKey(hKey);
    }
    else
    {
        println("无法打开注册表");
    }
    return succeed;
}

bool IsStartWithWindows()
{

    HKEY hKey;
    LPCTSTR lpSubKey = _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run");

    const auto title_name = LoadResourceString(IDS_APP_TITLE);

    const LPCTSTR lpValueName = title_name.data();

    bool succeed = false;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, lpSubKey, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS)
    {
	    wchar_t buffer[5120];
        DWORD buffer_size = sizeof(buffer);
        DWORD dwType = REG_SZ;
        LSTATUS ErrCode;
        if ((ErrCode = RegGetValue(hKey, nullptr, lpValueName, RRF_RT_ANY, &dwType, buffer, &buffer_size)) == ERROR_SUCCESS)
        {
            //println("查询到到开机启动项，{}", WideCharToAnsi(buffer));
            succeed = true;
        }
        else
        {
            //println("查询失败，Code:{}", ErrCode);
            succeed = false;
        }
        RegCloseKey(hKey);
    }
    return succeed;
}


export int APIENTRY Display_Exec(const std::function<void()>& callback, HINSTANCE hInstance = GetModuleHandle(nullptr))
{
    g_hInst = hInstance;
    RegisterWindowClass(szWindowClass, MAKEINTRESOURCE(IDC_NOTIFICATIONICON), WndProc);
    RegisterWindowClass(szFlyoutWindowClass, NULL, FlyoutWndProc);

    // Create the main window. This could be a hidden window if you don't need
    // any UI other than the notification icon.
    WCHAR szTitle[100];
    LoadString(hInstance, IDS_APP_TITLE, szTitle, ARRAYSIZE(szTitle));
    main_window_hwnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 250, 200, NULL, NULL, g_hInst, NULL);
    if (main_window_hwnd)
    {
        //ShowWindow(main_window_hwnd, /*nCmdShow*/false);

        // Main message loop:
        MSG msg;
        bool done = false;
        while (!done)
        {
            while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if(msg.message == WM_QUIT)
                {
                    done = true;
                }
            }
            

            callback();
        }
    }
    return 0;
}
export template<int = 0>
int APIENTRY Display_Exec()
{
    return Display_Exec([]{});
}
void RegisterWindowClass(PCWSTR pszClassName, PCWSTR pszMenuName, WNDPROC lpfnWndProc)
{
    WNDCLASSEX wcex = { sizeof(wcex) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = lpfnWndProc;
    wcex.hInstance = g_hInst;
    wcex.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_NOTIFICATIONICON));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = pszMenuName;
    wcex.lpszClassName = pszClassName;
    RegisterClassEx(&wcex);
}

BOOL AddNotificationIcon(HWND hwnd)
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = hwnd;
    // add the icon, setting the icon, tooltip, and callback message.
    // the icon will be identified with the GUID
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_SHOWTIP | NIF_GUID;
    nid.guidItem = __uuidof(PrinterIcon);
    nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;
    LoadIconMetric(g_hInst, MAKEINTRESOURCE(IDI_NOTIFICATIONICON), LIM_SMALL, &nid.hIcon);
    LoadString(g_hInst, IDS_TOOLTIP, nid.szTip, ARRAYSIZE(nid.szTip));
    Shell_NotifyIcon(NIM_ADD, &nid);

    // NOTIFYICON_VERSION_4 is prefered
    nid.uVersion = NOTIFYICON_VERSION_4;
    return Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

BOOL DeleteNotificationIcon()
{
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_GUID;
    nid.guidItem = __uuidof(PrinterIcon);
    return Shell_NotifyIcon(NIM_DELETE, &nid);
}

BOOL ShowLowInkBalloon()
{
    // Display a low ink balloon message. This is a warning, so show the appropriate system icon.
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_INFO | NIF_GUID;
    nid.guidItem = __uuidof(PrinterIcon);
    // respect quiet time since this balloon did not come from a direct user action.
    nid.dwInfoFlags = NIIF_WARNING | NIIF_RESPECT_QUIET_TIME;
    LoadString(g_hInst, IDS_LOWINK_TITLE, nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle));
    LoadString(g_hInst, IDS_LOWINK_TEXT, nid.szInfo, ARRAYSIZE(nid.szInfo));
    return Shell_NotifyIcon(NIM_MODIFY, &nid);
}

export BOOL ShowBalloon(std::wstring_view title, std::wstring_view info, DWORD flags = NIIF_INFO)
{
    // Display an out of ink balloon message. This is a error, so show the appropriate system icon.
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_INFO | NIF_GUID;
    nid.guidItem = __uuidof(PrinterIcon);
    nid.dwInfoFlags = flags;
    memcpy(nid.szInfoTitle, title.data(), title.size());
    memcpy(nid.szInfo, info.data(), info.size());
    return Shell_NotifyIcon(NIM_MODIFY, &nid);
}

BOOL ShowNoInkBalloon()
{
    // Display an out of ink balloon message. This is a error, so show the appropriate system icon.
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_INFO | NIF_GUID;
    nid.guidItem = __uuidof(PrinterIcon);
    nid.dwInfoFlags = NIIF_ERROR;
    LoadString(g_hInst, IDS_NOINK_TITLE, nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle));
    LoadString(g_hInst, IDS_NOINK_TEXT, nid.szInfo, ARRAYSIZE(nid.szInfo));
    return Shell_NotifyIcon(NIM_MODIFY, &nid);
}

BOOL ShowPrintJobBalloon()
{
    // Display a balloon message for a print job with a custom icon
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_INFO | NIF_GUID;
    nid.guidItem = __uuidof(PrinterIcon);
    nid.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
    LoadString(g_hInst, IDS_PRINTJOB_TITLE, nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle));
    LoadString(g_hInst, IDS_PRINTJOB_TEXT, nid.szInfo, ARRAYSIZE(nid.szInfo));
    LoadIconMetric(g_hInst, MAKEINTRESOURCE(IDI_NOTIFICATIONICON), LIM_LARGE, &nid.hBalloonIcon);
    return Shell_NotifyIcon(NIM_MODIFY, &nid);
}

BOOL RestoreTooltip()
{
    // After the balloon is dismissed, restore the tooltip.
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.uFlags = NIF_SHOWTIP | NIF_GUID;
    nid.guidItem = __uuidof(PrinterIcon);
    return Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void PositionFlyout(HWND hwnd, REFGUID guidIcon)
{
    // find the position of our printer icon
    NOTIFYICONIDENTIFIER nii = { sizeof(nii) };
    nii.guidItem = guidIcon;
    RECT rcIcon;
    HRESULT hr = Shell_NotifyIconGetRect(&nii, &rcIcon);
    if (SUCCEEDED(hr))
    {
        // display the flyout in an appropriate position close to the printer icon
        POINT const ptAnchor = { (rcIcon.left + rcIcon.right) / 2, (rcIcon.top + rcIcon.bottom) / 2 };

        RECT rcWindow;
        GetWindowRect(hwnd, &rcWindow);
        SIZE sizeWindow = { rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top };

        if (CalculatePopupWindowPosition(&ptAnchor, &sizeWindow, TPM_VERTICAL | TPM_VCENTERALIGN | TPM_CENTERALIGN | TPM_WORKAREA, &rcIcon, &rcWindow))
        {
            // position the flyout and make it the foreground window
            SetWindowPos(hwnd, HWND_TOPMOST, rcWindow.left, rcWindow.top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
        }
    }
}

HWND ShowFlyout(HWND hwndMainWindow)
{
    // size of the bitmap image (which will be the client area of the flyout window).
    RECT rcWindow = {};
    rcWindow.right = 214;
    rcWindow.bottom = 180;
    DWORD const dwStyle = WS_POPUP | WS_THICKFRAME;
    // adjust the window size to take the frame into account
    AdjustWindowRectEx(&rcWindow, dwStyle, FALSE, WS_EX_TOOLWINDOW);

    HWND hwndFlyout = CreateWindowEx(WS_EX_TOOLWINDOW, szFlyoutWindowClass, NULL, dwStyle,
        CW_USEDEFAULT, 0, rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top, hwndMainWindow, NULL, g_hInst, NULL);
    if (hwndFlyout)
    {
        PositionFlyout(hwndFlyout, __uuidof(PrinterIcon));
        SetForegroundWindow(hwndFlyout);
    }
    return hwndFlyout;
}

void HideFlyout(HWND hwndMainWindow, HWND hwndFlyout)
{
    DestroyWindow(hwndFlyout);

    // immediately after hiding the flyout we don't want to allow showing it again, which will allow clicking
    // on the icon to hide the flyout. If we didn't have this code, clicking on the icon when the flyout is open
    // would cause the focus change (from flyout to the taskbar), which would trigger hiding the flyout
    // (see the WM_ACTIVATE handler). Since the flyout would then be hidden on click, it would be shown again instead
    // of hiding.
    SetTimer(hwndMainWindow, HIDEFLYOUT_TIMER_ID, GetDoubleClickTime(), NULL);
}

HMENU hCurMenu;

void ShowContextMenu(HWND hwnd, POINT pt)
{
    HMENU hMenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDC_CONTEXTMENU));
    if (hMenu)
    {
        HMENU hSubMenu = GetSubMenu(hMenu, 0);
        hCurMenu = hSubMenu;
        if (hSubMenu)
        {
            // our window must be foreground before calling TrackPopupMenu or the menu will not disappear when the user clicks away
            SetForegroundWindow(hwnd);

            CheckMenuItem(hMenu, IDM_STARTUP, IsStartWithWindows() ? MF_CHECKED : MF_UNCHECKED);
        	CheckMenuItem(hMenu, IDM_SHOWWINDOW, IsWindowVisible(main_window_hwnd) ? MF_CHECKED : MF_UNCHECKED);

        	// respect menu drop alignment
            UINT uFlags = TPM_RIGHTBUTTON;
            if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
            {
                uFlags |= TPM_RIGHTALIGN;
            }
            else
            {
                uFlags |= TPM_LEFTALIGN;
            }

            GetCursorPos(&pt);
            println("x:{}, y:{}", pt.x, pt.y);
            
            TrackPopupMenuEx(hSubMenu, uFlags, pt.x, pt.y, hwnd, NULL);
        }
        DestroyMenu(hMenu);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HWND s_hwndFlyout = NULL;
    static BOOL s_fCanShowFlyout = TRUE;
    static BOOL bShowConfig = FALSE;
    switch (message)
    {
    case WM_CREATE:
        // add the notification icon
        if (!AddNotificationIcon(hwnd))
        {
            MessageBox(hwnd,
                L"Please read the ReadMe.txt file for troubleshooting",
                L"Error adding icon", MB_OK);
            return -1;
        }
        Log::slots().emplace_back([&, hwnd](std::string_view)
            {
	            if(!bShowConfig)
	            {
	                InvalidateRect(hwnd, nullptr, false);
	                UpdateWindow(hwnd);
	            }

            });
        break;
    case WM_CLOSE:
	    {
            if(hwnd == main_window_hwnd)
            {
                ShowWindow(hwnd, false);
                break;
            }
				
	    }
    case WM_PAINT:
	    {
			

			PAINTSTRUCT ps;
            
			const auto hdc = BeginPaint(hwnd, &ps);
            
            {
                RECT rect;
                GetClientRect(hwnd, &rect);
				std::string ms;
                if (bShowConfig)
                {
                    ms = std::format("Config:\n{}", nlohmann::json( get_config()).dump(4).data());
                }else
                {
                    for (auto& s : Log::message_deque())
                    {
                        ms += s;
                        ms += "                                                                                                       \n";
                    }
                }
                DrawTextA(hdc, ms.c_str(), static_cast<int>(ms.size()), &rect, DT_LEFT);
            }

            EndPaint(hwnd, &ps);
		    break;
	    }
    case WM_MOUSEMOVE:
	    {
            {
                //POINT point{};
                //GetCursorPos(&point);
                //const std::string s = std::format("MousePos: {}, {}"
                //    , point.x
                //    , point.y
                //);
                ////TextOutA(hdc, 0, 40, s.c_str(), s.size());
                //println(s);
                //RECT rect{
                //    0,40,999,60
                //};
                //InvalidateRect(hwnd, &rect, true);
                //UpdateWindow(hwnd);
            }
	    }
    case WM_COMMAND:
    {
        int const wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_LOWINK:
            ShowLowInkBalloon();
            break;

        case IDM_NOINK:
            ShowNoInkBalloon();
            break;

        case IDM_PRINTJOB:
            ShowPrintJobBalloon();
            break;

        case IDM_OPTIONS:
            // placeholder for an options dialog
            MessageBox(hwnd, L"Display the options dialog here.", L"Options", MB_OK);
            break;

        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;

        case IDM_STARTUP:
	        {
	            //s_hwndFlyout = ShowFlyout(hwnd);
	            const bool b = IsStartWithWindows();
	            SetStartWithWindows(!b);
	            //CheckMenuItem(hCurMenu, IDM_FLYOUT, !b);

	            break;
	        }
        case IDM_SHOWWINDOW:
        {
            ShowWindow(main_window_hwnd, !IsWindowVisible(main_window_hwnd));

            break;
        }
        case ID_TEST_SHOWCONFIG:
        {
			if(bShowConfig == false)
			{
                bShowConfig = true;
                InvalidateRect(hwnd, nullptr, true);
                UpdateWindow(hwnd);
			}
            break;
			
        }
        case ID_TEST_SHOWMESSAGE:
        {
            if (bShowConfig == TRUE)
            {
                bShowConfig = false;
                InvalidateRect(hwnd, nullptr, true);
                UpdateWindow(hwnd);
            }
            break;
        }
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
        }
    }
    break;

    case WMAPP_NOTIFYCALLBACK:
        switch (LOWORD(lParam))
        {
        case NIN_SELECT:
            // for NOTIFYICON_VERSION_4 clients, NIN_SELECT is prerable to listening to mouse clicks and key presses
            // directly.
            if (IsWindowVisible(s_hwndFlyout))
            {
                HideFlyout(hwnd, s_hwndFlyout);
                s_hwndFlyout = NULL;
                s_fCanShowFlyout = FALSE;
            }
            else if (s_fCanShowFlyout)
            {
                s_hwndFlyout = ShowFlyout(hwnd);
            }
            break;

        case NIN_BALLOONTIMEOUT:
            RestoreTooltip();
            break;

        case NIN_BALLOONUSERCLICK:
            RestoreTooltip();
            // placeholder for the user clicking on the balloon.
            //if(!IsWindowVisible(main_window_hwnd))
			//	ShowWindow(main_window_hwnd, true);
            //bShowConfig = false;
            //InvalidateRect(hwnd, nullptr, true);
            //UpdateWindow(hwnd);
            MessageBox(hwnd, L"The user clicked on the balloon.", L"User click", MB_OK);
            break;

        case WM_CONTEXTMENU:
        {
            POINT const pt = { LOWORD(wParam), HIWORD(wParam) };
            ShowContextMenu(hwnd, pt);
        }
        break;
        }
        break;

    case WMAPP_HIDEFLYOUT:
        HideFlyout(hwnd, s_hwndFlyout);
        s_hwndFlyout = NULL;
        s_fCanShowFlyout = FALSE;
        break;

    case WM_TIMER:
        if (wParam == HIDEFLYOUT_TIMER_ID)
        {
            // please see the comment in HideFlyout() for an explanation of this code.
            KillTimer(hwnd, HIDEFLYOUT_TIMER_ID);
            s_fCanShowFlyout = TRUE;
        }
        break;
    case WM_DESTROY:
        DeleteNotificationIcon();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

void FlyoutPaint(HWND hwnd, HDC hdc)
{
    // Since this is a DPI aware application (see DeclareDPIAware.manifest), if the flyout window
    // were to show text we would need to increase the size. We could also have multiple sizes of
    // the bitmap image and show the appropriate image for each DPI, but that would complicate the
    // sample.
    static HBITMAP hbmp = NULL;
    if (hbmp == NULL)
    {
        hbmp = (HBITMAP)LoadImage(g_hInst, MAKEINTRESOURCE(IDB_PRINTER), IMAGE_BITMAP, 0, 0, 0);
    }
    if (hbmp)
    {
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        HDC hdcMem = CreateCompatibleDC(hdc);
        if (hdcMem)
        {
            HGDIOBJ hBmpOld = SelectObject(hdcMem, hbmp);
            BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, hdcMem, 0, 0, SRCCOPY);
            SelectObject(hdcMem, hBmpOld);
            DeleteDC(hdcMem);
        }
    }
}
LRESULT CALLBACK FlyoutWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_PAINT:
    {
        // paint a pretty picture
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FlyoutPaint(hwnd, hdc);
        EndPaint(hwnd, &ps);
    }
    break;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            // when the flyout window loses focus, hide it.
            PostMessage(GetParent(hwnd), WMAPP_HIDEFLYOUT, 0, 0);
        }
        break;
    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}
