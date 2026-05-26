#define _CRT_SECURE_NO_WARNINGS

#include "his.h"
#include "gui_main.h"
#include <commctrl.h>

/*
 * HIS 系统 GUI 主入口
 */

// ==================== 全局句柄 ====================
HINSTANCE g_hInst    = NULL;
HWND      g_hMainWnd = NULL;
HFONT     g_hFont    = NULL, g_hTitleFont = NULL;

// ==================== 前向声明 ====================
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LoginWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK AdminWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK DoctorWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK PatientSelectWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK PatientMainWndProc(HWND, UINT, WPARAM, LPARAM);

/* 模态结果全局变量 */
int g_modalResult = IDCANCEL;

int DoModal(HWND hDlg, int width, int height) {
    g_modalResult = IDCANCEL;
    HWND hParent = GetWindow(hDlg, GW_OWNER);
    RECT rcParent;
    if (hParent) {
        GetWindowRect(hParent, &rcParent);
    } else {
        /* 无父窗口时以桌面为基准居中 */
        rcParent.left = 0; rcParent.top = 0;
        rcParent.right = GetSystemMetrics(SM_CXSCREEN);
        rcParent.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;
    if (x < 0) x = 0; if (y < 0) y = 0;
    SetWindowPos(hDlg, NULL, x, y, width, height, SWP_NOZORDER);
    if (hParent) EnableWindow(hParent, FALSE);
    /* 模态窗口淡入（AnimateWindow 自动显示窗口） */
    AnimateWindow(hDlg, 200, AW_BLEND);
    SetForegroundWindow(hDlg); UpdateWindow(hDlg);

    MSG msg;
    while (IsWindow(hDlg)) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { PostQuitMessage((int)msg.wParam);
                if (hParent) EnableWindow(hParent, TRUE); return IDCANCEL; }
            TranslateMessage(&msg); DispatchMessage(&msg);
        } Sleep(1);
    }
    if (hParent) { EnableWindow(hParent, TRUE); SetForegroundWindow(hParent); }
    return g_modalResult;
}

static HWND CreateModalWindow(const char* cls, const char* title, DWORD style, int w, int h, HWND hParent) {
    WCHAR wc[64], wt[128]; MultiByteToWideChar(CP_ACP,0,cls,-1,wc,64);
    MultiByteToWideChar(CP_ACP,0,title,-1,wt,128);
    return CreateWindowW(wc,wt,style,CW_USEDEFAULT,CW_USEDEFAULT,w,h,hParent,NULL,g_hInst,NULL);
}

/* ==================== 主窗口 ==================== */
/* 标题字体 */
static HFONT CreateTitleFont(void) {
    LOGFONT lf; ZeroMemory(&lf,sizeof(lf));
    lf.lfHeight = -MulDiv(18, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
    lf.lfWeight = FW_BOLD; lf.lfCharSet = GB2312_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"\x5FAE\x8F6F\x96C5\x9ED1");
    return CreateFontIndirect(&lf);
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static HWND s_hTitle, s_hSub, s_hVersion;
    switch (msg) {
    case WM_CREATE: {
        g_hTitleFont = CreateTitleFont();

        /* 顶部深蓝背景标题栏 */
        CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE,
            0, 0, 400, 48, hWnd, (HMENU)1060, g_hInst, NULL);
        s_hTitle = CreateWindowW(L"STATIC", L"HIS医院信息系统", WS_CHILD|WS_VISIBLE|SS_CENTER,
            0, 10, 400, 28, hWnd, (HMENU)-1, g_hInst, NULL);
        SendMessage(s_hTitle, WM_SETFONT, (WPARAM)g_hTitleFont, TRUE);

        s_hSub = CreateWindowW(L"STATIC", L"医院信息管理系统 v2.0",
            WS_CHILD|WS_VISIBLE|SS_CENTER, 0, 54, 400, 16, hWnd, (HMENU)-1, g_hInst, NULL);

        /* 蓝色分隔线 */
        CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE, 30, 78, 340, 3, hWnd, (HMENU)1061, g_hInst, NULL);

        /* 底部蓝色装饰条 */
        CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE, 0, 262, 400, 6, hWnd, (HMENU)1062, g_hInst, NULL);

        DWORD flatBtn = WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_FLAT;
        CreateWindowW(L"BUTTON", L"   管理员登录", flatBtn,
             50, 95, 140, 55, hWnd, (HMENU)IDC_BTN_ADMIN, g_hInst, NULL);
        CreateWindowW(L"BUTTON", L"   医生登录", flatBtn,
             210, 95, 140, 55, hWnd, (HMENU)IDC_BTN_DOCTOR, g_hInst, NULL);
        CreateWindowW(L"BUTTON", L"   患者自助服务", flatBtn,
             50, 165, 140, 55, hWnd, (HMENU)IDC_BTN_PATIENT, g_hInst, NULL);
        CreateWindowW(L"BUTTON", L"   退出系统", flatBtn,
             210, 165, 140, 55, hWnd, (HMENU)IDC_BTN_EXIT, g_hInst, NULL);

        s_hVersion = CreateWindowW(L"STATIC", L"版本 2.0.0", WS_CHILD|WS_VISIBLE|SS_CENTER,
            0, 248, 400, 15, hWnd, (HMENU)-1, g_hInst, NULL);

        gui_SetDialogFont(hWnd, g_hFont);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)w;
        HWND hw = (HWND)l;
        int id = GetDlgCtrlID(hw);
        /* 蓝色装饰条 */
        if (id == 1060) {
            static HBRUSH hHdr = NULL;
            if (!hHdr) hHdr = CreateSolidBrush(RGB(25, 85, 150));
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)hHdr;
        }
        if (id == 1061 || id == 1062) {
            static HBRUSH hAcc = NULL;
            if (!hAcc) hAcc = CreateSolidBrush(RGB(55, 130, 200));
            return (LRESULT)hAcc;
        }
        if (hw == s_hTitle) {
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkMode(hdc, TRANSPARENT);
            static HBRUSH hNull = NULL;
            if (!hNull) hNull = CreateSolidBrush(RGB(25, 85, 150));
            return (LRESULT)hNull;
        }
        if (hw == s_hSub) {
            SetTextColor(hdc, RGB(50, 100, 160));
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)(HBRUSH)GetStockObject(NULL_BRUSH);
        }
        if (hw == s_hVersion) {
            SetTextColor(hdc, RGB(150, 160, 170));
            SetBkMode(hdc, TRANSPARENT);
            return (LRESULT)(HBRUSH)GetStockObject(NULL_BRUSH);
        }
        return DefWindowProc(hWnd, msg, w, l);
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDC_BTN_ADMIN: {
            HWND hW = CreateModalWindow("HIS_LoginWnd", "管理员登录", WS_CAPTION|WS_SYSMENU, 280, 210, hWnd);
            if (DoModal(hW, 280, 210) == IDOK) {
                HWND hA = CreateWindowW(L"HIS_AdminWnd", L"管理员界面",
                    WS_CAPTION|WS_SYSMENU|WS_MAXIMIZEBOX|WS_SIZEBOX, CW_USEDEFAULT,CW_USEDEFAULT,
                    720, 520, hWnd, NULL, g_hInst, NULL);
                DoModal(hA, 720, 520); DestroyWindow(hA);
            }
            DestroyWindow(hW); break;
        }
        case IDC_BTN_DOCTOR: {
            HWND hW = CreateModalWindow("HIS_LoginWnd", "医生登录", WS_CAPTION|WS_SYSMENU, 280, 210, hWnd);
            if (DoModal(hW, 280, 210) == IDOK) {
                HWND hD = CreateWindowW(L"HIS_DoctorWnd", L"医生工作站",
                    WS_CAPTION|WS_SYSMENU|WS_MAXIMIZEBOX|WS_SIZEBOX, CW_USEDEFAULT,CW_USEDEFAULT,
                    720, 520, hWnd, NULL, g_hInst, NULL);
                DoModal(hD, 720, 520); DestroyWindow(hD);
                g_current_doctor_id[0] = '\0'; g_current_doctor_name[0] = '\0';
            }
            DestroyWindow(hW); break;
        }
        case IDC_BTN_PATIENT: {
            HWND hW = CreateModalWindow("HIS_PatientSelectWnd", "患者自助服务",
                       WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, 520, 440, hWnd);
            if (DoModal(hW, 520, 440) == IDOK) {
                HWND hP = CreateWindowW(L"HIS_PatientMainWnd", L"患者自助服务",
                    WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, CW_USEDEFAULT,CW_USEDEFAULT,
                    520, 460, hWnd, NULL, g_hInst, (LPVOID)g_patientId);
                DoModal(hP, 520, 460); DestroyWindow(hP);
            }
            DestroyWindow(hW); break;
        }
        case IDC_BTN_EXIT:
            if (gui_Confirm(hWnd, "确认退出系统？"))
                SendMessage(hWnd, WM_CLOSE, 0, 0);
            break;
        }
        return 0;
    case WM_CLOSE:
        saveAllHisData(); freeGlobalLists(); DestroyWindow(hWnd); PostQuitMessage(0);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)l;
        mmi->ptMinTrackSize.x = 400;
        mmi->ptMinTrackSize.y = 300;
        return 0;
    }
    }
    return DefWindowProc(hWnd, msg, w, l);
}

/* ==================== 窗口类注册 ==================== */

static int RegisterAllClasses(void) {
    WNDCLASS wc = { 0 };
    wc.style = CS_HREDRAW | CS_VREDRAW; wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszMenuName = NULL;

    wc.lpfnWndProc = MainWndProc; wc.lpszClassName = L"HIS_MainWnd";
    wc.hbrBackground = CreateSolidBrush(RGB(235, 242, 250)); /* 医疗蓝背景 */
    if (!RegisterClass(&wc)) return 0;

    wc.lpfnWndProc = LoginWndProc; wc.lpszClassName = L"HIS_LoginWnd";
    wc.hbrBackground = CreateSolidBrush(RGB(240, 243, 248)); /* 登录页背景 */
    if (!RegisterClass(&wc)) return 0;

    wc.lpfnWndProc = AdminWndProc; wc.lpszClassName = L"HIS_AdminWnd";
    wc.hbrBackground = CreateSolidBrush(RGB(242, 244, 249)); /* 管理页背景 */
    if (!RegisterClass(&wc)) return 0;

    wc.lpfnWndProc = DoctorWndProc; wc.lpszClassName = L"HIS_DoctorWnd";
    wc.hbrBackground = CreateSolidBrush(RGB(240, 246, 242)); /* 医生页浅绿背景 */
    if (!RegisterClass(&wc)) return 0;

    wc.lpfnWndProc = PatientSelectWndProc; wc.lpszClassName = L"HIS_PatientSelectWnd";
    wc.hbrBackground = CreateSolidBrush(RGB(238, 245, 244)); /* 患者页浅青背景 */
    if (!RegisterClass(&wc)) return 0;

    wc.lpfnWndProc = PatientMainWndProc; wc.lpszClassName = L"HIS_PatientMainWnd";
    wc.hbrBackground = CreateSolidBrush(RGB(238, 245, 244));
    return RegisterClass(&wc);
}

void gui_RefreshAllViews(void) {
    if (g_hMainWnd && IsWindow(g_hMainWnd))
        PostMessage(g_hMainWnd, WM_REFRESH_DATA, 0, 0);
}

/* ==================== WinMain ==================== */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nShowCmd) {
    (void)hPrevInstance; (void)lpCmdLine;

    INITCOMMONCONTROLSEX ix = { sizeof(ix), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&ix);
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
    g_hInst = hInstance;
    g_hFont = gui_CreateDefaultFont();
    if (!RegisterAllClasses()) {
        gui_MsgBox(NULL, "注册窗口类失败！", "错误", MB_ICONERROR); return 1;
    }
    gui_RegisterAdminClasses(hInstance);
    gui_RegisterDoctorClasses(hInstance);
    gui_RegisterPatientClasses(hInstance);
    gui_RegisterInputDlgClass(hInstance);

    initGlobalLists(); loadAllHisData();

    g_hMainWnd = CreateWindowW(L"HIS_MainWnd", L"HIS医院信息系统",
        WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        NULL, NULL, hInstance, NULL);
    if (!g_hMainWnd) {
        gui_MsgBox(NULL, "创建主窗口失败！", "错误", MB_ICONERROR); return 1;
    }
    /* 主屏幕居中 */
    { int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
      SetWindowPos(g_hMainWnd, NULL, (sw - 400) / 2, (sh - 300) / 2, 0, 0, SWP_NOSIZE); }
    /* 主窗口淡入（AnimateWindow 自动显示窗口） */
    AnimateWindow(g_hMainWnd, 300, AW_BLEND);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    if (g_hFont) { DeleteObject(g_hFont); g_hFont = NULL; }
    if (g_hTitleFont) { DeleteObject(g_hTitleFont); g_hTitleFont = NULL; }
    return (int)msg.wParam;
}
